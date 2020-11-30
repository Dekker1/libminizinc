/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_GEAS_SOLVER_INSTANCE_HH__
#define __MINIZINC_GEAS_SOLVER_INSTANCE_HH__

#include <minizinc/flattener.hh>
#include <minizinc/solver.hh>
#include <minizinc/solvers/incremental_interfaces.hh>

#include <geas/solver/solver.h>

namespace MiniZinc {

class GeasOptions : public SolverInstanceBase::Options {
public:
  bool all_solutions = false;
  int conflicts = 0;
  bool free_search = false;
  int nr_solutions = 1;
  int obj_probe_limit = 0;
  bool statistics = false;
  std::chrono::milliseconds time = std::chrono::milliseconds(0);
};

class GeasVariable {
public:
  enum Type { BOOL_TYPE, FLOAT_TYPE, INT_TYPE };

protected:
  Type _t;  // Type of the variable
  union {
    geas::patom_t _bv;
    geas::fp::fpvar _fv;
    geas::intvar _iv;
  };

public:
  explicit GeasVariable(const geas::patom_t& bv) : _t(BOOL_TYPE), _bv(bv){};
  explicit GeasVariable(const geas::fp::fpvar& fv) : _t(FLOAT_TYPE), _fv(fv){};
  explicit GeasVariable(const geas::intvar& iv) : _t(INT_TYPE), _iv(iv){};

  GeasVariable(const GeasVariable& gv) : _t(gv._t) {
    switch (_t) {
      case BOOL_TYPE:
        _bv = gv._bv;
        break;
      case FLOAT_TYPE:
        _fv = gv._fv;
        break;
      case INT_TYPE:
        _iv = gv._iv;
        break;
    }
  }

  bool isBool() const { return _t == BOOL_TYPE; }
  bool isFloat() const { return _t == FLOAT_TYPE; }
  bool isInt() const { return _t == INT_TYPE; }

  geas::patom_t boolVar() { return _bv; }
  geas::fp::fpvar floatVar() { return _fv; }
  geas::intvar intVar() { return _iv; }
};

class GeasTypes {
public:
  typedef GeasVariable Variable;
  typedef MiniZinc::Statistics Statistics;
};

class GeasSolverInstance : public SolverInstanceImpl<GeasTypes>, public TrailableSolverInstance {
public:
  GeasSolverInstance(std::ostream& log, SolverInstanceBase::Options* opt);
  ~GeasSolverInstance() override = default;
  void processFlatZinc() override;
  geas::solver_data* solver_data() { return _solver.data; }
  geas::solver& solver() { return _solver; }

  Status solve() override;
  Status next() override { return SolverInstance::ERROR; }  // TODO: Implement
  void resetSolver() override;

  Expression* getSolutionValue(Id* id) override;
  void printStatistics(bool fLegend) override;

  void addDefinition(const std::vector<BytecodeProc>& bs, Definition* def) override;
  Val getSolutionValue(Definition* def) override;

  // MiniZinc to Geas conversions
  static bool asBool(const Val& val) { return val().toInt() != 0; }
  vec<bool> asBoolVec(const Val& val);
  geas::patom_t asBoolVar(const Val& val);
  vec<geas::patom_t> asBoolVarVec(const Val& val);
  vec<int> asIntVec(const Val& val);
  static int asInt(const Val& val) { return val().toInt(); }
  geas::intvar asIntVar(const Val& val);
  vec<geas::intvar> asIntVarVec(const Val& val);

  void insertVar(const Definition* def, GeasVariable gv) {
    _variableMap.insert({def->timestamp(), gv});
  };

  // TODO: create only when necessary or use Geas internal
  geas::intvar zero;

  // Return to the solver into a position where no search decisions have been made. SolverInstance
  // must allow addDefinition calls after restart call.
  void restart() override { _solver.restart(); };

  // Returns the number of stored states
  size_t states() override { return stack.size(); }

  // Able to store the current solver state to the Trail.
  void pushState() override;

  // Able to restore the last solver state that was saved to the Trail.
  void popState() override;

  geas::patom_t currentState();

protected:
  geas::solver _solver;

  std::vector<geas::patom_t> stack;

  SolveI::SolveType _obj_type = SolveI::ST_SAT;
  std::unique_ptr<GeasTypes::Variable> _obj_var;

  GeasTypes::Variable& resolveVar(Definition* val);
  bool addSolutionNoGood();

  void registerConstraint(std::string name, poster p);
  void registerConstraints();
};

class Geas_SolverFactory : public SolverFactory {
public:
  Geas_SolverFactory();
  SolverInstanceBase::Options* createOptions() override;
  SolverInstanceBase* doCreateSI(std::ostream& log, SolverInstanceBase::Options* opt) override;

  std::string getDescription(SolverInstanceBase::Options* opt) override {
    return "Elsie Geas - Another Lazy Clause Generation Solver";
  };
  std::string getVersion(SolverInstanceBase::Options* opt) override { return "0.0.1"; }
  std::string getId() override { return "org.minizinc.geas"; }

  bool processOption(SolverInstanceBase::Options* opt, int& i,
                     std::vector<std::string>& argv) override;
  void printHelp(std::ostream& os) override;
};

}  // namespace MiniZinc

#endif  // __MINIZINC_GEAS_SOLVER_INSTANCE_HH__
