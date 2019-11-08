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
      INT_SUM,
      INT_MINUS,
      INT_TIMES,
      LINEXP,
      UNIFORM,
      MAX_ID=UNIFORM
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
        assert(!d->is_bounded());
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

    class IntSum : public PrimitiveMap::Primitive {
    public:
      IntSum(void) : PrimitiveMap::Primitive("int_sum",PrimitiveMap::INT_SUM,1) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<d->arg(0).size(); i++) {
          if (d->arg(0)[i].isDef()) {
            d->arg(0)[i].toDef()->subscribe(d, Definition::SES_ANY);
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

    class IntMinus : public PrimitiveMap::Primitive {
    public:
      IntMinus(void) : PrimitiveMap::Primitive("int_minus",PrimitiveMap::INT_MINUS,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        for (unsigned int i=0; i<2; i++) {
          if (d->arg(i).isDef()) {
            d->arg(i).toDef()->subscribe(d, Definition::SES_ANY);
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
        for (unsigned int i=0; i<2; i++) {
          if (d->arg(i).isDef()) {
            d->arg(i).toDef()->unsubscribe(d);
          }
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
    };

    class IntTimes : public PrimitiveMap::Primitive {
    public:
      IntTimes(void) : PrimitiveMap::Primitive("int_times",PrimitiveMap::INT_TIMES,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = true;
        if (d->arg(0).isDef()) {
          d->arg(0).toDef()->subscribe(d, Definition::SES_ANY);
          if (!d->arg(0).toDef()->is_bounded()) {
            propImmediately = false;
          }
        }
        if (d->arg(1).isDef()) {
          d->arg(1).toDef()->subscribe(d, Definition::SES_ANY);
          if (!d->arg(1).toDef()->is_bounded()) {
            propImmediately = false;
          }
        }
        if (propImmediately) {
          return propagate(i,d);
        } else {
          return PS_OK;
        }
      }
      virtual void unsubscribe(Interpreter& i, Definition* d) const {
        if (d->arg(0).isDef()) {
          d->arg(0).toDef()->unsubscribe(d);
        }
      }
      virtual PropStatus propagate(Interpreter& i, Definition* d) const {
        Val a = d->arg(0);
        Val b = d->arg(1);
        IntVal bounds[2];

        if ((a.isDef() && a.toDef()->domain() == Val(IntVal(0))) || (b.isDef() && a.toDef()->domain() == Val(IntVal(0)))) {
          return PS_OK;
        }

        if (a.isInt()) {
          bounds[0] = a();
          bounds[1] = a();
        } else {
          bounds[0] = a.toDef()->min();
          bounds[1] = a.toDef()->max();
        }

        if (b.isInt()) {
          bounds[0] *= b();
          bounds[1] *= b();
        } else {
          bounds[0] *= b.toDef()->min();
          bounds[1] *= b.toDef()->max();
        }

        if (bounds[0] == bounds[1]) {
          // TODO: Check if value is in the current domain
          d->domain(&i, Val(bounds[0]), true); // TODO: Is the domain binding when propagating??
          return PS_ENTAILED;
        } else {
          // TODO: Intersect new bounds with the current bounds
          d->domain(&i, {bounds[0], bounds[1]}, true); // TODO: Is the domain binding when propagating??
          return PS_OK;
        }
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
        generator = std::mt19937(rnd());
      }
      virtual void execute(Interpreter& i, const std::vector<Val>& args) {
        assert(args.size() == 2);
        assert(args[0].isInt() && args[1].isInt());

        std::uniform_int_distribution<> dis(args[0]().toInt(), args[1]().toInt());
        Val rnd(dis(generator));
        i.pushAgg(rnd, -1);
      };
    private:
      std::mt19937 generator;
    };
    
  }
}

#endif

