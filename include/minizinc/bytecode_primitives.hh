/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_BYTECODE_PRIMITIVES_HH__
#define __MINIZINC_BYTECODE_PRIMITIVES_HH__

#include <minizinc/bytecode.hh>
#include <random>

namespace MiniZinc {
  
  class PrimitiveMap {
  public:
    enum Id {
      MK_INTVAR,
      BOOLNOT,
      OP_NOT,
      CLAUSE,
      CLAUSE_REIF,
      FORALL,
      EXISTS,
      INT_TIMES,
      INT_LIN_EQ,
      INT_LIN_LE,
      UNIFORM,
      SOL,
      SORT,
      SORT_BY,
      INT_MAX_,
      INFINITY_,
      INFINITE_DOMAIN,
      BOOLEAN_DOMAIN,
      SLICE_XD,
      MAX_ID=SLICE_XD,
    };
    class Primitive {
    protected:
      std::string _name;
      int _n_args;
      Id _ident;
      Primitive(const std::string& name, const Id& ident0, int n_args0) : _name(name), _n_args(n_args0), _ident(ident0) {}
    public:
      PropStatus ps_combine(const PropStatus& ps0, const PropStatus& ps1) {
        if (ps0==PS_FAILED || ps1==PS_FAILED)
          return PS_FAILED;
        if (ps0==PS_ENTAILED || ps1==PS_ENTAILED)
          return PS_ENTAILED;
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
    std::unordered_map<std::string,Primitive*> _s;
    std::vector<std::string> _n;
  public:
    PrimitiveMap(void);
    Primitive* operator [](const std::string& s) { return _s[s]; }
    Primitive* operator [](int i) {
      assert(i >= 0 && i <= MAX_ID);
      assert(_p.size()==MAX_ID+1);
      return _p[i];
    }
    
    std::vector<Primitive*>::iterator begin(void) { return _p.begin(); }
    std::vector<Primitive*>::iterator end(void) { return _p.end(); }
    int size(void) const { return _n.size(); }
  };
  
  PrimitiveMap& primitiveMap(void);
  
  namespace BytecodePrimitives {

    class MkIntVar : public PrimitiveMap::Primitive {
    public:
      MkIntVar(void) : PrimitiveMap::Primitive("mk_intvar",PrimitiveMap::MK_INTVAR,1) {}
    };

    class BoolNot : public PrimitiveMap::Primitive {
    public:
      BoolNot(void) : PrimitiveMap::Primitive("bool_not",PrimitiveMap::BOOLNOT,2) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        bool propImmediately = false;
        for (unsigned int j=0; j<_n_args; j++) {
          Val arg = c->arg(j);
          if (arg.isVar()) {
            arg.toVar()->subscribe(c, Variable::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        if (propImmediately) {
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (unsigned int j=0; j<_n_args; j++) {
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
          return lhs() == rhs() ? PS_ENTAILED : PS_FAILED;
        }
        return rhs.toVar()->setVal(&i, 1 - lhs()) ? PS_ENTAILED : PS_FAILED;
      }
    };
    // Reserve procedure code for op_not operation (to create CSE entries)
    class OpNot: public PrimitiveMap::Primitive {
    public:
      OpNot(void) : PrimitiveMap::Primitive("f_op_not_vb",PrimitiveMap::OP_NOT,1) {}
    };

    class Clause : public PrimitiveMap::Primitive {
    public:
      Clause(void) : PrimitiveMap::Primitive("bool_clause",PrimitiveMap::CLAUSE,2) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<c->arg(0)[0].size(); i++) {
          if (c->arg(0)[0][i].isVar()) {
            c->arg(0)[0][i].toVar()->subscribe(c, Variable::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        for (unsigned int i=0; i<c->arg(1)[0].size(); i++) {
          if (c->arg(1)[0][i].isVar()) {
            c->arg(1)[0][i].toVar()->subscribe(c, Variable::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        if (propImmediately) {
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (unsigned int i=0; i<c->arg(0).size(); i++) {
          Val arg = Val::follow_alias(c->arg(0)[0][i]);
          if (arg.isVar()) {
            arg.toVar()->unsubscribe(c);
          }
        }
        for (unsigned int i=0; i<c->arg(1).size(); i++) {
          Val arg = Val::follow_alias(c->arg(1)[0][i]);
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
        for (unsigned int i=0; i<c->arg(0)[0].size(); i++) {
          if (c->arg(0)[0][i].isVar()) {
            c->arg(0)[0][i].toVar()->subscribe(c, Variable::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        for (unsigned int i=0; i<c->arg(1)[0].size(); i++) {
          if (c->arg(1)[0][i].isVar()) {
            c->arg(1)[0][i].toVar()->subscribe(c, Variable::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        if (c->arg(2).isVar()) {
            c->arg(2).toVar()->subscribe(c, Variable::SES_VAL);
        }
        if (propImmediately) {
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (unsigned int i=0; i<c->arg(0).size(); i++) {
          Val arg = Val::follow_alias(c->arg(0)[0][i]);
          if (arg.isVar()) {
            arg.toVar()->unsubscribe(c);
          }
        }
        for (unsigned int i=0; i<c->arg(1).size(); i++) {
          Val arg = Val::follow_alias(c->arg(1)[0][i]);
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
      Forall(void) : PrimitiveMap::Primitive("array_bool_and",PrimitiveMap::FORALL,2) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        bool propImmediately = false;
        for (unsigned int j=0; j<c->arg(0)[0].size(); j++) {
          if (c->arg(0)[0][j].isVar()) {
            c->arg(0)[0][j].toVar()->subscribe(c, Variable::SES_VAL);
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
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (unsigned int i=0; i<c->arg(0).size(); i++) {
          Val arg = Val::follow_alias(c->arg(0)[0][i]);
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
      Exists(void) : PrimitiveMap::Primitive("array_bool_or",PrimitiveMap::EXISTS,2) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        bool propImmediately = false;
        for (int j = 0; j < c->arg(0)[0].size(); ++j) {
          if (c->arg(0)[0][j].isVar()) {
            c->arg(0)[0][j].toVar()->subscribe(c, Variable::SES_VAL);
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
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (int j = 0; j < c->arg(0)[0].size(); ++j) {
          Val arg = Val::follow_alias(c->arg(0)[0][j], &i);
          if (arg.isVar()) {
            arg.toVar()->unsubscribe(c);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
        return PS_OK;
      }
    };

    class IntTimes : public PrimitiveMap::Primitive {
    public:
      IntTimes(void) : PrimitiveMap::Primitive("int_times",PrimitiveMap::INT_TIMES,3) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        assert(c->mode() == BytecodeProc::ROOT);
        bool propImmediately = true;
        for (int j = 0; j < _n_args; ++j) {
          Val arg = c->arg(j);
          if (arg.isVar()) {
            arg.toVar()->subscribe(c, Variable::SES_ANY);
            if (!arg.toVar()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,c);
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
      virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
        Val a = Val::follow_alias(c->arg(0), &i);
        Val b = Val::follow_alias(c->arg(1), &i);
        Val res = Val::follow_alias(c->arg(2), &i);
        if (b.isInt() && a.isVar()) {
          std::swap(a, b);
        }

        IntVal lb, ub;
        if ((a.isVar() && !a.toVar()->isBounded()) || (b.isVar() && !b.toVar()->isBounded())) {
          return PS_OK;
        } else if (a.isInt() && a.lb() == IntVal(1)) {
//          res.alias(&i, b);
          /// TODO! needs aliasing
          return PS_ENTAILED;
        }

        lb = a.lb();
        ub = a.ub();

        lb *= b.lb();
        ub *= b.ub();

        /// TODO: what if res is not a var?
        if (lb == ub) {
          return res.toVar()->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return res.toVar()->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };

    class IntLinEq : public PrimitiveMap::Primitive {
    public:
      IntLinEq(void) : PrimitiveMap::Primitive("int_lin_eq",PrimitiveMap::INT_LIN_EQ,3) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        {
          std::vector<Val> coeffs = c->arg(0)[0].toVec()->as_vector();
          std::vector<Val> vars = c->arg(1)[0].toVec()->as_vector();
          IntVal d = -c->arg(2)();
          simplify_linexp(coeffs, vars, d);

          Vec* ncoeffs = Vec::allocate_array(&i, i.newIdent(), coeffs);
          Vec* nvars = Vec::allocate_array(&i, i.newIdent(), vars);
          c->arg(&i, 0, Val(ncoeffs));
          c->arg(&i, 1, Val(nvars));
          c->arg(&i, 2, Val(-d));
        }

        bool propImmediately = false;
        int vars = 0;
        for (unsigned int j=0; j < c->arg(1)[0].size(); j++) {
          Val v = c->arg(1)[0][j];
          assert(v.isVar());
          v.toVar()->subscribe(c, Variable::SES_VAL);
        }
        if (c->arg(1)[0].size() <= 2 /* || propImmediately */) {
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (int j = 0; j < c->arg(1)[0].size(); ++j) {
          Val arg = Val::follow_alias(c->arg(1)[0][j], &i);
          if (arg.isVar()) {
            arg.toVar()->unsubscribe(c);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
        if (c->arg(1)[0].size() == 1) {
          Val v = Val::follow_alias(c->arg(1)[0][0], &i);
          if (v.isVar()) {
            if (c->arg(2)() % c->arg(0)[0][0]()==0) {
              return v.toVar()->setVal(&i, c->arg(2)() / c->arg(0)[0][0]()) ? PS_ENTAILED : PS_FAILED;
            } else {
              return PS_FAILED;
            }
          } else {
            // aliased to val
            return c->arg(0)[0][0]()*v() == c->arg(2)() ? PS_ENTAILED : PS_FAILED;
          }
        }
        if (c->arg(1)[0].size() == 2) {
          Val lhs = Val::follow_alias(c->arg(1)[0][0], &i);
          Val rhs = Val::follow_alias(c->arg(1)[0][1], &i);
          IntVal lhs_c = c->arg(0)[0][0]();
          IntVal rhs_c = c->arg(0)[0][1]();
          if (!lhs.isVar()) {
            std::swap(lhs, rhs);
            std::swap(lhs_c, rhs_c);
          }
          if (lhs.isVar()) {
            if (rhs.isVar()) {
              if (c->arg(2)() == 0 && (lhs_c+rhs_c) == 0) {
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
              if (c->arg(2)() % lhs_c==0) {
                return lhs.toVar()->setVal(&i, (c->arg(2)()-rhs_c*rhs()) / lhs_c) ? PS_ENTAILED : PS_FAILED;
              } else {
                return PS_FAILED;
              }
            }
          } else {
            return lhs()*lhs_c+rhs()*rhs_c==c->arg(2)() ? PS_ENTAILED : PS_FAILED;
          }
        }
        // More propagation?
        return PS_OK;
      }
    };

    class IntLinLe : public PrimitiveMap::Primitive {
    public:
      IntLinLe(void) : PrimitiveMap::Primitive("int_lin_le",PrimitiveMap::INT_LIN_LE,3) {}
      virtual PropStatus subscribe(Interpreter& i, Constraint* c) const {
        {
          std::vector<Val> coeffs = c->arg(0)[0].toVec()->as_vector();
          std::vector<Val> vars = c->arg(1)[0].toVec()->as_vector();
          IntVal d = -c->arg(2)();
          simplify_linexp(coeffs, vars, d);

          Vec* ncoeffs = Vec::allocate_array(&i, i.newIdent(), coeffs);
          Vec* nvars = Vec::allocate_array(&i, i.newIdent(), vars);
          c->arg(&i, 0, Val(ncoeffs));
          c->arg(&i, 1, Val(nvars));
          c->arg(&i, 2, Val(-d));
        }

        for (unsigned int j=0; j < c->arg(1)[0].size(); j++) {
          Val v = c->arg(1)[0][j];
          assert(v.isVar()); // cannot be alias because of simplify_linexp
          v.toVar()->subscribe(c, Variable::SES_VAL);
        }
        if (c->arg(1)[0].size() <= 2 /* || propImmediately */) {
          return propagate(i,c);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Constraint* c) const {
        for (int j = 0; j < c->arg(1)[0].size(); ++j) {
          Val arg = Val::follow_alias(c->arg(1)[0][j], &i);
          if (arg.isVar()) {
            arg.toVar()->unsubscribe(c);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Constraint* c) const {
        if (c->arg(1)[0].size() == 1) {
          Val v = Val::follow_alias(c->arg(1)[0][0], &i);
          if (v.isVar()) {
            IntVal newBound = c->arg(2)() / c->arg(0)[0][0]();
            if (c->arg(0)[0][0]() > 0) {
              return v.toVar()->setMax(&i, newBound) ? PS_ENTAILED : PS_FAILED;
            } else {
              return v.toVar()->setMin(&i, newBound) ? PS_ENTAILED : PS_FAILED;
            }
          } else {
            // aliased to val
            return c->arg(0)[0][0]()*v() <= c->arg(2)() ? PS_ENTAILED : PS_FAILED;
          }
        }
        // More propagation?
        return PS_OK;
      }
    };
  
    class Uniform : public PrimitiveMap::Primitive {
    public:
      Uniform() : PrimitiveMap::Primitive("uniform",PrimitiveMap::UNIFORM,2) {
        std::random_device rnd;
        generator = std::mt19937(0);
      }
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size() == 2);
        assert(args[0].isInt() && args[1].isInt());

        std::uniform_int_distribution<> dis(args[0]().toInt(), args[1]().toInt());
        Val rnd(IntVal(dis(generator)));
        i.pushAgg(rnd, -1);
      };
      void setSeed(int seed) {
        generator = std::mt19937(seed);
      }
    private:
      std::mt19937 generator;
    };

    class Sol : public PrimitiveMap::Primitive {
    public:
      Sol() : PrimitiveMap::Primitive("sol",PrimitiveMap::SOL, 1) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size() == 1);
        assert(args[0].isVar());

        auto it = i.solutions.find(args[0].timestamp());
        assert(it != i.solutions.end());
        Val sol(it->second);

        i.pushAgg(sol, -1);
      };
    };

    class Sort : public PrimitiveMap::Primitive {
    public:
      Sort() : PrimitiveMap::Primitive("internal_sort",PrimitiveMap::SORT, 1) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==1);

        Val al = args[0][0];
        std::vector<int> ai(al.size());
        for (int j=0; j < al.size(); j++) {
          ai[j] = al[j]().toInt();
        }
        std::stable_sort(ai.begin(), ai.end());

        std::vector<Val> sorted(al.size());
        for (int j=0; j < al.size(); j++) {
          sorted[j] = Val(ai[j]);
        }
        Vec* al_sorted = Vec::allocate_array(&i, i.newIdent(), sorted);

        i.pushAgg(Val(al_sorted), -1);
      };
    };

    class SortBy : public PrimitiveMap::Primitive {
    public:
      SortBy() : PrimitiveMap::Primitive("sort_by",PrimitiveMap::SORT_BY, 2) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==2);

        Val al = args[0][0];
        Val order_e = args[1][0];
        std::vector<IntVal> order(order_e.size());
        std::vector<int> a(order_e.size());
        for (int j=0; j < order.size(); j++) {
          a[j] = j;
          order[j] = order_e[j]();
        }
        struct Ord {
          std::vector<IntVal>& order;
          explicit Ord(std::vector<IntVal>& order0) : order(order0) {}
          bool operator()(int i, int j) {
            return order[i] < order[j];
          }
        } _ord(order);
        std::stable_sort(a.begin(), a.end(), _ord);
        std::vector<Val> sorted(a.size());
        for (int j = sorted.size(); j--;) {
          sorted[j] = al[a[j]];
        }
        Vec* al_sorted = Vec::allocate_array(&i, i.newIdent(), sorted);

        i.pushAgg(Val(al_sorted), -1);
      };
    };

    class IntMax : public PrimitiveMap::Primitive {
    public:
      IntMax(void) : PrimitiveMap::Primitive("int_max",PrimitiveMap::INT_MAX_,3) {}
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
          return propagate(i,c);
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
          } else if(b.isVar()) {
            return b.toVar()->setVal(&i, c()) ? PS_ENTAILED : PS_FAILED;
          } else {
            return b == c ? PS_ENTAILED : PS_FAILED;
          }
        } else if (b.ub() <= a.lb() || b.ub() < c.lb() || b.lb() > c.ub()) {
          if (c.isVar()) {
            c.toVar()->alias(&i, a);
            return PS_ENTAILED;
          } else if(a.isVar()) {
            return a.toVar()->setVal(&i, c()) ? PS_ENTAILED : PS_FAILED;
          } else {
            return a == c ? PS_ENTAILED : PS_FAILED;
          }
        }

        IntVal lb, ub;
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
      Infinity() : PrimitiveMap::Primitive("infinity",PrimitiveMap::INFINITY_, 1) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==1);
        assert(args[0].isInt());

        if (args[0]() > 0) {
          i.pushAgg(Val(IntVal::infinity()), -1);
        } else {
          i.pushAgg(Val(-IntVal::infinity()), -1);
        }
      };
    };

    class InfiniteDomain : public PrimitiveMap::Primitive {
    public:
      InfiniteDomain() : PrimitiveMap::Primitive("infinite_domain",PrimitiveMap::INFINITE_DOMAIN, 0) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==0);
        i.pushAgg(i.infinite_domain(), -1);
      };
    };

    class BooleanDomain : public PrimitiveMap::Primitive {
    public:
      BooleanDomain() : PrimitiveMap::Primitive("boolean_domain",PrimitiveMap::BOOLEAN_DOMAIN, 0) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==0);
        i.pushAgg(i.boolean_domain(), -1);
      };
    };

  class SliceXd : public PrimitiveMap::Primitive {
  public:
    SliceXd() : PrimitiveMap::Primitive("slice_Xd",PrimitiveMap::SLICE_XD, 3) {}
    virtual void execute(Interpreter& i, const std::vector<Val>& args) {
      assert(args.size()==3);
      assert(args[0].isVec() && args[1].isVec() && args[2].isVec() );
      assert(args[0][1].size() / 2 == args[1][0].size());

      std::vector<IntVal> idxs(args[1][0].size());
      std::vector<Val> slice;
      // Initialise indexes
      for (int j = 0; j < idxs.size(); ++j) {
        idxs[j] = args[0][1][j*2]();
      }

      // Walk through array and make slice selection
      int level = idxs.size() - 1;
      int it = 0;
      while (level >= 0) {
        bool in_slice = true;
        for (int k = 0; k < idxs.size(); ++k) {
          in_slice = in_slice && args[1][0][k][0]() <= idxs[k] && idxs[k] <= args[1][0][k][1]();
        }

        assert(it < args[0][0].size());
        if (in_slice) {
          slice.push_back(args[0][0][it]);
        }
        it++;

        while (level >= 0) {
          if (idxs[level] < args[0][1][level*2+1]()) {
            idxs[level]++;
            level = idxs.size() - 1;
            break;
          } else {
            idxs[level] = args[0][1][level*2]();
            level--;
          }
        }
      }

      // Format new index sets
      std::vector<Val> dom;
      dom.reserve(args[2][0].size() * 2);
      for (int j = 0; j < args[2][0].size(); ++j) {
        assert(args[2][0][j].size() == 2);
        dom.push_back(args[2][0][j][0]);
        dom.push_back(args[2][0][j][1]);
      }

      Vec* values = Vec::a(&i, i.newIdent(), slice);
      Vec* idx = Vec::a(&i, i.newIdent(), dom);
      Vec* nv = Vec::a(&i, i.newIdent(), {Val(values), Val(idx)});

      i.pushAgg(Val(nv), -1);
    };
  };
}
}

#endif

