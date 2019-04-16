/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef __MINIZINC_INCREMENTAL_INTERFACES_HH__
#define __MINIZINC_INCREMENTAL_INTERFACES_HH__

#include <minizinc/model.hh>

namespace MiniZinc {
  class Trailable {
  public:
    // Returns the number of stored states
    virtual size_t states() = 0;

    // Able to store the current solver state to the Trail.
    virtual void pushState() = 0;

    // Able to restore the last solver state that was saved to the Trail.
    virtual void popState() = 0;

    // Able to return to the solver into a position where no search decisions have been made.
    virtual void restart() = 0;

    // Able to add new constraints to the current solver state
    // Incumbent: Must be able to handle ``set_in`` constraints
    virtual void addConstraint(Call* c) = 0;

    // Able to add new variables to the current solver state
    virtual void addVariable(VarDecl* vd) = 0;
  };

  class Changeable {
  public:
    // Able to replace the solvers working FlatZinc model with a new model
    virtual void changeModel(Model* newFZN) = 0;
  };

}


#endif // __MINIZINC_INCREMENTAL_INTERFACES_HH__
