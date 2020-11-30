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

#include <minizinc/interpreter/rco.hh>
#include <minizinc/values.hh>

namespace MiniZinc {
class Val;
}
namespace std {
MiniZinc::Val abs(const MiniZinc::Val& x);
}

namespace MiniZinc {

class Interpreter;
class Variable;
class Vec;
class WeakVal;

enum PropStatus { PS_OK, PS_FAILED, PS_ENTAILED };

/// Value tagged union
class Val {
  friend class WeakVal;
  friend Val operator+(const Val& x, const Val& y);
  friend Val operator-(const Val& x, const Val& y);
  friend Val operator*(const Val& x, const Val& y);
  friend Val operator/(const Val& x, const Val& y);
  friend Val operator%(const Val& x, const Val& y);
  friend Val std::abs(const Val& x);
  friend bool operator==(const Val& x, const Val& y);
  friend bool operator!=(const Val& x, const Val& y);

protected:
  static const long long int maxUnboxedVal =
      (static_cast<long long int>(1) << (sizeof(void*) * 8 - 3)) - static_cast<long long int>(1);
  /// The value
  // Bit 0: 0=int, 1=RefCountedObject
  // Bit 1: 0=int, 1=Infinity
  // Bit 2: 0=negative, 1=positive
  void* _v;

  /// TODO: implement correct overflow handling for the new type (which is smaller than IntVal)
  static long long int safePlus(long long int x, long long int y) {
    if (x < 0) {
      if (y < -maxUnboxedVal - x) throw ArithmeticError("integer overflow");
    } else {
      if (y > maxUnboxedVal - x) throw ArithmeticError("integer overflow");
    }
    return x + y;
  }
  static long long int safeMinus(long long int x, long long int y) {
    if (x < 0) {
      if (y > x - -maxUnboxedVal) throw ArithmeticError("integer overflow");
    } else {
      if (y < x - maxUnboxedVal) throw ArithmeticError("integer overflow");
    }
    return x - y;
  }
  static long long int safeMult(long long int x, long long int y) {
    if (y == 0) return 0;
    long long unsigned int x_abs = (x < 0 ? 0 - x : x);
    long long unsigned int y_abs = (y < 0 ? 0 - y : y);
    if (x_abs > maxUnboxedVal / y_abs) throw ArithmeticError("integer overflow");
    return x * y;
  }
  static long long int safeDiv(long long int x, long long int y) {
    if (y == 0) throw ArithmeticError("integer division by zero");
    if (x == 0) return 0;
    if (x == -maxUnboxedVal && y == -1) throw ArithmeticError("integer overflow");
    return x / y;
  }
  static long long int safeMod(long long int x, long long int y) {
    if (y == 0) throw ArithmeticError("integer division by zero");
    if (y == -1) return 0;
    return x % y;
  }
  void safeSetVal(long long int i) {
    ptrdiff_t ubi_p;
    ubi_p = (static_cast<ptrdiff_t>(i < 0 ? -i : i) << 3);
    if (i < 0) {
      ubi_p = ubi_p | static_cast<ptrdiff_t>(4);
    }
    _v = reinterpret_cast<void*>(ubi_p);
  }

public:
  static Val follow_alias(const Val& v, Interpreter* interpreter = nullptr);

  bool isRCO(void) const {
    return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(1)) ==
           static_cast<ptrdiff_t>(1);
  }
  RefCountedObject* toRCO(void) const {
    assert(isRCO());
    return reinterpret_cast<RefCountedObject*>(reinterpret_cast<ptrdiff_t>(_v) &
                                               ~static_cast<ptrdiff_t>(1));
  }
  bool exists() const { return !isRCO() || toRCO()->exists(); }
  bool unique() const { return !isRCO() || toRCO()->unique(); }
  bool isVec(void) const { return isRCO() && toRCO()->rcoType() == RefCountedObject::VEC; }
  bool isInt(void) const {
    return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(1)) == 0;
  }
  bool isVar(void) const { return isRCO() && toRCO()->rcoType() == RefCountedObject::VAR; }
  bool containsVar(void) const {
    if (isInt()) return false;
    if (isVar()) return true;
    for (int i = 0; i < size(); ++i) {
      if ((*this)[i].containsVar()) return true;
    }
    return false;
  }
  bool contains(const Val& v) const {
    if (*this == v) {
      return true;
    }
    if (this->isVec()) {
      for (int i = 0; i < this->size(); ++i) {
        if ((*this)[i].contains(v)) {
          return true;
        }
      }
    }
    return false;
  }

  /// Access value as Variable
  Variable* toVar(void) const;

  long long int toInt(void) const {
    assert(isInt());
    unsigned long long int i = reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(7);
    bool inf = ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(2)) != 0);
    assert(!inf);
    bool pos = ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(4)) == 0);
    if (pos) {
      return static_cast<long long int>(i >> 3);
    } else {
      return -(static_cast<long long int>(i >> 3));
    }
  }
  IntVal toIntVal(void) const {
    assert(isInt());
    unsigned long long int i = reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(7);
    bool inf = ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(2)) != 0);
    bool pos = ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(4)) == 0);
    if (inf) {
      return pos ? IntVal::infinity() : -IntVal::infinity();
    }
    if (pos) {
      return static_cast<long long int>(i >> 3);
    } else {
      return -(static_cast<long long int>(i >> 3));
    }
  }

  int timestamp() const {
    assert(isRCO());
    return toRCO()->timestamp();
  }

  void rmRef(Interpreter* interpreter) {
    if (isRCO()) {
      RefCountedObject::rmRef(interpreter, toRCO());
    }
  };
  void addRef(Interpreter* interpreter) {
    if (isRCO()) {
      toRCO()->addRef(interpreter);
    }
  }
  void addMemRef(Interpreter* interpreter) {
    if (isRCO()) {
      toRCO()->addMemRef(interpreter);
    }
  };
  void rmMemRef(Interpreter* interpreter) {
    if (isRCO()) {
      RefCountedObject::rmMemRef(interpreter, toRCO());
    }
  };

  /// Access value as vector, return element \a i
  const Val& operator[](int i) const;
  /// Access value as vector, return size
  size_t size(void) const;
  /// Access value as vector
  Vec* toVec(void) const;

public:
  Val(const long long int i = 0);
  explicit Val(const RefCountedObject* d);
  static Val fromIntVal(const IntVal& iv);
  static Val infinity(void);
  ~Val(void);
  Val(const Val& v);
  Val(Val&& v);
  Val& operator=(const Val& v);
  Val& operator=(Val&& v);
  void assign(Interpreter* interpreter, const Val& v);
  void assign(Interpreter* interpreter, Val&& v);
  std::string toString(bool trim = false) const;
  Val lb() const;
  Val ub() const;
  bool isFixed() const;
  void finalizeLin(Interpreter* interpreter);

  // Integer interface
  bool isFinite(void) const;
  bool isPlusInfinity(void) const;
  bool isMinusInfinity(void) const;
  Val& operator+=(const Val& x);
  Val& operator-=(const Val& x);
  Val& operator*=(const Val& x);
  Val& operator/=(const Val& x);
  Val operator-() const;
  Val& operator++();
  Val operator++(int);
  Val& operator--();
  Val operator--(int);
  Val pow(const Val& exponent);
  /// Infinity-safe addition
  Val plus(int x) const;
  /// Infinity-safe subtraction
  Val minus(int x) const;
};

}  // namespace MiniZinc
