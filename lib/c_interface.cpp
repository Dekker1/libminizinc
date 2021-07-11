/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/c_interface.h>
#include <minizinc/interpreter.hh>
#include <minizinc/interpreter/primitives.hh>
#include <minizinc/solver.hh>
#include <minizinc/solvers/gecode_solverinstance.hh>

#include <cstdarg>
#include <iostream>

using namespace MiniZinc;

class Instance {
public:
  Instance(std::string file, std::string data_file, std::string solver)
      : slv({"--solver", solver, file, data_file}){};

  MznSolver slv;
  std::string result;
};

void set_rnd_seed(int seed) {
  dynamic_cast<BytecodePrimitives::Uniform*>(primitiveMap()[PrimitiveMap::UNIFORM])->setSeed(seed);
}

MZNInstance minizinc_instance_init(const char* mza_file, const char* data_file,
                                   const char* solver) {
  auto inst = new Instance(mza_file, data_file, solver);
  return reinterpret_cast<MZNInstance>(inst);
}

void minizinc_instance_destroy(MZNInstance _inst) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  delete inst;
}

void minizinc_add_call(MZNInstance _inst, const char* call, ...) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  auto it = inst->slv.resolve_call.find(call);
  assert(it != inst->slv.resolve_call.end());

  std::vector<Val> args;
  va_list my_args;
  va_start(my_args, call);
  for (int i = 0; i < it->second.second; ++i) {
    int num = va_arg(my_args, int);
    args.emplace_back(num);
  }

  inst->slv.interpreter->call(it->second.first, std::move(args));
}

void minizinc_set_solution(MZNInstance _inst, int def, int sol) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  auto it = inst->slv.interpreter->solutions.emplace(def, sol);
}

void minizinc_output_dict(MZNInstance _inst, bool b) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  inst->slv.output_dict = b;
}

void minizinc_push_state(MZNInstance _inst) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  inst->slv.interpreter->trail.save_state(inst->slv.interpreter);
  inst->slv.pushToSolver();
}
void minizinc_pop_state(MZNInstance _inst) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  inst->slv.interpreter->trail.untrail(inst->slv.interpreter);
  inst->slv.popFromSolver();
}

void minizinc_print_hedge(MZNInstance _inst) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  inst->slv.interpreter->dumpState(std::cerr);
}

void minizinc_set_limit(MZNInstance _inst, int limit) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  auto opt = static_cast<GecodeOptions*>(inst->slv.getSI_OPT());
  opt->nodes = limit;
}

std::string status_to_string(SolverInstance::Status s) {
  switch (s) {
    case SolverInstance::OPT:
      return "OPT";
    case SolverInstance::SAT:
      return "SAT";
    case SolverInstance::UNSAT:
      return "UNSAT";
    case SolverInstance::UNBND:
      return "UNBND";
    case SolverInstance::UNSATorUNBND:
      return "UNSATorUNBND";
    case SolverInstance::UNKNOWN:
      return "UNKNOWN";
    case SolverInstance::ERROR:
      return "ERROR";
    case SolverInstance::NONE:
      return "NONE";
  }
}

const char* minizinc_solve(MZNInstance _inst) {
  auto inst = reinterpret_cast<Instance*>(_inst);
  auto result = inst->slv.run();

  std::stringstream ss;
  ss << "{";
  ss << "\"status\": \"" << status_to_string(result.first) << "\",";
  if (result.first == MiniZinc::SolverInstance::SAT ||
      result.first == MiniZinc::SolverInstance::OPT) {
    ss << "\"solution\": " << result.second;
  } else {
    ss << R"("solution": "")";
  }
  ss << "}";

  inst->result = ss.str();
  return inst->result.c_str();
}
