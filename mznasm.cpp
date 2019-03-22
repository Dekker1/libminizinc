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

#include <minizinc/bytecode.hh>
#include <minizinc/prettyprinter.hh>

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
  
  std::ifstream t(filename, std::ifstream::in);
  std::string str((std::istreambuf_iterator<char>(t)),
                  std::istreambuf_iterator<char>());
  try {
    // Parse assembly file
    auto bs = parse(str);
    if (verbose) {
      std::cerr << "Disassembled code:\n";
      for (auto& b : bs) {
        for (int i=0; i<BytecodeProc::MAX_MODE; i++) {
          if (b.mode[i].size()>0) {
            std::cerr << ":" << b.name << ":" << BytecodeProc::mode_to_string[i] << "\n";
            std::cerr << b.mode[i].toString(bs);
          }
        }
      }
      std::cerr << "\n";
    }
    // Built-in procedures
    std::vector<Interpreter::builtin> builtins;
    // The main procedure is the last one in the file
    BytecodeFrame frame(bs.back().mode[BytecodeProc::ROOT]);
    Interpreter interpreter(bs, builtins, frame);
    if (verbose) {
      std::cerr << "Run:\n";
    }
    interpreter.run();
    bool delayed;
    do {
      bool delayed = interpreter.runDelayed();
    } while (delayed);
    if (verbose) {
      std::cerr << "Done\n";
      interpreter.dumpState(std::cerr);
      std::cerr << "----------------" << std::endl;
      auto fzn = interpreter.toFZN();
      debugprint(fzn);
    }
  } catch (Error& e) {
    std::cerr << e.msg() << "\n";
  }
  return 0;
  
}
