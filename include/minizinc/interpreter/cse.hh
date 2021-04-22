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

#include <minizinc/interpreter/bytecode.hh>
#include <minizinc/interpreter/values.hh>

#include <array>
#include <unordered_map>

namespace MiniZinc {

class Interpreter;

class WeakVal {
protected:
  // Value of the Val
  void* _v;

public:
  WeakVal() : _v(nullptr) {}
  explicit WeakVal(Interpreter& interpreter, const Val& val) {
    if (val.isInt()) {
      assert((reinterpret_cast<ptrdiff_t>(val._v) & static_cast<ptrdiff_t>(3)) == 0);
      _v = val._v;
    } else if (val.isVar()) {
      auto timestamp = val.timestamp();
      // TODO: assert timestamp <= unboxed int
      assert(timestamp >= 0);
      _v = reinterpret_cast<void*>(static_cast<ptrdiff_t>(timestamp) << 2 |
                                   static_cast<ptrdiff_t>(1));
    } else {
      assert(val.isVec());
      _v = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(val._v) | static_cast<ptrdiff_t>(3));
      toVec()->addMemRef(&interpreter);
    }
  }

  void destroy(Interpreter& interpreter) {
    if (isVec()) {
      RefCountedObject::rmMemRef(&interpreter, toVec());
    }
  }

  bool isVec(void) const {
    return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(3)) ==
           static_cast<ptrdiff_t>(3);
  }
  Vec* toVec() const {
    assert(isVec());
    return reinterpret_cast<Vec*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3));
  }
  size_t hash() const {
    if (isVec()) {
      return toVec()->hash();
    }
    std::hash<void*> h;
    return h(_v);
  }
  inline bool operator==(const WeakVal& rhs) const {
    if (isVec()) {
      return rhs.isVec() && (*toVec() == *rhs.toVec());
    }
    return reinterpret_cast<ptrdiff_t>(_v) == reinterpret_cast<ptrdiff_t>(rhs._v);
  }
  inline bool operator!=(const WeakVal& rhs) const { return !(*this == rhs); }
};

class CSEKey {
protected:
  size_t _hash;

public:
  CSEKey() : _hash(0) {}
  virtual ~CSEKey() {}
  // INVARIANT: Key is not used after destroy is called
  virtual void destroy(Interpreter& interpreter) = 0;
  const size_t hash() const { return _hash; }

  // Any implementation of CSEKey should implement a == operator for unordered_map;
  // virtual const bool operator==(const CSEKey& rhs) const = 0;
};

class VariadicKey : public CSEKey {
private:
  size_t _size;
  WeakVal* _vals;

public:
  VariadicKey() : _size(0), _vals(nullptr) {}

  VariadicKey(Interpreter& interpreter, arg_iter start, arg_iter end, size_t nargs) {
    _size = nargs;
    _vals = (WeakVal*)malloc(_size * sizeof(WeakVal));

    int i = 0;
    for (arg_iter it = start; it != end; ++it) {
      _vals[i++] = WeakVal(interpreter, *it);
    }
    assert(i == _size);
    _hash = compute_hash(*this);
  }
  VariadicKey(Interpreter& interpreter, const std::vector<Val>& vals) {
    _size = vals.size();
    _vals = (WeakVal*)malloc(_size * sizeof(WeakVal));

    for (int i = 0; i < _size; ++i) {
      _vals[i] = WeakVal(interpreter, vals[i]);
    }
    _hash = compute_hash(*this);
  }
  virtual ~VariadicKey() {}

  void destroy(Interpreter& interpreter) override {
    for (int i = 0; i < _size; ++i) {
      _vals[i].destroy(interpreter);
    }
    free(_vals);
  }

  const bool operator==(const VariadicKey& rhs) const {
    if (_size != rhs._size) {
      return false;
    }
    for (int i = 0; i < _size; ++i) {
      if (_vals[i] != rhs._vals[i]) {
        return false;
      }
    }
    return true;
  }

protected:
  static size_t compute_hash(const VariadicKey& k) {
    auto combine = [](size_t& incumbent, size_t h) {
      incumbent ^= h + 0x9e3779b9 + (incumbent << 6) + (incumbent >> 2);
    };
    size_t hash = 0;
    for (int i = 0; i < k._size; ++i) {
      combine(hash, k._vals[i].hash());
    }
    return hash;
  }
};

template <int nargs>
class FixedKey : public CSEKey {
private:
  std::array<WeakVal, nargs> _vals;

public:
  FixedKey() {}
  FixedKey(Interpreter& interpreter, arg_iter start, arg_iter end) {
    int i = 0;
    for (arg_iter it = start; it != end; ++it) {
      _vals[i++] = WeakVal(interpreter, *it);
    }
    assert(i == nargs);
    _hash = compute_hash(*this);
  }
  FixedKey(Interpreter& interpreter, const std::vector<Val>& vals) {
    assert(vals.size() == nargs);
    for (int i = 0; i < nargs; ++i) {
      _vals[i] = WeakVal(interpreter, vals[i]);
    }
    _hash = compute_hash(*this);
  }
  virtual ~FixedKey() {}

  void destroy(Interpreter& interpreter) override {
    for (int i = 0; i < nargs; ++i) {
      _vals[i].destroy(interpreter);
    }
  }
  const bool operator==(const FixedKey<nargs>& rhs) const {
    for (int i = 0; i < nargs; ++i) {
      if (_vals[i] != rhs._vals[i]) {
        return false;
      }
    }
    return true;
  }

protected:
  static size_t compute_hash(const FixedKey<nargs>& k) {
    auto combine = [](size_t& incumbent, size_t h) {
      incumbent ^= h + 0x9e3779b9 + (incumbent << 6) + (incumbent >> 2);
    };
    size_t hash = 0;
    for (int i = 0; i < nargs; ++i) {
      combine(hash, k._vals[i].hash());
    }
    return hash;
  }
};

/// CSE table: Saved results of historical executions
template <class Key>
class CSETable {
protected:
  struct Hash {
    size_t operator()(const Key& key) const { return key.hash(); }
  };
  struct Equals {
    bool operator()(const Key& lhs, const Key& rhs) const { return lhs == rhs; }
  };
  typedef std::unordered_map<Key, std::pair<BytecodeProc::Mode, Val>, Hash, Equals> impl;
  std::vector<impl> _table = std::vector<impl>(1);

public:
  ~CSETable() { assert(_table.size() == 1 && _table[0].empty()); }

  std::pair<Val, bool> find(Interpreter& interpreter, const Key& key, BytecodeProc::Mode& mode);
  void insert(Interpreter& interpreter, Key& key, const BytecodeProc::Mode& mode, Val& val);

  // TODO: Is this const_cast actually legal??
  void destroy(Interpreter* interpreter) {
    for (auto& table : _table) {
      for (auto& item : table) {
        const_cast<Key&>(item.first).destroy(*interpreter);
        item.second.second.rmMemRef(interpreter);
      }
    }
    _table = std::vector<impl>(1);
  }
  void push(Interpreter* interpreter, bool cleanup) {
    if (cleanup) {
      auto& table = _table.back();
      auto it = table.begin();
      while (it != table.end()) {
        if (!it->second.second.exists()) {
          const_cast<Key&>(it->first).destroy(*interpreter);
          it->second.second.rmMemRef(interpreter);
          it = table.erase(it);
        } else {
          ++it;
        }
      }
    }
    _table.emplace_back();
  }
  void pop(Interpreter* interpreter) {
    for (auto& item : _table.back()) {
      const_cast<Key&>(item.first).destroy(*interpreter);
      item.second.second.rmMemRef(interpreter);
    }
    _table.pop_back();
  }
};

}  // namespace MiniZinc
