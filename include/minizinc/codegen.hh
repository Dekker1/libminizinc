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

  T lookup(const ASTString& s) const {
    auto it(bindings.find(s));
    if(it != bindings.end())
      return (*it).second;
    assert(p);
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
  bool cache_lookup(Expression* e, ASTStSet e_scope, T& ret) {
    auto it(available.find(e));
    // Anything in the current table is hasn't been invalidated.
    if(it != available.end()) {
      ret = (*it).second;
      return true; 
    }
    if(!p) return false;

    // If there's a parent table, check whether we've re-bound
    // something in its scope.
    for(auto p : bindings) {
      if(e_scope.find(p.first) != e_scope.end())
        return false;
    }
    // If the scope hasn't been invalidated, check the parent.
    return p->cache_lookup(e, e_scope, ret);
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
  static CG_Env* spawn(CG_Env* p) { return new CG_Env<Loc>(p); }

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

struct CG {
  struct Builtin {
    enum T { MAKE_VAR, CLAUSE, ELEMENT, EQ, LE };
  };

  struct Mode {
    Mode(BytecodeProc::Mode _m) : m(_m) { }
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
    bool is_root(void) const {
      switch(m) {
        case BytecodeProc::ROOT:
        case BytecodeProc::ROOT_NEG:
          return true;
        default:
          return false;
      }
    }

    
    // Half
    Mode operator+(void) const {
      switch(m) {
        case BytecodeProc::ROOT:
        case BytecodeProc::IMP:
          return BytecodeProc::IMP;
        case BytecodeProc::ROOT_NEG:
        case BytecodeProc::IMP_NEG:
          return BytecodeProc::IMP_NEG; 
        case BytecodeProc::RAW:
          throw InternalError("Half-reified invalid mode.");
        default: // Already functional.
          return m;
      }
    }

    Mode operator-(void) const {
      switch(m) {
        case BytecodeProc::ROOT: return BytecodeProc::ROOT_NEG;
        case BytecodeProc::IMP: return BytecodeProc::IMP_NEG;
        case BytecodeProc::FUN: return BytecodeProc::FUN_NEG;
        case BytecodeProc::ROOT_NEG: return BytecodeProc::ROOT;
        case BytecodeProc::IMP_NEG: return BytecodeProc::IMP;
        case BytecodeProc::FUN_NEG: return BytecodeProc::FUN;
        default:
          throw InternalError("Negated invalid mode.");
      }
    }
    
    // Switch the current mode to functional.
    Mode operator*(void) const {
      switch(m) {
        case BytecodeProc::ROOT:
        case BytecodeProc::IMP:
        case BytecodeProc::FUN:
          return BytecodeProc::FUN;
        case BytecodeProc::ROOT_NEG:
        case BytecodeProc::IMP_NEG:
        case BytecodeProc::FUN_NEG:
          return BytecodeProc::FUN_NEG;
        default:
          throw InternalError("Reified invalid mode."); 
      }
    }
    operator BytecodeProc::Mode() const { return m; }

    BytecodeProc::Mode m;
  };

  inline static CG_Value g(int g) { return CG_Value::global(g); }
  inline static CG_Value r(int r) { return CG_Value::reg(r); }
  inline static CG_Value i(int i) { return CG_Value::immi(i); }
  inline static CG_Value l(int l) { return CG_Value::label(l); }

  static void run(CodeGen& cg, Model* m);

  // static void run(CodeGen& cg, Expression* e, BCtx ctx);

  // Expressions which are inherently total.
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
  
  static int locate_immi(int x, CodeGen& cg, CG_Builder& frag);

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
  static int locate_par(ITE* ite, CodeGen& cg, CG_Builder& frag);
  static int locate_par(BinOp* op, CodeGen& cg, CG_Builder& frag);
  static int locate_par(UnOp* op, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Call* call, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Let* let, CodeGen& cg, CG_Builder& frag);
  static int locate_par(Comprehension* let, CodeGen& cg, CG_Builder& frag);
};

// Partially compiled bytecode.
/*
struct CG_Proc {
  std::string ident; 
  CG_Builder body[BytecodeProc::MAX_MODE+1];
};
*/
struct CG_Proc {
  CG_Proc(std::string _ident, int _arity, CG::Mode _m)
    : ident(_ident), arity(_arity), m(_m) { }

  std::string ident;
  unsigned int arity;
  CG::Mode m;
  std::vector<CG_Instr> body;
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

struct CodeGen {
  typedef unsigned int proc_id;
  typedef unsigned int reg_id;
  CodeGen(void)
    : /*entry_proc(0)
    ,*/ current_env(new CG_Env<Loc>())
    , current_reg_count(0), current_label_count(0), temporary_reg(-1) {
    bytecode.push_back(CG_Proc("main", 0, BytecodeProc::ROOT));
    register_builtins();
  }

  void append(int proc, CG_Builder& b) {
    bytecode[proc].body.insert(bytecode[proc].body.end(),
      b.instrs.begin(), b.instrs.end());
    b.clear();
  }

  void env_push(void) { current_env = CG_Env<Loc>::spawn(current_env); }
  void env_pop(void) {
    assert(current_env);
    CG_Env<Loc>* c(current_env);
    current_env = current_env->p;
    delete c;
  }

  // Consult/update the available expressions in the current environment.
  bool cache_lookup(Expression* e, Loc& out);
  void cache_store(Expression* e, Loc l);

  // CG_Value find_builtin(CG::Builtin::T builtin);

  std::vector< CG_Proc > bytecode; // Bytecode we've built

  // Procedures
  // std::vector<std::pair<proc_id, CallSig> > proc_queue; // Typed calls yet to be compiled
  // std::unordered_map<CallSig, proc_id> proc_map; // call -> proc
  inline CG_Env<Loc>& env(void) { return *current_env; }

  // proc_id current_proc; // Procedure we're currently building
  CG_Env<Loc>* current_env; // Where are things in scope?

  unsigned int current_reg_count; // How many registers have been used?
  unsigned int current_label_count;
  unsigned int temporary_reg; // Which, if any, register is used for transient stuff.

  // Helper information. For an expression, which variables does it refer to?
  ASTStSet scope(Expression* e);
  ExprMap<ASTStSet>::t _exp_scope;

  // Procedure information
  void register_builtins(void);
  void register_builtin(std::string s, unsigned int p);
  CG_ProcID find_builtin(std::string s);
  std::vector<std::pair<std::string, unsigned int> > _builtins;
  std::unordered_map<std::string, CG_ProcID> _proc_map;

  // Procedures yet to be emitted.
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
