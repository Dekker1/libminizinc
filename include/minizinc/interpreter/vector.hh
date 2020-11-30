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

#include <minizinc/interpreter/_val_decl.hh>
#include <minizinc/interpreter/rco.hh>
#include <minizinc/interpreter/variable.hh>

namespace MiniZinc {

class Vec : public RefCountedObject {
protected:
  int _size : 31;
  bool _has_index : 1;
  Val _data[1];
  Vec(Interpreter* interpreter, int timestamp, const std::vector<Val>& v, bool has_index)
      : RefCountedObject(RefCountedObject::VEC, timestamp), _size(v.size()), _has_index(has_index) {
    for (unsigned int i = 0; i < v.size(); i++) {
      new (&_data[i]) Val(v[i]);
      _data[i].addRef(interpreter);
    }
  }
  ~Vec(void) = delete;

public:
  int size(void) const {
    assert(alive());
    return _size;
  }
  // TODO: Should vectors be indexed from 1 internally?
  const Val& operator[](int i) const {
    assert(alive());
    assert(i >= 0 && i < _size);
    return _data[i];
  }
  static Vec* a(Interpreter* interpreter, int timestamp, const std::vector<Val>& v,
                bool has_index = false) {
    Vec* nv = static_cast<Vec*>(
        ::malloc(sizeof(Vec) + sizeof(Val) * std::max(0, static_cast<int>(v.size() - 1))));
    new (nv) Vec(interpreter, timestamp, v, has_index);
    return nv;
  }
  void destroyModel(Interpreter* interpreter) {
    for (unsigned int i = 0; i < _size; i++) {
      if (_memory_ref_count > 0u) {
        _data[i].addMemRef(interpreter);
      }
      _data[i].rmRef(interpreter);
    }
  }
  void destroyMemory(Interpreter* interpreter) {
    for (unsigned int i = 0; i < _size; i++) {
      _data[i].rmMemRef(interpreter);
    }
  }
  void reconstruct(Interpreter* interpreter) {
    for (unsigned int i = 0; i < _size; i++) {
      _data[i].addRef(interpreter);
    }
  }
  inline bool operator==(const Vec& rhs) const {
    assert(alive());
    assert(rhs.alive());
    if (_size != rhs._size) {
      return false;
    }
    for (int i = 0; i < _size; ++i) {
      if ((*this)[i].isVar() && !(*this)[i].toVar()->exists()) {
        return false;
      }
      if (rhs[i].isVar() && !rhs[i].toVar()->exists()) {
        return false;
      }
      if (!((*this)[i] == rhs[i])) {
        return false;
      }
    }
    return true;
  }
  bool isPar() const;
  bool hasIndexSet() const { return _has_index; }
  Val raw_data() const {
    if (_has_index) {
      assert(_size == 2);
      assert((*this)[0].isVec());
      return (*this)[0];
    }
    return Val(this);
  }
  Val index_set() const {
    assert(_has_index);
    assert(_size == 2);
    assert((*this)[1].isVec());
    return (*this)[1];
  }
  std::vector<Val> as_vector() {
    assert(alive());
    std::vector<Val> nv;
    nv.reserve(size());
    for (int i = 0; i < size(); ++i) {
      nv.push_back(_data[i]);
    }
    return nv;
  }
  int count(Val v) {
    assert(alive());
    int count = 0;
    for (int i = 0; i < _size; ++i) {
      if (_data[i].isVec()) {
        count += _data[i].toVec()->count(v);
      } else {
        count += _data[i] == v;
      }
    }
    return count;
  }
  const size_t hash() const {
    auto combine = [](size_t& incumbent, size_t h) {
      incumbent ^= h + 0x9e3779b9 + (incumbent << 6) + (incumbent >> 2);
    };
    std::hash<int> h;
    size_t hash = 0;
    for (int i = 0; i < size(); ++i) {
      Val v = _data[i];
      if (v.isInt()) {
        combine(hash, h(v.toInt()));
      } else if (v.isVar()) {
        combine(hash, h(v.timestamp()));
      } else {
        assert(v.isVec());
        combine(hash, v.toVec()->hash());
      }
    }
    return hash;
  }

  void finalizeLin(Interpreter* interpreter) {
    for (int i = 0; i < _size; ++i) {
      _data[i].finalizeLin(interpreter);
    }
  }

  const Val* begin(void) const { return _data; }
  const Val* end(void) const { return _data + _size; }
};

}  // namespace MiniZinc
