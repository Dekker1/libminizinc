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
      ALIAS,
      MK_INTVAR,
      BOOLNOT,
      CLAUSE,
      FORALL,
      EXISTS,
      INT_PLUS,
      INT_SUM,
      INT_MINUS,
      INT_TIMES,
      LINEXP,
      UNIFORM,
      SOL,
      SORT_BY,
      INT_MAX_,
      MAX_ID=INT_MAX_
    };
    class Primitive {
    protected:
      std::string _name;
      int _n_args;
      Id _ident;
      Primitive(const std::string& name, const Id& ident0, int n_args0) : _name(name), _n_args(n_args0), _ident(ident0) {}
    public:
      enum PropStatus { PS_OK, PS_FAILED, PS_ENTAILED };
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
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        assert(false);
        throw Error("internal error");
      };
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        assert(false);
        throw Error("internal error");
      };
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
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

    class Alias : public PrimitiveMap::Primitive {
    public:
      Alias(void) : PrimitiveMap::Primitive("<alias>",PrimitiveMap::ALIAS,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const { return PS_OK; }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {}
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        assert(false);
        throw Error("internal error");
      }
    };

    class BoolNot : public PrimitiveMap::Primitive {
    public:
      BoolNot(void) : PrimitiveMap::Primitive("bool_not",PrimitiveMap::BOOLNOT,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        if (d->arg(0).isDef()) {
          d->arg(0).toDef()->subscribe(d, Definition::SES_VAL);
          return PS_OK;
        } else {
          return propagate(i,d);
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        if (d->arg(0).isDef()) {
          d->arg(0).toDef()->unsubscribe(d);
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
    };

    class MkIntVar : public PrimitiveMap::Primitive {
    public:
      MkIntVar(void) : PrimitiveMap::Primitive("mk_intvar",PrimitiveMap::MK_INTVAR,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        assert(!d->isBounded());
        if (d->arg(0).isVec()) {
          // Propagate declared domain to definition
          d->domain(&i, d->arg(0), false);
        }
        return PS_OK;
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        assert(false);
        throw Error("internal error");
      }
    };

    class Clause : public PrimitiveMap::Primitive {
    public:
      Clause(void) : PrimitiveMap::Primitive("clause",PrimitiveMap::CLAUSE,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<d->arg(0).size(); i++) {
          if (d->arg(0)[i].isDef()) {
            d->arg(0)[i].toDef()->subscribe(d, Definition::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        for (unsigned int i=0; i<d->arg(1).size(); i++) {
          if (d->arg(1)[i].isDef()) {
            d->arg(1)[i].toDef()->subscribe(d, Definition::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (unsigned int i=0; i<d->arg(0).size(); i++) {
          if (d->arg(0)[i].isDef()) {
            d->arg(0)[i].toDef()->unsubscribe(d);
          }
        }
        for (unsigned int i=0; i<d->arg(1).size(); i++) {
          if (d->arg(1)[i].isDef()) {
            d->arg(1)[i].toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
    };

    class Forall : public PrimitiveMap::Primitive {
    public:
      Forall(void) : PrimitiveMap::Primitive("forall",PrimitiveMap::FORALL,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<d->arg(0).size(); i++) {
          if (d->arg(0)[i].isDef()) {
            d->arg(0)[i].toDef()->subscribe(d, Definition::SES_VAL);
          } else {
            propImmediately = true;
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (unsigned int i=0; i<d->arg(0).size(); i++) {
          if (d->arg(0)[i].isDef()) {
            d->arg(0)[i].toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
    };

    class Exists : public PrimitiveMap::Primitive {
    public:
      Exists(void) : PrimitiveMap::Primitive("exists",PrimitiveMap::EXISTS,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (int j = 0; j < Val::follow_alias(d->arg(0), &i).size(); ++j) {
          Val arg = Val::follow_alias(d->arg(0)[j], &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (arg.toDef()->isFixed()) {
              propImmediately = true;
            }
          }
        }
        propImmediately = propImmediately || Val::follow_alias(d->arg(0), &i).size() <= 1;
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j = 0; j < Val::follow_alias(d->arg(0), &i).size(); ++j) {
          Val arg = Val::follow_alias(d->arg(0)[j], &i);
          if (arg.isDef()) {
            arg.toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        int size = Val::follow_alias(d->arg(0), &i).size();
        if (size == 0) {
          return PS_FAILED;
        } else if (size == 1) {
          Val arg = Val::follow_alias(d->arg(0)[0], &i);
          d->alias(&i, arg);
        }

        // TODO: More propagation
        return PS_OK;
      }
    };

    class IntSum : public PrimitiveMap::Primitive {
    public:
      IntSum(void) : PrimitiveMap::Primitive("int_sum",PrimitiveMap::INT_SUM,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        for (int j = 0; j < Val::follow_alias(d->arg(0), &i).size(); ++j) {
          Val arg = Val::follow_alias(d->arg(0)[j], &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (!arg.toDef()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j=0; j < d->arg(0).size(); j++) {
          if (d->arg(0)[j].isDef()) {
            d->arg(0)[j].toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        IntVal lb, ub;

        for (int j=0; j < Val::follow_alias(d->arg(0), &i).size(); j++) {
          Val v = Val::follow_alias(d->arg(0)[j], &i);
          if (v.isDef() && !v.toDef()->isBounded()) {
            return PS_OK;
          }
          lb += v.lb();
          ub += v.ub();
        }

        if (lb == ub) {
          return d->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return d->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };

    class IntPlus : public PrimitiveMap::Primitive {
    public:
      IntPlus(void) : PrimitiveMap::Primitive("int_plus",PrimitiveMap::INT_PLUS,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (!arg.toDef()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j=0; j < 2; j++) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        IntVal lb, ub;

        for (int j=0; j < 2; j++) {
          Val v = Val::follow_alias(d->arg(j), &i);
          if (v.isDef() && !v.toDef()->isBounded()) {
            return PS_OK;
          }
          lb += v.lb();
          ub += v.ub();
        }

        if (lb == ub) {
          return d->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return d->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };

    class IntMinus : public PrimitiveMap::Primitive {
    public:
      IntMinus(void) : PrimitiveMap::Primitive("int_minus",PrimitiveMap::INT_MINUS,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (!arg.toDef()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j=0; j < 2; j++) {
          Val arg = Val::follow_alias(d->arg(j));
          if (arg.isDef()) {
            arg.toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        IntVal lb, ub;

        lb = Val::follow_alias(d->arg(0), &i).lb();
        ub = Val::follow_alias(d->arg(0), &i).ub();

        lb -= Val::follow_alias(d->arg(1), &i).ub();
        ub -= Val::follow_alias(d->arg(1), &i).lb();

        if (lb == ub) {
          return d->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return d->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };

    class IntTimes : public PrimitiveMap::Primitive {
    public:
      IntTimes(void) : PrimitiveMap::Primitive("int_times",PrimitiveMap::INT_TIMES,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (!arg.toDef()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        Val a = Val::follow_alias(d->arg(0), &i);
        Val b = Val::follow_alias(d->arg(1), &i);
        if (b.isInt() && a.isDef()) {
          a = Val::follow_alias(d->arg(1), &i);
          b = Val::follow_alias(d->arg(0), &i);
        }

        IntVal lb, ub;
        if ((a.isDef() && !a.toDef()->isBounded()) || (b.isDef() && !b.toDef()->isBounded())) {
          return PS_OK;
        } else if (a.isInt() && a.lb() == IntVal(1)) {
          d->alias(&i, b);
          return PS_ENTAILED;
        }

        lb = a.lb();
        ub = a.ub();

        lb *= b.lb();
        ub *= b.ub();

        if (lb == ub) {
          return d->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return d->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };

    class LinExp : public PrimitiveMap::Primitive {
    public:
      LinExp(void) : PrimitiveMap::Primitive("lin_exp",PrimitiveMap::LINEXP,3) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<d->arg(1).size(); i++) {
          if (d->arg(1)[i].isDef()) {
            d->arg(1)[i].toDef()->subscribe(d, Definition::SES_ANY);
          } else {
            propImmediately = true;
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (unsigned int i=0; i<d->arg(1).size(); i++) {
          if (d->arg(1)[i].isDef()) {
            d->arg(1)[i].toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
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
        Val rnd(dis(generator));
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
        assert(args[0].isDef());

        auto it = i.solutions.find(args[0].timestamp());
        assert(it != i.solutions.end());
        Val sol(it->second);

        i.pushAgg(sol, -1);
      };
    };

    class SortBy : public PrimitiveMap::Primitive {
    public:
      SortBy() : PrimitiveMap::Primitive("sort_by",PrimitiveMap::SORT_BY, 2) {}
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size()==2);

        Val al = args[0];
        Val order_e = args[1];
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
        Vec* al_sorted = Vec::a(&i, i.newIdent(), sorted);

        i.pushAgg(Val(al_sorted), -1);
      };
    };

    class IntMax : public PrimitiveMap::Primitive {
    public:
      IntMax(void) : PrimitiveMap::Primitive("int_max",PrimitiveMap::INT_MAX_,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->subscribe(d, Definition::SES_ANY);
            if (!arg.toDef()->isBounded()) {
              propImmediately = false;
            }
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        for (int j = 0; j < 2; ++j) {
          Val arg = Val::follow_alias(d->arg(j), &i);
          if (arg.isDef()) {
            arg.toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        Val a = Val::follow_alias(d->arg(0), &i);
        Val b = Val::follow_alias(d->arg(1), &i);

        if ((a.isDef() && !a.toDef()->isBounded()) || (b.isDef() && !b.toDef()->isBounded())) {
          return PS_OK;
        } else if (a.ub() <= b.lb()) {
          d->alias(&i, b);
          return PS_ENTAILED;
        } else if (b.ub() <= a.lb()) {
          d->alias(&i, a);
          return PS_ENTAILED;
        }

        IntVal lb, ub;
        lb = std::max(a.lb(), b.lb());
        ub = std::max(a.ub(), b.ub());
        if (lb == ub) {
          return d->setVal(&i, lb) ? PS_ENTAILED : PS_FAILED;
        } else {
          return d->intersectDom(&i, {lb, ub}) ? PS_OK : PS_FAILED;
        }
        // TODO: Backwards Propagation
      }
    };
    
  }
}

#endif

