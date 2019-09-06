/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Gleb Belov <gleb.belov@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This (main) file coordinates flattening and solving.
 * The corresponding modules are flexibly plugged in
 * as derived classes, prospectively from DLLs.
 * A flattening module should provide MinZinc::GetFlattener()
 * A solving module should provide an object of a class derived from SolverFactory.
 * Need to get more flexible for multi-pass & multi-solving stuff  TODO
 */

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <ratio>

#include <minizinc/solver.hh>

using namespace std;
using namespace MiniZinc;

int main(int argc, const char** argv) {
  
  if (argc < 2) {
    std::cerr << "Usage: mznasm [-v] <ASMFILE>\n";
    return 1;
  }
  
  bool verbose = false;
  std::string filename = argv[1];
  if (filename=="-v") {
    verbose = true;
    if (argc < 3) {
      std::cerr << "Usage: mznasm [-v] <ASMFILE>\n";
      return 1;
    }
    filename = argv[2];
  }

  try {
    MznSolver slv(std::cout,std::cerr);
    std::vector<std::string> args = {"--solver", "org.minizinc.gecode_presolver"};
    if (verbose) {
      args.push_back("--verbose-compilation");
    }
    bool fSuccess = (slv.run(args, filename) != SolverInstance::ERROR);
    while (fSuccess) {
      //Do incremental things
      // interpreter.trail.save_state(&interpreter);
      // interpreter.call(24, BytecodeProc::ROOT, {});
      // slv.pushToSolver(interpreter);
      // slv.solve();
      // interpreter.trail.untrail(&interpreter);
      // slv.popFromSolver(interpreter);
      // slv.solve();

      fSuccess = false;
    }
  } catch (Error& e) {
    std::cerr << e.msg() << "\n";
  }
  return 0;
  
}
