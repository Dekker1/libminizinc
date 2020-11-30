/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Graeme Gange <graeme.gange@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_CODEGEN_ANALYSIS_HPP__
#define __MINIZINC_CODEGEN_ANALYSIS_HPP__
#include <minizinc/ast.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/codegen.hh>

#include <queue>

// From a given top-level expression and
// mode, what is the weakest mode covering
// all occurrences of each sub-expression?
namespace MiniZinc {

static void annotate_total(FunctionI* func) {
  class AnnotateTotal : public EVisitor {
  public:
    void vLet(Let& let) {
      let.addAnnotation(constants().ann.promise_total);
      ASTExprVec<Expression> bindings(let.let());
      for (auto expr : bindings) {
        if (expr->eid() != Expression::E_VARDECL) {
          // Must be a constraint, so it's a use.
          assert(expr->type().isbool());
          expr->addAnnotation(constants().ann.promise_total);
        }
      }
    }
  } _at;
  if (func->ann().contains(constants().ann.promise_total)) {
    topDown(_at, func->e());
  }
}

class ModeAnalysis {
  enum Occurrence { Def = 0, Use = 1 };

public:
  // There are two relevant modes for an expression: the mode
  // where the value is introduced, and the mode where the value
  // is used.
  // This distinction mostly matters for let-bindings or definitions
  // which construct results containing Booleans (i.e. array [int] of var bool).
  // At the end, Boolean values are built according to their use-context, and
  // non-Boolean values by their def-context.

  // We use user_flag0 and user_flag1 to track whether
  // the definition and use-modes are queued.
  inline void enqueue(Expression* e, Occurrence o) {
    if (e->isUnboxedVal()) return;
    if (o == Def) {
      if (!e->user_flag0()) {
        e->user_flag0(true);
        worklist.push(reinterpret_cast<uintptr_t>(e));
      }
    } else {
      if (!e->user_flag1()) {
        e->user_flag1(true);
        worklist.push(reinterpret_cast<uintptr_t>(e) | 1);
      }
    }
  }

  void update(Expression* e, Occurrence o, CG::Mode m) {
    if (e->type().isbool()) {
      if (o == Def) {
        // We only keep Use-modes for Boolean things.
        return;
      }
      if (e->ann().contains(constants().ann.promise_total)) {
        m = BytecodeProc::ROOT;
      }
    }
    ExprMap<CG::Mode>::t& t(o == Def ? def_map : use_map);
    auto it(t.find(e));
    if (it == t.end()) {
      // First time we've seen e.
      t.insert(std::make_pair(e, m));
    } else {
      if (it->second.is_submode(m)) return;
      it->second = it->second.join(m);
    }
    enqueue(e, o);
  }

  void update(FunctionI* fun, Occurrence o, CG::Mode m) {
    // We only keep Use-modes for Boolean things.
    /*
    if(o == Def) {
      if(e->type().isbool())
        return;
      if(e->ann().contains(constants().ann.promise_total))
        m = BytecodeProc::ROOT;
    }
    ExprMap<CG::Mode>::t& t(o == Def ? def_map : use_map);
    auto it(t.find(e));
    if(it == t.end()) {
      // First time we've seen e.
      t.insert(std::make_pair(e, m));
    } else {
      if(it->second.is_submode(m))
        return;
      it->second = it->second.join(m);
    }
    enqueue(e, o);
    */
  }

  // Better to make this fifo, but... whatever.
  void process(void) {
    while (!worklist.empty()) {
      uintptr_t tag(worklist.front());
      worklist.pop();
      Expression* e(reinterpret_cast<Expression*>(tag & ~((uintptr_t)1)));
      Occurrence o(static_cast<Occurrence>(tag & 1));
      // Clear the queued-ness flags
      if (o == Def)
        e->user_flag0(false);
      else
        e->user_flag1(false);

      // Get the updated mode for e.
      ExprMap<CG::Mode>::t& t(o == Def ? def_map : use_map);
      CG::Mode m(t.at(e));
      switch (e->eid()) {
        case Expression::E_INTLIT:
        case Expression::E_FLOATLIT:
        case Expression::E_SETLIT:
        case Expression::E_BOOLLIT:
        case Expression::E_STRINGLIT:
        case Expression::E_ANON:
          break;
        case Expression::E_ID:
          vId(*e->template cast<Id>(), o, m);
          break;
        case Expression::E_ARRAYLIT:
          vArrayLit(*e->template cast<ArrayLit>(), o, m);
          break;
        case Expression::E_ARRAYACCESS:
          vArrayAccess(*e->template cast<ArrayAccess>(), o, m);
          break;
        case Expression::E_COMP:
          vComprehension(*e->template cast<Comprehension>(), o, m);
          break;
        case Expression::E_ITE:
          vITE(*e->template cast<ITE>(), o, m);
          break;
        case Expression::E_BINOP:
          vBinOp(*e->template cast<BinOp>(), o, m);
          break;
        case Expression::E_UNOP:
          vUnOp(*e->template cast<UnOp>(), o, m);
          break;
        case Expression::E_CALL:
          vCall(*e->template cast<Call>(), o, m);
          break;
        case Expression::E_VARDECL:
          vVarDecl(*e->template cast<VarDecl>(), o, m);
          break;
        case Expression::E_LET:
          vLet(*e->template cast<Let>(), o, m);
          break;
          /*
        case Expression::E_TI:
          _t.vTypeInst(*c._e->template cast<TypeInst>());
          break;
        case Expression::E_TIID:
          _t.vTIId(*c._e->template cast<TIId>());
          break;
          */
        default:
          throw InternalError("Mode analyser encounted unexpected expression type.");
      }
    }
  }

  ModeAnalysis(void) {}

  void def(Expression* e, CG::Mode m) { update(e, Def, m); }

  void use(Expression* e, CG::Mode m) {
    if (e->type().isbool())
      update(e, Use, m);
    else
      update(e, Def, m);
  }

  ExprMap<CG::Mode>::t extract(void) {
    // Compute the fixpoint.
    process();
    // Now fuse the two maps.
    // Start from the def_map, and add any Boolean uses.
    ExprMap<CG::Mode>::t fused(def_map);
    for (auto p : use_map) {
      /*
      if(p.first->type().isbool())
        fused.insert(p);
        */
      // If we haven't stored a def-mode, we should
      // apply the use-mode.
      fused.insert(p);
    }
    return fused;
  }
  /*
  static ExprMap<CG::Mode> run(Expression* e, CG::Mode m) {
    ModeAnalysis analyzer;
    analyzer.update(e, Use, m);
    analyzer.process();

    // Now we fuse the two maps.
  }
  */

  std::queue<uintptr_t> worklist;
  ExprMap<CG::Mode>::t def_map;
  ExprMap<CG::Mode>::t use_map;

  /// Visit identifier
  void vId(Id&, Occurrence, CG::Mode);
  /// Visit array literal
  void vArrayLit(ArrayLit&, Occurrence, CG::Mode);
  /// Visit array access
  void vArrayAccess(ArrayAccess&, Occurrence, CG::Mode);
  /// Visit array comprehension
  void vComprehension(Comprehension&, Occurrence, CG::Mode);
  /// Visit if-then-else
  void vITE(ITE&, Occurrence, CG::Mode);
  /// Visit binary operator
  void vBinOp(BinOp&, Occurrence, CG::Mode);
  /// Visit unary operator
  void vUnOp(UnOp&, Occurrence, CG::Mode);
  /// Visit call
  void vCall(Call&, Occurrence, CG::Mode);
  /// Visit let
  void vLet(Let&, Occurrence, CG::Mode);
  /// Visit variable declaration
  void vVarDecl(VarDecl&, Occurrence, CG::Mode);

  // Process a function body.
  void vFunctionI(FunctionI&, Occurrence, CG::Mode);
};

};  // namespace MiniZinc

#endif
