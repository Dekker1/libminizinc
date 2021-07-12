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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ratio>

using namespace std;

#include <minizinc/flat_exp.hh>
#include <minizinc/solver.hh>
#include <minizinc/support/mza_parser.hh>

using namespace MiniZinc;

#ifdef HAS_GUROBI
#include <minizinc/solvers/MIP/MIP_gurobi_solverfactory.hh>
#endif
#ifdef HAS_CPLEX
#include <minizinc/solvers/MIP/MIP_cplex_solverfactory.hh>
#endif
#ifdef HAS_OSICBC
#include <minizinc/solvers/MIP/MIP_osicbc_solverfactory.hh>
#endif
#ifdef HAS_XPRESS
#include <minizinc/solvers/MIP/MIP_xpress_solverfactory.hh>
#endif
#ifdef HAS_GECODE
#include <minizinc/solvers/gecode_solverfactory.hh>
#endif
#ifdef HAS_GEAS
#include <minizinc/solvers/geas_solverfactory.hh>
#endif
#ifdef HAS_SCIP
#include <minizinc/solvers/MIP/MIP_scip_solverfactory.hh>
#endif
#include <minizinc/solvers/fzn_solverfactory.hh>
#include <minizinc/solvers/fzn_solverinstance.hh>
#include <minizinc/solvers/mzn_solverfactory.hh>
#include <minizinc/solvers/mzn_solverinstance.hh>
#include <minizinc/solvers/nl/nl_solverfactory.hh>
#include <minizinc/solvers/nl/nl_solverinstance.hh>

SolverInitialiser::SolverInitialiser(void) {
#ifdef HAS_GUROBI
  Gurobi_SolverFactoryInitialiser _gurobi_init;
#endif
#ifdef HAS_CPLEX
  static Cplex_SolverFactoryInitialiser _cplex_init;
#endif
#ifdef HAS_OSICBC
  static OSICBC_SolverFactoryInitialiser _osicbc_init;
#endif
#ifdef HAS_XPRESS
  static Xpress_SolverFactoryInitialiser _xpress_init;
#endif
#ifdef HAS_GECODE
  static Gecode_SolverFactoryInitialiser _gecode_init;
#endif
#ifdef HAS_GEAS
  static Geas_SolverFactoryInitialiser _geas_init;
#endif
#ifdef HAS_SCIP
  static SCIP_SolverFactoryInitialiser _scip_init;
#endif
  static FZN_SolverFactoryInitialiser _fzn_init;
  static MZN_SolverFactoryInitialiser _mzn_init;
  static NL_SolverFactoryInitialiser _nl_init;
}

MZNFZNSolverFlag MZNFZNSolverFlag::std(const std::string& n0) {
  const std::string argFlags("-I -n -p -r");
  if (argFlags.find(n0) != std::string::npos) return MZNFZNSolverFlag(FT_ARG, n0);
  return MZNFZNSolverFlag(FT_NOARG, n0);
}

MZNFZNSolverFlag MZNFZNSolverFlag::extra(const std::string& n0, const std::string& t0) {
  return MZNFZNSolverFlag(t0 == "bool" ? FT_NOARG : FT_ARG, n0);
}

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
SolverRegistry* MiniZinc::getGlobalSolverRegistry() {
  static SolverRegistry sr;
  return &sr;
}

void SolverRegistry::addSolverFactory(SolverFactory* pSF) {
  assert(pSF);
  sfstorage.push_back(pSF);
}

void SolverRegistry::removeSolverFactory(SolverFactory* pSF) {
  auto it = find(sfstorage.begin(), sfstorage.end(), pSF);
  assert(pSF);
  sfstorage.erase(it);
}

/// Function createSI also adds each SI to the local storage
SolverInstanceBase* SolverFactory::createSI(std::ostream& log, SolverInstanceBase::Options* opt) {
  SolverInstanceBase* pSI = doCreateSI(log, opt);
  if (!pSI) {
    throw InternalError("SolverFactory: failed to initialize solver " + getDescription());
  }
  sistorage.resize(sistorage.size() + 1);
  sistorage.back().reset(pSI);
  return pSI;
}

/// also providing a destroy function for a DLL or just special allocator etc.
void SolverFactory::destroySI(SolverInstanceBase* pSI) {
  auto it = sistorage.begin();
  for (; it != sistorage.end(); ++it)
    if (it->get() == pSI) break;
  if (sistorage.end() == it) {
    cerr << "  SolverFactory: failed to remove solver at " << pSI << endl;
    throw InternalError("  SolverFactory: failed to remove solver");
  }
  sistorage.erase(it);
}

MznSolver::MznSolver(std::vector<std::string> args0)
    : solver_configs(std::cerr),
      executable_name("minizinc"),
      os(std::cout),
      log(std::cerr),
      s2out(std::cout, std::cerr, solver_configs.mznlibDir()) {
  std::vector<std::string> args = {executable_name};
  args.insert(args.end(), args0.begin(), args0.end());
  switch (processOptions(args)) {
    case OPTION_FINISH:
      return;
    case OPTION_ERROR:
      printUsage();
      os << "More info with \"" << executable_name << " --help\"\n";
      return;
    case OPTION_OK:
      break;
  }

  flatten(file, file);
  //  Definition* head = interpreter->_agg[0].def_stack;
  //  def_ptr = head;
}

MznSolver::~MznSolver() {
  //   if (si)                         // first the solver
  //     CleanupSolverInterface(si);
  // TODO cleanup the used solver interfaces
  delete interpreter;
  si = 0;
  si_opt = nullptr;
  GC::trigger();
}

bool MznSolver::ifMzn2Fzn() { return is_mzn2fzn; }

bool MznSolver::ifSolns2out() { return s2out._opt.flag_standaloneSolns2Out; }

void MznSolver::addSolverInterface(SolverFactory* sf) {
  si = sf->createSI(log, si_opt);
  assert(si);
  Model* m = in_out_defs.model();
  if (m) {
    for (FunctionIterator it = m->begin_functions(); it != m->end_functions(); ++it) {
      if (!it->removed()) {
        FunctionI& fi = *it;
        if (fi.from_stdlib() || fi.ti()->type().isann() || fi.e()) {
          continue;
        }
        si->addFunction(&fi);
      }
    }
  }
  si->setSolns2Out(new Solns2Out(os, log, ""));
  // if (s2out.getEnv()==NULL)
  //   s2out.initFromEnv( flt.getEnv() );
  // si->setSolns2Out( &s2out );
  if (flag_compiler_verbose)
    log
        //     << "  ---------------------------------------------------------------------------\n"
        << "      % SOLVING PHASE\n"
        << sf->getDescription(si_opt) << endl;
}

void MznSolver::addSolverInterface() {
  GCLock lock;
  if (sf == NULL) {
    if (getGlobalSolverRegistry()->getSolverFactories().empty()) {
      log << " MznSolver: NO SOLVER FACTORIES LINKED." << endl;
      assert(0);
    }
    sf = getGlobalSolverRegistry()->getSolverFactories().back();
  }
  addSolverInterface(sf);
}

void MznSolver::printUsage() {
  os << executable_name << ": ";
  if (ifMzn2Fzn()) {
    os << "MiniZinc to FlatZinc converter.\n"
       << "Usage: " << executable_name
       << "  [<options>] [-I <include path>] <model>.mzn [<data>.dzn ...]" << std::endl;
  } else if (ifSolns2out()) {
    os << "Solutions to output translator.\n"
       << "Usage: " << executable_name << "  [<options>] <model>.ozn" << std::endl;
  } else {
    os << "MiniZinc driver.\n"
       << "Usage: " << executable_name
       << "  [<options>] [-I <include path>] <model>.mzn [<data>.dzn ...] or just <flat>.fzn"
       << std::endl;
  }
}

void MznSolver::printHelp(const std::string& selectedSolver) {
  printUsage();
  os << "General options:" << std::endl
     << "  --help, -h\n    Print this help message." << std::endl
     << "  --version\n    Print version information." << std::endl
     << "  --solvers\n    Print list of available solvers." << std::endl
     << "  --time-limit <ms>\n    Stop after <ms> milliseconds (includes compilation and solving)."
     << std::endl
     << "  --solver <solver id>, --solver <solver config file>.msc\n    Select solver to use."
     << std::endl
     << "  --help <solver id>\n    Print help for a particular solver." << std::endl
     << "  -v, -l, --verbose\n    Print progress/log statements. Note that some solvers may log to "
        "stdout."
     << std::endl
     << "  --verbose-compilation\n    Print progress/log statements for compilation." << std::endl
     << "  -s, --statistics\n    Print statistics." << std::endl
     << "  --compiler-statistics\n    Print statistics for compilation." << std::endl
     << "  -c, --compile\n    Compile only (do not run solver)." << std::endl
     << "  --config-dirs\n    Output configuration directories." << std::endl;

  if (selectedSolver.empty()) {
    // flt.printHelp(os);
    os << endl;
    if (!ifMzn2Fzn()) {
      s2out.printHelp(os);
      os << endl;
    }
    os << "Available solvers (get help using --help <solver id>):" << endl;
    std::vector<std::string> solvers = solver_configs.solvers();
    if (solvers.size() == 0) cout << "  none.\n";
    for (unsigned int i = 0; i < solvers.size(); i++) {
      cout << "  " << solvers[i] << endl;
    }
  } else {
    const SolverConfig& sc = solver_configs.config(selectedSolver);
    string solverId = sc.executable().empty() ? sc.id()
                                              : (sc.supportsMzn() ? string("org.minizinc.mzn-mzn")
                                                                  : string("org.minizinc.mzn-fzn"));
    bool found = false;
    for (auto it = getGlobalSolverRegistry()->getSolverFactories().rbegin();
         it != getGlobalSolverRegistry()->getSolverFactories().rend(); ++it) {
      if ((*it)->getId() == solverId) {
        os << endl;
        (*it)->printHelp(os);
        if (!sc.executable().empty() && !sc.extraFlags().empty()) {
          os << "Extra solver flags (use with ";
          os << (sc.supportsMzn() ? "--mzn-flags" : "--fzn-flags") << ")" << endl;
          for (const SolverConfig::ExtraFlag& ef : sc.extraFlags()) {
            os << "  " << ef.flag << endl << "    " << ef.description << endl;
          }
        }
        found = true;
      }
    }
    if (!found) {
      os << "No help found for solver " << selectedSolver << endl;
    }
  }
}

void addFlags(const std::string& sep, const std::vector<std::string>& in_args,
              std::vector<std::string>& out_args) {
  for (const std::string& arg : in_args) {
    out_args.push_back(sep);
    out_args.push_back(arg);
  }
}

MznSolver::OptionStatus MznSolver::processOptions(std::vector<std::string>& argv) {
  executable_name = argv[0];
  executable_name = executable_name.substr(executable_name.find_last_of("/\\") + 1);
  size_t lastdot = executable_name.find_last_of('.');
  if (lastdot != std::string::npos) {
    executable_name = executable_name.substr(0, lastdot);
  }
  string solver;
  bool mzn2fzn_exe = (executable_name == "mzn2fzn");
  if (mzn2fzn_exe) {
    is_mzn2fzn = true;
  } else if (executable_name == "solns2out") {
    s2out._opt.flag_standaloneSolns2Out = true;
    flag_is_solns2out = true;
  }
  bool compileSolutionChecker = false;
  int i = 1, j = 1;
  int argc = static_cast<int>(argv.size());
  if (argc < 2) return OPTION_ERROR;
  for (i = 1; i < argc; ++i) {
    if (argv[i] == "-h" || argv[i] == "--help") {
      if (argc > i + 1) {
        printHelp(argv[i + 1]);
      } else {
        printHelp();
      }
      return OPTION_FINISH;
    }
    if (argv[i] == "--version") {
      // flt.printVersion(cout);
      return OPTION_FINISH;
    }
    if (argv[i] == "--solvers") {
      cout << "MiniZinc driver.\nAvailable solver configurations:\n";
      std::vector<std::string> solvers = solver_configs.solvers();
      if (solvers.size() == 0) cout << "  none.\n";
      for (unsigned int i = 0; i < solvers.size(); i++) {
        cout << "  " << solvers[i] << endl;
      }
      cout << "Search path for solver configurations:\n";
      for (const string& p : solver_configs.solverConfigsPath()) {
        cout << "  " << p << endl;
      }
      return OPTION_FINISH;
    }
    if (argv[i] == "--solvers-json") {
      cout << solver_configs.solverConfigsJSON();
      return OPTION_FINISH;
    }
    if (argv[i] == "--config-dirs") {
      GCLock lock;
      cout << "{\n";
      cout << "  \"globalConfigFile\" : \""
           << Printer::escapeStringLit(FileUtils::global_config_file()) << "\",\n";
      cout << "  \"userConfigFile\" : \"" << Printer::escapeStringLit(FileUtils::user_config_file())
           << "\",\n";
      cout << "  \"userSolverConfigDir\" : \""
           << Printer::escapeStringLit(FileUtils::user_config_dir()) << "/solvers\",\n";
      cout << "  \"mznStdlibDir\" : \"" << Printer::escapeStringLit(solver_configs.mznlibDir())
           << "\"\n";
      cout << "}\n";
      return OPTION_FINISH;
    }
    if (argv[i] == "--time-limit") {
      ++i;
      if (i == argc) {
        log << "Argument required for --time-limit" << endl;
        return OPTION_ERROR;
      }
      flag_overall_time_limit = atoi(argv[i].c_str());
    } else if (argv[i] == "--solver") {
      ++i;
      if (i == argc) {
        log << "Argument required for --solver" << endl;
        return OPTION_ERROR;
      }
      if (solver.size() > 0 && solver != argv[i]) {
        log << "Only one --solver option allowed" << endl;
        return OPTION_ERROR;
      }
      solver = argv[i];
    } else if (argv[i] == "--output-dict") {
      output_dict = true;
    } else if (argv[i] == "-c" || argv[i] == "--compile") {
      is_mzn2fzn = true;
    } else if (argv[i] == "-v" || argv[i] == "--verbose" || argv[i] == "-l") {
      flag_verbose = true;
      flag_compiler_verbose = true;
    } else if (argv[i] == "--verbose-compilation") {
      flag_compiler_verbose = true;
    } else if (argv[i] == "-s" || argv[i] == "--statistics") {
      flag_statistics = true;
      flag_compiler_statistics = true;
    } else if (argv[i] == "--compiler-statistics") {
      flag_compiler_statistics = true;
    } else {
      if ((argv[i] == "--fzn-cmd" || argv[i] == "--flatzinc-cmd") && solver.empty()) {
        solver = "org.minizinc.mzn-fzn";
      }
      if (argv[i] == "--compile-solution-checker") {
        compileSolutionChecker = true;
      }
      if (argv[i] == "--ozn-file") {
        flag_is_solns2out = true;
      }
      argv[j++] = argv[i];
    }
  }
  argv.resize(j);
  argc = j;

  if ((mzn2fzn_exe || compileSolutionChecker) && solver.empty()) {
    solver = "org.minizinc.mzn-fzn";
  }

  //  if (flag_verbose) {
  //    argv.push_back("--verbose-solving");
  //    argc++;
  //  }
  if (flag_statistics) {
    argv.push_back("--solver-statistics");
    argc++;
  }

  // flt.set_flag_output_by_default(ifMzn2Fzn());

  bool isMznMzn = false;

  if (!flag_is_solns2out) {
    try {
      const SolverConfig& sc = solver_configs.config(solver);
      string solverId;
      if (sc.executable().empty()) {
        if (is_mzn2fzn) {
          solverId = "org.minizinc.mzn-fzn";
        } else {
          solverId = sc.id();
        }
      } else if (sc.supportsMzn()) {
        solverId = "org.minizinc.mzn-mzn";
      } else if (sc.supportsFzn()) {
        solverId = "org.minizinc.mzn-fzn";
      } else if (sc.supportsNL()) {
        solverId = "org.minizinc.mzn-nl";
      } else {
        log << "Selected solver does not support MiniZinc, FlatZinc or NL input." << endl;
        return OPTION_ERROR;
      }
      for (auto it = getGlobalSolverRegistry()->getSolverFactories().begin();
           it != getGlobalSolverRegistry()->getSolverFactories().end(); ++it) {
        if ((*it)->getId() ==
            solverId) {  /// TODO: also check version (currently assumes all ids are unique)
          sf = *it;
          if (si_opt) {
            delete si_opt;
          }
          si_opt = sf->createOptions();
          if (!sc.executable().empty() || solverId == "org.minizinc.mzn-fzn" ||
              solverId == "org.minizinc.mzn-nl") {
            std::vector<MZNFZNSolverFlag> acceptedFlags;
            for (auto& sf : sc.stdFlags()) acceptedFlags.push_back(MZNFZNSolverFlag::std(sf));
            for (auto& ef : sc.extraFlags())
              acceptedFlags.push_back(MZNFZNSolverFlag::extra(ef.flag, ef.flag_type));

            // Collect arguments required for underlying exe
            vector<string> fzn_mzn_flags;
            if (sc.needsStdlibDir()) {
              fzn_mzn_flags.push_back("--stdlib-dir");
              fzn_mzn_flags.push_back(FileUtils::share_directory());
            }
            if (sc.needsMznExecutable()) {
              fzn_mzn_flags.push_back("--minizinc-exe");
              fzn_mzn_flags.push_back(FileUtils::progpath() + "/" + executable_name);
            }

            if (sc.supportsMzn()) {
              isMznMzn = true;
              static_cast<MZN_SolverFactory*>(sf)->setAcceptedFlags(si_opt, acceptedFlags);
              std::vector<std::string> additionalArgs_s;
              additionalArgs_s.push_back("-m");
              if (sc.executable_resolved().size()) {
                additionalArgs_s.push_back(sc.executable_resolved());
              } else {
                additionalArgs_s.push_back(sc.executable());
              }

              if (!fzn_mzn_flags.empty()) {
                addFlags("--mzn-flag", fzn_mzn_flags, additionalArgs_s);
              }

              // This should maybe be moved to fill in fzn_mzn_flags when
              // --find-muses is implemented (these arguments will be passed
              // through to the subsolver of findMUS)
              if (!sc.mznlib().empty()) {
                if (sc.mznlib().substr(0, 2) == "-G") {
                  additionalArgs_s.push_back("--mzn-flag");
                  additionalArgs_s.push_back(sc.mznlib());
                } else {
                  additionalArgs_s.push_back("--mzn-flag");
                  additionalArgs_s.push_back("-I");
                  additionalArgs_s.push_back("--mzn-flag");
                  std::string _mznlib;
                  if (sc.mznlib_resolved().size()) {
                    _mznlib = sc.mznlib_resolved();
                  } else {
                    _mznlib = sc.mznlib();
                  }
                  additionalArgs_s.push_back(_mznlib);
                }
              }

              for (i = 0; i < additionalArgs_s.size(); ++i) {
                bool success = sf->processOption(si_opt, i, additionalArgs_s);
                if (!success) {
                  log << "Solver backend " << solverId << " does not recognise option "
                      << additionalArgs_s[i] << "." << endl;
                  return OPTION_ERROR;
                }
              }
            } else {
              // supports fzn or nl
              std::vector<std::string> additionalArgs;
              if (sc.supportsFzn()) {
                static_cast<FZN_SolverFactory*>(sf)->setAcceptedFlags(si_opt, acceptedFlags);
                additionalArgs.push_back("--fzn-cmd");
              } else {
                // supports nl
                additionalArgs.push_back("--nl-cmd");
              }
              if (sc.executable_resolved().size()) {
                additionalArgs.push_back(sc.executable_resolved());
              } else {
                additionalArgs.push_back(sc.executable());
              }
              if (!fzn_mzn_flags.empty()) {
                if (sc.supportsFzn()) {
                  addFlags("--fzn-flag", fzn_mzn_flags, additionalArgs);
                } else {
                  addFlags("--nl-flag", fzn_mzn_flags, additionalArgs);
                }
              }
              if (sc.needsPathsFile()) {
                // Instruct flattener to hold onto paths
                int i = 0;
                vector<string> args{"--keep-paths"};
                // flt.processOption(i, args);

                // Instruct FznSolverInstance to write a path file
                // and pass it to the executable with --paths arg
                additionalArgs.push_back("--fzn-needs-paths");
              }
              if (!sc.needsSolns2Out()) {
                additionalArgs.push_back("--fzn-output-passthrough");
              }
              int i = 0;
              for (i = 0; i < additionalArgs.size(); ++i) {
                bool success = sf->processOption(si_opt, i, additionalArgs);
                if (!success) {
                  log << "Solver backend " << solverId << " does not recognise option "
                      << additionalArgs[i] << "." << endl;
                  return OPTION_ERROR;
                }
              }
            }
          }
          if (!sc.mznlib().empty()) {
            if (sc.mznlib().substr(0, 2) == "-G") {
              std::vector<std::string> additionalArgs({sc.mznlib()});
              int i = 0;
              // if (!flt.processOption(i, additionalArgs)) {
              //   log << "Flattener does not recognise option " << sc.mznlib() << endl;
              //   return OPTION_ERROR;
              // }
            } else {
              std::vector<std::string> additionalArgs(2);
              additionalArgs[0] = "-I";
              if (sc.mznlib_resolved().size()) {
                additionalArgs[1] = sc.mznlib_resolved();
              } else {
                additionalArgs[1] = sc.mznlib();
              }
              int i = 0;
              // if (!flt.processOption(i, additionalArgs)) {
              //   log << "Flattener does not recognise option -I." << endl;
              //   return OPTION_ERROR;
              // }
            }
          }
          if (!sc.defaultFlags().empty()) {
            std::vector<std::string> addedArgs;
            addedArgs.push_back(argv[0]);  // excutable name
            for (auto& df : sc.defaultFlags()) {
              addedArgs.push_back(df);
            }
            for (int i = 1; i < argv.size(); i++) {
              addedArgs.push_back(argv[i]);
            }
            argv = addedArgs;
            argc = addedArgs.size();
          }
          break;
        }
      }

    } catch (ConfigException& e) {
      log << "Config exception: " << e.msg() << endl;
      return OPTION_ERROR;
    }

    if (sf == NULL) {
      log << "Solver " << solver << " not found." << endl;
      return OPTION_ERROR;
    }

    for (i = 1; i < argc; ++i) {
      if (!ifMzn2Fzn() ? s2out.processOption(i, argv) : false) {
        // } else if ((!isMznMzn || is_mzn2fzn) && flt.processOption(i, argv)) {
      } else if (sf != NULL && sf->processOption(si_opt, i, argv)) {
      } else {
        size_t last_dot = argv[i].find_last_of('.');
        if (last_dot != string::npos) {
          std::string extension = argv[i].substr(last_dot, string::npos);
          if (extension == ".uzn" || extension == ".mza") {
            assert(file == "");
            file = argv[i];
          } else if (extension == ".dzn") {
            data_files.push_back(argv[i]);
          }
        } else {
          std::string executable_name(argv[0]);
          executable_name = executable_name.substr(executable_name.find_last_of("/\\") + 1);
          log << executable_name << ": Unrecognized option or bad format `" << argv[i] << "'"
              << endl;
          return OPTION_ERROR;
        }
      }
    }
    return OPTION_OK;

  } else {
    for (i = 1; i < argc; ++i) {
      if (s2out.processOption(i, argv)) {
      } else {
        std::string executable_name(argv[0]);
        executable_name = executable_name.substr(executable_name.find_last_of("/\\") + 1);
        log << executable_name << ": Unrecognized option or bad format `" << argv[i] << "'" << endl;
        return OPTION_ERROR;
      }
    }
    return OPTION_OK;
  }
}

Val MznSolver::eval_val(EnvI& env, Expression* e) {
  if (e->type().dim() > 0) {
    ArrayLit* al = eval_array_lit(env, e);
  std:
    vector<Val> content(al->size());
    for (size_t i = 0; i < al->size(); ++i) {
      content[i] = eval_val(env, (*al)[i]);
    }
    Val ret = Val(Vec::a(interpreter, interpreter->newIdent(), content));

    if (al->dims() > 1 || al->min(0) != 1) {
      std::vector<Val> idxs(al->dims() * 2);
      for (size_t i = 0; i < al->dims(); ++i) {
        idxs[i * 2] = Val(al->min(i));
        idxs[i * 2 + 1] = Val(al->max(i));
      }
      Vec* ranges = Vec::a(interpreter, interpreter->newIdent(), idxs);
      ret = Val(Vec::a(interpreter, interpreter->newIdent(), {ret, Val(ranges)}, true));
    }

    return ret;
  }
  if (e->type().is_set()) {
    // TODO: Might not be int
    IntSetVal* sl = eval_intset(env, e);
    std::vector<Val> vranges(sl->size() * 2);
    for (size_t i = 0; i < sl->size(); ++i) {
      vranges[i * 2] = Val::fromIntVal(sl->min(i));
      vranges[i * 2 + 1] = Val::fromIntVal(sl->max(i));
    }
    return Val(Vec::a(interpreter, interpreter->newIdent(), vranges));
  }
  if (e->type().isbool()) {
    bool b(eval_bool(env, e));
    return Val(b);
  }
  if (e->type().isfloat()) {
    throw InternalError("Unsupported Type");
  }
  IntVal iv(eval_int(env, e));
  return Val::fromIntVal(iv);
}

void MznSolver::flatten(const std::string& filename, const std::string& modelName) {
  if (!FileUtils::file_exists(filename)) {
    std::cerr << "Error: cannot open assembly file '" << filename << "'." << std::endl;
    return;
  }
  bool verbose = flag_compiler_verbose;
  std::ifstream t(filename, std::ifstream::in);
  std::string line;
  std::string mzn_defs;
  while (std::getline(t, line)) {
    if (line == "@@@@@@@@@@") {
      break;
    }
    mzn_defs += line + "\n";
  }
  std::string assembly;
  while (std::getline(t, line)) {
    assembly += line + "\n";
  }
  if (assembly.empty()) {
    std::swap(mzn_defs, assembly);
  }
  // Parse assembly file
  int max_glob;
  std::tie(max_glob, bs) = parse_mza(assembly);
  if (verbose) {
    std::cerr << "Disassembled code:\n";
    int b_count = 0;
    for (auto& b : bs) {
      for (int i = 0; i < BytecodeProc::MAX_MODE; i++) {
        if (b.mode[i].size() > 0) {
          std::cerr << ":" << b.name << ":" << BytecodeProc::mode_to_string[i] << " %% " << b_count
                    << "\n";
          std::cerr << b.mode[i].toString(bs);
        }
      }
      b_count++;
    }
    std::cerr << "\n";
  }
  for (size_t i = 0; i < bs.size(); i++) {
    resolve_call.insert({bs[i].name, {i, bs[i].nargs}});
  }
  // The main procedure is the last one in the file
  BytecodeFrame frame(bs.back().mode[BytecodeProc::ROOT], bs.size() - 1, BytecodeProc::ROOT);
  interpreter = new Interpreter(bs, frame, max_glob);
  // Parse and add data
  if (!mzn_defs.empty()) {
    GCLock lock;
    std::vector<SyntaxError> syntaxErrors;
    Model* m = parse(in_out_defs, {}, data_files, mzn_defs, file,
                     {solver_configs.mznlibDir() + "/std"}, false, false, verbose, std::cerr);
    if (!m) {
      throw Error("Unable to parse MiniZinc Declarations");
    }
    assert(!in_out_defs.model());
    in_out_defs.model(m);
    long long int idn = 0;
    std::vector<TypeError> typeErrors;
    typecheck(in_out_defs, in_out_defs.model(), typeErrors, false, true, false);
    registerBuiltins(in_out_defs);
    if (typeErrors.size() > 0) {
      for (unsigned int i = 0; i < typeErrors.size(); i++) {
        if (flag_verbose) log << std::endl;
        log << typeErrors[i].loc() << ":" << std::endl;
        log << typeErrors[i].what() << ": " << typeErrors[i].msg() << std::endl;
      }
      throw Error("multiple type errors");
    }
    if (verbose) {
      std::cerr << "Input Data:\n";
    }
    for (VarDeclIterator it = in_out_defs.model()->begin_vardecls();
         it != in_out_defs.model()->end_vardecls(); ++it) {
      if (it->removed()) {
        continue;
      }
      GCLock lock;
      Env& env = in_out_defs;
      Model* m = in_out_defs.model();

      VarDecl* vd = it->e();
      if (vd->type().isann()) {
        continue;
      }
      assert(vd->e());
      Call* global_ann = vd->ann().getCall(constants().ann.global_register);
      if (!global_ann) {
        throw TypeError(env.envi(), vd->loc(), "Unkown global " + vd->id()->str().str());
      }
      IntVal global = eval_int(env.envi(), global_ann->arg(0));

      if (vd->type().dim() > 0) {
        ArrayLit* al = eval_array_lit(env.envi(), vd->e());
        checkIndexSets(env.envi(), vd, al);
      }
      Val v = eval_val(env.envi(), vd->e());

      interpreter->globals.assign(interpreter, global.toInt(), v);
      if (verbose) {
        std::cerr << " - R" << global << "(" << vd->id()->str() << ") = " << v.toString()
                  << std::endl;
      }
    }
  }
  // Start interpreter
  Timer tm01;
  if (verbose) {
    std::cerr << "Run:\n";
  }
  bool delayed = true;
  interpreter->run();
  while (interpreter->status() == Interpreter::ROGER && delayed) {
    delayed = interpreter->runDelayed();
  }
  flatten_time = tm01.s();
  // FIXME: Global registers should not be removed, merely hidden
  // interpreter->clear_globals();
  if (verbose) {
    std::cerr << "Status: " << Interpreter::status_to_string[interpreter->status()] << std::endl;
    if (interpreter->status() == Interpreter::ROGER) {
      interpreter->dumpState(std::cerr);
    }
    std::cerr << "----------------" << std::endl;
  }
  switch (interpreter->status()) {
    case Interpreter::ROGER:
      interpreter_status = SolverInstance::UNKNOWN;
      break;
    case Interpreter::ERROR:
    case Interpreter::ABORTED:
      interpreter_status = SolverInstance::ERROR;
      break;
    case Interpreter::INCONSISTENT:
      interpreter_status = SolverInstance::UNSAT;
      break;
  }
}

std::pair<SolverInstance::Status, std::string> MznSolver::solve() {
  SolverInstance::Status status = getSI()->solve();
  std::string sol = printSolution(status);
  if (si_opt->printStatistics) getSI()->printStatistics();
  /* if (flag_statistics) */
  /*   getSI()->getSolns2Out()->printStatistics(log); */
  return {status, sol};
}

void MznSolver::printStatistics() {  // from flattener too?   TODO
  if (si) getSI()->printStatistics();
}

std::string MznSolver::printSolution(SolverInstance::Status s) {
  std::stringstream ss;
  switch (s) {
    case SolverInstance::SAT:
    case SolverInstance::OPT: {
      if (output) {
        Val vec = output->arg(0);
        ss << (output_dict ? '{' : '[');
        for (int i = 0; i < vec.size(); ++i) {
          Val v = vec[i];
          if (i > 0) {
            ss << ", ";
          }
          if (v.isVar()) {
            if (output_dict) {
              ss << "\"" << v.timestamp() << "\""
                 << ": ";
            }
            v = Val::follow_alias(v);
            if (v.isVar()) {
              ss << si->getSolutionValue(v.toVar()).toString();
            } else {
              ss << v.toString();
            }
          } else {
            assert(!output_dict);
            ss << v.toString();
          }
        }
        ss << (output_dict ? '}' : ']') << std::endl;

        // Set output for sol() builtin
        interpreter->solutions.clear();
        for (int i = 0; i < vec.size(); ++i) {
          Val v = Val::follow_alias(vec[i]);
          if (v.isVar()) {
            interpreter->solutions.emplace(v.timestamp(), si->getSolutionValue(v.toVar()));
          }
        }
      } else {
        interpreter->solutions.clear();
        bool first = true;
        ss << "{" << std::endl;
        for (Variable* v = interpreter->root()->next(); v != interpreter->root(); v = v->next()) {
          Val va = Val::follow_alias(Val(v));
          if (va.isVar()) {
            int timestamp = va.timestamp();
            if (timestamp >= 0) {
              /// TODO: all timestamps >= 0 ?
              if (!first) {
                ss << "," << std::endl;
              }
              ss << "    \"" << timestamp << "\""
                 << ": ";
              Val sv = si->getSolutionValue(va.toVar());
              ss << sv.toString();
              first = false;
              // Set output for sol() builtin
              interpreter->solutions.emplace(timestamp, sv);
            }
          }
        }
        ss << std::endl << "}" << std::endl;
      }
    } break;
    case SolverInstance::UNSAT:
      ss << "=====UNSATISFIABLE=====" << std::endl;
      break;
    case SolverInstance::UNKNOWN:
      ss << "=====UNKNOWN=====" << std::endl;
      break;
    case SolverInstance::ERROR:
    default:
      ss << "=====ERROR=====" << std::endl;
      break;
  }
  return ss.str();
}

std::pair<SolverInstance::Status, std::string> MznSolver::run() {
  using namespace std::chrono;
  steady_clock::time_point startTime = steady_clock::now();

  if (!ifMzn2Fzn() && flag_overall_time_limit != 0) {
    steady_clock::time_point afterFlattening = steady_clock::now();
    milliseconds passed = duration_cast<milliseconds>(afterFlattening - startTime);
    milliseconds time_limit(flag_overall_time_limit);
    if (passed > time_limit) {
      s2out.evalStatus(getFltStatus());
      return {SolverInstance::UNKNOWN, ""};
    }
    int time_left = (time_limit - passed).count();
    std::vector<std::string> timeoutArgs(2);
    timeoutArgs[0] = "--solver-time-limit";
    std::ostringstream oss;
    oss << time_left;
    timeoutArgs[1] = oss.str();
    int i = 0;
    sf->processOption(si_opt, i, timeoutArgs);
  }

  if (flag_statistics) {
    os << "%%%mzn-stat: flatTime=" << flatten_time << endl;
  }

  if (getFltStatus() != SolverInstance::UNKNOWN) {
    if (ifMzn2Fzn()) {
      GCLock lock;
      std::ofstream os(file.substr(0, file.size() - 4) + std::string(".fzn"));
      Printer p(os, 0, true);
      auto c = new Call(Location().introduce(), constants().ids.bool_eq,
                        {constants().lit_true, constants().lit_false});
      auto ci = new ConstraintI(Location().introduce(), c);
      p.print(ci);
      p.print(SolveI::sat(Location().introduce()));
    }
    return {getFltStatus(), printSolution(getFltStatus())};
  }

  if (!si) {  // only then
    // GCLock lock;                  // better locally, to enable cleanup after ProcessFlt()
    addSolverInterface();
  }
  addDefinitions();
  if (ifMzn2Fzn()) {
    assert(dynamic_cast<FZNSolverInstance*>(si));
    // Print flatzinc to file
    static_cast<FZNSolverInstance*>(si)->printFlatZincToFile(file.substr(0, file.size() - 4) +
                                                             std::string(".fzn"));
    static_cast<FZNSolverInstance*>(si)->printOutputToFile(file.substr(0, file.size() - 4) +
                                                           std::string(".ozn"));

    if (flag_statistics) {
      si->printStatistics();
    }
    return {SolverInstance::UNKNOWN, ""};
  }
  return solve();
}

void MznSolver::addDefinitions() {
  /// TODO: currently this will always add all variables and constraints
  Variable* v_start;
  size_t c_start;
  std::tie(v_start, c_start) = def_stack.back();
  std::set<int> output_vars;
  Variable* root = interpreter->root();

  auto fzn = dynamic_cast<FZNSolverInstance*>(si);
  if (v_start == nullptr) {
    v_start = root;
    for (Constraint* c : v_start->definitions()) {
      if (interpreter->_procs[c->pred()].name == "output_this") {
        assert(c->size() == 1);
        output = c;
        Val arg = c->arg(0);
        assert(arg.isVec());
        for (int i = 0; i < arg.size(); ++i) {
          Val real = Val::follow_alias(Val(arg[i]));
          if (real.isVar()) {
            output_vars.insert(real.toVar()->timestamp());
          }
        }
        break;
      }
    }
  }

  // Add all new variables
  for (Variable* v = v_start->next(); v != interpreter->root(); v = v->next()) {
    // Only add variables that are not aliased
    if (Val(v) == Val::follow_alias(Val(v))) {
      si->addVariable(v,
                      output != nullptr || output_vars.find(v->timestamp()) != output_vars.end());
    }
  }
  // Add defining constraints
  for (Variable* v = v_start->next(); v != interpreter->root(); v = v->next()) {
    for (Constraint* c : v->definitions()) {
      si->addConstraint(interpreter->_procs, c);
    }
  }
  // Add new root level constraints
  for (size_t i = c_start; i < root->definitions().size(); ++i) {
    Constraint* c = root->definitions()[i];
    if (interpreter->_procs[c->pred()].name == "output_this") {
      if (fzn) {
        assert(c->size() == 1);
        fzn->outputArray(c->arg(0).toVec());
      }
      continue;
    } else {
      si->addConstraint(interpreter->_procs, c);
    }
  }
  // Force output statement
  if (output == nullptr && fzn != nullptr) {
    fzn->outputDict(interpreter->root());
  }
  // TODO: Domain Changes
  def_stack[def_stack.size() - 1] = {root->prev(), root->definitions().size()};
}

void MznSolver::pushToSolver() {
  assert(interpreter->trail.len() > 0);

  if (auto tsi = dynamic_cast<TrailableSolverInstance*>(si)) {
    assert(interpreter->trail.len() == tsi->states() + 1);
    tsi->restart();
    addDefinitions();
    def_stack.push_back(def_stack.back());
    tsi->pushState();
  } else {
    assert(false);
  }
}

void MznSolver::popFromSolver() {
  if (auto tsi = dynamic_cast<TrailableSolverInstance*>(si)) {
    assert(interpreter->trail.len() == tsi->states() - 1);
    tsi->restart();
    tsi->popState();
    def_stack.pop_back();
  } else {
    assert(false);
    delete si;
    si = nullptr;
    // TODO: do we need to reconstruct the model here or can we trust there is a push before
    // solving
  }
}
