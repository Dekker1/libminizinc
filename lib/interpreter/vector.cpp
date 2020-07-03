/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/interpreter/vector.hh>
#include <minizinc/interpreter.hh>

namespace MiniZinc {

  Vec* Vec::allocate_array(Interpreter* interpreter, int timestamp, const std::vector<Val>& v) {
    Vec* values = a(interpreter, interpreter->newIdent(), v);
    Vec* idx = a(interpreter, interpreter->newIdent(), {1, v.size()});
    Vec* nv = a(interpreter, timestamp, {Val(values), Val(idx)});
    return nv;
  }

  bool
  Vec::isPar() const {
    assert(alive());
    for (int i = 0; i < this->size(); ++i) {
      Val v = this->operator[](i);
      if (v.isVec() && (!v.toVec()->isPar())) {
        return false;
      }
      if (v.isVar()) {
        return false;
      }
    }
    return true;
  }

}
