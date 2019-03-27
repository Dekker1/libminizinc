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

namespace MiniZinc {
  
  class PrimitiveMap {
  public:
    enum Id {
      ALIAS,
      BOOLNOT,
      CLAUSE,
      FORALL,
      EXISTS,
      INT_SUM,
      INT_TIMES,
      LINEXP,
      MAX_ID=LINEXP
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
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const = 0;
      virtual void unsubscribe(Interpreter& i, Definition* d) const = 0;
      virtual PropStatus propagate(Interpreter& i, Definition* d) const = 0;
    };
  protected:
    std::vector<Primitive*> _p;
    std::unordered_map<std::string,Primitive*> _s;
    std::vector<std::string> _n;
  public:
    PrimitiveMap(void);
    Primitive* operator [](const std::string& s) { return _s[s]; }
    Primitive* operator [](int i) { assert(i >= 0 && i <= MAX_ID); assert(_p.size()==MAX_ID+1); return _p[i]; }
    
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
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { assert(false); }
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

    class IntTimes : public PrimitiveMap::Primitive {
    public:
      IntTimes(void) : PrimitiveMap::Primitive("int_times",PrimitiveMap::INT_TIMES,2) {}
      virtual PropStatus subscribe(Interpreter& i, Definition* d) const {
        bool propImmediately = false;
        if (d->arg(0).isDef()) {
          d->arg(0).toDef()->subscribe(d, Definition::SES_ANY);
        } else {
          propImmediately = true;
        }
        if (d->arg(1).isDef()) {
          d->arg(1).toDef()->subscribe(d, Definition::SES_ANY);
        } else {
          propImmediately = true;
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
      virtual PropStatus propagate(Interpreter& i, Definition* d) const { return PS_OK; }
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
    
  }
}

#endif

