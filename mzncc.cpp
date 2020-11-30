/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
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

#include <minizinc/astexception.hh>
#include <minizinc/codegen.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/reader.hh>
#include <minizinc/timer.hh>
#include <minizinc/typecheck.hh>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ratio>

using namespace std;
using namespace MiniZinc;

int main(int argc, const char** argv) {
  Timer starttime;
  GCLock lock;
  try {
    MznReader r(std::cout, std::cerr);
    Model* m(nullptr);
    try {
      /*
      std::vector<std::string> args(argc-1);
      for (int i=1; i<argc; i++)
        args[i-1] = argv[i];
      fSuccess = (slv.run(args,"",argv[0]) != SolverInstance::ERROR);
      */
      std::vector<std::string> args(argc - 1);
      for (int i = 1; i < argc; i++) args[i - 1] = argv[i];
      switch (r.processOptions(args)) {
        case MznReader::OPTION_FINISH:
          return 0;
        case MznReader::OPTION_ERROR:
          return 1;
        default:
          break;
      }

      Model* m(r.read());

      Env env(m, std::cout, std::cerr);
      std::vector<TypeError> typeErrors;
      typecheck(env, m, typeErrors, true /* ignoreUndefinedParameters */,
                false /* allowMultiAssignment */, false /* isFlatZinc */);
      // debugprint(m);
      m = env.envi().model;

      CodeGen cg;
      CG::run(cg, m);
    } catch (const LocationException& e) {
      if (r.get_flag_verbose()) std::cerr << std::endl;
      std::cerr << e.loc() << ":" << std::endl;
      std::cerr << e.what() << ": " << e.msg() << std::endl;
    } catch (const Exception& e) {
      if (r.get_flag_verbose()) std::cerr << std::endl;
      std::string what = e.what();
      std::cerr << what << (what.empty() ? "" : ": ") << e.msg() << std::endl;
    } catch (const exception& e) {
      if (r.get_flag_verbose()) std::cerr << std::endl;
      std::cerr << e.what() << std::endl;
    } catch (...) {
      if (r.get_flag_verbose()) std::cerr << std::endl;
      std::cerr << "  UNKNOWN EXCEPTION." << std::endl;
    }

    if (r.get_flag_verbose()) {
      std::cerr << "   Done (";
      cerr << "overall time " << starttime.stoptime() << ")." << std::endl;
    }
  } catch (const Exception& e) {
    std::string what = e.what();
    std::cerr << what << (what.empty() ? "" : ": ") << e.msg() << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return 0;
}  // int main()
