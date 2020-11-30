/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_BYTECODE_PRIMITIVES_HH__
#define __MINIZINC_BYTECODE_PRIMITIVES_HH__

#include <minizinc/interpreter/bytecode.hh>
#include <minizinc/interpreter/constraint.hh>
#include <minizinc/interpreter/values.hh>

#include <random>

namespace MiniZinc {

class PrimitiveMap {
public:
  enum Id {
    // Partial Linear Primitives
    INT_PLUS,
    INT_MINUS,
    INT_SUM,
    INT_TIMES,

    // Linear Primitives
    INT_LIN_EQ,
    INT_LIN_EQ_REIF,
    INT_LIN_LE,
    INT_LIN_LE_REIF,

    MK_INTVAR,
    BOOLNOT,
    OP_NOT,
    CLAUSE,
    CLAUSE_REIF,
    FORALL,
    EXISTS,
    UNIFORM,
    SOL,
    SORT,
    SORT_BY,
    INT_MAX_,
    INFINITY_,
    INFINITE_DOMAIN,
    BOOLEAN_DOMAIN,
    SLICE_XD,
    ARRAY_XD,
    INDEX_SET,

    PARTIAL_LINEAR = INT_TIMES,
    MAX_LIN = INT_LIN_LE_REIF,
    MAX_ID = INDEX_SET,
  };
  class Primitive {
  protected:
    std::string _name;
    int _n_args;
    Id _ident;
    Primitive(const std::string& name, const Id& ident0, int n_args0)
        : _name(name), _n_args(n_args0), _ident(ident0) {}

  public:
    PropStatus ps_combine(const PropStatus& ps0, const PropStatus& ps1) {
      if (ps0 == PS_FAILED || ps1 == PS_FAILED) return PS_FAILED;
      if (ps0 == PS_ENTAILED || ps1 == PS_ENTAILED) return PS_ENTAILED;
      return PS_OK;
    }
    const Id& ident(void) const { return _ident; }
    int n_args(void) const { return _n_args; }
    const std::string& name(void) const { return _name; }
    virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
      assert(false);
      throw Error("internal error");
    };
    virtual void unsubscribe(Interpreter& i, Constraint* c) const {
      assert(false);
      throw Error("internal error");
    };
    virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
      assert(false);
      throw Error("internal error");
    };
    virtual void execute(Interpreter& i, const std::vector<Val>& args) {
      assert(false);
      throw Error("internal error");
    };
  };

protected:
  std::vector<Primitive*> _p;
  std::unordered_map<std::string, Primitive*> _s;
  std::vector<std::string> _n;

public:
  PrimitiveMap(void);
  Primitive* operator[](const std::string& s) { return _s[s]; }
  Primitive* operator[](int i) {
    assert(i >= 0 && i <= MAX_ID);
    assert(_p.size() == MAX_ID + 1);
    return _p[i];
  }

  std::vector<Primitive*>::iterator begin(void) { return _p.begin(); }
  std::vector<Primitive*>::iterator end(void) { return _p.end(); }
  int size(void) const { return _n.size(); }
};

PrimitiveMap& primitiveMap(void);

namespace BytecodePrimitives {

class IntPlus : public PrimitiveMap::Primitive {
public:
  IntPlus(void) : PrimitiveMap::Primitive("int_plus", PrimitiveMap::INT_PLUS, 3) {}
  virtual PropStatus subscribe(Interpreter& interpreter, Constraint* c) const {
    Val lb = Val::follow_alias(c->arg(0), &interpreter).lb() +
             Val::follow_alias(c->arg(1), &interpreter).lb();
    Val ub = Val::follow_alias(c->arg(0), &interpreter).ub() +
             Val::follow_alias(c->arg(1), &interpreter).ub();

    std::vector<Val> ndom = {lb, ub};
    // TODO: We officially don't know that it is not binding
    c->arg(2).toVar()->domain(&interpreter, ndom, false);

    return PS_OK;
  }
};

class IntMinus : public PrimitiveMap::Primitive {
public:
  IntMinus(void) : PrimitiveMap::Primitive("int_minus", PrimitiveMap::INT_MINUS, 3) {}
  virtual PropStatus subscribe(Interpreter& interpreter, Constraint* c) const {
    Val lb = Val::follow_alias(c->arg(0), &interpreter).lb() -
             Val::follow_alias(c->arg(1), &interpreter).ub();
    Val ub = Val::follow_alias(c->arg(0), &interpreter).ub() -
             Val::follow_alias(c->arg(1), &interpreter).lb();

    std::vector<Val> ndom = {lb, ub};
    // TODO: We officially don't know that it is not binding
    c->arg(2).toVar()->domain(&interpreter, ndom, false);

    return PS_OK;
  }
};

class IntSum : public PrimitiveMap::Primitive {
public:
  IntSum(void) : PrimitiveMap::Primitive("int_sum", PrimitiveMap::INT_SUM, 2) {}
  virtual PropStatus subscribe(Interpreter& interpreter, Constraint* c) const {
    Val lb = 0;
    Val ub = 0;

    Val arr = c->arg(0).toVec()->raw_data();

    for (int i = 0; i < arr.size(); ++i) {
      lb += Val::follow_alias(arr[i], &interpreter).lb();
      ub += Val::follow_alias(arr[i], &interpreter).ub();
    }

    std::vector<Val> ndom = {lb, ub};
    // TODO: We officially don't know that it is not binding
    c->arg(1).toVar()->domain(&interpreter, ndom, false);

    return PS_OK;
  }
};

class IntTimes : public PrimitiveMap::Primitive {
public:
  IntTimes(void) : PrimitiveMap::Primitive("int_times", PrimitiveMap::INT_TIMES, 3) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    assert(c->mode() == BytecodeProc::ROOT);
    bool propImmediately = true;
    for (int j = 0; j < _n_args; ++j) {
      Val arg = Val::follow_alias(c->arg(j), &i);
      if (arg.isVar()) {
        arg.toVar()->subscribe(c, Variable::SES_ANY);
        if (j <= 1 && !arg.toVar()->isBounded()) {
          propImmediately = false;
        }
      }
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    for (int j = 0; j < _n_args; ++j) {
      Val arg = Val::follow_alias(c->arg(j));
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val a = Val::follow_alias(c->arg(0), &i);
    Val b = Val::follow_alias(c->arg(1), &i);
    Val res = Val::follow_alias(c->arg(2), &i);
    if (b.isInt() && a.isVar()) {
      std::swap(a, b);
    }

    Val lb, ub;
    if ((a.isVar() && !a.toVar()->isBounded()) || (b.isVar() && !b.toVar()->isBounded())) {
      return PS_OK;
    }
    if (a.isInt() && a == 1) {
      if (res.isVar()) {
        if (b.isVar()) {
          bool success = res.toVar()->intersectDom(&i, Val(b.toVar()->domain()));
          if (!success) {
            return PS_FAILED;
          }
          res.toVar()->alias(&i, b);
          return PS_ENTAILED;
        } else {
          return res.toVar()->setVal(&i, b.toInt()) ? PS_ENTAILED : PS_FAILED;
        }
      } else {
        if (b.isVar()) {
          return b.toVar()->setVal(&i, res.toInt()) ? PS_ENTAILED : PS_FAILED;
        } else {
          return res.toInt() == b.toInt() ? PS_ENTAILED : PS_FAILED;
        }
      }
    }

    if (a.lb() > 0 && b.lb() > 0) {
      lb = a.lb() * b.lb();
      ub = a.ub() * b.ub();
    } else {
      std::vector<Val> mults = {a.lb() * b.lb(), a.lb() * b.ub(), a.ub() * b.lb(), a.ub() * b.ub()};
      lb = *std::min_element(mults.begin(), mults.end());
      ub = *std::max_element(mults.begin(), mults.end());
    }

    if (lb == ub) {
      if (res.isVar()) {
        return res.toVar()->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
      } else {
        return res.toInt() == lb ? PS_ENTAILED : PS_FAILED;
      }
    } else {
      if (res.isVar()) {
        return res.toVar()->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
      } else {
        return (res.toInt() >= lb && res.toInt() <= ub) ? PS_OK : PS_FAILED;
      }
    }
    // TODO: Backwards Propagation
  }
};

class IntLinEq : public PrimitiveMap::Primitive {
public:
  IntLinEq(void) : PrimitiveMap::Primitive("int_lin_eq", PrimitiveMap::INT_LIN_EQ, 3) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    // Check me: linear equation should be in its simplified form.

    bool propImmediately = false;
    int vars = 0;
    Val arr = c->arg(1).toVec()->raw_data();
    for (unsigned int j = 0; j < arr.size(); j++) {
      Val v = arr[j];
      assert(v.isVar());
      v.toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (arr.size() <= 2 /* || propImmediately */) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(1).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      Val arg = Val::follow_alias(arr[j], &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val var = c->arg(1).toVec()->raw_data();
    Val weight = c->arg(0).toVec()->raw_data();
    if (var.size() == 1) {
      Val v = Val::follow_alias(var[0], &i);
      if (v.isVar()) {
        if (c->arg(2) % weight[0] == 0) {
          return v.toVar()->setVal(&i, c->arg(2) / weight[0]) ? PS_ENTAILED : PS_FAILED;
        } else {
          return PS_FAILED;
        }
      } else {
        // aliased to val
        return weight[0] * v == c->arg(2) ? PS_ENTAILED : PS_FAILED;
      }
    }
    if (var.size() == 2) {
      Val lhs = Val::follow_alias(var[0], &i);
      Val rhs = Val::follow_alias(var[1], &i);
      Val lhs_c = weight[0];
      Val rhs_c = weight[1];
      if (!lhs.isVar()) {
        std::swap(lhs, rhs);
        std::swap(lhs_c, rhs_c);
      }
      if (lhs.isVar()) {
        if (rhs.isVar()) {
          if (c->arg(2) == 0 && (lhs_c + rhs_c) == 0) {
            if (lhs.toVar()->timestamp() < rhs.toVar()->timestamp()) {
              std::swap(lhs, rhs);
            }
            bool success = rhs.toVar()->intersectDom(&i, Val(lhs.toVar()->domain()));
            if (!success) {
              return PS_FAILED;
            }
            lhs.toVar()->alias(&i, rhs);
            return PS_ENTAILED;
          }
        } else {
          if (c->arg(2) % lhs_c == 0) {
            return lhs.toVar()->setVal(&i, (c->arg(2) - rhs_c * rhs) / lhs_c) ? PS_ENTAILED
                                                                              : PS_FAILED;
          } else {
            return PS_FAILED;
          }
        }
      } else {
        return lhs * lhs_c + rhs * rhs_c == c->arg(2) ? PS_ENTAILED : PS_FAILED;
      }
    }
    // More propagation?
    return PS_OK;
  }
};

class IntLinEqReif : public PrimitiveMap::Primitive {
public:
  IntLinEqReif(void)
      : PrimitiveMap::Primitive("int_lin_eq_reif", PrimitiveMap::INT_LIN_EQ_REIF, 4) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    // Check me: linear equation should be in its simplified form.

    bool propImmediately = false;
    int vars = 0;
    Val arr = c->arg(1).toVec()->raw_data();
    for (unsigned int j = 0; j < arr.size(); j++) {
      Val v = arr[j];
      assert(v.isVar());
      v.toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (c->arg(3).isVar()) {
      c->arg(3).toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (arr.size() <= 1 /* || propImmediately */) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(1).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      Val arg = Val::follow_alias(arr[j], &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
    Val r = Val::follow_alias(c->arg(3), &i);
    if (r.isVar()) {
      r.toVar()->subscribe(c, Variable::SES_VAL);
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val var = c->arg(1).toVec()->raw_data();
    Val weight = c->arg(0).toVec()->raw_data();
    Val r = Val::follow_alias(c->arg(3), &i);
    if (r.isInt()) {
      // TODO: Rewrite to int_lin_eq
      return PS_OK;
    }
    if (var.size() == 1) {
      Val v = Val::follow_alias(var[0], &i);
      Val mult = Val::follow_alias(weight[0], &i);
      if (v.isVar()) {
        if (c->arg(2) % mult == 0) {
          Val res = c->arg(2) / mult;
          Vec* dom = v.toVar()->domain();
          bool indom = true;
          for (int j = 0; j < dom->size(); j += 2) {
            if ((*dom)[j] <= res && res <= (*dom)[j + 1]) {
              return PS_OK;
            }
          }
          return r.toVar()->setVal(&i, false) ? PS_ENTAILED : PS_FAILED;
        } else {
          return PS_FAILED;
        }
      } else {
        // aliased to val
        return r.toVar()->setVal(&i, mult * v == c->arg(2)) ? PS_ENTAILED : PS_FAILED;
      }
    }
    // More propagation?
    return PS_OK;
  }
};

class IntLinLe : public PrimitiveMap::Primitive {
public:
  IntLinLe(void) : PrimitiveMap::Primitive("int_lin_le", PrimitiveMap::INT_LIN_LE, 3) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    // Check me: linear equation should be in its simplified form.

    Val arr = c->arg(1).toVec()->raw_data();
    for (unsigned int j = 0; j < arr.size(); j++) {
      Val v = arr[j];
      assert(v.isVar());  // cannot be alias because of simplify_linexp
      v.toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (arr.size() <= 2 /* || propImmediately */) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(1).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      Val arg = Val::follow_alias(arr[j], &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val var = c->arg(1).toVec()->raw_data();
    Val weight = c->arg(0).toVec()->raw_data();
    if (var.size() == 1) {
      Val v = Val::follow_alias(var[0], &i);
      if (v.isVar()) {
        Val newBound = c->arg(2) / weight[0];
        if (weight[0] > 0) {
          return v.toVar()->setMax(&i, newBound) ? PS_ENTAILED : PS_FAILED;
        } else {
          return v.toVar()->setMin(&i, newBound) ? PS_ENTAILED : PS_FAILED;
        }
      } else {
        // aliased to val
        return weight[0] * v <= c->arg(2) ? PS_ENTAILED : PS_FAILED;
      }
    }
    // More propagation?
    return PS_OK;
  }
};

class IntLinLeReif : public PrimitiveMap::Primitive {
public:
  IntLinLeReif(void)
      : PrimitiveMap::Primitive("int_lin_le_reif", PrimitiveMap::INT_LIN_LE_REIF, 4) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    // Check me: linear equation should be in its simplified form.

    Val arr = c->arg(1).toVec()->raw_data();
    for (unsigned int j = 0; j < arr.size(); j++) {
      Val v = arr[j];
      assert(v.isVar());  // cannot be alias because of simplify_linexp
      v.toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (c->arg(3).isVar()) {
      c->arg(3).toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (arr.size() <= 1 /* || propImmediately */) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(1).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      Val arg = Val::follow_alias(arr[j], &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
    Val r = Val::follow_alias(c->arg(3), &i);
    if (r.isVar()) {
      r.toVar()->unsubscribe(c);
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val var = c->arg(1).toVec()->raw_data();
    Val weight = c->arg(0).toVec()->raw_data();
    Val r = Val::follow_alias(c->arg(3), &i);
    if (r.isInt()) {
      // TODO: Replace with int_lin_le
      return PS_OK;
    }
    if (var.size() == 1) {
      Val mult = weight[0];
      Val v = Val::follow_alias(var[0], &i);
      if (v.isVar()) {
        if (mult * v.lb() <= c->arg(2) && mult * v.ub() <= c->arg(2)) {
          return r.toVar()->setVal(&i, true) ? PS_ENTAILED : PS_FAILED;
        } else if (mult * v.lb() > c->arg(2) && mult * v.ub() > c->arg(2)) {
          return r.toVar()->setVal(&i, false) ? PS_ENTAILED : PS_FAILED;
        }
      } else {
        // aliased to val
        return r.toVar()->setVal(&i, mult * v <= c->arg(2)) ? PS_ENTAILED : PS_FAILED;
      }
    }
    // More propagation?
    return PS_OK;
  }
};

class MkIntVar : public PrimitiveMap::Primitive {
public:
  MkIntVar(void) : PrimitiveMap::Primitive("mk_intvar", PrimitiveMap::MK_INTVAR, 1) {}
};

class BoolNot : public PrimitiveMap::Primitive {
public:
  BoolNot(void) : PrimitiveMap::Primitive("bool_not", PrimitiveMap::BOOLNOT, 2) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = false;
    for (unsigned int j = 0; j < _n_args; j++) {
      Val arg = c->arg(j);
      if (arg.isVar()) {
        arg.toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    for (unsigned int j = 0; j < _n_args; j++) {
      Val arg = Val::follow_alias(c->arg(j), &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
    Val lhs = Val::follow_alias(c->arg(0), &i);
    Val rhs = Val::follow_alias(c->arg(1), &i);
    if (!lhs.isInt()) {
      std::swap(lhs, rhs);
    }
    if (!lhs.isInt()) {
      return PS_OK;
    }
    if (rhs.isInt()) {
      return lhs == rhs ? PS_ENTAILED : PS_FAILED;
    }
    return rhs.toVar()->setVal(&i, 1 - lhs) ? PS_ENTAILED : PS_FAILED;
  }
};
// Reserve procedure code for op_not operation (to create CSE entries)
class OpNot : public PrimitiveMap::Primitive {
public:
  OpNot(void) : PrimitiveMap::Primitive("f_op_not_vb", PrimitiveMap::OP_NOT, 1) {}
};

class Clause : public PrimitiveMap::Primitive {
public:
  Clause(void) : PrimitiveMap::Primitive("bool_clause", PrimitiveMap::CLAUSE, 2) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = false;
    Val pos = c->arg(0).toVec()->raw_data();
    Val neg = c->arg(1).toVec()->raw_data();

    for (unsigned int i = 0; i < pos.size(); i++) {
      if (pos[i].isVar()) {
        pos[i].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    for (unsigned int i = 0; i < neg.size(); i++) {
      if (neg[i].isVar()) {
        neg[i].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val pos = c->arg(0).toVec()->raw_data();
    Val neg = c->arg(1).toVec()->raw_data();

    for (unsigned int i = 0; i < pos.size(); i++) {
      Val arg = Val::follow_alias(pos[i]);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
    for (unsigned int i = 0; i < neg.size(); i++) {
      Val arg = Val::follow_alias(neg[i]);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const { return PS_OK; }
};

class ClauseReif : public PrimitiveMap::Primitive {
public:
  ClauseReif(void) : PrimitiveMap::Primitive("bool_clause_reif", PrimitiveMap::CLAUSE_REIF, 3) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = false;
    Val pos = c->arg(0).toVec()->raw_data();
    Val neg = c->arg(1).toVec()->raw_data();

    for (unsigned int i = 0; i < pos.size(); i++) {
      if (pos[i].isVar()) {
        pos[i].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    for (unsigned int i = 0; i < neg.size(); i++) {
      if (neg[i].isVar()) {
        neg[i].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    if (c->arg(2).isVar()) {
      c->arg(2).toVar()->subscribe(c, Variable::SES_VAL);
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val pos = c->arg(0).toVec()->raw_data();
    Val neg = c->arg(1).toVec()->raw_data();

    for (unsigned int i = 0; i < pos.size(); i++) {
      Val arg = Val::follow_alias(pos[i]);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
    for (unsigned int i = 0; i < neg.size(); i++) {
      Val arg = Val::follow_alias(neg[i]);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }

    Val arg = Val::follow_alias(c->arg(2));
    if (arg.isVar()) {
      arg.toVar()->unsubscribe(c);
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const { return PS_OK; }
};

class Forall : public PrimitiveMap::Primitive {
public:
  Forall(void) : PrimitiveMap::Primitive("array_bool_and", PrimitiveMap::FORALL, 2) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = false;

    Val arr = c->arg(0).toVec()->raw_data();
    for (unsigned int j = 0; j < arr.size(); j++) {
      if (arr[j].isVar()) {
        arr[j].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    if (c->arg(1).isVar()) {
      c->arg(1).toVar()->subscribe(c, Variable::SES_VAL);
    } else {
      propImmediately = true;
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(0).toVec()->raw_data();
    for (unsigned int i = 0; i < arr.size(); i++) {
      Val arg = Val::follow_alias(arr[i]);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
    Val arg = Val::follow_alias(c->arg(1));
    if (arg.isVar()) {
      arg.toVar()->unsubscribe(c);
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const { return PS_OK; }
};

class Exists : public PrimitiveMap::Primitive {
public:
  Exists(void) : PrimitiveMap::Primitive("array_bool_or", PrimitiveMap::EXISTS, 2) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = false;

    Val arr = c->arg(0).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      if (arr[j].isVar()) {
        arr[j].toVar()->subscribe(c, Variable::SES_VAL);
      } else {
        propImmediately = true;
      }
    }
    if (c->arg(1).isVar()) {
      c->arg(1).toVar()->subscribe(c, Variable::SES_VAL);
    } else {
      propImmediately = true;
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    Val arr = c->arg(0).toVec()->raw_data();
    for (int j = 0; j < arr.size(); ++j) {
      Val arg = Val::follow_alias(arr[j], &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* c) const { return PS_OK; }
};

class Uniform : public PrimitiveMap::Primitive {
public:
  Uniform() : PrimitiveMap::Primitive("uniform", PrimitiveMap::UNIFORM, 2) {
    std::random_device rnd;
    generator = std::mt19937(0);
  }
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
  void setSeed(int seed) { generator = std::mt19937(seed); }

private:
  std::mt19937 generator;
};

class Sol : public PrimitiveMap::Primitive {
public:
  Sol() : PrimitiveMap::Primitive("sol", PrimitiveMap::SOL, 1) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class Sort : public PrimitiveMap::Primitive {
public:
  Sort() : PrimitiveMap::Primitive("internal_sort", PrimitiveMap::SORT, 1) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class SortBy : public PrimitiveMap::Primitive {
public:
  SortBy() : PrimitiveMap::Primitive("sort_by", PrimitiveMap::SORT_BY, 2) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class IntMax : public PrimitiveMap::Primitive {
public:
  IntMax(void) : PrimitiveMap::Primitive("int_max", PrimitiveMap::INT_MAX_, 3) {}
  virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
    bool propImmediately = true;
    for (int j = 0; j < _n_args; ++j) {
      Val arg = Val::follow_alias(c->arg(j), &i);
      if (arg.isVar()) {
        arg.toVar()->subscribe(c, Variable::SES_ANY);
        if (!arg.toVar()->isBounded()) {
          propImmediately = false;
        }
      }
    }
    if (propImmediately) {
      return propagate(i, c);
    } else {
      return PS_OK;
    }
  }
  virtual void unsubscribe(Interpreter& i, Constraint* c) const {
    for (int j = 0; j < _n_args; ++j) {
      Val arg = Val::follow_alias(c->arg(j), &i);
      if (arg.isVar()) {
        arg.toVar()->unsubscribe(c);
      }
    }
  }
  virtual PropStatus propagate(Interpreter& i, Constraint* con) const {
    Val a = Val::follow_alias(con->arg(0), &i);
    Val b = Val::follow_alias(con->arg(1), &i);
    Val c = Val::follow_alias(con->arg(2), &i);

    if ((a.isVar() && !a.toVar()->isBounded()) || (b.isVar() && !b.toVar()->isBounded())) {
      return PS_OK;
    } else if (a.ub() <= b.lb() || a.ub() < c.lb() || a.lb() > c.ub()) {
      if (c.isVar()) {
        c.toVar()->alias(&i, b);
        return PS_ENTAILED;
      } else if (b.isVar()) {
        return b.toVar()->setVal(&i, c) ? PS_ENTAILED : PS_FAILED;
      } else {
        return b == c ? PS_ENTAILED : PS_FAILED;
      }
    } else if (b.ub() <= a.lb() || b.ub() < c.lb() || b.lb() > c.ub()) {
      if (c.isVar()) {
        c.toVar()->alias(&i, a);
        return PS_ENTAILED;
      } else if (a.isVar()) {
        return a.toVar()->setVal(&i, c) ? PS_ENTAILED : PS_FAILED;
      } else {
        return a == c ? PS_ENTAILED : PS_FAILED;
      }
    }

    Val lb, ub;
    lb = std::max(a.lb(), b.lb());
    ub = std::max(a.ub(), b.ub());
    // FIXME: c is not guaranteed to be a variable
    if (lb == ub) {
      return c.toVar()->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
    } else {
      return c.toVar()->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
    }
  }
};

class Infinity : public PrimitiveMap::Primitive {
public:
  Infinity() : PrimitiveMap::Primitive("infinity", PrimitiveMap::INFINITY_, 1) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class InfiniteDomain : public PrimitiveMap::Primitive {
public:
  InfiniteDomain() : PrimitiveMap::Primitive("infinite_domain", PrimitiveMap::INFINITE_DOMAIN, 0) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class BooleanDomain : public PrimitiveMap::Primitive {
public:
  BooleanDomain() : PrimitiveMap::Primitive("boolean_domain", PrimitiveMap::BOOLEAN_DOMAIN, 0) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};

class SliceXd : public PrimitiveMap::Primitive {
public:
  SliceXd() : PrimitiveMap::Primitive("slice_Xd", PrimitiveMap::SLICE_XD, 3) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};
class ArrayXd : public PrimitiveMap::Primitive {
public:
  ArrayXd() : PrimitiveMap::Primitive("array_Xd", PrimitiveMap::ARRAY_XD, 2) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};
class IndexSet : public PrimitiveMap::Primitive {
public:
  IndexSet() : PrimitiveMap::Primitive("index_set", PrimitiveMap::INDEX_SET, 2) {}
  virtual void execute(Interpreter& i, const std::vector<Val>& args);
};
}  // namespace BytecodePrimitives
}  // namespace MiniZinc

#endif
