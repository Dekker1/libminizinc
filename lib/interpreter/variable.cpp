/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/eval_par.hh>
#include <minizinc/interpreter.hh>
#include <minizinc/interpreter/values.hh>
#include <minizinc/interpreter/variable.hh>

namespace MiniZinc {

Variable::Variable(Interpreter* interpreter, Val domain, int ident)
    : RefCountedObject(RefCountedObject::VAR, ident),
      _prev(this),
      _next(this),
      _domain(domain),
      _binding(true),
      _aliased(false) {
  assert(_domain.isVec());
  _domain.addRef(interpreter);
  _ann.addRef(interpreter);
  addRef(interpreter);
}

Variable::Variable(Interpreter* interpreter, Val domain, bool binding, int ident, Val ann)
    : RefCountedObject(RefCountedObject::VAR, ident),
      _prev(this),
      _next(this),
      _domain(domain),
      _ann(ann),
      _binding(binding),
      _aliased(false) {
  assert(_domain.isVec());
  _domain.addRef(interpreter);
  _ann.addRef(interpreter);
  if (binding) addRef(interpreter);
  // insert into root variable list
  Variable* v = interpreter->root();
  interpreter->trail.trail_ptr(this, &_prev);
  _prev = v->_prev;
  interpreter->trail.trail_ptr(this, &_next);
  _next = v;
  interpreter->trail.trail_ptr(v->_prev, &v->_prev->_next);
  v->_prev->_next = this;
  interpreter->trail.trail_ptr(v, &v->_prev);
  v->_prev = this;
}

bool Variable::setMin(Interpreter* interpreter, Val i, bool binding) {
  assert(!aliased());
  assert(_domain.isVec());
  assert(_domain.size() % 2 == 0);
  size_t j = 0;
  while (j < _domain.size() && _domain[j] < i) {
    ++j;
  }
  if (j == 0) {
    return true;
  }
  if (j == _domain.size()) {
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
    ndom.addRef(interpreter);
    domain(interpreter, ndom, binding);
    ndom.rmRef(interpreter);
    return false;
  }
  std::vector<Val> dom;
  if (j % 2 == 1) {
    dom.emplace_back(i);
  }
  for (; j < _domain.size(); ++j) {
    dom.push_back(_domain[j]);
  }
  Val ndom(Vec::a(interpreter, interpreter->newIdent(), dom));
  ndom.addRef(interpreter);
  domain(interpreter, ndom, binding);
  ndom.rmRef(interpreter);
  return true;
}

bool Variable::setMax(Interpreter* interpreter, Val i, bool binding) {
  assert(!aliased());
  assert(_domain.isVec());
  assert(_domain.size() % 2 == 0);
  int j = _domain.size() - 1;
  while (j >= 0 && _domain[j] > i) {
    --j;
  }
  if (j == _domain.size() - 1) {
    return true;
  }
  if (j < 0) {
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
    ndom.addRef(interpreter);
    domain(interpreter, ndom, binding);
    ndom.rmRef(interpreter);
    return false;
  }
  std::vector<Val> dom;
  for (size_t k = 0; k <= j; ++k) {
    dom.push_back(_domain[k]);
  }
  if (j % 2 == 0) {
    dom.emplace_back(i);
  }
  Val ndom(Vec::a(interpreter, interpreter->newIdent(), dom));
  ndom.addRef(interpreter);
  domain(interpreter, ndom, binding);
  ndom.rmRef(interpreter);
  return true;
}

bool Variable::setVal(Interpreter* interpreter, Val i, bool binding) {
  assert(!aliased());
  assert(_domain.isVec());
  assert(_domain.size() % 2 == 0);
  for (int j = 0; j < _domain.size(); j += 2) {
    if (_domain[j] <= i && i <= _domain[j + 1]) {
      this->binding(interpreter, binding);
      alias(interpreter, i);
      return true;
    }
  }
  Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
  ndom.addRef(interpreter);
  domain(interpreter, ndom, binding);
  ndom.rmRef(interpreter);
  return false;
}

bool Variable::intersectDom(Interpreter* interpreter, const std::vector<Val>& dom, bool binding) {
  if (!isBounded()) {
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), dom));
    ndom.addRef(interpreter);
    domain(interpreter, ndom, binding);
    ndom.rmRef(interpreter);
    return true;
  }
  assert(!aliased());
  assert(_domain.isVec());
  assert(_domain.size() % 2 == 0);
  VecSetRanges vsr1(_domain.toVec());
  StdVecSetRanges vsr2(&dom);
  Ranges::Inter<Val, VecSetRanges, StdVecSetRanges> inter(vsr1, vsr2);
  std::vector<Val> result;
  for (; inter(); ++inter) {
    result.emplace_back(inter.min());
    result.emplace_back(inter.max());
  }
  Val ndom(Vec::a(interpreter, interpreter->newIdent(), result));
  ndom.addRef(interpreter);
  domain(interpreter, ndom, binding);
  ndom.rmRef(interpreter);
  return !result.empty();
}

bool Variable::intersectDom(Interpreter* interpreter, Val dom, bool binding) {
  assert(!dom.isVar());
  if (dom.isInt()) {
    return setVal(interpreter, dom);
  }
  // TODO: Allocation is not really necessary;
  std::vector<Val> vdom(dom.size());
  for (int i = 0; i < dom.size(); ++i) {
    vdom[i] = dom[i];
  }
  return intersectDom(interpreter, vdom, binding);
}

void Variable::domain(Interpreter* interpreter, const Val& newDomain, bool binding0) {
  assert(!aliased());
  assert(!newDomain.isRCO() || newDomain.toRCO()->alive());
  assert(_domain.isVec());
  interpreter->trail.trail_domain(interpreter, this, _domain.toVec());
  SubscriptionEvent sev;
  if (newDomain.isInt()) {
    alias(interpreter, newDomain);
    sev = SEV_VAL;
  } else if (newDomain.size() == 2 && newDomain[0] == newDomain[1]) {
    alias(interpreter, newDomain[0]);
    sev = SEV_VAL;
  } else {
    Val nd = newDomain;
    nd.addRef(interpreter);
    _domain.rmRef(interpreter);
    _domain = newDomain;
    sev = SEV_DOM;
  }
  for (auto& s : _subscriptions) {
    if (sev == SEV_VAL || s.second == SES_ANY) {
      interpreter->schedule(s.first, sev);
    }
  }
  binding(interpreter, binding0);
}
void Variable::domain(Interpreter* interpreter, const std::vector<Val>& newDomain, bool binding0) {
  if (!isBounded()) {
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), newDomain));
    ndom.addRef(interpreter);
    domain(interpreter, ndom, binding0);
    ndom.rmRef(interpreter);
  } else {
    bool did_update = false;
    if (newDomain.size() != _domain.size()) {
      did_update = true;
    } else {
      for (int i = 0; i < newDomain.size(); i++) {
        if (newDomain[i] != _domain[i]) {
          did_update = true;
          break;
        }
      }
    }
    if (did_update) {
      Val ndv(Vec::a(interpreter, interpreter->newIdent(), newDomain));
      ndv.addRef(interpreter);
      domain(interpreter, ndv, binding0);
      ndv.rmRef(interpreter);
    }
  }
}

void Variable::destroy(MiniZinc::Interpreter* interpreter) {
  _model_ref_count = (1u << 31u) - 1u;
  _domain.rmRef(interpreter);
  _ann.rmRef(interpreter);
  interpreter->trail.trail_ptr(_prev, &(_prev->_next));
  _prev->_next = _next;
  interpreter->trail.trail_ptr(_next, &(_next->_prev));
  _next->_prev = _prev;
  interpreter->trail.trail_ptr(this, &_next);
  _next = this;
  interpreter->trail.trail_ptr(this, &_prev);
  _prev = this;

  for (auto c : _definitions) {
    c->destroy(interpreter);
  }
  _model_ref_count = 0;
}

void Variable::reconstruct(Interpreter* interpreter) {
  /// TODO: what about subscriptions?
  assert(_model_ref_count == 0);
  _ann.addRef(interpreter);
  _domain.addRef(interpreter);
  for (auto c : _definitions) {
    c->reconstruct(interpreter);
  }
  _model_ref_count = 0;
}

void Variable::addDefinition(Interpreter* interpreter, Constraint* c) {
  _definitions.push_back(c);
  if (this != interpreter->root()) {
    // Remove reference counts for this variable from each argument in c
    for (int i = 0; i < c->size(); i++) {
      if (c->arg(i).isVar()) {
        if (c->arg(i).timestamp() == _timestamp) {
          RefCountedObject::rmRef(interpreter, this);
        }
      } else if (c->arg(i).isVec()) {
        Val content = c->arg(i).toVec()->raw_data();
        bool hasVar = false;
        for (int j = 0; j < content.size(); j++) {
          if (content[j].isVar() && content[j].timestamp() == _timestamp) {
            hasVar = true;
            break;
          }
        }
        if (hasVar) {
          if (!c->arg(i).unique() || !content.unique()) {
            // make vectors unique so that we can safely decrement reference counts
            std::vector<Val> vals(content.size());
            for (int i = 0; i < vals.size(); i++) {
              vals[i] = content[i];
            }
            Val v = Val(Vec::a(interpreter, interpreter->newIdent(), vals));
            if (c->arg(i).toVec()->hasIndexSet()) {
              v = Val(Vec::a(interpreter, interpreter->newIdent(),
                             {v, c->arg(i).toVec()->index_set()}));
            }
            c->arg(interpreter, i, Val(v));
          }
          for (int j = 0; j < content.size(); j++) {
            if (content[j].timestamp() == _timestamp) {
              RefCountedObject::rmRef(interpreter, this);
            }
          }
        }
      }
    }
  }
  if (c->delayed()) {
    interpreter->register_delayed(c, this);
  }
  interpreter->trail.trail_add_def(this, c);
}

void Variable::alias(Interpreter* interpreter, Val v) {
  assert(!_aliased);
  assert(!v.isVar() || v.toVar() != this);
  // Move defining constraints to current context

  for (Constraint* c : _definitions) {
    for (int i = 0; i < c->size(); ++i) {
      Val arg = c->arg(i);
      if (arg.isVec()) {
        _model_ref_count += arg.toVec()->count(Val(this));
      } else {
        _model_ref_count += (arg == Val(this));
      }
    }
    interpreter->trail.trail_rm_def(this, c);
    interpreter->pushConstraint(c);
  }
  _definitions.clear();

  // Destroy old domain
  _domain.rmRef(interpreter);
  _ann.rmRef(interpreter);
  _ann = 0;

  // Reset reference count for binding status
  // TODO: this may be incorrect if we are aliasing a variable with
  // TODO: binding domain to one with non-binding domain
  binding(interpreter, false);

  // Transfer subscriptions to new value and schedule propagators
  for (auto& s : _subscriptions) {
    if (v.isVar()) {
      v.toVar()->subscribe(s.first, s.second);
    }
    if (s.second == SES_VALUNIFY || s.second == SES_ANY) {
      interpreter->schedule(s.first, SEV_UNIFY);
    }
  }
  _subscriptions.clear();

  // Set Alias
  interpreter->trail.trail_alias(interpreter, this);
  _aliased = true;
  _domain = v;
  v.addRef(interpreter);
}

void Variable::unalias(Interpreter* interpreter, Val dom) {
  /// TODO!!!
  //    auto ref_count = _ref_count;
  //    _pred = proc;
  //    _size = size;
  //    _args[0].destroy(interpreter);
  //    _args[0] = arg0;
  //    for (int i = 0; i < _size; ++i) {
  //      _args[i].construct(interpreter);
  //    }
  //    // TODO: Transfer back subscriptions moved on aliasing?
  //    interpreter->subscribe(this);
  //    _ann.construct(interpreter);
  //    _domain->addRef(interpreter);
  //    _ref_count = ref_count;
}

void Variable::binding(Interpreter* interpreter, bool f) {
  if (!_binding && f) {
    addRef(interpreter);
  } else if (_binding && !f) {
    RefCountedObject::rmRef(interpreter, this);
  }
  _binding = f;
}

void Variable::subscribe(Constraint* c, const SubscriptionEventSet& events) {
  Variable* sub = this;
  while (sub && sub->aliased()) {
    if (sub->_domain.isVar()) {
      sub = sub->_domain.toVar();
    } else {
      sub = nullptr;
    }
  }
  if (sub) {
    sub->_subscriptions.insert(std::make_pair(c, events));
  }
}
void Variable::unsubscribe(Constraint* c) {
  Variable* sub = this;
  while (sub && sub->aliased()) {
    if (sub->_domain.isVar()) {
      sub = sub->_domain.toVar();
    } else {
      sub = nullptr;
    }
  }
  if (sub) {
    sub->_subscriptions.erase(c);
  }
}

void Variable::dump(Variable* head, const std::vector<BytecodeProc>& bs, std::ostream& os) {
  Variable* d = head;
  do {
    d = d->next();
    if (d != head) {
      if (d->timestamp() >= 0) {
        os << d->timestamp() << "(";
      }
      os << d << "." << d->_model_ref_count;
      if (d->timestamp() >= 0) {
        os << ")";
      }
      os << ":\t";
      if (d->aliased()) {
        os << " alias " << d->alias().toString() << "\n";
      } else {
        if (d->domain()) {
          if (d->_binding) {
            os << " binding";
          }
          os << " domain: " << Val(d->domain()).toString();
        }
      }
      os << "\n";
      if (!d->_subscriptions.empty()) {
        os << "    subscriptions: ";
        for (auto& s : d->_subscriptions) {
          os << s.first << " ";
        }
        os << "\n";
      }
    }
    for (Constraint* c : d->_definitions) {
      if (d != head) {
        os << "    ";
      }
      os << c << " " << bs[c->pred()].name << "(";
      for (int i = 0; i < c->size(); i++) {
        os << c->arg(i).toString();
        if (i < c->size() - 1) os << ", ";
      }
      os << ")\n";
    }
  } while (d != head);
}

}  // namespace MiniZinc
