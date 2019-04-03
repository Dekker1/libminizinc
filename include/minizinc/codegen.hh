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

#include <vector>
#include <set>
#include <iostream>

#include <minizinc/bytecode.hh>
#include <minizinc/flatten_internal.hh>
#include <minizinc/codegen_support.hh>

namespace MiniZinc {

struct CodeGen;

// Location is either a register or global.
class Loc {
private:
  Loc(int _x) : x(_x) { }
public:
  static Loc reg(int r) { return r<<1; }
  static Loc global(int g) { return (g<<1)|1; }

  inline bool is_reg(void) const { return !(x&1); }
  inline bool is_global(void) const { return x&1; }
  inline int index(void) const { return x>>1; }

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
  CG_Value(CG_ValueKind _kind, int _value)
    : kind(_kind), value(_value) { }
public:

  CG_Value(void)
    : kind(V_Immi), value(0) { }
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
  CG_Instr(unsigned int _tag) : tag(_tag) { }
public:
  static CG_Instr instr(BytecodeStream::Instr i) { return static_cast<unsigned int>(i)<<1; }
  static CG_Instr label(unsigned int l) { return (l<<1)+1; }

  unsigned int tag;
  std::vector<CG_Value> params;
};

struct CG_ProcID {
protected:
  CG_ProcID(int _p) : p(_p) { }
public:
  static CG_ProcID builtin(int b) { return CG_ProcID((b<<1)|1); }
  static CG_ProcID proc(int p) { return CG_ProcID(p<<1); }
  bool is_builtin(void) const { return p&1; }
  unsigned int id(void) const { return p>>1; }

  static CG_ProcID of_val(CG_Value v) { assert(v.kind == CG_Value::V_Proc); return CG_ProcID(v.value); }
  
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

// An environment should never outlive its parent.
template<class T>
class CG_Env {
private:
  CG_Env(CG_Env<T>* _p)
    : p(_p), sz(p ? p->sz : 0) { }

  // Forbid copy and assignment operators.
  CG_Env(const CG_Env& _o) = delete;
  CG_Env& operator=(const CG_Env& o) = delete;
public:
  CG_Env(void)
    : p(nullptr), sz(0) { }
  CG_Env(CG_Env&& o)
    : bindings(std::move(o.bindings))
    , available(std::move(o.available))
    , available_csts(std::move(o.available_csts))
    , occurs(std::move(o.occurs)), p(o.p), sz(o.sz) { }


  class NotFound : public std::exception {
  public:
    NotFound(void) { }
  };
 
  T lookup(const ASTString& s) const {
    auto it(bindings.find(s));
    if(it != bindings.end())
      return (*it).second;
    if(!p)
      throw NotFound();

    return p->lookup(s);
  }

  void bind(const ASTString& s, T val) {
    auto it(bindings.find(s));
    if(it != bindings.end())
      bindings.erase(it);
    else
      sz++;
    bindings.insert(std::make_pair(s, val));

    // Invalidate any cached values mentioning s.
    auto o_it(occurs.find(s));
    if(o_it != occurs.end()) {
      // We're lazy here, in that we don't remove e from
      // other occurs lists.
      for(Expression* e : (*o_it).second)
        available.erase(e);
      occurs.erase(o_it);
    }
  }

  // Check whether e is already available in an enclosing environment.
  T cache_lookup(Expression* e, ASTStSet e_scope) {
    auto it(available.find(e));
    // Anything in the current table is hasn't been invalidated.
    if(it != available.end()) {
      return (*it).second;
    }
    if(!p) throw NotFound();

    // If there's a parent table, check whether we've re-bound
    // something in its scope.
    for(auto p : bindings) {
      if(e_scope.find(p.first) != e_scope.end())
        throw NotFound();
    }
    // If the scope hasn't been invalidated, check the parent.
    return p->cache_lookup(e, e_scope);
  }

  void cache_store(Expression* e, ASTStSet e_scope, T val) {
    available.insert(std::make_pair(e, val));
    // Add e to the occurs lists for variables in its scope.
    for(ASTString s : e_scope)
      occurs[s].push_back(e);
  }

  bool cache_lookup_cst(int x, T& ret) {
    auto it(available_csts.find(x));
    // Anything in the current table is hasn't been invalidated.
    if(it != available_csts.end()) {
      ret = (*it).second;
      return true; 
    }
    if(!p) return false;
    return p->cache_lookup_cst(x, ret);
  }
  void cache_store_cst(int x, T val) {
    available_csts.insert(std::make_pair(x, val));
  }

  /*
  CG_Env clone(const CG_Env& o) {
    return CG_Env(o);
  }
  */
  static CG_Env* spawn(CG_Env* p) { return new CG_Env<T>(p); }

  unsigned int size(void) const { return sz; }

  typename ASTStringMap<T>::t bindings;

  typename ExprMap<T>::t available;
  std::unordered_map<int, T> available_csts;
//  std::unordered_map<std::pair<int, int>, T> available_ranges;
  typename ASTStringMap<std::vector<Expression*> >::t occurs;

  // Parent environment.
  CG_Env<T>* p;
  unsigned int sz;
};

// When we bind a non-Boolean expression, we also construct
// a set of conditions we need to insert into any use-contexts.

// We use CG_Cond to track the conditionality of values.
struct CG_Cond {
  // FIXME: Currently not GC'd, so this will leak a bunch of memory.
  enum Kind { CC_Reg, CC_Call, CC_And, CC_Or };
  class C_Reg;
  class C_Call;
  class C_And;
  class C_Or;

  class T {
  public:
    int is_root : 1;
    int is_seen : 1;
    int reg : 30;

    T(void) : is_root(0), is_seen(0), reg(-1) { }
    T(int _reg) : is_root(0), is_seen(0), reg(_reg) { }

    virtual Kind kind(void) const = 0;
  };

  class C_Reg : public T {
  public:
    static const Kind _kind = CC_Reg;
    Kind kind(void) const { return _kind; }
    C_Reg(int reg) : T(reg) { }
  };
  class C_Call : public T {
  public:
    static const Kind _kind = CC_Call;
    Kind kind(void) const { return _kind; }

    C_Call(CG_ProcID _p, BytecodeProc::Mode _m, std::vector<CG_Value>& _params)
      : p(_p), m(_m), params(_params) { }
    
    CG_ProcID p;
    BytecodeProc::Mode m;
    std::vector<CG_Value>& params;
  };
  class C_And : public T {
  public:
    static const Kind _kind = CC_And;
    Kind kind(void) const { return _kind; }

    C_And(BytecodeProc::Mode _m, std::vector<CG_Cond::T*>& _children)
      : m(_m), children(_children) { } 

    BytecodeProc::Mode m;
    std::vector<CG_Cond::T*> children;
  };
  class C_Or : public T {
  public:
    static const Kind _kind = CC_Or;
    Kind kind(void) const { return _kind; }

    C_Or(BytecodeProc::Mode _m, std::vector<CG_Cond::T*>& _children)
      : m(_m), children(_children) { }

    BytecodeProc::Mode m;
    std::vector<CG_Cond::T*> children;
  };

  static T* reg(int r) {
    return new C_Reg(r);
  }

  template<typename ...Args>
  static T* call(CG_ProcID p, BytecodeProc::Mode m, Args... args) {
    std::vector<CG_Value> params;
    return _call(p, m, params, args...);
  }
  static T* call(CG_ProcID p, BytecodeProc::Mode m, std::vector<CG_Value>& params) { return _call(p, m, params); }

  template<typename ...Args>
  static T* _call(CG_ProcID p, BytecodeProc::Mode m, std::vector<CG_Value>& params, CG_Value next, Args... rest) {
    params.push_back(next);
    return _call(p, m, params, rest...);
  }
  static T* _call(CG_ProcID p, BytecodeProc::Mode m, std::vector<CG_Value>& params) {
    return new C_Call(p, m, params);
  }

  static void dedup(std::vector<CG_Cond::T*>& args) {
    auto dest(args.begin());
    for(CG_Cond::T* e : args) {
      if(!e->is_seen) {
        e->is_seen = 1;
        *dest = e;
        ++dest;
      }
    }
    args.erase(dest, args.end());
    for(CG_Cond::T* e : args)
      e->is_seen = 0;
  }

  template<typename ...Args>
  static T* _forall(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args, CG_Cond::T* next, Args... rest) {
    args.push_back(next);
    return _forall(m, args, rest...);
  }
  static T* _forall(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args) {
    if(args.size() == 0)
      return nullptr;
    dedup(args);
    if(args.size() == 1)
      return args[0];
    return new C_And(m, args);
  }
  template<typename ...Args>
  static T* forall(BytecodeProc::Mode m, Args... args) {
    std::vector<CG_Cond::T*> vec;
    return _forall(m, vec, args...);
  }
  static T* forall(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args) { return _forall(m, args); }

  template<typename ...Args>
  static T* _exists(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args, CG_Cond::T* next, Args... rest) {
    args.push_back(next);
    return _exists(m, args, rest...);
  }
  static T* _exists(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args) {
    assert(args.size() > 0);
    dedup(args);
    for(CG_Cond::T* e : args) {
      if(!e)
        return nullptr;
    }
    if(args.size() == 1)
      return args[0];
    return new C_Or(m, args);
  }
  template<typename ...Args>
  static T* exists(BytecodeProc::Mode m, Args... args) {
    std::vector<CG_Cond::T*> vec;
    return _exists(m, vec, args...);
  }
  static T* exists(BytecodeProc::Mode m, std::vector<CG_Cond::T*>& args) { return _exists(m, args); }
};

struct CG {
  typedef std::pair<int, CG_Cond::T*> Binding;

  // This is currently (probably) sound, but unnecessarily weak. If an expression ever appears in
  // root context, the root appearance should dominate.
  struct Mode {
    enum Strength { Root = 0, Imp = 1, Fun = 2 };
    Mode(BytecodeProc::Mode _m) : m(_m) { }
    Mode(Strength s, bool is_neg) {
      switch(s) {
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
      switch(m) {
        case BytecodeProc::ROOT_NEG:
        case BytecodeProc::IMP_NEG:
        case BytecodeProc::FUN_NEG:
          return true;
        default:
          return false;
      }
    }

    Strength strength(void) const {
      switch(m) {
        case BytecodeProc::ROOT:
        case BytecodeProc::ROOT_NEG:
          return Root;
        case BytecodeProc::IMP:
        case BytecodeProc::IMP_NEG:
          return Imp;
        case BytecodeProc::FUN:
        case BytecodeProc::FUN_NEG:
          return Imp;
      default:
        throw InternalError("Unexpected mode.");
      }
    }
    bool is_root(void) const { return strength() == Root; }

    Mode join(Mode o) {
      if(m == BytecodeProc::RAW) return o.m;
      if(o.m == BytecodeProc::RAW) return m;
      if(is_neg() != o.is_neg())
        return BytecodeProc::FUN;
      return Mode(std::max(strength(), o.strength()), is_neg());
    }
    bool is_submode(Mode o) {
      if(m == BytecodeProc::RAW) return true;
      if(o.m == BytecodeProc::RAW) return false;
      return is_neg() == o.is_neg() && strength() <= o.strength();
    }
    
    // Half
    Mode operator+(void) const {
      if(m == BytecodeProc::RAW) return m;
      return Mode(strength() == Root ? Imp : strength(), is_neg());
    }
    Mode operator-(void) const {
      if(m == BytecodeProc::RAW) return m;
      return Mode(strength(), !is_neg());
    }
    
    // Switch the current mode to functional.
    Mode operator*(void) const {
      if(m ==BytecodeProc::RAW) return m;
      return Mode(Fun, is_neg());
    }

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
  static CG_Cond::T* compile(Expression* e, CodeGen& cg, CG_Builder& frag);
  // Reify a condion, putting it in a register.
  static int force(CG_Cond::T* cond, CodeGen& cg, CG_Builder& frag);

  static void run(CodeGen& cg, Model* m);

  // static void run(CodeGen& cg, Expression* e, BCtx ctx);

  /*
  static void eval(IntLit* z, CodeGen& cg, CG_Builder& frag);
  static void eval(FloatLit* f, CodeGen& cg, CG_Builder& frag);
  static void eval(SetLit* s, CodeGen& cg, CG_Builder& frag);
  static void eval(StringLit* s, CodeGen& cg, CG_Builder& frag);
  static void eval(AnonVar* v, CodeGen& cg, CG_Builder& frag);
  static void eval(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  
  // For Id and BoolLit, we need the context, to know whether we're
  // emitting the negated form.
  static void eval(BoolLit* b, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Id* id, Mode ctx, CodeGen& cg, CG_Builder& frag);
  */
  
  static int locate_immi(int x, CodeGen& cg, CG_Builder& frag);

  /*
  static int locate(IntLit* id, CodeGen& cg, CG_Builder& frag);
  static int locate(FloatLit* f, CodeGen& cg, CG_Builder& frag);
  static int locate(SetLit* s, CodeGen& cg, CG_Builder& frag);
  static int locate(StringLit* s, CodeGen& cg, CG_Builder& frag);

  static int locate(BoolLit* b, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static int locate(Id* id, Mode ctx, CodeGen& cg, CG_Builder& frag);

  // For Boolean expressions
  static void eval(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static int locate(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag);

  // When we know an expression is par. (And total?)
  static int locate_par(Expression* e, CodeGen& cg, CG_Builder& frag);

  // For other, possibly partial, expressions.
  static void eval(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value);
  // FIXME: Locate always binds the result in the partial fragment, so the result is available
  // in following calls. So the [value] argument is ignored.
  // GKG: Check that this behaves correctly for comprehensions.
  static int locate(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value);

  static int locate(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  */

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

  static CG_Cond::T* compile(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(BinOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static CG_Cond::T* compile(Comprehension* let, Mode ctx, CodeGen& cg, CG_Builder& frag);

  /*
  static void eval(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(BinOp* op, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Call* call, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Let* let, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Comprehension* let, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);

  static void eval(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(BinOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(UnOp* op, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Comprehension* let, Mode ctx, CodeGen& cg, CG_Builder& frag);

  static int locate_par(ArrayAccess* a, CodeGen& cg, CG_Builder& frag);
  static int locate_par(ArrayLit* a, CodeGen& cg, CG_Builder& frag);
  static int locate_par(ITE* ite, CodeGen& cg, CG_Builder& frag);
  static int locate_par(BinOp* op, CodeGen& cg, CG_Builder& frag);
  static int locate_par(UnOp* op, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Call* call, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Let* let, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Comprehension* let, CodeGen& cg, CG_Builder& frag);
  */
};

// Partially compiled bytecode.
/*
struct CG_Proc {
  std::string ident; 
  CG_Builder body[BytecodeProc::MAX_MODE+1];
};
*/
struct CG_Proc {
  typedef std::vector<CG_Instr> body_t;

  static unsigned char mode_mask(BytecodeProc::Mode m) {
    return 1<<(static_cast<unsigned char>(m));
  }
  struct mode_iterator {
    mode_iterator(unsigned int _x) : x(_x) { }
    bool operator!=(const mode_iterator& o) const { return x != o.x; }
    BytecodeProc::Mode operator*(void) const { assert(x); return static_cast<BytecodeProc::Mode>(find_lsb(x)); }
    mode_iterator& operator++(void) { x &= (x-1); return *this; }

    unsigned int x;
  };
  mode_iterator begin(void) { return mode_iterator(available_modes); }
  mode_iterator end(void) { return mode_iterator(0); }

  CG_Proc(std::string _ident, int _arity)
    : ident(_ident), arity(_arity), available_modes(0) { }

  CG_Proc(CG_Proc&& o)
    : ident(o.ident), arity(o.arity), available_modes(o.available_modes) {
    unsigned char rm(available_modes);
    while(rm) {
      unsigned char m(find_lsb(rm));
      rm &= (rm-1);
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

    if(!(available_modes & mode_mask(m))) {
      available_modes |= mode_mask(m);
      new (_body + m) body_t();
    }
    return _body[m];
  }

  unsigned char available_modes;
  std::vector<CG_Instr> _body[BytecodeProc::MAX_MODE+1];
};

// For identifying a call...
struct CallSig {
  ASTString id;  
  std::vector<Type> params;

  struct HashSig {
    size_t operator()(const CallSig& c) const { return c.hash(); }
  };
  struct EqSig {
    bool operator()(const CallSig& x, const CallSig& y) const { return x == y; }
  };

  bool operator==(const CallSig& o) const {
    if (id != o.id || params.size() != o.params.size())
      return false;
    for(int ii = 0; ii < params.size(); ++ii) {
      if(params[ii] != o.params[ii])
        return false;
    }
    return true;
  }

  size_t hash(void) const {
    size_t h(id.hash());
    for(int ii = 0; ii < params.size(); ++ii)
      h ^= params[ii].toInt() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

template<class T>
struct SigMap {
  typedef std::unordered_map<CallSig, T, CallSig::HashSig, CallSig::EqSig> t;
};

// Handle for dealing with function stuff.
class CG_FunID {
  friend class CodeGen;
  CG_FunID(int _f) : f(_f) { }

  int f;
};

struct CG_FunInfo {
  CG_FunInfo(FunctionI* _def)
    : def(_def), is_total(false), available_modes(0) 
  { }

  FunctionI* def;
  bool is_total;
  unsigned char available_modes; // Bit-vector of instantiated modes.
  std::vector<CG_Instr> bodies[BytecodeProc::MAX_MODE+1];
};

struct CG_FunMap {
  struct CG_FunDefn {
    CG_FunDefn(ASTString _id)
      : id(_id) { }

    ASTString id;
    std::vector<FunctionI*> bodies;
  };

  
  ASTStringMap<unsigned int>::t id_map;
  std::vector<CG_FunDefn> functions;
  
  void add_body(FunctionI* f) {
    unsigned int fun_id;
    ASTString id(f->id());
    auto it(id_map.find(id));
    if(it != id_map.end()) {
      fun_id = (*it).second;
    } else {
      fun_id = functions.size();
      id_map.insert(std::make_pair(id, fun_id));
      functions.push_back(CG_FunDefn(id));
    }
    functions[fun_id].bodies.push_back(f);
  }

  void filter_bodies(std::vector<FunctionI*>::iterator& dest, std::vector<FunctionI*>::iterator b, std::vector<FunctionI*>::iterator e, int arg, int sz) {
    if(!(b != e)) // Empty partition
      return;
    if(arg == sz) {
      // Find the best candidate between b and e, add it to the output.
      // FIXME
      (*dest) = (*b);
      ++dest;
      return;
    }
    // Otherwise, partition the arguments and recurse.
    std::vector<FunctionI*>::iterator mid = std::partition(b, e, [arg](FunctionI* b) { return b->params()[arg]->type().ispar(); });
    filter_bodies(dest, b, mid, arg+1, sz);
    filter_bodies(dest, mid, e, arg+1, sz);
  }

  std::vector<FunctionI*> get_bodies(unsigned int fun_id, std::vector<Type>& args) {
    CG_FunDefn& defn(functions[fun_id]);

    // First, restrict consideration to feasible specialisations.
    std::vector<FunctionI*> candidates;
    int sz = args.size();
    for(FunctionI* b : defn.bodies) {
      ASTExprVec<VarDecl> b_params(b->params());
      if(b_params.size() == sz) {
        for(int pi = 0; pi < sz; ++pi) {
          if(!args[pi].isSubtypeOf(b_params[pi]->type(), false)) // CHECK
            goto get_bodies_continue;
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

  std::vector<FunctionI*> get_bodies(Call* call) {
    auto it(id_map.find(call->id()));
    if(it == id_map.end())
      throw InternalError("Attempted to call function not in CG_FunMap.");
    unsigned int fun_id((*it).second);
    std::vector<Type> args;
    int sz(call->n_args());
    for(int ii = 0; ii < sz; ++ii)
      args.push_back(call->arg(ii)->type());
    return get_bodies(fun_id, args);
  }
};

struct CodeGen {
  typedef unsigned int proc_id;
  typedef unsigned int reg_id;
  typedef std::pair<int, CG_Cond::T*> Binding;
  CodeGen(void)
    : /*entry_proc(0)
    ,*/ current_env(new CG_Env<Binding>())
    , num_globals(0)
    , current_reg_count(0), current_label_count(0) /*, temporary_reg(-1) */ {
    bytecode.push_back(CG_Proc("main", 0));
    register_builtins();
  }

  void append(int proc, BytecodeProc::Mode m, CG_Builder& b) {
    std::vector<CG_Instr>& body(bytecode[proc].body(m));

    body.insert(body.end(),
      b.instrs.begin(), b.instrs.end());
    b.clear();
  }

  void env_push(void) { current_env = CG_Env<Binding>::spawn(current_env); }
  void env_pop(void) {
    assert(current_env);
    CG_Env<Binding>* c(current_env);
    current_env = current_env->p;
    delete c;
  }

  // Consult/update the available expressions in the current environment.
  Binding cache_lookup(Expression* e);
  void cache_store(Expression* e, Binding l);

  // Function resolution
  void register_function(FunctionI* f) { fun_map.add_body(f); }

  CG_ProcID resolve_val_fun(Call* c);
  CG_ProcID resolve_pred_fun(Call* c, BytecodeProc::Mode m);

  CG_FunID resolve_fun(FunctionI* f);
  CG_ProcID resolve_val_def(FunctionI* f);
  CG_ProcID resolve_pred_def(FunctionI* f, BytecodeProc::Mode m);

  std::vector< CG_Proc > bytecode; // Bytecode we've built

  // Procedures
  // std::vector<std::pair<proc_id, CallSig> > proc_queue; // Typed calls yet to be compiled
  // std::unordered_map<CallSig, proc_id> proc_map; // call -> proc
  inline CG_Env<Binding>& env(void) { return *current_env; }

  // proc_id current_proc; // Procedure we're currently building
  CG_Env<Binding>* current_env; // Where are things in scope?
  ASTStringMap<int>::t globals_env;
  int num_globals;

  std::vector<unsigned int> reg_trail;
  unsigned int current_reg_count; // How many registers have been used?
  unsigned int current_label_count;
  // unsigned int temporary_reg; // Which, if any, register is used for transient stuff.

  // Helper information. For an expression, which variables does it refer to?
  ASTStSet scope(Expression* e);
  ExprMap<ASTStSet>::t _exp_scope;

  ExprMap<CG::Mode>::t mode_map;
  
  bool is_total(Expression* e);
  ExprMap<bool> _exp_is_total;
  
  // Procedure information
  void register_builtins(void);
  void register_builtin(std::string s, unsigned int p);
  CG_ProcID find_builtin(std::string s);
  std::vector<std::pair<std::string, unsigned int> > _builtins;
  std::unordered_map<std::string, CG_ProcID> _proc_map;

  // Procedures yet to be emitted.
  CG_FunMap fun_map;
  /*
  std::vector< std::pair<Expression*, unsigned int> > let_queue;
  std::vector< std::pair<FunctionI*, unsigned int> > fun_queue;
  std::vector< std::pair<CallSig, unsigned int> > call_queue;

  std::unordered_map<Expression*, unsigned int> let_map;
  std::unordered_map<FunctionI*, unsigned int> fun_map;
  SigMap<unsigned int>::t call_map;
  */
};

const char* instr_name(BytecodeStream::Instr i);
const char* agg_name(AggregationCtx::Symbol s);
const char* mode_name(BytecodeProc::Mode m);

};

#endif
