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

namespace MiniZinc {

namespace BytecodePrimitives {
IntPlus int_plus;
IntMinus int_minus;
IntSum int_sum;
IntTimes int_times;

IntLinEq int_lin_eq;
IntLinEqReif int_lin_eq_reif;
IntLinLe int_lin_le;
IntLinLeReif int_lin_le_reif;

MkIntVar mk_intvar;
BoolNot boolnot;
OpNot opnot;
Clause clause;
ClauseReif clause_reif;
Forall forall;
Exists exists;
Uniform uniform;
Sol sol;
Sort sort;
SortBy sortby;
IntMax intmax;
Infinity infinity;
InfiniteDomain inf_dom;
BooleanDomain bool_dom;
SliceXd slice_xd;
ArrayXd array_xd;
IndexSet index_set;

PrimitiveMap::Primitive* AllPrimitives[] = {
    &int_plus,    &int_minus,
    &int_sum,     &int_times,

    &int_lin_eq,  &int_lin_eq_reif,
    &int_lin_le,  &int_lin_le_reif,

    &mk_intvar,   &boolnot,
    &opnot,       &clause,
    &clause_reif, &forall,
    &exists,      &uniform,
    &sol,         &sort,
    &sortby,      &intmax,
    &infinity,    &inf_dom,
    &bool_dom,    &slice_xd,
    &array_xd,    &index_set,
};
}  // namespace BytecodePrimitives

PrimitiveMap::PrimitiveMap(void) : _p(PrimitiveMap::MAX_ID + 1) {
  for (PrimitiveMap::Primitive* p : BytecodePrimitives::AllPrimitives) {
    _p[p->ident()] = p;
  }
  for (Primitive* p : _p) {
    _s.insert(std::make_pair(p->name(), p));
  }
  _n.resize(_s.size());
  for (auto& entry : _s) {
    _n[entry.second->ident()] = entry.first;
  }
}

PrimitiveMap& primitiveMap(void) {
  static PrimitiveMap _pm;
  return _pm;
}

namespace BytecodePrimitives {

void Uniform::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 2);
  assert(args[0].isInt() && args[1].isInt());

  std::uniform_int_distribution<> dis(args[0].toInt(), args[1].toInt());
  Val rnd = dis(generator);
  i.pushAgg(rnd, -1);
}

void Sol::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 1);
  if (args[0].isVar()) {
    auto it = i.solutions.find(args[0].timestamp());
    assert(it != i.solutions.end());
    i.pushAgg(Val(it->second), -1);
  } else {
    assert(args[0].isInt());
    i.pushAgg(args[0], -1);
  }
};

void Sort::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 1);

  Val al = args[0].toVec()->raw_data();
  std::vector<int> ai(al.size());
  for (int j = 0; j < al.size(); j++) {
    ai[j] = al[j].toInt();
  }
  std::stable_sort(ai.begin(), ai.end());

  std::vector<Val> sorted(al.size());
  for (int j = 0; j < al.size(); j++) {
    sorted[j] = Val(ai[j]);
  }
  Vec* al_sorted = Vec::a(&i, i.newIdent(), sorted);

  i.pushAgg(Val(al_sorted), -1);
};

void SortBy::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 2);

  Val al = args[0].toVec()->raw_data();
  Val order_e = args[1].toVec()->raw_data();
  std::vector<Val> order(order_e.size());
  std::vector<int> a(order_e.size());
  for (int j = 0; j < order.size(); j++) {
    a[j] = j;
    order[j] = order_e[j];
  }
  struct Ord {
    std::vector<Val>& order;
    explicit Ord(std::vector<Val>& order0) : order(order0) {}
    bool operator()(int i, int j) { return order[i] < order[j]; }
  } _ord(order);
  std::stable_sort(a.begin(), a.end(), _ord);
  std::vector<Val> sorted(a.size());
  for (int j = sorted.size(); j--;) {
    sorted[j] = al[a[j]];
  }
  Vec* al_sorted = Vec::a(&i, i.newIdent(), sorted);

  i.pushAgg(Val(al_sorted), -1);
};

void Infinity::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 1);
  assert(args[0].isInt());

  if (args[0] > 0) {
    i.pushAgg(Val::infinity(), -1);
  } else {
    i.pushAgg(-Val::infinity(), -1);
  }
}

void InfiniteDomain::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 0);
  i.pushAgg(i.infinite_domain(), -1);
}

void BooleanDomain::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 0);
  i.pushAgg(i.boolean_domain(), -1);
};

void SliceXd::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 3);
  assert(args[0].isVec() && args[1].isVec() && args[2].isVec());

  Val content = args[0].toVec()->raw_data();
  Val selection = args[1].toVec()->raw_data();
  Val new_idxs = args[2].toVec()->raw_data();

  std::vector<Val> idxs(args[1].size());
  std::vector<Val> slice;
  // Initialise indexes to the index lower bound
  if (args[0].toVec()->hasIndexSet()) {
    Val index_set = args[0].toVec()->index_set();
    assert(index_set.size() / 2 == selection.size());
    for (int j = 0; j < idxs.size(); ++j) {
      idxs[j] = index_set[j * 2];
    }
  } else {
    assert(selection.size() == 1);
    idxs[0] = 1;
  }

  // Walk through array and make slice selection
  int level = idxs.size() - 1;
  int it = 0;
  while (level >= 0) {
    bool in_slice = true;
    for (int k = 0; k < idxs.size(); ++k) {
      assert(selection[k].size() == 2);
      in_slice = in_slice && selection[k][0] <= idxs[k] && idxs[k] <= selection[k][1];
    }

    assert(it < content.size());
    if (in_slice) {
      slice.push_back(content[it]);
    }
    it++;

    while (level >= 0) {
      if (args[0].toVec()->hasIndexSet()) {
        Val index_set = args[0].toVec()->index_set();
        if (idxs[level] < index_set[level * 2 + 1]) {
          idxs[level]++;
          level = idxs.size() - 1;
          break;
        } else {
          idxs[level] = index_set[level * 2];
          level--;
        }
      } else {
        assert(level == 0);
        if (idxs[0] < content.size()) {
          idxs[0]++;
          // No need to reset level, there is only one.
          break;
        } else {
          // Done (no need to reset index values).
          level--;
        }
      }
    }
  }

  // Format new index sets
  std::vector<Val> dom;
  dom.reserve(new_idxs.size() * 2);
  for (int j = 0; j < new_idxs.size(); ++j) {
    assert(new_idxs[j].size() == 2);
    dom.push_back(new_idxs[j][0]);
    dom.push_back(new_idxs[j][1]);
  }

  Vec* values = Vec::a(&i, i.newIdent(), slice);
  Vec* idx = Vec::a(&i, i.newIdent(), dom);
  Vec* nv = Vec::a(&i, i.newIdent(), {Val(values), Val(idx)}, true);

  i.pushAgg(Val(nv), -1);
}

void ArrayXd::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 2);
  assert(args[0].isVec());

  // Array Elements
  Val arr = args[0].toVec()->raw_data();

  if (args[1].isInt()) {
    assert(args[1].toInt() == 0);
    i.pushAgg(arr, -1);
    return;
  }

  assert(args[1].isVec());
  assert(args[1].size() % 2 == 0);
  // Check if the index sets actually match up
  int prod = 1;
  for (int i = 0; i < args[1].size(); i += 2) {
    prod *= (args[1][i + 1].toInt() - args[1][i].toInt() + 1);
  }
  if (arr.size() != prod) {
    throw std::runtime_error("ArrayXd cardinality mismatch");
  }

  Val ret = arr;
  if (args[1].size() != 0 && !(args[1].size() == 2 && args[1][0].toInt() == 1)) {
    ret = Val(Vec::a(&i, i.newIdent(), {arr, args[1]}, true));
  }
  i.pushAgg(ret, -1);
}

void IndexSet::execute(Interpreter& i, const std::vector<Val>& args) {
  assert(args.size() == 2);
  assert(args[0].isVec() && args[1].isInt());

  Val ret;
  if (!args[0].toVec()->hasIndexSet()) {
    assert(args[1].toInt() == 1 || args[1] == 0);
    ret = Val(Vec::a(&i, i.newIdent(), {Val(1), Val(args[0].size())}));
  } else if (args[1] == 0) {
    ret = args[0].toVec()->index_set();
  } else {
    assert(args[0].size() == 2);
    assert(args[0][1].isVec());
    assert(args[0][1].size() / 2 >= args[1].toInt());
    Val index_sets = args[0].toVec()->index_set();
    ret = Val(
        Vec::a(&i, i.newIdent(),
               {index_sets[(args[1].toInt() - 1) * 2], index_sets[(args[1].toInt() - 1) * 2 + 1]}));
  }
  i.pushAgg(ret, -1);
}

}  // namespace BytecodePrimitives
}  // namespace MiniZinc
