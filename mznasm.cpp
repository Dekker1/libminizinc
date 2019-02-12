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

using namespace std;
using namespace MiniZinc;

int main(int argc, const char** argv) {

  Val v1(123);
  assert(v1.isInt());
  assert(v1()==123);
  Val v2(Ref(123));
  assert(v2.isRef());
  assert(v2.r()()==123);
  std::vector<Val> v(3);
  v[0] = v1;
  v[1] = v2;
  v[2] = v1;
  Val v3(Vec::a(v));
  assert(v3.isVec());
  assert(v3[0]()==v1());
  assert(v3[1].r()()==v2.r()());
  assert(v3[2]()==v1());
  
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
    auto bs = parse(str);
    if (verbose) {
      std::cerr << "Disassembled code:\n";
      for (auto& b : bs) {
        std::cerr << ":" << b.name() << ":\n";
        std::cerr << b.toString(bs);
      }
      std::cerr << "\n";
    }
    // The main procedure is the last one in the file
    BytecodeFrame frame(bs.back());
    Interpreter interpreter(bs, frame);
    if (verbose) {
      std::cerr << "Run:\n";
    }
    interpreter.run();
    if (verbose) {
      std::cerr << "Done\n";
      for (unsigned int i=0; i<interpreter.defStack().size(); i++) {
        std::cerr << i << ":\t";
        std::cerr << bs[interpreter.defStack()[i].call.pred].name() << " ";
        std::cerr << interpreter.defStack()[i].call.args.toString() << "\n";
      }
    }
  } catch (Error& e) {
    std::cerr << e.msg() << "\n";
  }
  return 0;
  
}
