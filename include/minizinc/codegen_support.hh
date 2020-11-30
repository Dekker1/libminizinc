/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Graeme Gange <graeme.gange@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_CODEGEN_SUPPORT_HH__
#define __MINIZINC_CODEGEN_SUPPORT_HH__

#include <minizinc/ast.hh>
#include <minizinc/interpreter/bytecode.hh>

#include <iostream>
#include <set>
#include <vector>

#ifdef _MSC_VER
// Include header for _BitScanForward intrinsic.
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif

// Support data structures for code generation.
// Maps/tables/etc.
namespace MiniZinc {

// TODO: Use a more efficient structure for tracking scopes, along
// the lines of fast-mergeable maps.
struct cmp_ASTString {
  bool operator()(const ASTString& s, const ASTString& t) const {
    if (s.size() != t.size()) return s.size() < t.size();
    return s.size() > 0 && strncmp(s.c_str(), t.c_str(), s.size()) < 0;
  }
};
typedef std::set<ASTString, cmp_ASTString> ASTStSet;

struct eq_Expression {
  bool operator()(Expression* e, Expression* f) const { return Expression::equal(e, f); }
};
struct hash_Expression {
  bool operator()(Expression* e) const { return Expression::hash(e); }
};

template <class T>
struct ExprMap {
  typedef std::unordered_map<Expression*, T, hash_Expression, eq_Expression> t;
};

#ifdef _MSC_VER
inline unsigned int find_lsb(unsigned int x) {
  unsigned long p;
  _BitScanForward(&p, x);
  return p;
}
#else
// Assuming GCC or clang
inline unsigned int find_lsb(unsigned int x) { return __builtin_ctz(x); }
#endif

};  // namespace MiniZinc

#endif
