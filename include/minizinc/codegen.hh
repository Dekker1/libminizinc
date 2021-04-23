/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Graeme Gange <graeme.gange@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_CODEGEN_HH__
#define __MINIZINC_CODEGEN_HH__

// Simple single-pass bytecode compiler for MiniZinc.

#include <minizinc/codegen_support.hh>
#include <minizinc/flatten_internal.hh>
#include <minizinc/interpreter.hh>

#include <iostream>
#include <set>
#include <vector>

namespace MiniZinc {

struct CodeGen;

// Location is either a register or global.
class Loc {
private:
  Loc(int _x) : x(_x) {}

public:
  static Loc reg(int r) { return r << 1; }
  static Loc global(int g) { return (g << 1) | 1; }

  inline bool is_reg(void) const { return !(x & 1); }
  inline bool is_global(void) const { return x & 1; }
  inline int index(void) const { return x >> 1; }

  int x;
};

// Code generators for structures.
// Expressions can be executed in three modes:
// - If Boolean, it is always just executed.
// - If non-Boolean but total, it is similarly just executed.
// - If non-Boolean and partial, we first evaluate its partiality
//   constraints in the enclosing Boolean expression, _then_
//   evaluate its numeric component.
// For any function that is not total, we generate two procedures:
// one which is
// A slightly more structured representation for code generation.
class CG_Value {
public:
  enum CG_ValueKind { V_Immi, V_Global, V_Reg, V_Proc, V_Label };

protected:
  CG_Value(CG_ValueKind _kind, int _value) : kind(_kind), value(_value) {}

public:
  CG_Value(void) : kind(V_Immi), value(0) {}
  static CG_Value reg(int r) { return CG_Value(V_Reg, r); }
  static CG_Value global(int r) { return CG_Value(V_Global, r); }
  static CG_Value immi(long long int r) { return CG_Value(V_Immi, r); }
  static CG_Value proc(int p) { return CG_Value(V_Proc, p); }
  static CG_Value label(int l) { return CG_Value(V_Label, l); }

  CG_ValueKind kind;
  long long int value;
};

struct CG_Instr {
protected:
  CG_Instr(unsigned int _tag) : tag(_tag) {}

public:
  static CG_Instr instr(BytecodeStream::Instr i) { return static_cast<unsigned int>(i) << 1; }
  static CG_Instr label(unsigned int l) { return (l << 1) + 1; }

  unsigned int tag;
  std::vector<CG_Value> params;
};

struct CG_ProcID {
protected:
  CG_ProcID(int _p) : p(_p) {}

public:
  static CG_ProcID builtin(int b) { return CG_ProcID((b << 1) | 1); }
  static CG_ProcID proc(int p) { return CG_ProcID(p << 1); }
  bool is_builtin(void) const { return p & 1; }
  unsigned int id(void) const { return p >> 1; }

  static CG_ProcID of_val(CG_Value v) {
    assert(v.kind == CG_Value::V_Proc);
    return CG_ProcID(v.value);
  }

  unsigned int p;
};

struct CG_Builder {
  std::vector<CG_Instr> instrs;

  void append(CG_Builder& o) {
    instrs.insert(instrs.end(), o.instrs.begin(), o.instrs.end());
    o.instrs.clear();
  }
  void clear(void) { instrs.clear(); }
};

// When we bind a non-Boolean expression, we also construct
// a set of conditions we need to insert into any use-contexts.

// We use CG_Cond to track the conditionality of values.
struct CG_Cond {
  // FIXME: Currently not GC'd, so this will leak a bunch of memory.
  enum Kind { CC_Reg, CC_Call, CC_And };
  class C_Reg;
  class C_Call;
  class C_And;

  struct cond_reg {
    cond_reg(void) : is_root(0), is_seen(0), is_par(0), reg(-1) {}
    cond_reg(int _reg, bool _par) : is_root(0), is_seen(0), is_par(_par), reg(_reg) {}

    int operator*(void) const {
      assert(reg >= 0);
      return reg;
    }
    bool has_reg(void) const { return reg >= 0; }

    int is_root : 1;
    int is_seen : 1;
    int is_par : 1;
    int reg : 29;
  };

  class _T {
  public:
    cond_reg reg[2];

    _T(void) {}
    _T(int r, bool is_par) {
      reg[0].reg = r;
      reg[0].is_par = is_par;
    }

    virtual Kind kind(void) const = 0;
  };

  class T {
    T(uintptr_t _p) : p(_p) {}

  public:
    T(void) : p(0) {}

    static T ttt(void) { return T(0); }
    static T fff(void) { return T(1); }
    static T of_ptr(_T* p) { return T(reinterpret_cast<uintptr_t>(p)); }

    T operator~(void) const { return T(p ^ 1); }
    T operator^(bool b) const { return T(p ^ b); }

    bool sign(void) const { return p & 1; }
    _T* get(void) const { return reinterpret_cast<_T*>(p & ~((uintptr_t)1)); }

    uintptr_t p;
  };

  static T ttt(void) { return T::ttt(); }
  static T fff(void) { return T::fff(); }

  class C_Reg : public _T {
  public:
    static const Kind _kind = CC_Reg;
    Kind kind(void) const { return _kind; }
    C_Reg(int reg, bool is_par) : _T(reg, is_par) {}
  };
  class C_Call : public _T {
  public:
    static const Kind _kind = CC_Call;
    Kind kind(void) const { return _kind; }

    C_Call(ASTString _ident, BytecodeProc::Mode _m, bool _cse, const std::vector<Type>& _ty,
           const std::vector<CG_Value>& _params)
        : ident(_ident), m(std::move(_m)), ty(_ty), cse(_cse), params(_params) {}

    ASTString ident;
    // Return Types + argument types
    BytecodeProc::Mode m;
    bool cse;
    std::vector<Type> ty;
    std::vector<CG_Value> params;
  };
  class C_And : public _T {
  public:
    static const Kind _kind = CC_And;
    Kind kind(void) const { return _kind; }

    C_And(BytecodeProc::Mode _m, std::vector<T>& _children) : m(_m), children(_children) {
      assert(children.size() > 1);
    }

    BytecodeProc::Mode m;
    std::vector<T> children;
  };

  static T reg(int r, bool is_par) { return T::of_ptr(new C_Reg(r, is_par)); }

  static T call(ASTString ident, BytecodeProc::Mode m, bool cse, const std::vector<Type>& ty,
                const std::vector<CG_Value>& params) {
    return _call(ident, m, cse, ty, params);
  }

  static T _call(ASTString ident, BytecodeProc::Mode m, bool cse, const std::vector<Type>& ty,
                 const std::vector<CG_Value>& params) {
    return T::of_ptr(new C_Call(ident, m, cse, ty, params));
  }

  template <typename... Args>
  static T _forall(BytecodeProc::Mode m, std::vector<T>& args, T next, Args... rest) {
    args.push_back(next);
    return _forall(m, args, rest...);
  }
  template <class It>
  static void clear_seen(It b, It e) {
    for (; b != e; ++b) {
      if (!b->get()) continue;
      b->get()->reg[b->sign()].is_seen = false;
    }
  }
  static T _forall(BytecodeProc::Mode m, std::vector<T>& args) {
    size_t count = 0;
    for (T x : args) {
      _T* p(x.get());
      if (!p) {
        // Either true or false.
        if (x.sign()) {
          clear_seen(args.begin(), args.end());
          return T::fff();
        }
        continue;
      }
      // Otherwise, check if we've already seen this or its negation.
      if (p->reg[1 - x.sign()].is_seen || p->reg[1 - x.sign()].is_root) {
        clear_seen(args.begin(), args.end());
        return T::fff();
      }
      if (p->reg[x.sign()].is_seen || p->reg[x.sign()].is_root) continue;
      // Haven't seen this yet, so save and mark it.
      p->reg[x.sign()].is_seen = true;
      args[count] = x;
      count++;
    }
    clear_seen(args.begin(), args.end());
    args.resize(count);
    if (count == 0) {
      return T::ttt();
    }
    if (count == 1) {
      return args[0];
    }
    return T::of_ptr(new C_And(m, args));
  }

  template <typename... Args>
  static T forall(BytecodeProc::Mode m, Args... args) {
    std::vector<CG_Cond::T> vec;
    return _forall(m, vec, args...);
  }
  static T forall(BytecodeProc::Mode m, std::vector<T>& args) { return _forall(m, args); }

  template <typename... Args>
  static T _exists(BytecodeProc::Mode m, std::vector<T>& args, T next, Args... rest) {
    args.push_back(next);
    return _exists(m, args, rest...);
  }
  static T _exists(BytecodeProc::Mode m, std::vector<T>& args);

  template <typename... Args>
  static T exists(BytecodeProc::Mode m, Args... args) {
    std::vector<T> vec;
    return _exists(m, vec, args...);
  }
  static T exists(BytecodeProc::Mode m, std::vector<T>& args) { return _exists(m, args); }
};

// An environment should never outlive its parent.
template <class T>
class CG_Env {
private:
  CG_Env(CG_Env<T>* _p) : p(_p), sz(p ? p->sz : 0) {}

  // Forbid copy and assignment operators.
  CG_Env(const CG_Env& _o) = delete;
  CG_Env& operator=(const CG_Env& o) = delete;

public:
  CG_Env(void) : p(nullptr), sz(0) {}
  CG_Env(CG_Env&& o)
      : bindings(std::move(o.bindings)),
        available(std::move(o.available)),
        available_csts(std::move(o.available_csts)),
        available_ranges(std::move(o.available_ranges)),
        cached_conds(std::move(o.cached_conds)),
        occurs(std::move(o.occurs)),
        p(o.p),
        sz(o.sz) {}

  void clear_cached_conds() {
    for (CG_Cond::T c : cached_conds) {
      CG_Cond::_T* p(c.get());
      bool sign(c.sign());
      p->reg[sign].reg = -1;
    }
    cached_conds.clear();
  }
  void record_cached_cond(CG_Cond::T c) { cached_conds.push_back(c); }

  class NotFound : public std::exception {
  public:
    NotFound(void) {}
  };

  T lookup(const ASTString& s) const {
    auto it(bindings.find(s));
    if (it != bindings.end()) return (*it).second;
    if (!p) throw NotFound();

    return p->lookup(s);
  }

  void bind(const ASTString& s, T val) {
    auto it(bindings.find(s));
    if (it != bindings.end())
      bindings.erase(it);
    else
      sz++;
    bindings.insert(std::make_pair(s, val));

    // Invalidate any cached values mentioning s.
    auto o_it(occurs.find(s));
    if (o_it != occurs.end()) {
      // We're lazy here, in that we don't remove e from
      // other occurs lists.
      for (Expression* e : (*o_it).second) available.erase(e);
      occurs.erase(o_it);
    }
  }

  // Check whether e is already available in an enclosing environment.
  T cache_lookup(Expression* e, ASTStSet e_scope) {
    auto it(available.find(e));
    // Anything in the current table is hasn't been invalidated.
    if (it != available.end()) {
      return (*it).second;
    }
    if (!p) throw NotFound();

    // If there's a parent table, check whether we've re-bound
    // something in its scope.
    for (auto p : bindings) {
      if (e_scope.find(p.first) != e_scope.end()) throw NotFound();
    }
    // If the scope hasn't been invalidated, check the parent.
    return p->cache_lookup(e, e_scope);
  }

  void cache_store(Expression* e, ASTStSet e_scope, T val) {
    available.insert(std::make_pair(e, val));
    // Add e to the occurs lists for variables in its scope.
    for (ASTString s : e_scope) occurs[s].push_back(e);
  }

  bool cache_lookup_cst(int x, T& ret) {
    auto it(available_csts.find(x));
    // Anything in the current table is hasn't been invalidated.
    if (it != available_csts.end()) {
      ret = (*it).second;
      return true;
    }
    if (!p) return false;
    return p->cache_lookup_cst(x, ret);
  }
  void cache_store_cst(int x, T val) { available_csts.insert(std::make_pair(x, val)); }

  static uint64_t range_key(int l, int u) { return (((uint64_t)u) << 32ull | (uint64_t)l); }
  bool cache_lookup_range(int l, int u, T& ret) {
    auto it(available_ranges.find(range_key(l, u)));
    // Anything in the current table is hasn't been invalidated.
    if (it != available_ranges.end()) {
      ret = (*it).second;
      return true;
    }
    if (!p) return false;
    return p->cache_lookup_range(l, u, ret);
  }
  void cache_store_range(int l, int u, T val) {
    available_ranges.insert(std::make_pair(range_key(l, u), val));
  }

  static CG_Env* spawn(CG_Env* p) { return new CG_Env<T>(p); }

  unsigned int size(void) const { return sz; }

  typename ASTStringMap<T>::t bindings;

  typename ExprMap<T>::t available;
  std::unordered_map<int, T> available_csts;
  std::unordered_map<uint64_t, T> available_ranges;
  //  std::unordered_map<std::pair<int, int>, T> available_ranges;
  typename ASTStringMap<std::vector<Expression*>>::t occurs;

  std::vector<CG_Cond::T> cached_conds;

  // Parent environment.
  CG_Env<T>* p;
  unsigned int sz;
};

struct CG {
  typedef std::pair<int, CG_Cond::T> Binding;

  // This is currently (probably) sound, but unnecessarily weak. If an expression ever appears in
  // root context, the root appearance should dominate.
  struct Mode {
    enum Strength { Root = 0, Imp = 1, Fun = 2 };
    Mode(BytecodeProc::Mode _m) : m(_m) {}
    Mode(Strength s, bool is_neg) {
      switch (s) {
        case Root:
          m = is_neg ? BytecodeProc::ROOT_NEG : BytecodeProc::ROOT;
          break;
        case Imp:
          m = is_neg ? BytecodeProc::IMP_NEG : BytecodeProc::IMP;
          break;
        case Fun:
          m = is_neg ? BytecodeProc::FUN_NEG : BytecodeProc::FUN;
          break;
      }
    }

    bool is_neg(void) const {
      switch (m) {
        case BytecodeProc::ROOT_NEG:
        case BytecodeProc::IMP_NEG:
        case BytecodeProc::FUN_NEG:
          return true;
        default:
          return false;
      }
    }

    Strength strength(void) const {
      switch (m) {
        case BytecodeProc::ROOT:
        case BytecodeProc::ROOT_NEG:
          return Root;
        case BytecodeProc::IMP:
        case BytecodeProc::IMP_NEG:
          return Imp;
        case BytecodeProc::FUN:
        case BytecodeProc::FUN_NEG:
          return Fun;
        default:
          throw InternalError("Unexpected mode.");
      }
    }
    bool is_root(void) const { return strength() == Root; }

    Mode join(Mode o) {
      if (is_neg() != o.is_neg()) return BytecodeProc::FUN;
      return Mode(std::max(strength(), o.strength()), is_neg());
    }
    bool is_submode(Mode o) { return is_neg() == o.is_neg() && strength() <= o.strength(); }

    // Half
    Mode operator+(void) const { return Mode(strength() == Root ? Imp : strength(), is_neg()); }
    Mode operator-(void) const { return Mode(strength(), !is_neg()); }

    // Switch the current mode to functional.
    Mode operator*(void) const { return Mode(Fun, is_neg()); }

    operator BytecodeProc::Mode() const { return m; }

    BytecodeProc::Mode m;
  };

  inline static CG_Value g(int g) { return CG_Value::global(g); }
  inline static CG_Value r(int r) { return CG_Value::reg(r); }
  inline static CG_Value i(int i) { return CG_Value::immi(i); }
  inline static CG_Value l(int l) { return CG_Value::label(l); }

  // Place a non-Boolean value in a register, and collect its partiality.
  static Binding bind(Expression* e, CodeGen& cg, CG_Builder& frag);
  // Compile a Boolean expression into a condition.
  static CG_Cond::T compile(Expression* e, CodeGen& cg, CG_Builder& frag);
  // Reify a condion, putting it in a register.
  static int force(CG_Cond::T cond, Mode ctx, CodeGen& cg, CG_Builder& frag);
  // Force compiled expression or Bind value depending on type
  static Binding force_or_bind(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static int force_or_bind(Expression* e, Mode ctx, std::vector<CG_Cond::T>& cond, CodeGen& cg,
                           CG_Builder& frag);

  static void run(CodeGen& cg, Model* m);

  static int locate_immi(int x, CodeGen& cg, CG_Builder& frag);

  static int vec2array(std::vector<int> vec, CodeGen& cg, CG_Builder& frag);

  static Binding bind(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(SetLit* l, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(BinOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static Binding bind(Comprehension* let, Mode ctx, CodeGen& cg, CG_Builder& frag);

  static CG_Cond::T compile(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(BinOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T compile(Comprehension* let, Mode ctx, CodeGen& cg, CG_Builder& frag);
};

inline CG_Cond::T CG_Cond::_exists(BytecodeProc::Mode m, std::vector<T>& args) {
  size_t count = 0;
  for (T x : args) {
    _T* p(x.get());
    if (!p) {
      // Either true or false.
      if (!x.sign()) {
        clear_seen(args.begin(), args.end());
        return T::ttt();
      }
      continue;
    }
    // Otherwise, check if we've already seen this or its negation.
    if (p->reg[1 - x.sign()].is_seen || p->reg[x.sign()].is_root) {
      clear_seen(args.begin(), args.end());
      return T::ttt();
    }
    if (p->reg[x.sign()].is_seen || p->reg[1 - x.sign()].is_root) {
      continue;
    }
    // Haven't seen this yet, so save and mark it.
    p->reg[x.sign()].is_seen = true;
    args[count] = x;
    count++;
  }
  clear_seen(args.begin(), args.end());
  args.resize(count);
  if (count == 0) {
    return T::fff();
  }
  if (count == 1) {
    return args[0];
  }
  return ~T::of_ptr(new C_And(-CG::Mode(m), args));
}

// Partially compiled bytecode.
struct CG_Proc {
  typedef std::vector<CG_Instr> body_t;

  static unsigned char mode_mask(BytecodeProc::Mode m) {
    return 1 << (static_cast<unsigned char>(m));
  }
  struct mode_iterator {
    mode_iterator(unsigned int _x) : x(_x) {}
    bool operator!=(const mode_iterator& o) const { return x != o.x; }
    BytecodeProc::Mode operator*(void) const {
      assert(x);
      return static_cast<BytecodeProc::Mode>(find_lsb(x));
    }
    mode_iterator& operator++(void) {
      x &= (x - 1);
      return *this;
    }

    unsigned int x;
  };
  mode_iterator begin(void) { return mode_iterator(available_modes); }
  mode_iterator end(void) { return mode_iterator(0); }

  CG_Proc(std::string _ident, int _arity) : ident(_ident), arity(_arity), available_modes(0) {}

  CG_Proc(CG_Proc&& o) : ident(o.ident), arity(o.arity), available_modes(o.available_modes) {
    unsigned char rm(available_modes);
    while (rm) {
      unsigned char m(find_lsb(rm));
      rm &= (rm - 1);
      new (_body + m) body_t(std::move(o._body[m]));
      o._body[m].~body_t();
    }
    o.available_modes = 0;
  }

  std::string ident;
  unsigned int arity;

  bool is_available(BytecodeProc::Mode m) const { return available_modes & mode_mask(m); }

  std::vector<CG_Instr>& body(BytecodeProc::Mode m) {
    static_assert(BytecodeProc::MAX_MODE < 8 * sizeof(unsigned char),
                  "Too many modes to to represent as unsigned char.");

    if (!(available_modes & mode_mask(m))) {
      available_modes |= mode_mask(m);
      new (_body + m) body_t();
    }
    return _body[m];
  }

  unsigned char available_modes;
  std::vector<CG_Instr> _body[BytecodeProc::MAX_MODE + 1];
};

// For identifying a call...
struct CallSig {
  ASTString id;
  std::vector<Type> params;

  CallSig(ASTString _id, std::vector<Type> _params) : id(_id) {
    // Normalize the call types to par.
    for (Type p : _params) {
      p.ti(Type::TI_PAR);
      params.push_back(p);
    }
  }

  struct HashSig {
    size_t operator()(const CallSig& c) const { return c.hash(); }
  };
  struct EqSig {
    bool operator()(const CallSig& x, const CallSig& y) const { return x == y; }
  };

  bool operator==(const CallSig& o) const {
    if (id != o.id || params.size() != o.params.size()) return false;
    for (int ii = 0; ii < params.size(); ++ii) {
      if (params[ii] != o.params[ii]) return false;
    }
    return true;
  }

  size_t hash(void) const {
    size_t h(id.hash());
    for (int ii = 0; ii < params.size(); ++ii)
      h ^= params[ii].toInt() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

template <class T>
struct SigMap {
  typedef std::unordered_map<CallSig, T, CallSig::HashSig, CallSig::EqSig> t;
};

struct CG_FunMap {
  struct CG_FunDefn {
    CG_FunDefn(ASTString _id) : id(_id) {}

    ASTString id;
    std::vector<FunctionI*> bodies;
  };

  ASTStringMap<unsigned int>::t id_map;
  std::vector<CG_FunDefn> functions;

  void add_body(FunctionI* f) {
    // FIXME: Currently discarding anything with var-set.
    ASTExprVec<VarDecl> params(f->params());
    for (int ii = 0; ii < params.size(); ++ii) {
      if (params[ii]->type().is_set() && !params[ii]->type().ispar()) return;
    }

    unsigned int fun_id;
    ASTString id(f->id());
    auto it(id_map.find(id));
    if (it != id_map.end()) {
      fun_id = (*it).second;
    } else {
      fun_id = functions.size();
      id_map.insert(std::make_pair(id, fun_id));
      functions.push_back(CG_FunDefn(id));
    }
    functions[fun_id].bodies.push_back(f);
  }

  bool dominates(FunctionI* f, FunctionI* g) {
    auto f_params(f->params());
    auto g_params(g->params());
    assert(f_params.size() == g_params.size());
    int sz(f_params.size());
    for (int ii = 0; ii < sz; ++ii) {
      if (!Type::bt_subtype(f_params[ii]->type(), g_params[ii]->type(), false)) return false;
    }
    return true;
  }
  void filter_bodies(std::vector<FunctionI*>::iterator& dest, std::vector<FunctionI*>::iterator b,
                     std::vector<FunctionI*>::iterator e, int arg, int sz) {
    if (!(b != e))  // Empty partition
      return;
    if (arg == sz) {
      // Find the best candidate between b and e, add it to the output.
      // FIXME
      auto best(b);
      for (++b; b != e; ++b) {
        if (dominates(*b, *best)) best = b;
      }
      (*dest) = (*best);
      ++dest;
      return;
    }
    // Otherwise, partition the arguments and recurse.
    std::vector<FunctionI*>::iterator mid =
        std::partition(b, e, [arg](FunctionI* b) { return b->params()[arg]->type().ispar(); });
    filter_bodies(dest, b, mid, arg + 1, sz);
    filter_bodies(dest, mid, e, arg + 1, sz);
  }

  std::vector<FunctionI*> get_bodies(unsigned int fun_id, const std::vector<Type>& args) {
    CG_FunDefn& defn(functions[fun_id]);

    // First, restrict consideration to feasible specialisations.
    std::vector<FunctionI*> candidates;
    int sz = args.size();
    for (FunctionI* b : defn.bodies) {
      ASTExprVec<VarDecl> b_params(b->params());
      if (b_params.size() == sz) {
        for (int pi = 0; pi < sz; ++pi) {
          if (!args[pi].isSubtypeOf(b_params[pi]->type(), false)) goto get_bodies_continue;
        }
        // Can coerce args to b_params.
        candidates.push_back(b);
      }
    get_bodies_continue:
      continue;
    }
    // Now collect the relevant par-based refinements.
    std::vector<FunctionI*>::iterator dest(candidates.begin());
    filter_bodies(dest, candidates.begin(), candidates.end(), 0, sz);
    candidates.erase(dest, candidates.end());
    return candidates;
  }

  std::vector<FunctionI*> get_bodies(const ASTString& ident, const std::vector<Type>& args) {
    auto it(id_map.find(ident));
    if (it == id_map.end()) return {};
    unsigned int fun_id((*it).second);
    return get_bodies(fun_id, args);
  }

  std::vector<FunctionI*> get_bodies(Call* call) {
    // Normalize all types to par, so isSubtype does what we want.
    std::vector<Type> args;
    int sz(call->n_args());
    for (int ii = 0; ii < sz; ++ii) {
      Type arg(call->arg(ii)->type());
      arg.ti(Type::TI_PAR);
      args.push_back(arg);
    }
    return get_bodies(call->id(), args);
  }

  std::pair<bool, ASTString> defines_mode(const ASTString& ident, const std::vector<Type>& args,
                                          BytecodeProc::Mode mode) {
    GCLock lock;
    ASTString nident;
    switch (mode) {
      case BytecodeProc::ROOT_NEG:
        nident = ident.str() + "_neg";
        break;
      case BytecodeProc::FUN:
        nident = ident.str() + "_reif";
        break;
      case BytecodeProc::FUN_NEG:
        nident = ident.str() + "_neg_reif";
        break;
      case BytecodeProc::IMP:
        nident = ident.str() + "_imp";
        break;
      case BytecodeProc::IMP_NEG:
        nident = ident.str() + "_neg_imp";
        break;
      default:
        return {false, ASTString("")};
    }
    auto it(id_map.find(nident));
    if (it == id_map.end()) {
      return {false, nident};
    }
    unsigned int fun_id((*it).second);
    std::vector<Type> reif_args;
    reif_args.reserve(args.size() + 1);
    for (int ii = 0; ii < args.size(); ++ii) {
      Type arg(args[ii]);
      arg.ti(Type::TI_PAR);
      reif_args.push_back(arg);
    }
    if (mode != BytecodeProc::ROOT && mode != BytecodeProc::ROOT_NEG) {
      reif_args.push_back(Type::parbool());
    }
    return {!get_bodies(fun_id, reif_args).empty(), nident};
  }
};

struct CodeGen {
  typedef unsigned int proc_id;
  typedef unsigned int reg_id;
  typedef std::pair<int, CG_Cond::T> Binding;
  CodeGen(void)
      : /*entry_proc(0)
      ,*/
        current_env(new CG_Env<Binding>()),
        num_globals(0),
        current_reg_count(0),
        current_label_count(0) {
    register_builtins();
  }

  void append(int proc, BytecodeProc::Mode m, CG_Builder& b) {
    std::vector<CG_Instr>& body(bytecode[proc].body(m));

    body.insert(body.end(), b.instrs.begin(), b.instrs.end());
    b.clear();
  }

  void env_push(void) { current_env = CG_Env<Binding>::spawn(current_env); }
  void env_pop(void) {
    assert(current_env);
    CG_Env<Binding>* c(current_env);
    c->clear_cached_conds();
    current_env = current_env->p;
    delete c;
  }

  // Consult/update the available expressions in the current environment.
  Binding cache_lookup(Expression* e);
  void cache_store(Expression* e, Binding l);

  // Function resolution
  void register_function(FunctionI* f) { fun_map.add_body(f); }

  CG_ProcID resolve_fun(FunctionI* f, bool reserved_name = false);
  // CG_ProcID resolve_fun_pred(FunctionI* f);
  // CG_ProcID resolve_pred_def(FunctionI* f, BytecodeProc::Mode m);

  std::vector<CG_Proc> bytecode;  // Bytecode we've built

  inline CG_Env<Binding>& env(void) { return *current_env; }

  CG_Env<Binding>* current_env;  // Where are things in scope?
  // Id -> <Register, input?>
  std::unordered_map<VarDecl*, std::pair<int, bool>> globals_env;
  int num_globals;
  std::vector<FunctionI*> req_solver_predicates;

  int add_global(VarDecl* vd, bool input = false) {
    globals_env.insert(std::make_pair(vd, std::make_pair(num_globals, input)));
    return num_globals++;
  }

  int find_global(VarDecl* str) {
    auto pair = globals_env.at(str);
    return pair.first;
  }

  std::vector<unsigned int> reg_trail;
  unsigned int current_reg_count;  // How many registers have been used?
  unsigned int current_label_count;

  // Helper information. For an expression, which variables does it refer to?
  ASTStSet scope(Expression* e);
  ExprMap<ASTStSet>::t _exp_scope;

  ExprMap<CG::Mode>::t mode_map;

  // Procedure information
  void register_builtins(void);
  CG_ProcID register_builtin(std::string s, unsigned int p);
  CG_ProcID find_builtin(std::string s);
  std::vector<std::pair<std::string, unsigned int>> _builtins;
  std::unordered_map<std::string, CG_ProcID> _proc_map;

  // Procedures yet to be emitted.
  CG_FunMap fun_map;

  SigMap<std::pair<CG_ProcID, bool>>::t dispatch;

  std::unordered_map<FunctionI*, CG_ProcID> fun_bodies;
  std::vector<std::pair<FunctionI*, std::pair<BytecodeProc::Mode, BytecodeProc::Mode>>>
      pending_bodies;
};

const char* instr_name(BytecodeStream::Instr i);
const char* agg_name(AggregationCtx::Symbol s);
const char* mode_name(BytecodeProc::Mode m);

std::tuple<CG_ProcID, BytecodeProc::Mode, bool> find_call_fun(CodeGen& cg, const ASTString& ident,
                                                              const Type& ret_type,
                                                              std::vector<Type> arg_types,
                                                              BytecodeProc::Mode m,
                                                              bool reserved_name = false);
std::tuple<CG_ProcID, BytecodeProc::Mode, bool> find_call_fun(CodeGen& cg, Call* call,
                                                              BytecodeProc::Mode m,
                                                              bool reserved_name = false);

void eval_let_body(Let* let, BytecodeProc::Mode ctx, CodeGen& cg, CG_Builder& frag,
                   std::vector<CG_Cond::T>& conj);

};  // namespace MiniZinc

#endif
