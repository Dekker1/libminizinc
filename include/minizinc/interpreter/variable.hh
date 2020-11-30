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

namespace MiniZinc {

class BytecodeProc;
class Constraint;
class Vec;

class Variable : public RefCountedObject {
  friend class Trail;
  friend class Interpreter;
  friend class Val;

public:
  enum SubscriptionEvent { SEV_VAL, SEV_UNIFY, SEV_DOM, SEV };
  /// Event sets propagators can subscribe to: only value events, value+unification, or any change
  enum SubscriptionEventSet { SES_VAL, SES_VALUNIFY, SES_ANY };
  typedef std::unordered_map<Constraint*, SubscriptionEventSet> Subscriptions;

protected:
  Variable* _prev;
  Variable* _next;
  Val _domain;  // could be the domain, or the variable or value this variable is aliased to
  Val _ann;
  Subscriptions _subscriptions;
  std::vector<Constraint*> _definitions;

  /// Whether domain is binding
  /// TODO: tag _domain pointer instead
  bool _binding;

  /// Whether variable is aliased
  /// TODO: tag _domain pointer instead
  bool _aliased;

  Variable(Interpreter* interpreter, Val domain, int ident);
  Variable(Interpreter* interpreter, Val domain, bool binding, int ident, Val ann);
  static Variable* createRoot(Interpreter* interpreter, Val domain, int ident) {
    Variable* v = static_cast<Variable*>(::malloc(sizeof(Variable)));
    return new (v) Variable(interpreter, domain, ident);
  }

public:
  bool aliased(void) const { return _aliased; }
  Vec* domain(void) const {
    assert(!aliased());
    return _domain.toVec();
  }
  Val lb() const {
    assert(!aliased());
    assert(_domain[0].isInt());
    return _domain[0];
  }
  Val ub() const {
    assert(!aliased());
    assert(_domain[_domain.size() - 1].isInt());
    return _domain[_domain.size() - 1];
  }
  bool isBounded() const { return lb().isFinite() && ub().isFinite(); }

  /// Set new minimum value included in the domain
  bool setMin(Interpreter* interpreter, Val i, bool binding = true);
  /// Set new maximum value included in the domain
  bool setMax(Interpreter* interpreter, Val i, bool binding = true);
  /// Restrict domain to a single value
  bool setVal(Interpreter* interpreter, Val i, bool binding = true);
  /// Intersect current domain with given domain
  bool intersectDom(Interpreter* interpreter, const std::vector<Val>& dom, bool binding = true);
  bool intersectDom(Interpreter* interpreter, Val dom, bool binding = true);
  /// Set domain to \a newDomain, schedule propagators
  void domain(Interpreter* interpreter, const Val& newDomain, bool binding);
  /// Set domain to \a newDomain, schedule propagators
  void domain(Interpreter* interpreter, const std::vector<Val>& newDomain, bool binding);
  Val ann(void) const { return _ann; }

  Constraint* defined_by() { return (_definitions.size() == 1) ? _definitions[0] : nullptr; }
  const std::vector<Constraint*>& definitions(void) const { return _definitions; }
  void addDefinition(Interpreter* interpreter, Constraint* c);

  static Variable* a(Interpreter* interpreter, Val domain, bool binding, int ident, Val ann = 0) {
    Variable* v = static_cast<Variable*>(::malloc(sizeof(Variable)));
    return new (v) Variable(interpreter, domain, binding, ident, ann);
  }
  static void free(Variable* var) {
    // INVARIANT: var->destroy() must be called before free(var);
    assert(var->_model_ref_count == 0 && var->_memory_ref_count == 0);
    for (auto c : var->_definitions) {
      ::free(c);
    }
    ::free(var);
  }
  ~Variable(void) = delete;
  /// Destroy and unlink this variable
  void destroy(Interpreter* interpreter);
  void reconstruct(Interpreter* interpreter);
  void alias(Interpreter* interpreter, Val v);
  void unalias(Interpreter* interpreter, Val dom);
  Val alias(void) {
    assert(aliased());
    return _domain;
  };
  Variable* prev(void) const { return _prev; }
  Variable* next(void) const { return _next; }

  static void dump(Variable* d, const std::vector<BytecodeProc>& bs, std::ostream& os);

  // Propagation interface
  /// Flag whether definition's domain is binding
  bool binding(void) const { return _binding == 1; }
  /// Set flag whether definition's domain is binding
  void binding(Interpreter* interpreter, bool f);
  /// Add \a c to set of subscribed constraints
  void subscribe(Constraint* d, const SubscriptionEventSet& events);
  /// Remove \a c from set of subscribed constraints
  void unsubscribe(Constraint* c);
};

}  // namespace MiniZinc
