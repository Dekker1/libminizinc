/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/interpreter.hh>
#include <minizinc/interpreter/constraint.hh>
#include <minizinc/interpreter/primitives.hh>

namespace MiniZinc {

Constraint::Constraint(Interpreter* interpreter, int pred, char mode, arg_iter arg_start,
                       arg_iter arg_end, size_t nargs, Val ann, Val defines, bool delayed)
    : _pred(pred), _mode(mode), _size(nargs), _scheduled(0), _delayed(delayed) {
  _ann.addRef(interpreter);
  int i = 0;
  for (auto it = arg_start; it != arg_end; ++it) {
    new (&_args[i]) Val(*it);
    _args[i].addRef(interpreter);
    if (!_delayed && pred > PrimitiveMap::MAX_LIN) {
      _args[i].finalizeLin(interpreter);
    }
    ++i;
  }
  assert(i == nargs);
}

Constraint::Constraint(Interpreter* interpreter, int pred, char mode, const std::vector<Val>& args,
                       Val ann, Val defines, bool delayed)
    : _pred(pred), _mode(mode), _size(args.size()), _scheduled(0), _delayed(delayed) {
  _ann.addRef(interpreter);
  for (unsigned int i = 0; i < args.size(); i++) {
    new (&_args[i]) Val(args[i]);
    _args[i].addRef(interpreter);
    if (!_delayed && pred > PrimitiveMap::MAX_LIN) {
      _args[i].finalizeLin(interpreter);
    }
  }
}

std::pair<Constraint*, bool> Constraint::a(Interpreter* interpreter, int pred, char mode,
                                           arg_iter arg_start, arg_iter arg_end, size_t nargs,
                                           Val defines, Val ann, bool delayed) {
  Constraint* c = static_cast<Constraint*>(
      ::malloc(sizeof(Constraint) + sizeof(Val) * (std::max(0, static_cast<int>(nargs) - 1))));
  c = new (c) Constraint(interpreter, pred, mode, arg_start, arg_end, nargs, ann, defines, delayed);
  PropStatus ps = interpreter->subscribe(c);
  if (ps == PS_ENTAILED || ps == PS_FAILED) {
    interpreter->unsubscribe(c);
    c->destroy(interpreter);
    ::free(c);
    return {nullptr, ps == PS_ENTAILED};
  }
  return {c, true};
}

std::pair<Constraint*, bool> Constraint::a(Interpreter* interpreter, int pred, char mode,
                                           const std::vector<Val>& args, Val defines, Val ann,
                                           bool delayed) {
  Constraint* c = static_cast<Constraint*>(::malloc(
      sizeof(Constraint) + sizeof(Val) * (std::max(0, static_cast<int>(args.size()) - 1))));
  c = new (c) Constraint(interpreter, pred, mode, args, ann, defines, delayed);
  PropStatus ps = interpreter->subscribe(c);
  if (ps == PS_ENTAILED || ps == PS_FAILED) {
    interpreter->unsubscribe(c);
    c->destroy(interpreter);
    ::free(c);
    return {nullptr, ps == PS_ENTAILED};
  }
  return {c, true};
}

void Constraint::destroy(Interpreter* interpreter) {
  for (unsigned int i = 0; i < _size; i++) {
    _args[i].rmRef(interpreter);
  }
  if (delayed()) {
    interpreter->remove_delayed(this);
  }
}

void Constraint::reconstruct(Interpreter* interpreter) {
  for (unsigned int i = 0; i < _size; i++) {
    _args[i].addRef(interpreter);
  }
}

}  // namespace MiniZinc
