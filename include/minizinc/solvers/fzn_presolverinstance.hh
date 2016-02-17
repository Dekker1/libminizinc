/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip@dekker.li>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_FZN_PRESOLVER_INSTANCE_HH__
#define __MINIZINC_FZN_PRESOLVER_INSTANCE_HH__

#include <minizinc/solvers/fzn_solverinstance.hh>

namespace MiniZinc {

  class FZNPreSolverInstance : public FZNSolverInstance {
  protected:
    class Solns2Vector : public Solns2Out {
    public:
      std::vector<ASTStringMap<DE>::t> solutions;

      Solns2Vector(Env* e ) { this->initFromEnv(e); }

    protected:
      virtual bool evalOutput();
      virtual bool evalStatus(SolverInstance::Status status) {}
    };
    Solns2Vector solns2Vector;

    typedef pair<VarDecl*, KeepAlive> DE;
    ASTStringMap<DE>::t solution;

  public:
    FZNPreSolverInstance(Env& env, const Options& options);

    Status solve(void);

    Status next(void);

    ASTStringMap<std::pair<MiniZinc::VarDecl*, MiniZinc::KeepAlive>>::t& getSolution() {
      return solution;
    }
  };

}

#endif //__MINIZINC_FZN_PRESOLVER_INSTANCE_HH__
