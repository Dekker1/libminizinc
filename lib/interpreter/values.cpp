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
#include <minizinc/interpreter/primitives.hh>
#include <minizinc/interpreter/values.hh>

namespace MiniZinc {

std::tuple<std::vector<Val>, std::vector<Val>, Val> simplify_linexp(Val v) {
  std::vector<Val> coeffs = {Val(1)};
  std::vector<Val> vars = {v};
  Val d = 0;
  simplify_linexp(coeffs, vars, d);
  return {coeffs, vars, d};
};

void simplify_linexp(std::vector<Val>& coeffs, std::vector<Val>& vars, Val& d) {
  assert(coeffs.size() == vars.size());
  std::vector<std::pair<Val, Val>> defs;
  defs.reserve(vars.size());
  for (int j = vars.size() - 1; j >= 0; --j) {
    if (coeffs[j] != 0) {
      defs.emplace_back(coeffs[j], vars[j]);
    }
  }
  coeffs.clear();
  vars.clear();

  std::vector<int> idx;
  while (!defs.empty()) {
    Val coeff = defs.back().first;
    Val stacktop = Val::follow_alias(defs.back().second);
    defs.pop_back();
    if (coeff == 0) continue;
    if (stacktop.isInt()) {
      d += coeff * stacktop;
    } else {
      Variable* cur = stacktop.toVar();
      if (Constraint* defby = cur->defined_by()) {
        switch (defby->pred()) {
          case PrimitiveMap::INT_PLUS: {
            assert(stacktop == Val::follow_alias(defby->arg(2)));
            for (int i = 0; i < 2; ++i) {
              Val arg = Val::follow_alias(defby->arg(i));
              if (arg.isInt()) {
                d += coeff * arg;
              } else {
                defs.emplace_back(coeff, arg);
              }
            }
            continue;
          }
          case PrimitiveMap::INT_MINUS: {
            assert(stacktop == Val::follow_alias(defby->arg(2)));
            Val lhs = Val::follow_alias(defby->arg(0));
            if (lhs.isInt()) {
              d += coeff * lhs;
            } else {
              defs.emplace_back(coeff, lhs);
            }
            Val rhs = Val::follow_alias(defby->arg(1));
            if (rhs.isInt()) {
              d += coeff * -rhs;
            } else {
              defs.emplace_back(-coeff, rhs);
            }
            continue;
          }
          case PrimitiveMap::INT_SUM: {
            assert(stacktop == Val::follow_alias(defby->arg(1)));
            Val arr = defby->arg(0).toVec()->raw_data();
            for (int i = 0; i < arr.size(); ++i) {
              Val arg = Val::follow_alias(arr[i]);
              if (arg.isInt()) {
                d += coeff * arg;
              } else {
                defs.emplace_back(coeff, arg);
              }
            }
            continue;
          }
          case PrimitiveMap::INT_TIMES: {
            assert(stacktop == Val::follow_alias(defby->arg(2)));
            Val lhs = Val::follow_alias(defby->arg(0));
            Val rhs = Val::follow_alias(defby->arg(1));
            if (lhs.isInt()) {
              if (rhs.isInt()) {
                // both constants, compute result
                d += coeff * lhs * rhs;
              } else {
                defs.emplace_back(coeff * lhs, rhs);
              }
              continue;
            }
            if (rhs.isInt()) {
              defs.emplace_back(coeff * rhs, lhs);
              continue;
            }
            break;
          }
          default: {
          }
        }
      }
      if (coeff != 0) {
        coeffs.emplace_back(coeff);
        vars.emplace_back(cur);
        idx.push_back(idx.size());
      }
    }
  }

  if (coeffs.size() > 1) {
    // Find and merge duplicate variables
    class CmpValIdx {
    public:
      std::vector<Val>& x;
      explicit CmpValIdx(std::vector<Val>& x0) : x(x0) {}
      bool operator()(int i, int j) const { return x[i].timestamp() < x[j].timestamp(); }
    };
    std::sort(idx.begin(), idx.end(), CmpValIdx(vars));
    std::vector<Val> coeffs_simple;
    coeffs_simple.reserve(coeffs.size());
    std::vector<Val> vars_simple;
    vars_simple.reserve(vars.size());

    int ci = 0;
    coeffs_simple.push_back(coeffs[idx[0]]);
    vars_simple.push_back(vars[idx[0]]);
    bool foundDuplicates = false;
    for (unsigned int i = 1; i < idx.size(); i++) {
      if (vars[idx[i]].timestamp() == vars_simple[ci].timestamp()) {
        coeffs_simple[ci] += coeffs[idx[i]];
        foundDuplicates = true;
      } else {
        coeffs_simple.push_back(coeffs[idx[i]]);
        vars_simple.push_back(vars[idx[i]]);
        ci++;
      }
    }
    int j = 0;
    for (unsigned int i = 0; i < coeffs_simple.size(); i++) {
      if (coeffs_simple[i] != 0) {
        coeffs[j] = coeffs_simple[i];
        vars[j++] = vars_simple[i];
      }
    }
    coeffs.resize(j);
    vars.resize(j);
  }
}

std::string Val::toString(bool trim) const {
  std::ostringstream oss;
  Val v = follow_alias(*this);
  if (v.isInt()) {
    oss << v.toIntVal();
  } else if (v.isVar()) {
    if (v.timestamp() >= 0) {
      oss << "X" << timestamp() << "(";
    }
    oss << v.lb().toString() << "," << v.ub().toString();
    if (v.toVar()->timestamp() >= 0) {
      oss << ")";
    }
  } else {
    oss << "X" << v.toVec()->timestamp() << "[";
    if (!trim || v.size() <= 4) {
      for (size_t i = 0; i < v.size(); i++) {
        oss << v[i].toString(trim);
        if (i < v.size() - 1) oss << ",";
      }
    } else {
      oss << "<->";
    }
    oss << "]";
  }
  return oss.str();
}

// WARNING: Provide interpreter variable only if v is an uncopied reference
// with a strong reference count. The reference (and the reference count of
// the RCO) will be adjusted in case of an alias.
Val Val::follow_alias(const Val& v, Interpreter* interpreter) {
  if (v.isVar() && v.toVar()->aliased()) {
    Val nval = v;
    while (nval.isVar() && nval.toVar()->aliased()) {
      nval = nval.toVar()->alias();
    }
    if (interpreter && !interpreter->trail.is_trailed(v.toVar())) {
      Val& mut_v = const_cast<Val&>(v);
      nval.addRef(interpreter);
      mut_v.rmRef(interpreter);
      mut_v._v = nval._v;
    }
    return nval;
  } else {
    return v;
  }
}

Val Val::lb() const {
  if (isInt()) {
    return *this;
  } else if (isVec()) {
    // Assume it is a set (sorted Vec of integer ranges)
    assert(operator[](0).isInt());
    return operator[](0);
  } else {
    return toVar()->lb();
  }
}

Val Val::ub() const {
  if (isInt()) {
    return *this;
  } else if (isVec()) {
    // Assume it is a set (sorted Vec of integer ranges)
    assert(operator[](size() - 1).isInt());
    return operator[](size() - 1);
  } else {
    return toVar()->ub();
  }
}

void Val::finalizeLin(Interpreter* interpreter) {
  if (isInt()) {
    return;
  }
  if (isVar()) {
    Constraint* c = this->toVar()->defined_by();
    if (c && c->pred() <= PrimitiveMap::PARTIAL_LINEAR) {
      // Create linear equation
      std::vector<Val> coeffs = {Val(1)};
      std::vector<Val> vars = {*this};
      Val d = 0;
      simplify_linexp(coeffs, vars, d);

      Constraint* nc = nullptr;
      if (vars.empty()) {
        bool succes = this->toVar()->setVal(interpreter, d);
        assert(succes);
      } else if (c->pred() == PrimitiveMap::INT_TIMES && vars.size() == 1 && vars[0] == *this) {
        // Times operation between two variables. Just leave it as it is.
        return;
      } else {
        assert(std::none_of(vars.begin(), vars.end(), [&](Val v) { return v == *this; }));
        coeffs.push_back(Val(-1));
        vars.push_back(*this);

        Vec* ncoeffs = Vec::a(interpreter, interpreter->newIdent(), coeffs);
        ncoeffs->addRef(interpreter);
        Vec* nvars = Vec::a(interpreter, interpreter->newIdent(), vars);
        nvars->addRef(interpreter);

        std::vector<Val> args = {Val(ncoeffs), Val(nvars), Val(-d)};
        FixedKey<3> cse_key(*interpreter, args);
        BytecodeProc::Mode mode = BytecodeProc::ROOT;
        auto lookup = interpreter->cse_find(PrimitiveMap::INT_LIN_EQ, cse_key, mode);
        if (lookup.second) {
          // CSE Match found (perform cleanup)
          assert(lookup.first.isInt());
          cse_key.destroy(*interpreter);

          // This is the assumption that the linear expression doesn't exist
          // in negated form in the CSE. If this would happen then the state
          // should have been marked inconsistent.
          assert(lookup.first.toInt() == 1);
        } else {
          bool b;
          std::tie(nc, b) = Constraint::a(interpreter, PrimitiveMap::INT_LIN_EQ, mode, args);
          assert(nc || b);
          Val cse_val(1);
          interpreter->cse_insert(PrimitiveMap::INT_LIN_EQ, cse_key, mode, cse_val);
        }

        RefCountedObject::rmRef(interpreter, ncoeffs);
        RefCountedObject::rmRef(interpreter, nvars);
      }
      // Need to check whether variable still has a definition
      // (may have been aliased during the construction of nc)
      c = this->toVar()->defined_by();
      this->toVar()->_definitions.clear();
      if (nc) {
        this->toVar()->addDefinition(interpreter, nc);
      }

      if (c) {
        // still had a definition, so destroy it
        // FIXME: c->destroy will remove a non-existing reference to this.
        this->toVar()->addRef(interpreter);
        c->destroy(interpreter);
        ::free(c);
      }
    }
  } else {
    this->toVec()->finalizeLin(interpreter);
  }
}

}  // namespace MiniZinc
