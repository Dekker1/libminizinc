/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_MZA_PARSER_HH__
#define __MINIZINC_MZA_PARSER_HH__

// Assembly Parser Requirements
#include <minizinc/interpreter.hh>

#include <list>

// This is a workaround for a bug in flex that only shows up
// with the Microsoft C++ compiler
#if defined(_MSC_VER)
#define YY_NO_UNISTD_H
#ifdef __cplusplus
extern "C" int isatty(int);
#endif
#endif

// The Microsoft C++ compiler marks certain functions as deprecated,
// so let's take the alternative definitions
#if defined(_MSC_VER)
#define strdup _strdup
#define fileno _fileno
#endif

// Anonymous struct for when yyparse is exported
typedef struct MZAContext MZAContext;
// Parser generated header
#include <minizinc/support/mza_parser.tab.hh>

using namespace MiniZinc;

// Parsing function
std::pair<int, std::vector<BytecodeProc>> parse_mza(const std::string& assembly_str);

#endif  //__MINIZINC_MZA_PARSER_HH__
