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

// During code generation, we track the mapping between
// identifiers and their locations.
// typedef std::unordered_map<ASTString, Loc> CG_Env;

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
  enum CG_ValueKind { V_Immi, V_Global, V_Reg, V_Proc, V_Label };

  CG_Value(CG_ValueKind _kind, int _value)
    : kind(_kind), value(_value) { }

public:
  CG_Value(void)
    : kind(V_Immi), value(0) { }
  static CG_Value reg(int r) { return CG_Value(V_Reg, r); }
  static CG_Value global(int r) { return CG_Value(V_Global, r); }
  static CG_Value immi(long long int r) { return CG_Value(V_Immi, r); }
//   static CG_Value proc(int p) { return CG_Value(V_Proc, p); }
  static CG_Value label(int l) { return CG_Value(V_Label, l); }

  CG_ValueKind kind;
  long long int value;
};

struct CG_Instr {
  BytecodeStream::Instr i; 
  std::vector<CG_Value> params;

  unsigned int label;
};

struct CG_ProcID {
  CG_ProcID(int _p) : p(_p) { }
  int p;
};

struct CG_Builder {
  std::vector<CG_Instr> instrs;

  void append(CG_Builder& o) {
    instrs.insert(instrs.end(), o.instrs.begin(), o.instrs.end());
    o.instrs.clear();
  }
  void clear(void) { instrs.clear(); }
};

/*
class CG_Frag {
  std::vector<CG_Instr> instrs;
  CG_Frag* pred;
  CG_Frag* succ;
};

class CG_Builder {
  CG_Frag* hd;
  CG_Frag* tl;
};
*/

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
    : bindings(std::move(o.bindings)), available(std::move(o.available)), p(o.p), sz(o.sz) { }

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
  }

  /*
  CG_Env clone(const CG_Env& o) {
    return CG_Env(o);
  }
  */
  static CG_Env* spawn(CG_Env* p) { return new CG_Env<Loc>(p); }

  unsigned int size(void) const { return sz; }

  std::unordered_map<ASTString, T> bindings;
  std::unordered_map<Expression*, T> available;

  // Parent environment.
  CG_Env<T>* p;
  unsigned int sz;
};

struct CG {
  struct Builtin {
    enum T { CLAUSE, ELEMENT };
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
  static void eval(BoolLit* b, CodeGen& cg, CG_Builder& frag);
  static void eval(StringLit* s, CodeGen& cg, CG_Builder& frag);
  static void eval(Id* id, CodeGen& cg, CG_Builder& frag);
  static void eval(AnonVar* v, CodeGen& cg, CG_Builder& frag);
  static void eval(ArrayLit* a, BCtx ctx, CodeGen& cg, CG_Builder& frag);

  static int locate_immi(int x, CodeGen& cg, CG_Builder& frag);

  static int locate(IntLit* id, CodeGen& cg, CG_Builder& frag);
  static int locate(FloatLit* f, CodeGen& cg, CG_Builder& frag);
  static int locate(SetLit* s, CodeGen& cg, CG_Builder& frag);
  static int locate(BoolLit* b, CodeGen& cg, CG_Builder& frag);
  static int locate(StringLit* s, CodeGen& cg, CG_Builder& frag);
  static int locate(Id* id, CodeGen& cg, CG_Builder& frag);

  // For Boolean expressions
  static void eval(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static int locate(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& frag);

  // For other, possibly partial, expressions.
  static void eval(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value);
  static int locate(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value);
  /*
  static void run(CodeGen& cg, ArrayAccess* a, BCtx ctx);
  static void run(CodeGen& cg, Comprehension* c, BCtx ctx);
  static void run(CodeGen& cg, ITE* ite, BCtx ctx);
  static void run(CodeGen& cg, BinOp* op, BCtx ctx);
  static void run(CodeGen& cg, UnOp* op, BCtx ctx);
  static void run(CodeGen& cg, Call* call, BCtx ctx);
  static void run(CodeGen& cg, Let* let, BCtx ctx);

  static void run_condition(CodeGen& cg, ArrayAccess* a, BCtx ctx);
  static void run_condition(CodeGen& cg, ITE* ite, BCtx ctx);
  static void run_condition(CodeGen& cg, BinOp* op, BCtx ctx);
  static void run_condition(CodeGen& cg, UnOp* op, BCtx ctx);
  static void run_condition(CodeGen& cg, Call* call, BCtx ctx);
  static void run_condition(CodeGen& cg, Let* let, BCtx ctx);
  */
  static void eval(ArrayAccess* a, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(ITE* ite, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(BinOp* op, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(UnOp* op, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Let* let, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);
  static void eval(Comprehension* let, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value);

  static void eval(ArrayAccess* a, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(ITE* ite, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(BinOp* op, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(UnOp* op, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Let* let, BCtx ctx, CodeGen& cg, CG_Builder& frag);
  static void eval(Comprehension* let, BCtx ctx, CodeGen& cg, CG_Builder& frag);
};

// Partially compiled bytecode.
struct CG_Proc {
  std::string ident; 
  CG_Builder body[BytecodeProc::MAX_MODE+1];
};

struct CodeGen {
  struct cmp_ASTString {
    bool operator()(const ASTString& s, const ASTString& t) {
      return s.hash() < t.hash();
    }
  };
  typedef std::set<ASTString, cmp_ASTString> ASTStSet;
  
  typedef unsigned int proc_id;
  typedef unsigned int reg_id;
  CodeGen(void)
    : /*entry_proc(0)
    ,*/ current_env(new CG_Env<Loc>())
    , current_reg_count(0), current_label_count(0), temporary_reg(-1) {
    bytecode.push_back(std::vector<CG_Instr>());
  }

  void append(int proc, CG_Builder& b) {
    bytecode[proc].insert(bytecode[proc].end(),
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

  CG_Value find_builtin(CG::Builtin::T builtin);

  std::vector< std::vector<CG_Instr> > bytecode; // Bytecode we've built

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
  std::unordered_map<Expression*, ASTStSet> _exp_scope;
};

const char* instr_name(BytecodeStream::Instr i);

};

#endif
