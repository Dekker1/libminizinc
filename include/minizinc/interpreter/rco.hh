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

namespace MiniZinc {

class Interpreter;

class RefCountedObject {
public:
  enum RCOType { VEC, VAR };

protected:
  unsigned int _model_ref_count;
  unsigned int _memory_ref_count : 31;
  unsigned int _rco_type : 1;
  int _timestamp;
  RefCountedObject(const RCOType& t, int timestamp)
      : _model_ref_count(0),
        _memory_ref_count(0),
        _rco_type(t == VEC ? 1 : 0),
        _timestamp(timestamp) {}

public:
  RCOType rcoType(void) const { return _rco_type == 1 ? VEC : VAR; }
  const int timestamp() const { return _timestamp; }

  bool exists() const { return _model_ref_count > 0; }
  bool alive() const { return _model_ref_count + _memory_ref_count > 0; }
  bool unique() const { return _model_ref_count == 1; }

  void addRef(Interpreter* interpreter) { _model_ref_count++; }
  void addMemRef(Interpreter* interpreter) { _memory_ref_count++; }

  static void rmRef(Interpreter* interpreter, RefCountedObject* rco);
  static void rmMemRef(Interpreter* interpreter, RefCountedObject* rco);
};

}  // namespace MiniZinc
