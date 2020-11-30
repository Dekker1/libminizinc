/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <minizinc/solver_instance.hh>

#include <string>

#ifdef ERROR  // Microsoft.
#undef ERROR
#endif

namespace MiniZinc {

class RestartableSolverInstance {
public:
  // Able to return to the solver into a position where no search decisions
  // have been made. SolverInstance must allow addDefinition calls after
  // restart call.
  virtual void restart() = 0;
};

class TrailableSolverInstance : public RestartableSolverInstance {
public:
  // Returns the number of stored states
  virtual size_t states() = 0;

  // Able to store the current solver state to the Trail.
  virtual void pushState() = 0;

  // Able to restore the last solver state that was saved to the Trail.
  virtual void popState() = 0;
};

}  // namespace MiniZinc
