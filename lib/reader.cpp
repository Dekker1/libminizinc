
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

#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/parser.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/reader.hh>
#include <minizinc/utils.hh>

namespace MiniZinc {

MznReader::MznReader(std::ostream& os0, std::ostream& log0)
    : solver_configs(log0), /*flt(os0,log0,solver_configs.mznlibDir()), */ os(os0), log(log0) {}

MznReader::~MznReader() { GC::trigger(); }

void MznReader::printUsage() {
  os << "TODO: print usage" << std::endl;
  /*
  os << executable_name << ": ";
  if ( ifMzn2Fzn() ) {
    os
      << "MiniZinc to FlatZinc converter.\n"
      << "Usage: "  << executable_name
      << "  [<options>] [-I <include path>] <model>.mzn [<data>.dzn ...]" << std::endl;
  } else if (ifSolns2out()) {
    os
      << "Solutions to output translator.\n"
      << "Usage: "  << executable_name
      << "  [<options>] <model>.ozn" << std::endl;
  } else {
    os
      << "MiniZinc driver.\n"
      << "Usage: "  << executable_name
      << "  [<options>] [-I <include path>] <model>.mzn [<data>.dzn ...] or just <flat>.fzn" <<
  std::endl;
  }
  */
}

void MznReader::printHelp(const std::string& selectedSolver) {
  printUsage();
  os << "TODO: print help" << std::endl;
  /*
  os
    << "General options:" << std::endl
    << "  --help, -h\n    Print this help message." << std::endl
    << "  --version\n    Print version information." << std::endl
    << "  --solvers\n    Print list of available solvers." << std::endl
    << "  --time-limit <ms>\n    Stop after <ms> milliseconds (includes compilation and solving)."
  << std::endl
    << "  --solver <solver id>, --solver <solver config file>.msc\n    Select solver to use." <<
  std::endl
    << "  --help <solver id>\n    Print help for a particular solver." << std::endl
    << "  -v, -l, --verbose\n    Print progress/log statements. Note that some solvers may log to
  stdout." << std::endl
    << "  --verbose-compilation\n    Print progress/log statements for compilation." << std::endl
    << "  -s, --statistics\n    Print statistics." << std::endl
    << "  --compiler-statistics\n    Print statistics for compilation." << std::endl
    << "  -c, --compile\n    Compile only (do not run solver)." << std::endl
    << "  --config-dirs\n    Output configuration directories." << std::endl;

  if (selectedSolver.empty()) {
    flt.printHelp(os);
    os << endl;
    if ( !ifMzn2Fzn() ) {
      s2out.printHelp(os);
      os << endl;
    }
    os << "Available solvers (get help using --help <solver id>):" << endl;
    std::vector<std::string> solvers = solver_configs.solvers();
    if (solvers.size()==0)
      cout << "  none.\n";
    for (unsigned int i=0; i<solvers.size(); i++) {
      cout << "  " << solvers[i] << endl;
    }
  } else {
    const SolverConfig& sc = solver_configs.config(selectedSolver);
    string solverId = sc.executable().empty() ? sc.id() : (sc.supportsMzn() ?
  string("org.minizinc.mzn-mzn") : string("org.minizinc.mzn-fzn")); bool found = false; for (auto it
  = getGlobalSolverRegistry()->getSolverFactories().rbegin(); it !=
  getGlobalSolverRegistry()->getSolverFactories().rend(); ++it) { if ((*it)->getId()==solverId) { os
  << endl;
        (*it)->printHelp(os);
        if (!sc.executable().empty() && !sc.extraFlags().empty()) {
          os << "Extra solver flags (use with ";
          os << (sc.supportsMzn() ? "--mzn-flags" : "--fzn-flags") << ")" << endl;
          for (const SolverConfig::ExtraFlag& ef: sc.extraFlags()) {
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
  */
}

bool MznReader::processOption(int& i, std::vector<std::string>& argv) {
  CLOParser cop(i, argv);
  string buffer;

  if (cop.getOption("-I --search-dir", &buffer)) {
    includePaths.push_back(buffer + string("/"));
  } else if (cop.getOption("--ignore-stdlib")) {
    flag_ignoreStdlib = true;
    /*
    } else if ( cop.getOption( "--instance-check-only") ) {
      flag_instance_check_only = true;
    } else if ( cop.getOption( "-e --model-check-only") ) {
      flag_model_check_only = true;
    } else if ( cop.getOption( "--model-interface-only") ) {
      flag_model_interface_only = true;
    } else if ( cop.getOption( "--model-types-only") ) {
      flag_model_types_only = true;
      */
  } else if (cop.getOption("-v --verbose")) {
    flag_verbose = true;
    /*
  } else if ( cop.getOption( "--no-optimize --no-optimise") ) {
    flag_optimize = false;
  } else if ( cop.getOption( "--no-output-ozn -O-") ) {
    flag_no_output_ozn = true;
  } else if ( cop.getOption( "--output-base", &flag_output_base ) ) {
  } else if ( cop.getOption(
    fOutputByDefault ?
      "-o --fzn --output-to-file --output-fzn-to-file"
      : "--fzn --output-fzn-to-file", &flag_output_fzn) ) {
  } else if ( cop.getOption( "--output-paths-to-file", &flag_output_paths) ) {
    fopts.collect_mzn_paths = true;
  } else if ( cop.getOption( "--output-paths") ) {
    fopts.collect_mzn_paths = true;
    */
    /*
  } else if ( cop.getOption( "--output-to-stdout --output-fzn-to-stdout" ) ) {
    flag_output_fzn_stdout = true;
  } else if ( cop.getOption( "--output-ozn-to-stdout" ) ) {
    flag_output_ozn_stdout = true;
  } else if ( cop.getOption( "--output-paths-to-stdout" ) ) {
    fopts.collect_mzn_paths = true;
    flag_output_paths_stdout = true;
  } else if ( cop.getOption( "--output-mode", &buffer ) ) {
    if (buffer == "dzn") {
      flag_output_mode = FlatteningOptions::OUTPUT_DZN;
    } else if (buffer == "json") {
      flag_output_mode = FlatteningOptions::OUTPUT_JSON;
    } else if (buffer == "item") {
      flag_output_mode = FlatteningOptions::OUTPUT_ITEM;
    } else {
      return false;
    }
  } else if ( cop.getOption( "--output-objective" ) ) {
    flag_output_objective = true;
  } else if ( cop.getOption( "- --input-from-stdin" ) ) {
      flag_stdinInput = true;
    */
  } else if (cop.getOption("-d --data", &buffer)) {
    if (buffer.length() <= 4 || buffer.substr(buffer.length() - 4, string::npos) != ".dzn")
      return false;
    datafiles.push_back(buffer);
  } else if (cop.getOption("--stdlib-dir", &std_lib_dir)) {
  } else if (cop.getOption("-G --globals-dir --mzn-globals-dir", &globals_dir)) {
  } else if (cop.getOption("-D --cmdline-data", &buffer)) {
    datafiles.push_back("cmd:/" + buffer);
  } else if (cop.getOption("--allow-unbounded-vars")) {
    flag_allow_unbounded_vars = true;
  } else if (cop.getOption("--only-range-domains")) {
    flag_only_range_domains = true;
    /*
  } else if ( cop.getOption( "--no-MIPdomains" ) ) {   // internal
    flag_noMIPdomains = true;
  } else if ( cop.getOption( "--MIPDMaxIntvEE", &opt_MIPDmaxIntvEE ) ) {
  } else if ( cop.getOption( "--MIPDMaxDensEE", &opt_MIPDmaxDensEE ) ) {
    */
  } else if (cop.getOption("-Werror")) {
    flag_werror = true;
    /*
  } else if (string(argv[i])=="--use-gecode") {
#ifdef HAS_GECODE
    flag_two_pass = true;
    flag_gecode = true;
#else
    log << "warning: Gecode not available. Ignoring '--use-gecode'\n";
#endif
  } else if (string(argv[i])=="--sac") {
#ifdef HAS_GECODE
    flag_two_pass = true;
    flag_gecode = true;
    flag_sac = true;
#else
    log << "warning: Gecode not available. Ignoring '--sac'\n";
#endif

  } else if (string(argv[i])=="--shave") {
#ifdef HAS_GECODE
    flag_two_pass = true;
    flag_gecode = true;
    flag_shave = true;
#else
    log << "warning: Gecode not available. Ignoring '--shave'\n";
#endif
  } else if (string(argv[i])=="--two-pass") {
    flag_two_pass = true;
  } else if (string(argv[i])=="--npass") {
    i++;
    if (i==argv.size()) return false;
    log << "warning: --npass option is deprecated --two-pass\n";
    int passes = atoi(argv[i].c_str());
    if(passes == 1) flag_two_pass = false;
    else if(passes == 2) flag_two_pass = true;
  } else if (string(argv[i])=="--pre-passes") {
    i++;
    if (i==argv.size()) return false;
    int passes = atoi(argv[i].c_str());
    if(passes >= 0) {
      flag_pre_passes = static_cast<unsigned int>(passes);
    }
  } else if (string(argv[i])=="-O0") {
    flag_optimize = false;
  } else if (string(argv[i])=="-O1") {
    // Default settings
  } else if (string(argv[i])=="-O2") {
    flag_two_pass = true;
#ifdef HAS_GECODE
  } else if (string(argv[i])=="-O3") {
    flag_two_pass = true;
    flag_gecode = true;
  } else if (string(argv[i])=="-O4") {
    flag_two_pass = true;
    flag_gecode = true;
    flag_shave = true;
  } else if (string(argv[i])=="-O5") {
    flag_two_pass = true;
    flag_gecode = true;
    flag_sac = true;
#else
  } else if (string(argv[i])=="-O3" || string(argv[i])=="-O4" || string(argv[i])=="-O5") {
    log << "% Warning: This compiler does not have Gecode builtin, cannot process -O3,-O4,-O5.\n";
    return false;
#endif
    */
    // ozn options must be after the -O<n> optimisation options
    /*
  } else if ( cop.getOption( "-O --ozn --output-ozn-to-file", &flag_output_ozn) ) {
  } else if (string(argv[i])=="--keep-paths") {
    flag_keep_mzn_paths = true;
    fopts.collect_mzn_paths = true;
  } else if (string(argv[i])=="--only-toplevel-presolve") {
    fopts.only_toplevel_paths = true;
  } else if ( cop.getOption( "--allow-multiple-assignments" ) ) {
    flag_allow_multi_assign = true;
  } else if (string(argv[i])=="--input-is-flatzinc") {
    is_flatzinc = true;
  } else if ( cop.getOption( "--compile-solution-checker", &buffer) ) {
    if (buffer.length()>=8 && buffer.substr(buffer.length()-8,string::npos) == ".mzc.mzn") {
      flag_compile_solution_check_model = true;
      flag_model_check_only = true;
      filenames.push_back(buffer);
    } else {
      log << "Error: solution checker model must have extension .mzc.mzn" << std::endl;
      return false;
    }
    */
  } else {
    std::string input_file(argv[i]);
    if (input_file.length() <= 4) {
      return false;
    }
    size_t last_dot = input_file.find_last_of('.');
    if (last_dot == string::npos) {
      return false;
    }
    std::string extension = input_file.substr(last_dot, string::npos);
    /*
    if ( extension == ".mzc" || (input_file.length()>=8 &&
    input_file.substr(input_file.length()-8,string::npos) == ".mzc.mzn") ) {
      flag_solution_check_model = input_file;
    } else */
    if (extension == ".mzn" || extension == ".fzn") {
      /*
      if ( extension == ".fzn" ) {
        is_flatzinc = true;
        if ( fOutputByDefault )        // mzn2fzn mode
          return false;
      }
      */
      filenames.push_back(input_file);
    } else if (extension == ".dzn" || extension == ".json") {
      datafiles.push_back(input_file);
    } else {
      // if ( fOutputByDefault )
      log << "Error: cannot handle file extension " << extension << "." << std::endl;
      return false;
    }
  }
  return true;
}

MznReader::OptionStatus MznReader::processOptions(std::vector<std::string>& argv) {
  std_lib_dir = solver_configs.mznlibDir();

  int i(0);
  int argc(argv.size());
  for (; i < argv.size(); ++i) {
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
      cout << "MiniZinc model reader. Version EXTREMELY-ALPHA." << endl;
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
    /*
    if (argv[i]=="--solvers-json") {
      cout << solver_configs.solverConfigsJSON();
      return OPTION_FINISH;
    }
    */
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
    if (!processOption(i, argv)) return OPTION_ERROR;
  }
  return OPTION_OK;
}

Model* MznReader::read(const std::string& modelString, const std::string& modelName) {
  Env env(NULL, os, log);

  if (std_lib_dir == "") {
    throw Error(
        "Error: unknown minizinc standard library directory.\n"
        "Specify --stdlib-dir on the command line or set the\n"
        "MZN_STDLIB_DIR environment variable.");
  }
  if (globals_dir != "") {
    includePaths.insert(includePaths.begin(), std_lib_dir + "/" + globals_dir + "/");
  }
  includePaths.push_back(std_lib_dir + "/std/");

  for (unsigned int i = 0; i < includePaths.size(); i++) {
    if (!FileUtils::directory_exists(includePaths[i])) {
      throw Error("Cannot access include directory " + includePaths[i]);
    }
  }

  Model* m(nullptr);

  m = parse(env, filenames, datafiles, modelString, modelName.empty() ? "stdin" : modelName,
            includePaths, flag_ignoreStdlib, false, flag_verbose, log);
  return m;
}

#if 0
void addFlags(const std::string& sep,
              const std::vector<std::string>& in_args,
              std::vector<std::string>& out_args) {
  for(const std::string& arg : in_args) {
    out_args.push_back(sep);
    out_args.push_back(arg);
  }
}

MznReader::OptionStatus MznReader::processOptions(std::vector<std::string>& argv)
{
  /*
  executable_name = argv[0];
  executable_name = executable_name.substr(executable_name.find_last_of("/\\") + 1);
  size_t lastdot = executable_name.find_last_of('.');
  if (lastdot != std::string::npos) {
    executable_name = executable_name.substr(0, lastdot);
  }
  */
  string solver;
  /*
  bool mzn2fzn_exe = (executable_name=="mzn2fzn");
  if (mzn2fzn_exe) {
    is_mzn2fzn=true;
  } else if (executable_name=="solns2out") {
    s2out._opt.flag_standaloneSolns2Out=true;
  }
  bool compileSolutionChecker = false;
  */
  int i=1, j=1;
  int argc = static_cast<int>(argv.size());
  if (argc < 2)
    return OPTION_ERROR;
  for (i=1; i<argc; ++i) {
    if (argv[i]=="-h" || argv[i]=="--help") {
      if (argc > i+1) {
        printHelp(argv[i+1]);
      } else {
        printHelp();
      }
      return OPTION_FINISH;
    }
    if (argv[i]=="--version") {
      flt.printVersion(cout);
      return OPTION_FINISH;
    }
    if (argv[i]=="--solvers") {
      cout << "MiniZinc driver.\nAvailable solver configurations:\n";
      std::vector<std::string> solvers = solver_configs.solvers();
      if (solvers.size()==0)
        cout << "  none.\n";
      for (unsigned int i=0; i<solvers.size(); i++) {
        cout << "  " << solvers[i] << endl;
      }
      cout << "Search path for solver configurations:\n";
      for (const string& p : solver_configs.solverConfigsPath()) {
        cout << "  " << p << endl;
      }
      return OPTION_FINISH;
    }
    if (argv[i]=="--solvers-json") {
      cout << solver_configs.solverConfigsJSON();
      return OPTION_FINISH;
    }
    if (argv[i]=="--config-dirs") {
      GCLock lock;
      cout << "{\n";
      cout << "  \"globalConfigFile\" : \"" << Printer::escapeStringLit(FileUtils::global_config_file()) << "\",\n";
      cout << "  \"userConfigFile\" : \"" << Printer::escapeStringLit(FileUtils::user_config_file()) << "\",\n";
      cout << "  \"userSolverConfigDir\" : \"" << Printer::escapeStringLit(FileUtils::user_config_dir()) << "/solvers\",\n";
      cout << "  \"mznStdlibDir\" : \"" << Printer::escapeStringLit(solver_configs.mznlibDir()) << "\"\n";
      cout << "}\n";
      return OPTION_FINISH;
    }
    if (argv[i]=="--time-limit") {
      ++i;
      if (i==argc) {
        log << "Argument required for --time-limit" << endl;
        return OPTION_ERROR;
      }
      flag_overall_time_limit = atoi(argv[i].c_str());
    } else if (argv[i]=="--solver") {
      ++i;
      if (i==argc) {
        log << "Argument required for --solver" << endl;
        return OPTION_ERROR;
      }
      if (solver.size()>0 && solver != argv[i]) {
        log << "Only one --solver option allowed" << endl;
        return OPTION_ERROR;
      }
      solver = argv[i];
    } else if (argv[i]=="-c" || argv[i]=="--compile") {
      is_mzn2fzn = true;
    } else if (argv[i]=="-v" || argv[i]=="--verbose" || argv[i]=="-l") {
      flag_verbose = true;
      flag_compiler_verbose = true;
    } else if (argv[i]=="--verbose-compilation") {
      flag_compiler_verbose = true;
    } else if (argv[i]=="-s" || argv[i]=="--statistics") {
      flag_statistics = true;
      flag_compiler_statistics = true;
    } else if (argv[i]=="--compiler-statistics") {
      flag_compiler_statistics = true;
    } else {
      if ((argv[i]=="--fzn-cmd" || argv[i]=="--flatzinc-cmd") && solver.empty()) {
        solver = "org.minizinc.mzn-fzn";
      }
      if (argv[i]=="--compile-solution-checker") {
        compileSolutionChecker = true;
      }
      argv[j++] = argv[i];
    }
  }
  argv.resize(j);
  argc = j;

  /*
  if ( (mzn2fzn_exe || compileSolutionChecker) && solver.empty()) {
    solver = "org.minizinc.mzn-fzn";
  }
  */
  
  if (flag_verbose) {
    argv.push_back("--verbose-solving");
    argc++;
  }
  if (flag_statistics) {
    argv.push_back("--solver-statistics");
    argc++;
  }
  
  // flt.set_flag_output_by_default(ifMzn2Fzn());

  // bool isMznMzn = false;
  
  if (/*!ifSolns2out()*/ true) {
    try {
      const SolverConfig& sc = solver_configs.config(solver);
      string solverId = sc.executable().empty() ? sc.id() : (sc.supportsMzn() ?  string("org.minizinc.mzn-mzn") : string("org.minizinc.mzn-fzn"));
      for (auto it = getGlobalSolverRegistry()->getSolverFactories().begin();
           it != getGlobalSolverRegistry()->getSolverFactories().end(); ++it) {
        if ((*it)->getId()==solverId) { /// TODO: also check version (currently assumes all ids are unique)
          sf = *it;
          si_opt = sf->createOptions();
          if (!sc.executable().empty() || solverId=="org.minizinc.mzn-fzn") {
            std::vector<MZNFZNSolverFlag> acceptedFlags;
            for (auto& sf : sc.stdFlags())
              acceptedFlags.push_back(MZNFZNSolverFlag::std(sf));
            for (auto& ef : sc.extraFlags())
              acceptedFlags.push_back(MZNFZNSolverFlag::extra(ef.flag,ef.flag_type));

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

              if(!fzn_mzn_flags.empty()) {
                addFlags("--mzn-flag", fzn_mzn_flags, additionalArgs_s);
              }

              // This should maybe be moved to fill in fzn_mzn_flags when
              // --find-muses is implemented (these arguments will be passed
              // through to the subsolver of findMUS)
              if (!sc.mznlib().empty()) {
                if (sc.mznlib().substr(0,2)=="-G") {
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

              for (i=0; i<additionalArgs_s.size(); ++i) {
                bool success = sf->processOption(si_opt, i, additionalArgs_s);
                if (!success) {
                  log << "Solver backend " << solverId << " does not recognise option " << additionalArgs_s[i]  << "." << endl;
                  return OPTION_ERROR;
                }
              }
            } else {
              static_cast<FZN_SolverFactory*>(sf)->setAcceptedFlags(si_opt, acceptedFlags);
              std::vector<std::string> additionalArgs;
              additionalArgs.push_back("--fzn-cmd");
              if (sc.executable_resolved().size()) {
                additionalArgs.push_back(sc.executable_resolved());
              } else {
                additionalArgs.push_back(sc.executable());
              }
              if(!fzn_mzn_flags.empty()) {
                addFlags("--fzn-flag", fzn_mzn_flags, additionalArgs);
              }
              if (sc.needsPathsFile()) {
                // Instruct flattener to hold onto paths
                int i=0;
                vector<string> args {"--keep-paths"};
                flt.processOption(i, args);

                // Instruct FznSolverInstance to write a path file
                // and pass it to the executable with --paths arg
                additionalArgs.push_back("--fzn-needs-paths");
              }
              if (!sc.needsSolns2Out()) {
                additionalArgs.push_back("--fzn-output-passthrough");
              }
              int i=0;
              for (i=0; i<additionalArgs.size(); ++i) {
                bool success = sf->processOption(si_opt, i, additionalArgs);
                if (!success) {
                  log << "Solver backend " << solverId << " does not recognise option " << additionalArgs[i]  << "." << endl;
                  return OPTION_ERROR;
                }
              }
            }
          }
          if (!sc.mznlib().empty()) {
            if (sc.mznlib().substr(0,2)=="-G") {
              std::vector<std::string> additionalArgs({sc.mznlib()});
              int i=0;
              if (!flt.processOption(i, additionalArgs)) {
                log << "Flattener does not recognise option " << sc.mznlib() << endl;
                return OPTION_ERROR;
              }
            } else {
              std::vector<std::string>  additionalArgs(2);
              additionalArgs[0] = "-I";
              if (sc.mznlib_resolved().size()) {
                additionalArgs[1] = sc.mznlib_resolved();
              } else {
                additionalArgs[1] = sc.mznlib();
              }
              int i=0;
              if (!flt.processOption(i, additionalArgs)) {
                log << "Flattener does not recognise option -I." << endl;
                return OPTION_ERROR;
              }
            }
          }
          if (!sc.defaultFlags().empty()) {
            std::vector<std::string> addedArgs;
            addedArgs.push_back(argv[0]); // excutable name
            for (auto& df : sc.defaultFlags()) {
              addedArgs.push_back(df);
            }
            for (int i=1; i<argv.size(); i++) {
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

    if (sf==NULL) {
      log << "Solver " << solver << " not found." << endl;
      return OPTION_ERROR;
    }
    
    for (i=1; i<argc; ++i) {
      if ( !ifMzn2Fzn() ? s2out.processOption( i, argv ) : false ) {
      } else if ((!isMznMzn || is_mzn2fzn) && flt.processOption(i, argv)) {
      } else if (sf != NULL && sf->processOption(si_opt, i, argv)) {
      } else {
        std::string executable_name(argv[0]);
        executable_name = executable_name.substr(executable_name.find_last_of("/\\") + 1);
        log << executable_name << ": Unrecognized option or bad format `" << argv[i] << "'" << endl;
        return OPTION_ERROR;
      }
    }
    return OPTION_OK;

  } else {
    for (i=1; i<argc; ++i) {
      if ( s2out.processOption( i, argv ) ) {
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

/*
void MznReader::flatten(const std::string& modelString, const std::string& modelName)
{
  flt.set_flag_verbose(flag_compiler_verbose);
  flt.set_flag_statistics(flag_compiler_statistics);
  Timer tm01;
  flt.flatten(modelString, modelName);
  /// The following message tells mzn-test.py that flattening succeeded.
  if (flag_compiler_verbose)
    log << "  Flattening done, " << tm01.stoptime() << std::endl;
}
*/

SolverInstance::Status MznSolver::solve()
{
  { // To be able to clean up flatzinc after PrcessFlt()
    GCLock lock;
    getSI()->processFlatZinc();
  }
  SolverInstance::Status status = getSI()->solve();
  GCLock lock;
  if (status==SolverInstance::SAT || status==SolverInstance::OPT) {
    if ( !getSI()->getSolns2Out()->fStatusPrinted )
      getSI()->getSolns2Out()->evalStatus( status );
  }
  else {
    if ( !getSI()->getSolns2Out()->fStatusPrinted )
      getSI()->getSolns2Out()->evalStatus( status );
  }
  if (si_opt->printStatistics)
    printStatistics();
  return status;
}

void MznSolver::printStatistics()
{ // from flattener too?   TODO
  if (si)
    getSI()->printStatistics();
}

SolverInstance::Status MznSolver::run(const std::vector<std::string>& args0, const std::string& model,
                                      const std::string& exeName, const std::string& modelName) {
  using namespace std::chrono;
  steady_clock::time_point startTime = steady_clock::now();
  std::vector<std::string> args = {exeName};
  for (auto a : args0)
    args.push_back(a);
  switch (processOptions(args)) {
    case OPTION_FINISH:
      return SolverInstance::NONE;
    case OPTION_ERROR:
      printUsage();
      os << "More info with \"" << exeName << " --help\"\n";
      return SolverInstance::ERROR;
    case OPTION_OK:
      break;
  }
  if (!(!ifMzn2Fzn() && sf!=NULL && sf->getId() == "org.minizinc.mzn-mzn") && !flt.hasInputFiles() && model.empty()) {
    // We are in solns2out mode
    while ( std::cin.good() ) {
      string line;
      getline( std::cin, line );
      line += '\n';                // need eols as in t=raw stream
      s2out.feedRawDataChunk( line.c_str() );
    }
    return SolverInstance::NONE;
  }

  if (!ifMzn2Fzn() && sf->getId() == "org.minizinc.mzn-mzn") {
    Env env;
    si = sf->createSI(env, log, si_opt);
    si->setSolns2Out( &s2out );
    { // To be able to clean up flatzinc after PrcessFlt()
      GCLock lock;
      getSI()->_options->verbose = get_flag_verbose();
      getSI()->_options->printStatistics = get_flag_statistics();
    }
    getSI()->solve();
    return SolverInstance::NONE;
  }
  
  flatten(model,modelName);

  if (!ifMzn2Fzn() && flag_overall_time_limit != 0) {
    steady_clock::time_point afterFlattening = steady_clock::now();
    milliseconds passed = duration_cast<milliseconds>(afterFlattening-startTime);
    milliseconds time_limit(flag_overall_time_limit);
    if (passed > time_limit) {
      s2out.evalStatus( getFltStatus() );
      return SolverInstance::UNKNOWN;
    }
    int time_left = (time_limit-passed).count();
    std::vector<std::string> timeoutArgs(2);
    timeoutArgs[0] = "--solver-time-limit";
    std::ostringstream oss;
    oss << time_left;
    timeoutArgs[1] = oss.str();
    int i=0;
    sf->processOption(si_opt, i, timeoutArgs);
  }

  if (SolverInstance::UNKNOWN == getFltStatus())
  {
    if ( !ifMzn2Fzn() ) {          // only then
      // GCLock lock;                  // better locally, to enable cleanup after ProcessFlt()
      addSolverInterface();
      return solve();
    }
    return SolverInstance::NONE;
  } else {
    if ( !ifMzn2Fzn() )
      s2out.evalStatus( getFltStatus() );
    return getFltStatus();
  }                                   //  Add evalOutput() here?   TODO
}
#endif

};  // namespace MiniZinc
