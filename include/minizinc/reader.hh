/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 *     Graeme Gange <graeme.gange@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_READER_HH__
#define __MINIZINC_READER_HH__

#include <minizinc/ast.hh>
#include <minizinc/exception.hh>
#include <minizinc/solver_config.hh>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace MiniZinc {
// Class MznReader handles option handling and model parsing.
// Essentiially the first half of the MznSolver object.
class MznReader {
private:
  /// Solver configurations
  SolverConfigs solver_configs;

  std::string executable_name;
  std::ostream& os;
  std::ostream& log;

public:
  enum OptionStatus { OPTION_OK, OPTION_ERROR, OPTION_FINISH };
  // Search paths
  std::vector<std::string> filenames;
  std::vector<std::string> datafiles;
  std::vector<std::string> includePaths;

  std::string std_lib_dir;
  std::string globals_dir;

  /// global options
  bool flag_verbose = false;
  bool flag_statistics = false;
  /*
  bool flag_compiler_verbose=false;
  bool flag_compiler_statistics=false;
  int flag_overall_time_limit=0;
  */

  bool flag_ignoreStdlib = false;
  bool flag_typecheck = true;
  bool flag_werror = false;
  bool flag_only_range_domains = false;
  bool flag_allow_unbounded_vars = false;
  bool flag_stdinInput = false;

  // Timer starttime;

public:
  MznReader(std::ostream& os = std::cout, std::ostream& log = std::cerr);
  ~MznReader();

  Model* read(const std::string& modelString = std::string(),
              const std::string& modelName = std::string("stdin"));
  OptionStatus processOptions(std::vector<std::string>& argv);
  bool get_flag_verbose() { return flag_verbose; }
  void printUsage();

private:
  bool processOption(int& i, std::vector<std::string>& argv);

  void printHelp(const std::string& selectedSolver = std::string());
  void printStatistics();

  bool get_flag_statistics() { return flag_statistics; }
};

}  // namespace MiniZinc

#endif
