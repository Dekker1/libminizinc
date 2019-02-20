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
#include <iostream>

#include <minizinc/bytecode.hh>

namespace MiniZinc {

struct CodeGen;

// Code generators for expressions.
// If Boolean, generates code to leave its value on the
// Boolean aggregation stack, otherwise the value aggregation
// stack.
struct CG {
  static void run(CodeGen& cg, Model* m);

  static void run(CodeGen& cg, Expression* e);

  static void run(CodeGen& cg, IntLit* z);
  static void run(CodeGen& cg, FloatLit* f);
  static void run(CodeGen& cg, SetLit* s);
  static void run(CodeGen& cg, BoolLit* b);
  static void run(CodeGen& cg, StringLit* s);
  static void run(CodeGen& cg, Id* id);
  static void run(CodeGen& cg, AnonVar* v);
  static void run(CodeGen& cg, ArrayLit* a);
  static void run(CodeGen& cg, ArrayAccess* a);
  static void run(CodeGen& cg, Comprehension* c);
  static void run(CodeGen& cg, ITE* ite);
  static void run(CodeGen& cg, BinOp* op);
  static void run(CodeGen& cg, UnOp* op);
  static void run(CodeGen& cg, Call* call);
  static void run(CodeGen& cg, VarDecl* decl);
  static void run(CodeGen& cg, Let* let);
  static void run(CodeGen& cg, TypeInst* ti);
};

// Partially compiled bytecode.
struct CodeGen {
  typedef unsigned int proc_id;
  typedef unsigned int reg_id;
  CodeGen(void)
    : current_proc(0), current_reg_count(0) {
    bytecode.push_back(BytecodeStream());
  }

  std::vector<BytecodeStream> bytecode; // Bytecode we've built

  // Procedures
  // std::vector<std::pair<proc_id, CallSig> > proc_queue; // Typed calls yet to be compiled
  // std::unordered_map<CallSig, proc_id> proc_map; // call -> proc

  proc_id current_proc; // Procedure we're currently building
  std::unordered_map<ASTString, reg_id> current_env; // Where are things in scope?
  unsigned int current_reg_count; // How many registers have been used?
};

};

#endif
