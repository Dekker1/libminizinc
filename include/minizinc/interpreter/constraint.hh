/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <minizinc/interpreter/values.hh>

namespace MiniZinc {

class Interpreter;
class Trail;

class Constraint {
  friend class Trail;

protected:
  Val _ann;
  unsigned int _pred : 24;
  unsigned int _mode : 8;
  unsigned int _size : 30;
  /// Whether the constraint is scheduled for propagation
  unsigned int _scheduled : 1;
  unsigned int _delayed : 1;
  Val _defines;
  Val _args[1];
  Constraint(Interpreter* interpreter, int pred, char mode, arg_iter arg_start, arg_iter arg_end,
             size_t nargs, Val ann, Val defines, bool delayed);
  Constraint(Interpreter* interpreter, int pred, char mode, const std::vector<Val>& args, Val ann,
             Val defines, bool delayed);

public:
  static std::pair<Constraint*, bool> a(Interpreter* interpreter, int pred, char mode,
                                        const std::vector<Val>& args, Val ann = 0, Val defines = 1,
                                        bool delayed = false);
  static std::pair<Constraint*, bool> a(Interpreter* interpreter, int pred, char mode,
                                        arg_iter arg_start, arg_iter arg_end, size_t nargs,
                                        Val ann = 0, Val defines = 1, bool delayed = false);
  void destroy(Interpreter* interpreter);
  void reconstruct(Interpreter* interpreter);

  ~Constraint(void) = delete;

  Val ann(void) const { return _ann; }
  int pred(void) const { return _pred; }
  char mode(void) const { return _mode; }
  int size(void) const { return _size; }
  bool delayed(void) const { return _delayed == 1; }
  const Val& arg(int i) const {
    assert(i < _size);
    return _args[i];
  }
  void arg(Interpreter* interpreter, int i, Val nv) {
    assert(i < _size);
    assert(_pred != 8 || i != 1 || nv.isVec());
    nv.addRef(interpreter);
    _args[i].rmRef(interpreter);
    _args[i] = nv;
  }

  /// Flag whether constraint is currently scheduled
  bool scheduled(void) const { return _scheduled == 1; }
  /// Set flag whether constraint is currently scheduled
  void scheduled(bool f) { _scheduled = f; }
};

}  // namespace MiniZinc
