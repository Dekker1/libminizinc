/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/bytecode_primitives.hh>

namespace MiniZinc {

  namespace BytecodePrimitives {
    Alias alias;
    BoolNot boolnot;
    MkIntVar mk_intvar;
    Clause clause;
    Forall forall;
    Exists exists;
    IntSum intsum;
    IntTimes inttimes;
    IntLinEq int_lin_eq;
    Uniform uniform;
    Sol sol;
    SortBy sortby;
    IntMax intmax;
    Infinity infinity;
    SliceXd slice_xd;

    PrimitiveMap::Primitive* AllPrimitives[] = {
      &alias, &mk_intvar, &boolnot, &clause, &forall, &exists, &intsum, &inttimes, &int_lin_eq, &uniform, &sol, &sortby, &intmax, &infinity, &slice_xd,
    };
  }
  
  PrimitiveMap::PrimitiveMap(void) : _p(PrimitiveMap::MAX_ID+1) {
    for (PrimitiveMap::Primitive* p : BytecodePrimitives::AllPrimitives) {
      _p[p->ident()] = p;
    }
    for (Primitive* p : _p) {
      _s.insert(std::make_pair(p->name(),p));
    }
    _n.resize(_s.size());
    for (auto& entry : _s) {
      _n[entry.second->ident()] = entry.first;
    }
  }
  
  PrimitiveMap& primitiveMap(void) {
    static PrimitiveMap _pm;
    return _pm;
  }

}
