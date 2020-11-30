/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-static-cast-downcast"

#include <minizinc/solvers/geas/geas_constraints.hh>
#include <minizinc/solvers/geas_solverinstance.hh>

#include <geas/constraints/builtins.h>
#include <geas/constraints/flow/flow.h>

namespace MiniZinc {
namespace GeasConstraints {

#define SI static_cast<GeasSolverInstance&>(s)
#define SD SI.solver_data()
#define SOL SI.solver()
#define STATE SI.currentState()
#define EXPR(X) call->arg(X)
#define BOOL(X) GeasSolverInstance::asBool(EXPR(X))
#define BOOLARRAY(X) SI.asBoolVec(ARRAY(X))
#define BOOLVAR(X) SI.asBoolVar(EXPR(X))
#define BOOLVARARRAY(X) SI.asBoolVarVec(ARRAY(X))
#define INT(X) GeasSolverInstance::asInt(EXPR(X))
#define INTARRAY(X) SI.asIntVec(ARRAY(X))
#define INTVAR(X) SI.asIntVar(EXPR(X))
#define INTVARARRAY(X) SI.asIntVarVec(ARRAY(X))
#define PAR(X) call->arg(X).isInt()
#define ARRAY(X) call->arg(X)

geas::patom_t lit_and(geas::solver& s, geas::patom_t a, geas::patom_t b) {
  if (a == geas::at_False || b == geas::at_False) {
    return geas::at_False;
  } else if (a == geas::at_True) {
    return b;
  } else if (b == geas::at_True) {
    return a;
  } else {
    geas::patom_t var = s.new_boolvar();
    geas::add_clause(s.data, ~var, a);
    geas::add_clause(s.data, ~var, b);
    geas::add_clause(s.data, var, ~a, ~b);
    return var;
  }
}

void p_mk_intvar(SolverInstanceBase& s, const Definition* def) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(def->mode()) == BytecodeProc::RAW);
  assert(def->timestamp() != -1);
  assert(def->domain().isVec());

  const Val& dom = def->domain();
  assert(dom.size() == 2);

  auto var = SOL.new_intvar(static_cast<geas::intvar::val_t>(dom[0]().toInt()),
                            static_cast<geas::intvar::val_t>(dom[1]().toInt()));
  //      if (isv->size() > 1) {
  //        vec<int> vals(static_cast<int>(isv->card().toInt()));
  //        int i = 0;
  //        for (int j = 0; j < isv->size(); ++j) {
  //          for (auto k = isv->min(i).toInt(); k <= isv->max(j).toInt(); ++k) {
  //            vals[i++] = static_cast<int>(k);
  //          }
  //        }
  //        assert(i == isv->card().toInt());
  //        auto res = geas::make_sparse(var, vals);
  //        assert(res);
  //      }
  SI.insertVar(def, GeasVariable(var));
}

void p_int_eq(SolverInstanceBase& s, const Definition* call) {
  auto m = static_cast<BytecodeProc::Mode>(call->mode());
  switch (m) {
    case BytecodeProc::ROOT:
      geas::int_eq(SD, INTVAR(0), INTVAR(1), STATE);
      break;
    case BytecodeProc::ROOT_NEG:
      geas::int_ne(SD, INTVAR(0), INTVAR(1), STATE);
      break;
    case BytecodeProc::FUN:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_eq(SD, INTVAR(0), INTVAR(1), STATE);
        } else {
          geas::int_ne(SD, INTVAR(0), INTVAR(1), STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_eq(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, var));
        geas::int_ne(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::FUN_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_ne(SD, INTVAR(0), INTVAR(1), STATE);
        } else {
          geas::int_eq(SD, INTVAR(0), INTVAR(1), STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_ne(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, var));
        geas::int_eq(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::IMP:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_eq(SD, INTVAR(0), INTVAR(1), STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_eq(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, var));
      }
    case BytecodeProc::IMP_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_ne(SD, INTVAR(0), INTVAR(1), STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_ne(SD, INTVAR(0), INTVAR(1), lit_and(SOL, STATE, var));
      }
      break;
    case BytecodeProc::RAW:
      assert(false);
      break;
  }
}

void p_int_le(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  auto m = static_cast<BytecodeProc::Mode>(call->mode());
  switch (m) {
    case BytecodeProc::ROOT:
      geas::int_le(SD, INTVAR(0), INTVAR(1), 0, STATE);
      break;
    case BytecodeProc::ROOT_NEG:
      geas::int_le(SD, INTVAR(1), INTVAR(0), -1, STATE);
      break;
    case BytecodeProc::FUN:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(0), INTVAR(1), 0, STATE);
        } else {
          geas::int_le(SD, INTVAR(1), INTVAR(0), -1, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), 0, lit_and(SOL, STATE, var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), -1, lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::FUN_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(1), INTVAR(0), -1, STATE);
        } else {
          geas::int_le(SD, INTVAR(0), INTVAR(1), 0, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), -1, lit_and(SOL, STATE, var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), 0, lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::IMP:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(0), INTVAR(1), 0, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), 0, lit_and(SOL, STATE, var));
      }
    case BytecodeProc::IMP_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(1), INTVAR(0), -1, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), -1, lit_and(SOL, STATE, var));
      }
      break;
    case BytecodeProc::RAW:
      assert(false);
      break;
  }
}

void p_int_lt(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  auto m = static_cast<BytecodeProc::Mode>(call->mode());
  switch (m) {
    case BytecodeProc::ROOT:
      geas::int_le(SD, INTVAR(0), INTVAR(1), -1, STATE);
      break;
    case BytecodeProc::ROOT_NEG:
      geas::int_le(SD, INTVAR(1), INTVAR(0), 0, STATE);
      break;
    case BytecodeProc::FUN:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(0), INTVAR(1), -1, STATE);
        } else {
          geas::int_le(SD, INTVAR(1), INTVAR(0), 0, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), -1, lit_and(SOL, STATE, var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), 0, lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::FUN_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(1), INTVAR(0), 0, STATE);
        } else {
          geas::int_le(SD, INTVAR(0), INTVAR(1), -1, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), 0, lit_and(SOL, STATE, var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), -1, lit_and(SOL, STATE, ~var));
      }
      break;
    case BytecodeProc::IMP:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(0), INTVAR(1), -1, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(0), INTVAR(1), -1, lit_and(SOL, STATE, var));
      }
    case BytecodeProc::IMP_NEG:
      if (call->domain().isInt()) {
        if (GeasSolverInstance::asBool(call->domain())) {
          geas::int_le(SD, INTVAR(1), INTVAR(0), 0, STATE);
        }
      } else {
        auto var = SOL.new_boolvar();
        SI.insertVar(call, GeasVariable(var));
        geas::int_le(SD, INTVAR(1), INTVAR(0), 0, lit_and(SOL, STATE, var));
      }
      break;
    case BytecodeProc::RAW:
      assert(false);
      break;
  }
}

void p_int_abs(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::int_abs(SD, INTVAR(1), INTVAR(0), STATE);
}

void p_int_times(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::int_mul(SD, INTVAR(0), INTVAR(1), INTVAR(2), STATE);
}

void p_int_div(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::int_div(SD, INTVAR(2), INTVAR(0), INTVAR(1), STATE);
}

void p_int_max(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> vars = {INTVAR(0), INTVAR(1)};
  geas::int_max(SD, INTVAR(2), vars, STATE);
}

void p_int_min(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> vars = {-INTVAR(0), -INTVAR(1)};
  geas::int_max(SD, -INTVAR(2), vars, STATE);
}

void p_int_lin_eq(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> pos = INTARRAY(0);
  vec<int> neg(pos.size());
  for (int i = 0; i < neg.size(); ++i) {
    neg[i] = -pos[i];
  }
  vec<geas::intvar> vars = INTVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::linear_le(SD, pos, vars, INT(2), STATE);
  geas::linear_le(SD, neg, vars, -INT(2), STATE);
}

void p_int_lin_ne(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::intvar> vars = INTVARARRAY(1);
  geas::linear_ne(SD, cons, vars, INT(2), STATE);
}

void p_int_lin_le(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::intvar> vars = INTVARARRAY(1);
  geas::linear_le(SD, cons, vars, INT(2), STATE);
}

void p_int_lin_eq_imp(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> pos = INTARRAY(0);
  vec<int> neg(pos.size());
  for (int i = 0; i < neg.size(); ++i) {
    neg[i] = -pos[i];
  }
  vec<geas::intvar> vars = INTVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::linear_le(SD, pos, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
  geas::linear_le(SD, neg, vars, -INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
}

void p_int_lin_ne_imp(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::intvar> vars = INTVARARRAY(1);
  geas::linear_ne(SD, cons, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
}

void p_int_lin_le_imp(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::intvar> vars = INTVARARRAY(1);
  geas::linear_le(SD, cons, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
}

void p_int_lin_eq_reif(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> pos = INTARRAY(0);
  vec<int> neg(pos.size());
  for (int i = 0; i < neg.size(); ++i) {
    neg[i] = -pos[i];
  }
  vec<geas::intvar> vars = INTVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::linear_le(SD, pos, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
  geas::linear_le(SD, neg, vars, -INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
  geas::linear_ne(SD, pos, vars, INT(2), lit_and(SOL, STATE, ~BOOLVAR(3)));
}

void p_int_lin_ne_reif(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> pos = INTARRAY(0);
  vec<int> neg(pos.size());
  for (int i = 0; i < neg.size(); ++i) {
    neg[i] = -pos[i];
  }
  vec<geas::intvar> vars = INTVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::linear_ne(SD, pos, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
  geas::linear_le(SD, pos, vars, INT(2), lit_and(SOL, STATE, ~BOOLVAR(3)));
  geas::linear_le(SD, neg, vars, -INT(2), lit_and(SOL, STATE, ~BOOLVAR(3)));
}

void p_int_lin_le_reif(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> pos = INTARRAY(0);
  vec<int> neg(pos.size());
  for (int i = 0; i < neg.size(); ++i) {
    neg[i] = -pos[i];
  }
  vec<geas::intvar> vars = INTVARARRAY(1);
  geas::linear_le(SD, pos, vars, INT(2), lit_and(SOL, STATE, BOOLVAR(3)));
  geas::linear_le(SD, neg, vars, -INT(2) - 1, lit_and(SOL, STATE, ~BOOLVAR(3)));
}

void p_bool_eq(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(0)) {
    SOL.post(BOOL(0) ? BOOLVAR(1) : ~BOOLVAR(1));
  } else if (PAR(2)) {
    SOL.post(BOOL(1) ? BOOLVAR(0) : ~BOOLVAR(0));
  } else {
    geas::add_clause(SD, BOOLVAR(0), ~BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(0), BOOLVAR(1));
  }
}

void p_bool_ne(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(0)) {
    SOL.post(BOOL(0) ? ~BOOLVAR(1) : BOOLVAR(1));
  } else if (PAR(1)) {
    SOL.post(BOOL(1) ? ~BOOLVAR(0) : BOOLVAR(0));
  } else {
    geas::add_clause(SD, BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_le(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(0)) {
    if (BOOL(0)) {
      SOL.post(BOOLVAR(1));
    }
  } else if (PAR(1)) {
    if (!BOOL(1)) {
      SOL.post(~BOOLVAR(0));
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(0), BOOLVAR(1));
  }
}

void p_bool_lt(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  SOL.post(~BOOLVAR(0));
  SOL.post(BOOLVAR(1));
}

void p_bool_eq_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_eq(s, call);
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_ne_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_ne(s, call);
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_le_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_le(s, call);
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), BOOLVAR(1));
  }
}

void p_bool_lt_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_lt(s, call);
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0));
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(1));
  }
}

void p_bool_eq_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_eq(s, call);
    } else {
      p_bool_ne(s, call);
    }
  } else {
    geas::add_clause(SD, BOOLVAR(2), BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(0), ~BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_ne_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_ne(s, call);
    } else {
      p_bool_eq(s, call);
    }
  } else {
    geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, BOOLVAR(2), BOOLVAR(0), ~BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), BOOLVAR(1));
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_le_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_bool_le(s, call);
    } else {
      //          auto nc = new Call(Location().introduce(), call->id(), {call->arg(1),
      //          call->arg(0)}); p_bool_lt(s, nc);
    }
  } else {
    geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(1));
    geas::add_clause(SD, BOOLVAR(2), BOOLVAR(0));
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0), BOOLVAR(1));
  }
}

void p_bool_lt_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (PAR(2)) {
    if (BOOL(2)) {
      p_int_lt(s, call);
    } else {
      //          auto nc = new Call(Location().introduce(), call->id(), {call->arg(1),
      //          call->arg(0)}); p_int_le(s, nc);
    }
  } else {
    geas::add_clause(SD, ~BOOLVAR(2), ~BOOLVAR(0));
    geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(1));
    geas::add_clause(SD, BOOLVAR(2), BOOLVAR(0), ~BOOLVAR(1));
  }
}

void p_bool_or(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(0));
  geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(1));
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), BOOLVAR(1));
}

void p_bool_and(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0));
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(1));
  geas::add_clause(SD, BOOLVAR(2), ~BOOLVAR(0), ~BOOLVAR(1));
}

void p_bool_xor(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  if (call->size() == 2) {
    p_bool_ne(s, call);
  } else {
    p_bool_ne_reif(s, call);
  }
}

void p_bool_not(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  p_bool_ne(s, call);
}

void p_bool_or_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0), BOOLVAR(1));
}

void p_bool_and_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(0));
  geas::add_clause(SD, ~BOOLVAR(2), BOOLVAR(1));
}

void p_bool_xor_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  p_bool_ne_imp(s, call);
}

void p_bool_clause(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto& gi = static_cast<GeasSolverInstance&>(s);
  auto pos = ARRAY(0);
  auto neg = ARRAY(1);
  vec<geas::clause_elt> clause;
  for (int i = 0; i < pos.size(); ++i) {
    clause.push(SI.asBoolVar(pos[i]));
  }
  for (int j = 0; j < neg.size(); ++j) {
    clause.push(~SI.asBoolVar(neg[j]));
  }
  geas::add_clause(*SD, clause);
}

void p_array_bool_or(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto arr = ARRAY(0);
  vec<geas::clause_elt> clause;
  clause.push(~BOOLVAR(1));
  for (int i = 0; i < arr.size(); ++i) {
    geas::patom_t elem = SI.asBoolVar(arr[i]);
    geas::add_clause(SD, BOOLVAR(1), ~elem);
    clause.push(elem);
  }
  geas::add_clause(*SD, clause);
}

void p_array_bool_and(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto arr = ARRAY(0);
  vec<geas::clause_elt> clause;
  clause.push(BOOLVAR(1));
  for (int i = 0; i < arr.size(); ++i) {
    geas::patom_t elem = SI.asBoolVar(arr[i]);
    geas::add_clause(SD, ~BOOLVAR(1), elem);
    clause.push(~elem);
  }
  geas::add_clause(*SD, clause);
}

void p_bool_clause_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto pos = ARRAY(0);
  auto neg = ARRAY(1);
  vec<geas::clause_elt> clause;
  clause.push(~BOOLVAR(2));
  for (int i = 0; i < pos.size(); ++i) {
    clause.push(SI.asBoolVar(pos[i]));
  }
  for (int j = 0; j < neg.size(); ++j) {
    clause.push(~SI.asBoolVar(neg[j]));
  }
  geas::add_clause(*SD, clause);
}

void p_array_bool_or_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto arr = ARRAY(0);
  vec<geas::clause_elt> clause;
  clause.push(~BOOLVAR(1));
  for (int i = 0; i < arr.size(); ++i) {
    geas::patom_t elem = SI.asBoolVar(arr[i]);
    clause.push(elem);
  }
  geas::add_clause(*SD, clause);
}

void p_array_bool_and_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto arr = ARRAY(0);
  for (int i = 0; i < arr.size(); ++i) {
    geas::add_clause(SD, ~BOOLVAR(1), SI.asBoolVar(arr[i]));
  }
}

void p_bool_clause_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto pos = ARRAY(0);
  auto neg = ARRAY(1);
  vec<geas::clause_elt> clause;
  clause.push(~BOOLVAR(2));
  for (int i = 0; i < pos.size(); ++i) {
    geas::patom_t elem = SI.asBoolVar(pos[i]);
    geas::add_clause(SD, BOOLVAR(2), ~elem);
    clause.push(elem);
  }
  for (int j = 0; j < neg.size(); ++j) {
    geas::patom_t elem = SI.asBoolVar(neg[j]);
    geas::add_clause(SD, BOOLVAR(2), elem);
    clause.push(~elem);
  }
  geas::add_clause(*SD, clause);
}

void p_bool_lin_eq(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::bool_linear_le(SD, geas::at_True, SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ge(SD, geas::at_True, SI.zero, cons, vars, -INT(2));
}

void p_bool_lin_ne(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  geas::bool_linear_ne(SD, cons, vars, INT(2));
}

void p_bool_lin_le(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  geas::bool_linear_le(SD, geas::at_True, SI.zero, cons, vars, -INT(2));
}

void p_bool_lin_eq_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::bool_linear_le(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ge(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
}

void p_bool_lin_ne_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  geas::bool_linear_ne(SD, cons, vars, INT(2), BOOLVAR(3));
}

void p_bool_lin_le_imp(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  geas::bool_linear_le(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
}

void p_bool_lin_eq_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::bool_linear_le(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ge(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ne(SD, cons, vars, INT(2), ~BOOLVAR(3));
}

void p_bool_lin_ne_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::bool_linear_ne(SD, cons, vars, INT(2), BOOLVAR(3));
  geas::bool_linear_le(SD, ~BOOLVAR(3), SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ge(SD, ~BOOLVAR(3), SI.zero, cons, vars, -INT(2));
}

void p_bool_lin_le_reif(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<int> cons = INTARRAY(0);
  vec<geas::patom_t> vars = BOOLVARARRAY(1);
  // TODO: Rewrite using MiniZinc Library??
  geas::bool_linear_le(SD, BOOLVAR(3), SI.zero, cons, vars, -INT(2));
  geas::bool_linear_ge(SD, ~BOOLVAR(3), SI.zero, cons, vars, -INT(2) - 1);
}

void p_bool2int(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  geas::add_clause(SD, BOOLVAR(0), INTVAR(1) <= 0);
  geas::add_clause(SD, ~BOOLVAR(0), INTVAR(1) >= 1);
}

void p_array_int_element(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  //      assert(ARRAY(1)->min(0) == 1 && ARRAY(1)->max(0) == ARRAY(1)->size()+1);
  vec<int> vals = INTARRAY(1);
  if (PAR(0)) {
    SOL.post(INTVAR(2) == vals[INT(0) - 1]);
  } else if (PAR(2)) {
    for (int j = 0; j < vals.size(); ++j) {
      if (vals[j] != INT(2)) {
        SOL.post(INTVAR(0) != j + 1);
      }
    }
  } else {
    geas::int_element(SD, INTVAR(2), INTVAR(0), vals);
  }
}

void p_array_bool_element(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  //      assert(ARRAY(1)->min(0) == 1 && ARRAY(1)->max(0) == ARRAY(1)->size()+1);
  vec<bool> vals = BOOLARRAY(1);
  if (PAR(0)) {
    SOL.post(vals[INT(0) - 1] ? BOOLVAR(2) : ~BOOLVAR(2));
  } else if (PAR(2)) {
    for (int j = 0; j < vals.size(); ++j) {
      if (vals[j] != BOOL(2)) {
        SOL.post(INTVAR(0) != j + 1);
      }
    }
  } else {
    for (int j = 0; j < vals.size(); ++j) {
      geas::add_clause(SD, INTVAR(0) != j + 1, vals[j] ? BOOLVAR(2) : ~BOOLVAR(2));
    }
  }
}

void p_array_var_int_element(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  //      assert(ARRAY(1)->min(0) == 1 && ARRAY(1)->max(0) == ARRAY(1)->size()+1);
  if (PAR(1)) {
    return p_array_int_element(s, call);
  }
  if (PAR(0) && PAR(2)) {
    SOL.post(SI.asIntVar(ARRAY(1)[INT(0) - 1]) == INT(2));
  } else if (PAR(0)) {
    Val elem = ARRAY(1)[INT(0) - 1];
    if (elem.isInt()) {
      return p_array_int_element(s, call);
    } else {
      geas::int_eq(SD, SI.asIntVar(elem), INTVAR(2));
    }
  } else if (PAR(2)) {
    for (int j = 0; j < ARRAY(1).size(); ++j) {
      Val elem = ARRAY(1)[j];
      if (elem.isDef()) {
        geas::add_clause(SD, INTVAR(0) != j + 1, SI.asIntVar(elem) == INT(2));
      } else {
        if (SI.asInt(elem) != INT(2)) {
          SOL.post(INTVAR(0) != j + 1);
        }
      }
    }
  } else {
    vec<geas::intvar> vals = INTVARARRAY(1);
    geas::var_int_element(SD, INTVAR(2), INTVAR(0), vals);
  }
}

void p_array_var_bool_element(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  //      assert(ARRAY(1)->min(0) == 1 && ARRAY(1)->max(0) == ARRAY(1)->size()+1);
  if (PAR(1)) {
    return p_array_bool_element(s, call);
  }
  if (PAR(0) && PAR(2)) {
    SOL.post(BOOL(2) ? SI.asBoolVar(ARRAY(1)[INT(0) - 1]) : ~SI.asBoolVar(ARRAY(1)[INT(0) - 1]));
  } else if (PAR(0)) {
    Val elem = ARRAY(1)[INT(0) - 1];
    if (elem.isInt()) {
      return p_array_bool_element(s, call);
    } else {
      geas::add_clause(SD, BOOLVAR(2), ~SI.asBoolVar(elem));
      geas::add_clause(SD, ~BOOLVAR(2), SI.asBoolVar(elem));
    }
  } else if (PAR(2)) {
    for (int j = 0; j < ARRAY(1).size(); ++j) {
      Val elem = ARRAY(1)[j];
      if (elem.isDef()) {
        geas::add_clause(SD, INTVAR(0) != j + 1, INT(2) ? SI.asBoolVar(elem) : ~SI.asBoolVar(elem));
      } else {
        if (SI.asBool(elem) != INT(2)) {
          SOL.post(INTVAR(0) != j + 1);
        }
      }
    }
  } else {
    auto vars = BOOLVARARRAY(1);
    for (int j = 0; j < vars.size(); ++j) {
      geas::add_clause(SD, INTVAR(0) != j + 1, ~vars[j], BOOLVAR(2));
      geas::add_clause(SD, INTVAR(0) != j + 1, vars[j], ~BOOLVAR(2));
    }
  }
}

void p_all_different(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> vars = INTVARARRAY(0);
  geas::all_different_int(SD, vars, STATE);
}

void p_all_different_except_0(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> vars = INTVARARRAY(0);
  geas::all_different_except_0(SD, vars, STATE);
}

void p_at_most(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> ivars = INTVARARRAY(1);
  vec<geas::patom_t> bvars;
  for (auto& ivar : ivars) {
    bvars.push(ivar == INT(2));
  }

  if (INT(0) == 1) {
    geas::atmost_1(SD, bvars, STATE);
  } else {
    geas::atmost_k(SD, bvars, INT(0), STATE);
  }
}

void p_at_most1(SolverInstanceBase& s, const Definition* call) {
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> ivars = INTVARARRAY(0);
  vec<geas::patom_t> bvars;
  for (auto& ivar : ivars) {
    bvars.push(ivar == INT(1));
  }
  geas::atmost_1(SD, bvars, STATE);
}

void p_cumulative(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> st = INTVARARRAY(0);
  if (PAR(1) && PAR(2) && PAR(3)) {
    vec<int> d = INTARRAY(1);
    vec<int> r = INTARRAY(2);
    geas::cumulative(SD, st, d, r, INT(3));
  } else {
    vec<geas::intvar> d = INTVARARRAY(1);
    vec<geas::intvar> r = INTVARARRAY(2);
    geas::cumulative_var(SD, st, d, r, INTVAR(3));
  }
}

void p_disjunctive(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> st = INTVARARRAY(0);
  if (PAR(1)) {
    vec<int> d = INTARRAY(1);
    geas::disjunctive_int(SD, st, d);
  } else {
    vec<geas::intvar> d = INTVARARRAY(1);
    geas::disjunctive_var(SD, st, d);
  }
}

void p_global_cardinality(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  vec<geas::intvar> x = INTVARARRAY(0);
  vec<int> cover = INTARRAY(1);
  vec<int> count = INTARRAY(2);

  vec<int> srcs(x.size(), 1);
  vec<geas::bflow> flows;
  for (int i = 0; i < x.size(); ++i) {
    for (int j = 0; j < cover.size(); ++j) {
      if (x[i].lb(SD) <= cover[j] && cover[j] <= x[i].ub(SD)) {
        flows.push({i, j, x[i] == cover[j]});
      }
    }
  }
  geas::bipartite_flow(SD, srcs, count, flows);
}

void p_table_int(SolverInstanceBase& s, const Definition* call) {
  assert(STATE == geas::at_True);
  assert(static_cast<BytecodeProc::Mode>(call->mode()) == BytecodeProc::ROOT);
  auto& gi = static_cast<GeasSolverInstance&>(s);
  vec<geas::intvar> vars = INTVARARRAY(0);
  vec<int> tmp = INTARRAY(1);
  assert(tmp.size() % vars.size() == 0);
  vec<vec<int>> table(tmp.size() / vars.size());
  for (int i = 0; i < table.size(); ++i) {
    vec<int> row(vars.size());
    for (int j = 0; j < vars.size(); ++j) {
      row[j] = tmp[i * vars.size() + j];
    }
    table.push(row);
  }
  geas::table_id id = geas::table::build(SD, table);
  // TODO: Annotations for table versions
  geas::table::post(SD, id, vars);
}

}  // namespace GeasConstraints
}  // namespace MiniZinc

#pragma clang diagnostic pop