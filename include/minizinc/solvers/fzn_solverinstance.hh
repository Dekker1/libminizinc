/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_FZN_SOLVER_INSTANCE_HH__
#define __MINIZINC_FZN_SOLVER_INSTANCE_HH__

#include <minizinc/flattener.hh>
#include <minizinc/solver.hh>
#include <minizinc/solvers/incremental_interfaces.hh>
//#include <minizinc/solver_instance_base.hh>

namespace MiniZinc {

class FZNSolverOptions : public SolverInstanceBase::Options {
public:
  std::string fzn_solver;
  std::string backend;
  std::vector<std::string> fzn_flags;
  int numSols = 1;
  bool allSols = false;
  std::string parallel;
  int fzn_time_limit_ms = 0;
  int solver_time_limit_ms = 0;
  bool fzn_sigint = false;

  bool fzn_needs_paths = false;
  bool fzn_output_passthrough = false;

  bool supports_a = false;
  bool supports_n = false;
  bool supports_f = false;
  bool supports_p = false;
  bool supports_s = false;
  bool supports_r = false;
  bool supports_v = false;
  bool supports_t = false;
  std::vector<MZNFZNSolverFlag> fzn_solver_flags;
};

struct VarStore {
  KeepAlive int_var = nullptr;
  KeepAlive bool_var = nullptr;
  bool used_int = false;
  bool used_bool = false;
  VarStore(KeepAlive iv, KeepAlive bv, bool ui, bool ub)
      : int_var(iv), bool_var(bv), used_int(ui), used_bool(ub){};
};

class FZNSolverInstance : public SolverInstanceBase, public TrailableSolverInstance {
private:
  std::string _fzn_solver;
  std::unordered_map<int, VarStore> vdmap;
  std::unordered_set<int> uninitialised_vars;
  Model* _model;
  Env env;
  std::vector<unsigned int> stack;

public:
  FZNSolverInstance(std::ostream& log, SolverInstanceBase::Options* opt);

  ~FZNSolverInstance(void);

  Status next(void) override { return SolverInstance::ERROR; }

  Status solve(void) override;
  void printStatistics(bool fLegend = 0) override;
  void printFlatZincToFile(std::string filename);
  void printOutputToFile(std::string filename);
  void outputArray(Vec* arr);
  void outputDict(Variable* start);

  void processFlatZinc(void) override;

  void addFunction(FunctionI* fi) override;
  void addConstraint(const std::vector<BytecodeProc>& bs, Constraint* c) override;
  void addVariable(Variable* var, bool isOuput) override;
  Val getSolutionValue(Variable* var) override;

  // Able to return to the solver into a position where no search decisions
  // have been made. SolverInstance must allow addDefinition calls after
  // restart call.
  void restart() override{};

  // Returns the number of stored states
  size_t states() override { return stack.size(); }

  // Able to store the current solver state to the Trail.
  void pushState() override;

  // Able to restore the last solver state that was saved to the Trail.
  void popState() override;

  void resetSolver(void) override;

protected:
  void createFunctionItems();
  Expression* getSolutionValue(Id* id);
  Expression* val_to_expr(Type ty, Val v);
  VarDecl* add_var_to_model(int ident, TypeInst* ti, bool view = false, bool isOutput = false);
};

class FZN_SolverFactory : public SolverFactory {
protected:
  virtual SolverInstanceBase* doCreateSI(std::ostream& log, SolverInstanceBase::Options* opt);

public:
  FZN_SolverFactory(void);
  virtual SolverInstanceBase::Options* createOptions(void);
  virtual std::string getDescription(SolverInstanceBase::Options* opt = NULL);
  virtual std::string getVersion(SolverInstanceBase::Options* opt = NULL);
  virtual std::string getId(void);
  virtual bool processOption(SolverInstanceBase::Options* opt, int& i,
                             std::vector<std::string>& argv);
  virtual void printHelp(std::ostream& os);
  void setAcceptedFlags(SolverInstanceBase::Options* opt,
                        const std::vector<MZNFZNSolverFlag>& flags);
};

}  // namespace MiniZinc

#endif
