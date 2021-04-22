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
#include <minizinc/interpreter/vector.hh>

namespace MiniZinc {

typedef std::vector<Val>::const_iterator arg_iter;

void simplify_linexp(std::vector<Val>& coeffs, std::vector<Val>& vars, Val& d);
std::tuple<std::vector<Val>, std::vector<Val>, Val> simplify_linexp(Val v);

inline Val::Val(const long long int i) {
  static const unsigned int pointerBits = sizeof(void*) * 8;
  static const long long int maxUnboxedVal =
      (static_cast<long long int>(1) << (pointerBits - 3)) - static_cast<long long int>(1);
  assert(i >= -maxUnboxedVal && i <= maxUnboxedVal);
  ptrdiff_t ubi_p;
  ubi_p = (static_cast<ptrdiff_t>(i < 0 ? -i : i) << 3);
  if (i < 0) {
    ubi_p = ubi_p | static_cast<ptrdiff_t>(4);
  }
  _v = reinterpret_cast<void*>(ubi_p);
}
inline Val::Val(const RefCountedObject* d) {
  assert(d != nullptr);
  _v = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(d) | static_cast<ptrdiff_t>(1));
}

inline Val::~Val(void) {}
inline Val::Val(const Val& v) : _v(v._v) {}
inline Val& Val::operator=(const Val& v) {
  _v = v._v;
  return *this;
}
inline Val& Val::operator=(Val&& v) {
  _v = v._v;
  v._v = nullptr;
  return *this;
}
inline Val::Val(Val&& v) : _v(v._v) { v._v = nullptr; }
inline void Val::assign(Interpreter* interpreter, const Val& v) {
  if (_v != v._v) {
    if (v.isRCO()) {
      v.toRCO()->addRef(interpreter);
    }
    rmRef(interpreter);
    _v = v._v;
  }
}
inline void Val::assign(Interpreter* interpreter, Val&& v) {
  if (_v != v._v) {
    rmRef(interpreter);
    _v = v._v;
    v._v = nullptr;
  }
}

inline const Val& Val::operator[](int i) const {
  assert(isVec());
  return (*toVec())[i];
}
/// Access value as vector, return size
inline size_t Val::size(void) const {
  assert(isVec());
  return toVec()->size();
}

inline bool operator==(const Val& lhs, const Val& rhs) {
  if ((reinterpret_cast<ptrdiff_t>(lhs._v) & static_cast<ptrdiff_t>(3)) !=
      (reinterpret_cast<ptrdiff_t>(rhs._v) & static_cast<ptrdiff_t>(3))) {
    return false;
  } else if (lhs.isVec() && rhs.isVec()) {
    return (*lhs.toVec()) == (*rhs.toVec());
  } else if (lhs.isInt() && rhs.isInt()) {
    return lhs.toInt() == rhs.toInt();
  } else {
    return reinterpret_cast<ptrdiff_t>(lhs._v) == reinterpret_cast<ptrdiff_t>(rhs._v);
  }
}

inline bool operator<=(const Val& x, const Val& y) {
  return y.isPlusInfinity() || x.isMinusInfinity() ||
         (x.isFinite() && y.isFinite() && x.toInt() <= y.toInt());
}
inline bool operator<(const Val& x, const Val& y) {
  return (y.isPlusInfinity() && !x.isPlusInfinity()) ||
         (x.isMinusInfinity() && !y.isMinusInfinity()) ||
         (x.isFinite() && y.isFinite() && x.toInt() < y.toInt());
}
inline bool operator>=(const Val& x, const Val& y) { return y <= x; }
inline bool operator>(const Val& x, const Val& y) { return y < x; }
inline bool operator!=(const Val& x, const Val& y) { return !(x == y); }

inline Val Val::operator-() const {
  Val r = *this;
  r._v = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(r._v) ^ static_cast<ptrdiff_t>(4));
  return r;
}
inline Val Val::infinity(void) {
  Val v;
  v._v = reinterpret_cast<void*>(static_cast<ptrdiff_t>(2));
  return v;
}
inline Val Val::fromIntVal(const IntVal& iv) {
  if (iv.isPlusInfinity()) return Val::infinity();
  if (iv.isMinusInfinity()) return -Val::infinity();
  return iv.toIntUnsafe();
}
inline bool Val::isFinite(void) const {
  assert(isInt());
  return ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(2)) == 0);
}
inline bool Val::isPlusInfinity(void) const {
  assert(isInt());
  return ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(6)) ==
          static_cast<ptrdiff_t>(2));
}
inline bool Val::isMinusInfinity(void) const {
  assert(isInt());
  return ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(6)) ==
          static_cast<ptrdiff_t>(6));
}

inline Val& Val::operator+=(const Val& x) {
  if (!(isFinite() && x.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safePlus(toInt(), x.toInt()));
  return *this;
}
inline Val& Val::operator-=(const Val& x) {
  if (!(isFinite() && x.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safeMinus(toInt(), x.toInt()));
  return *this;
}
inline Val& Val::operator*=(const Val& x) {
  if (!(isFinite() && x.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safeMult(toInt(), x.toInt()));
  return *this;
}
inline Val& Val::operator/=(const Val& x) {
  if (!(isFinite() && x.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safeDiv(toInt(), x.toInt()));
  return *this;
}
inline Val& Val::operator++() {
  if (!isFinite()) throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safePlus(toInt(), 1));
  return *this;
}
inline Val Val::operator++(int) {
  if (!isFinite()) throw ArithmeticError("arithmetic operation on infinite value");
  Val ret = *this;
  safeSetVal(safePlus(toInt(), 1));
  return ret;
}
inline Val& Val::operator--() {
  if (!isFinite()) throw ArithmeticError("arithmetic operation on infinite value");
  safeSetVal(safeMinus(toInt(), 1));
  return *this;
}
inline Val Val::operator--(int) {
  if (!isFinite()) throw ArithmeticError("arithmetic operation on infinite value");
  Val ret = *this;
  safeSetVal(safeMinus(toInt(), 1));
  return ret;
}
inline Val Val::pow(const Val& exponent) {
  if (!exponent.isFinite() || !isFinite())
    throw ArithmeticError("arithmetic operation on infinite value");
  if (exponent == 0) return 1;
  if (exponent == 1) return *this;
  Val result = 1;
  for (int i = 0; i < exponent.toInt(); i++) {
    result *= *this;
  }
  return result;
}
inline Val Val::plus(int x) const {
  if (isFinite())
    return safePlus(toInt(), x);
  else
    return *this;
}
inline Val Val::minus(int x) const {
  if (isFinite())
    return safeMinus(toInt(), x);
  else
    return *this;
}
/// Return whether an interval ending with \a x overlaps with an interval starting at \a y
inline bool overlaps(const Val& x, const Val& y) { return x.plus(1) >= y; }
inline Val nextHigher(const Val& x) { return x.plus(1); }
inline Val nextLower(const Val& x) { return x.minus(1); }
inline Val operator+(const Val& x, const Val& y) {
  if (!(x.isFinite() && y.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  return Val::safePlus(x.toInt(), y.toInt());
}
inline Val operator-(const Val& x, const Val& y) {
  if (!(x.isFinite() && y.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  return Val::safeMinus(x.toInt(), y.toInt());
}
inline Val operator*(const Val& x, const Val& y) {
  if (!x.isFinite()) {
    if (y.isFinite()) {
      if (y == 1) return x;
      if (y == -1) return -x;
    }
  } else if (!y.isFinite()) {
    if (x == 1) return y;
    if (x == -1) return -y;
  } else {
    return Val::safeMult(x.toInt(), y.toInt());
  }
  throw ArithmeticError("arithmetic operation on infinite value");
}
inline Val operator/(const Val& x, const Val& y) {
  if (y.isFinite()) {
    if (y == 1) return x;
    if (y == -1) return -x;
  }
  if (!(x.isFinite() && y.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  return Val::safeDiv(x.toInt(), y.toInt());
}
inline Val operator%(const Val& x, const Val& y) {
  if (!(x.isFinite() && y.isFinite()))
    throw ArithmeticError("arithmetic operation on infinite value");
  return Val::safeMod(x.toInt(), y.toInt());
}

inline Variable* Val::toVar(void) const {
  assert(isVar());
  return static_cast<Variable*>(toRCO());
}
inline Vec* Val::toVec(void) const {
  assert(isVec());
  return static_cast<Vec*>(toRCO());
}

/// Iterator over a Vec interpreted as a range set
class VecSetRanges {
  /// The vector
  const Vec* rs;
  /// The current range
  int n;

public:
  /// Constructor
  VecSetRanges(const Vec* r) : rs(r), n(0) {}
  /// Check if iterator is still valid
  bool operator()(void) const { return n + 1 < rs->size(); }
  /// Move to next range
  void operator++(void) { n += 2; }
  /// Return minimum of current range
  Val min(void) const { return (*rs)[n]; }
  /// Return maximum of current range
  Val max(void) const { return (*rs)[n + 1]; }
  /// Return width of current range
  Val width(void) const { return (*rs)[n + 1] - (*rs)[n] + 1; }
};

/// Iterator over a std::vector interpreted as a range set
class StdVecSetRanges {
  /// The vector
  const std::vector<Val>* rs;
  /// The current range
  int n;

public:
  /// Constructor
  StdVecSetRanges(const std::vector<Val>* r) : rs(r), n(0) {}
  /// Check if iterator is still valid
  bool operator()(void) const { return n + 1 < rs->size(); }
  /// Move to next range
  void operator++(void) { n += 2; }
  /// Return minimum of current range
  Val min(void) const { return (*rs)[n]; }
  /// Return maximum of current range
  Val max(void) const { return (*rs)[n + 1]; }
  /// Return width of current range
  Val width(void) const { return (*rs)[n + 1] - (*rs)[n] + 1; }
};

}  // namespace MiniZinc
