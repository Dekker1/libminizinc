/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/bytecode.hh>
#include <minizinc/bytecode_primitives.hh>

#include <minizinc/model.hh>
#include <minizinc/iter.hh>

#include <minizinc/solver_instance_base.hh>

#include <iostream>
#include <unordered_map>
#include <fstream>
#include <streambuf>
#include <minizinc/eval_par.hh>

/* #define DBG_INTERPRETER(msg) std::cerr << msg */
#define DBG_INTERPRETER(msg) do {} while(0)
#define DBG_TRIM_OUTPUT true

namespace MiniZinc {

  Val
  AggregationCtx::createVec(Interpreter* interpreter, Definition* def, int timestamp) const {
    for (const Val& v : stack) {
      if (v.isDef() && v.toDef()->timestamp() >= def_ident_start) {
        // this is a new definition created during this aggregation and needs to be added to the hedge
        if (v.toDef()->prev() == v.toDef()) {
          if (def) {
            v.toDef()->insertBefore(interpreter, def);
          } else {
            def = v.toDef();
          }
        }
      }
    }
    return Val(Vec::a(interpreter, timestamp, stack));
  }

  Vec* Vec::allocate_array(Interpreter* interpreter, int timestamp, const std::vector<Val>& v) {
    Vec* values = a(interpreter, interpreter->newIdent(), v);
    Vec* idx = a(interpreter, interpreter->newIdent(), {IntVal(1), IntVal(v.size())});
    Vec* nv = a(interpreter, timestamp, {Val(values), Val(idx)});
    return nv;
  }

  bool
  Vec::isPar() const {
    for (int i = 0; i < this->size(); ++i) {
      Val v = this->operator[](i);
      if (v.isVec() && (!v.toVec()->isPar())) {
        return false;
      }
      if (v.isDef() && (!v.toDef()->domain().isInt())) {
        return false;
      }
    }
    return true;
  }

  void
  Definition::dump(Definition* head, const std::vector<BytecodeProc>& bs, std::ostream& os, int indent) {
    Definition* d = head->next();
    while (d != head) {
      for (unsigned int i=0; i<indent; i++) {
        os << "  ";
      }
      if (d->timestamp() >=0) {
        os << d->timestamp() << "(";
      }
      os << d << "." << d->_ref_count;
      if (d->timestamp() >=0) {
        os << ")";
      }
      os << ":\t";
      os << bs[d->pred()].name << "(";
      for (int i=0; i<d->size(); i++) {
        os << d->arg(i).toString();
        if (i<d->size()-1)
          os << ", ";
      }
      os << ")";
      os << " domain: " << d->domain().toString() << "\n";
      if (!d->subscriptions().empty()) {
        for (unsigned int i=0; i<indent; i++) {
          os << "  ";
        }
        os << "    subscriptions: ";
        for (auto& s : d->subscriptions()) {
          os << s.first << " ";
        }
        os << "\n";
      }
      if (d->defs())
        dump(d->defs(),bs,os,indent+2);
      d = d->next();
    }
  }

  Definition::Definition(Interpreter* interpreter,Val domain,bool binding,int pred,char mode,const std::vector<Val>& args,int ident,Val ann)
  : RefCountedObject(RefCountedObject::DEF,ident), _prev(this), _next(this),
  _domain(domain), _ann(ann), _defs(nullptr), _pred(pred), _size(args.size()), _flag(0), _binding(binding), _mode(mode) {
    _domain.construct(interpreter);
    _ann.construct(interpreter);
    for (unsigned int i=0; i<args.size(); i++) {
      new (&_args[i]) Val(args[i]);
      _args[i].construct(interpreter);
    }
    interpreter->subscribe(this);
    if (binding)
      addRef(interpreter);
  }

  void
  Definition::binding(Interpreter* interpreter, bool f) {
    if (!_binding && f) {
      addRef(interpreter);
    } else if (_binding && !f) {
      RefCountedObject::rmRef(interpreter, this);
    }
    _binding = f;
  }
  
  void Definition::destroy(MiniZinc::Interpreter* interpreter)  {
    _ref_count = (1u<<31u)-1u;
    if (_defs) {
      Definition* cur = _defs->next();
      while (cur != _defs) {
        Definition* nxt = cur->next();
        if (cur->_ref_count > 0) {
          // Promote cur to parent level
          cur->unlink(interpreter);
          cur->insertBefore(interpreter, this->prev());
        } else {
          cur->destroy(interpreter);
          if(cur->_weak_ref_count > 0) {
            // Cut cur: it is kept alive for a CSE entry
            cur->unlink(interpreter);
          } else if (!interpreter->trail.is_trailed(this)) {
            // Free cur: it will not be used again
            ::free(cur);
          }
        }
        cur = nxt;
      }
      if (_defs->next() == _defs) {
        if (!interpreter->trail.trail_ptr(this, &_defs)) {
          ::free(_defs);
        }
        _defs = nullptr;
      }
    }
    assert(_defs == nullptr || interpreter->trail.is_trailed(this));

    _domain.destroy(interpreter);
    _ann.destroy(interpreter);
    interpreter->unsubscribe(this);
    for (unsigned int i=0; i<_size; i++) {
      _args[i].destroy(interpreter);
    }
    _ref_count = 0;
    interpreter->trail.trail_ptr(_prev, &(_prev->_next));
    _prev->_next = _next;
    interpreter->trail.trail_ptr(_next, &(_next->_prev));
    _next->_prev = _prev;
  }

  void Definition::reconstruct(Interpreter* interpreter) {
    assert(_ref_count == 0);
    for (int i = 0; i < _size; ++i) {
      _args[i].construct(interpreter);
    }
    interpreter->subscribe(this);
    _ann.construct(interpreter);
    _domain.construct(interpreter);
    _ref_count = 0;
  }

  
  void Definition::insertBefore(Interpreter* interpreter, Definition* d) {
    assert(_prev==_next);
    interpreter->trail.trail_ptr(this, &_prev);
    _prev = d->_prev;
    interpreter->trail.trail_ptr(this, &_next);
    _next = d;
    interpreter->trail.trail_ptr(d->_prev, &d->_prev->_next);
    d->_prev->_next = this;
    interpreter->trail.trail_ptr(d, &d->_prev);
    d->_prev = this;
  }

  void Definition::appendBefore(Interpreter* interpreter, Definition* d) {
    Definition* e1 = _prev;
    Definition* e2 = d->_prev;
    interpreter->trail.trail_ptr(d, &d->_prev);
    d->_prev = e1;
    interpreter->trail.trail_ptr(e1, &e1->_next);
    e1->_next = d;
    interpreter->trail.trail_ptr(e2, &e2->_next);
    e2->_next = this;
    interpreter->trail.trail_ptr(this, &_prev);
    _prev = e2;
  }

  void Definition::unlink(Interpreter* interpreter) {
    interpreter->trail.trail_ptr(_prev, &_prev->_next);
    _prev->_next = _next;
    interpreter->trail.trail_ptr(_prev, &_next->_prev);
    _next->_prev = _prev;
    interpreter->trail.trail_ptr(this, &_next);
    _next = this;
    interpreter->trail.trail_ptr(this, &_prev);
    _prev = this;
  }

  std::map<const std::string, const std::string> negated_constraints = {
    {"int_eq", "int_ne"},
    {"int_le", "int_gt"},
    {"int_lt", "int_ge"},
    {"int_lin_eq", "int_lin_ne"},
    {"int_lin_le", "int_lin_gt"},
    {"int_lin_lt", "int_lin_ge"},
  };

  VarDecl* Definition::varDecl(Definition* d) {
    auto mode = static_cast<BytecodeProc::Mode>(d->mode());
    assert(mode != BytecodeProc::ROOT && mode != BytecodeProc::ROOT_NEG);

    Val dom = d->domain();
    SetLit* dom_set = nullptr;
    if (dom.isVec()) {
      assert(dom.size() >= 2 && dom.size() % 2 == 0);
      std::vector<IntSetVal::Range> ranges;
      for (int i = 0; i < dom.size(); i += 2) {
        ranges.emplace_back(dom[i](), dom[i+1]());
      }
      dom_set = new SetLit(Location().introduce(), IntSetVal::a(ranges));
    }
    auto ti = new TypeInst(Location().introduce(), Type::varint(), dom_set);
    auto vd = new VarDecl(Location().introduce(), ti, d->timestamp());
    vd->addAnnotation(constants().ann.output_var);

    return vd;
  }

  void Definition::toFZN(Definition* head, const std::vector<BytecodeProc>& bs, Model* model,
                         std::unordered_map<int, VarDecl*>& vdmap, Interpreter* interpreter) {
    GCLock lock;
    auto fzn = model ? model : new Model();
    if (head->next()==head)
      return;
    Definition* d = head->next(); // Ignore dummy head
    // Forward-declaration of current hedge variables
    while (d != head) {
      assert(d != d->next());
      if (d->pred() == 0) {
        d = d->next();
        continue;
      }
      auto mode = static_cast<BytecodeProc::Mode>(d->mode());
      if (mode != BytecodeProc::ROOT && mode != BytecodeProc::ROOT_NEG) {
        auto vd = varDecl(d);
        vdmap.emplace(d->timestamp(), vd);
      }
      d = d->next();
    }

    d = head->next(); // Ignore dummy head
    // Create FZNItems for current hedge
    while (d != head) {
      assert(d != d->next());
      if (d->pred() == 0) {
        d = d->next();
        continue;
      }
      toFZNItem(d, bs, fzn, vdmap, interpreter);
      if (d->defs()) {
        toFZN(d->defs(), bs, fzn, vdmap, interpreter);
      }
      d = d->next();
    }
  }

  void Definition::toFZNItem(Definition* d, const std::vector<BytecodeProc>& bs,
                             Model* model, std::unordered_map<int, VarDecl*>& vdmap, Interpreter* interpreter) {
    const BytecodeProc& proc = bs[d->pred()];
    auto mode = static_cast<BytecodeProc::Mode>(d->mode());
    std::string name = proc.name;
    if (BytecodeProc::is_neg(mode)) {
      auto it = negated_constraints.find(name);
      assert(it != negated_constraints.end());
      name = it->second;
      mode = BytecodeProc::negate(mode);
    }

    if (proc.name == "mk_intvar") {
      GCLock lock;
      auto vd = Definition::varDecl(d);
      vdmap.emplace(d->timestamp(), vd);
      auto vdi = new VarDeclI(Location().introduce(), vd);
      model->addItem(vdi);
    } else if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
      std::vector<Expression*> args(proc.nargs);
      for (int i = 0; i < proc.nargs; ++i) {
        Val v = Val::follow_alias(d->arg(i), interpreter);
        args[i] = v.toFZN(vdmap);
      }
      auto c = new Call(Location().introduce(), name, args);
      auto ci = new ConstraintI(Location().introduce(), c);
      model->addItem(ci);
    } else {
      auto vdit = vdmap.find(d->timestamp());
      assert(vdit != vdmap.end());
      model->addItem(new VarDeclI(Location().introduce(), vdit->second));
      auto ret = vdmap.emplace(d->timestamp(), vdit->second);

      std::vector<Expression*> args(proc.nargs + 1);
      for (int i = 0; i < proc.nargs; ++i) {
        Val v = Val::follow_alias(d->arg(i), interpreter);
        args[i] = v.toFZN(vdmap);
      }
      args.back() = Val(d).toFZN(vdmap);
      if (mode == BytecodeProc::FUN) {
        name += "_reif";
      } else {
        assert(mode == BytecodeProc::IMP);
        name += "_imp";
      }
      auto c = new Call(Location().introduce(), name, args);
      model->addItem(new ConstraintI(Location().introduce(), c));
    }
  }


  void Definition::addToSolver(Interpreter* interpreter, Definition* head,
                   const std::vector<BytecodeProc>& bs, SolverInstanceBase* si) {
    if (head->next()==head)
      return;
    Definition* d = head->next(); // Ignore dummy head
    while (d != head) {
      assert(d != d->next());
      if (d->pred() == 0) {
        d = d->next();
        continue;
      }
      si->addDefinition(bs, d);
      if (d->defs()) {
        addToSolver(interpreter, d->defs(), bs, si);
      }
      d = d->next();
    }
  }

  void Definition::alias(Interpreter* interpreter, Val v) {
    assert(size() >= 1);
    // Move constraints in _defs
    if (_defs) {
      Definition* cur = _defs->next();
      while (cur != _defs) {
        Definition* nxt = cur->next();
        for (int i = 0; i < cur->size(); ++i) {
          Val arg = cur->arg(i);
          if (arg.isVec()) {
            _ref_count += arg.toVec()->count(Val(this));
          } else {
            _ref_count += (arg == Val(this));
          }
        }
        cur->unlink(interpreter);
        cur->insertBefore(interpreter, this->next());
        cur = nxt;
      }
      if (!interpreter->trail.trail_ptr(this, &_defs)) {
        ::free(_defs);
      }
      _defs = nullptr;
    }
    assert(_defs == nullptr || interpreter->trail.is_trailed(this));

    // Destroy old definition
    _domain.destroy(interpreter);
    _domain = Val(IntVal(0));
    _ann.destroy(interpreter);
    _ann = Val(IntVal(0));
    for (unsigned int i=0; i<_size; i++) {
      _args[i].destroy(interpreter);
    }

    if (!_subscriptions.empty()) {
      // Transfer subscriptions to new value and schedule propagators
      Definition* nv = v.isDef() ? v.toDef() : nullptr;
      for (auto& s : _subscriptions) {
        if (nv) {
          nv->subscribe(s.first, s.second);
        }
        if (s.second==SES_VALUNIFY || s.second==SES_ANY) {
          interpreter->schedule(s.first, SEV_UNIFY);
        }
      }
    }
    
    // Set Alias
    interpreter->trail.trail_alias(interpreter, this);
    _pred = PrimitiveMap::ALIAS;
    _size = 1;
    _args[0] = v;
    v.construct(interpreter);
  }

  void Definition::unalias(Interpreter* interpreter, int proc, int size, const Val& arg0) {
    auto ref_count = _ref_count;
    _pred = proc;
    _size = size;
    _args[0].destroy(interpreter);
    _args[0] = arg0;
    for (int i = 0; i < _size; ++i) {
      _args[i].construct(interpreter);
    }
    // TODO: Transfer back subscriptions moved on aliasing?
    interpreter->subscribe(this);
    _ann.construct(interpreter);
    _domain.construct(interpreter);
    _ref_count = ref_count;
  }

  void
  Definition::subscribe(Definition* d, const SubscriptionEventSet& events) {
    Definition* sub = this;
    while (sub && sub->pred()==PrimitiveMap::ALIAS) {
      if (sub->arg(0).isDef()) {
        sub = sub->arg(0).toDef();
      } else {
        sub = nullptr;
      }
    }
    if (sub) {
      sub->_subscriptions.insert(std::make_pair(d,events));
    }
  }
  /// Remove \a d from set of subscribed constraints
  void
  Definition::unsubscribe(Definition* d) {
    Definition* sub = this;
    while (sub && sub->pred()==PrimitiveMap::ALIAS) {
      if (sub->arg(0).isDef()) {
        sub = sub->arg(0).toDef();
      } else {
        sub = nullptr;
      }
    }
    if (sub) {
      sub->_subscriptions.erase(d);
    }
  }

  void
  Definition::domain(Interpreter* interpreter, const Val& newDomain, bool binding0) {
    interpreter->trail.trail_domain(interpreter, this, _domain);
    binding(interpreter,binding0);
    _domain.destroy(interpreter);
    _domain = newDomain;
    _domain.construct(interpreter);
    if (_domain.isVec() && _domain.size() == 2 && _domain[0]() == _domain[1]()) {
      _domain.destroy(interpreter);
      _domain = _domain[0];
      _domain.construct(interpreter);
    }
    interpreter->schedule(this, isFixed() ? Definition::SEV_VAL : Definition::SEV_DOM);
    // TODO: This is currently not correct. We cannot just remove a constraint when its domain is fixed.
//    if (isFixed() && _pred != PrimitiveMap::MK_INTVAR) {
//      // This is a constrained expression, turn it into toplevel constraint
//
//      // Create new constraint
//      std::vector<Val> args(size());
//      for (unsigned int i=0; i<size(); i++) {
//        args[i] = arg(i);
//      }
//      Definition* nd = Definition::a(interpreter,newDomain,true,_pred,BytecodeProc::ROOT,args,-1);
//      interpreter->pushDef(nd);
//
//      // Turn this definition into a (fixed) variable
//      _pred = PrimitiveMap::MK_INTVAR;
//      binding(interpreter,false);
//      for (unsigned int i=0; i<_size; i++) {
//        _args[i].destroy(interpreter);
//        _args[i] = IntVal(0);
//      }
//      interpreter->unsubscribe(this);
//
//      // Promote hedge to parent
//      if (_defs) {
//        interpreter->pushDefs(_defs);
//        _defs = nullptr;
//      }
//    }
  }
  void
  Definition::domain(Interpreter* interpreter, const std::vector<Val>& newDomain, bool binding0) {
    if (!isBounded()) {
      domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), newDomain)), binding0);
    } else {
      assert(!isFixed());
      bool did_update = false;
      Vec* d = _domain.toVec();
      if (newDomain.size() != d->size()) {
        did_update = true;
      } else {
        for (int i=0; i<newDomain.size(); i++) {
          if (newDomain[i]() != (*d)[i]()) {
            did_update = true;
            break;
          }
        }
      }
      if (did_update) {
        domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), newDomain)), binding0);
      }
    }
  }

  bool Definition::setMin(Interpreter* interpreter, IntVal i) {
    if (isFixed()) {
      return lb() >= i;
    }
    assert(_domain.size() % 2 == 0);
    size_t j = 0;
    while (j < _domain.size() && _domain[j]() < i) {
      ++j;
    }
    if (j == 0) {
      return true;
    }
    if (j == _domain.size()) {
      domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), {})), false); // TODO: Is the domain binding when propagating??
      return false;
    }
    std::vector<Val> dom;
    if (j % 2 == 1) {
      dom.emplace_back(i);
    }
    for (; j < _domain.size(); ++j) {
      dom.push_back(_domain[j]);
    }
    domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), dom)), false); // TODO: Is the domain binding when propagating??
    return true;
  }

  bool Definition::setMax(Interpreter* interpreter, IntVal i) {
    if (isFixed()) {
      return ub() <= i;
    }
    assert(_domain.size() % 2 == 0);
    size_t j = _domain.size() - 1;
    while (j >= 0 && _domain[j]() > i) {
      --j;
    }
    if (j == _domain.size() - 1) {
      return true;
    }
    if (j < 0 ) {
      domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), {})), false); // TODO: Is the domain binding when propagating??
      return false;
    }
    std::vector<Val> dom;
    for (size_t k = 0; k <= j; ++j) {
      dom.push_back(_domain[j]);
    }
    if (j % 2 == 0) {
      dom.emplace_back(i);
    }
    domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), dom)), false); // TODO: Is the domain binding when propagating??
    return true;
  }

  bool Definition::setVal(Interpreter* interpreter, IntVal i) {
    if (isFixed()) {
      return i == _domain();
    }
    assert(_domain.size() % 2 == 0);
    for (int j = 0; j < _domain.size(); j+=2) {
      if (_domain[j]() <= i && i <= _domain[j+1]()) {
        domain(interpreter, Val(i), false); // TODO: Is the domain binding when propagating??
        return true;
      }
    }
    domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), {})), false); // TODO: Is the domain binding when propagating??
    return false;
  }

  bool Definition::intersectDom(Interpreter* interpreter, const std::vector<Val>& dom) {
    if (isFixed()) {
      assert(dom.size() % 2 == 0);
      for (int i = 0; i < dom.size(); i+=2) {
        if (dom[i]() <= _domain() && _domain() <= dom[i+1]()) {
          return true;
        }
      }
      return false;
    }
    if (!isBounded()) {
      domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), dom)), false);
      return true;
    }
    VecSetRanges vsr1(_domain.toVec());
    StdVecSetRanges vsr2(&dom);
    Ranges::Inter<IntVal,VecSetRanges,StdVecSetRanges> inter(vsr1,vsr2);
    std::vector<Val> result;
    for (; inter(); ++inter) {
      result.emplace_back(inter.min());
      result.emplace_back(inter.max());
    }
    domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), result)), false); // TODO: Is the domain binding when propagating??
    return !result.empty();
  }

  bool Definition::intersectDom(Interpreter* interpreter, Val dom) {
    assert(!dom.isDef());
    if (dom.isInt()) {
      return setVal(interpreter, dom());
    }
    // TODO: Allocation is not really necessary;
    std::vector<Val> vdom(dom.size());
    for (int i = 0; i < dom.size(); ++i) {
      vdom[i] = dom[i];
    }
    return intersectDom(interpreter, vdom);
  }

  std::tuple<std::vector<Val>, std::vector<Val>, IntVal> simplify_linexp(Val v) {
    std::vector<Val> coeffs = {Val(1)};
    std::vector<Val> vars = {v};
    IntVal d = 0;
    simplify_linexp(coeffs, vars, d);
    return {coeffs, vars, d};
  };

  void simplify_linexp(std::vector<Val>& coeffs, std::vector<Val>& vars, IntVal& d) {
    assert(coeffs.size() == vars.size());
    std::vector<std::pair<IntVal,Val>> defs;
    defs.reserve(vars.size());
    for (int j = vars.size()-1; j >= 0; --j) {
      defs.emplace_back(coeffs[j](), vars[j]);
    }
    coeffs.clear();
    vars.clear();

    std::vector<int> idx;
    while (!defs.empty()) {
      IntVal coeff = defs.back().first;
      Val stacktop = defs.back().second;
      defs.pop_back();
      if (stacktop.isInt()) {
        d += coeff*stacktop();
      } else {
        Definition* cur = stacktop.toDef();
        assert(cur->pred() == PrimitiveMap::MK_INTVAR);
        if (Definition* defby = cur->defined_by()) {
          switch (defby->pred()) {
            case PrimitiveMap::INT_LIN_EQ: {
              IntVal cur_coeff = 0;
              for (int i = defby->arg(1)[0].size() - 1; i >= 0; --i) {
                if (defby->arg(1)[0][i] == Val(cur)) {
                  cur_coeff += defby->arg(0)[0][i]();
                  break; // TODO: Can we assume no dumplicates?
                }
              }
              assert(cur_coeff != 0);
              if (std::abs(coeff) == std::abs(cur_coeff)) {
                IntVal mult = ((coeff > 0) == (cur_coeff > 0)) ? -1 : 1;
                for (int i = 0; i < defby->arg(0)[0].size(); i++) {
                  if (defby->arg(1)[0][i] != Val(cur)) {
                    defs.emplace_back(mult * defby->arg(0)[0][i](), defby->arg(1)[0][i]);
                  }
                }
                d += mult * -defby->arg(2)();
                continue;
              }
              if (std::abs(cur_coeff) == 1) {
                if (((coeff > 0) == (cur_coeff > 0))) {
                  coeff = -1 * coeff;
                }
                for (int i = 0; i < defby->arg(0)[0].size(); i++) {
                  if (defby->arg(1)[0][i] != Val(cur)) {
                    defs.emplace_back(coeff * defby->arg(0)[0][i](), defby->arg(1)[0][i]);
                  }
                }
                d += coeff * -defby->arg(2)();
                continue;
              }
              break;
            }
            case PrimitiveMap::INT_TIMES: {
              assert(Val(cur) == defby->arg(2));
              if (defby->arg(0).isInt()) {
                if (defby->arg(1).isInt()) {
                  // both constants, compute result
                  d += coeff * defby->arg(0)() * defby->arg(1)();
                } else {
                  defs.emplace_back(coeff * defby->arg(0)(), defby->arg(1));
                }
                continue;
              }
              if (defby->arg(1).isInt()) {
                defs.emplace_back(coeff * defby->arg(1)(), defby->arg(0));
                continue;
              }
              break;
            }
            default: {}
          }
        }
        coeffs.emplace_back(coeff);
        vars.emplace_back(cur);
        idx.push_back(idx.size());
      }
    }

    if (coeffs.size()>1) {
      // Find and merge duplicate variables
      class CmpValIdx {
      public:
        std::vector<Val>& x;
        explicit CmpValIdx(std::vector<Val>& x0) : x(x0) {}
        bool operator ()(int i, int j) const {
          return x[i].timestamp() < x[j].timestamp();
        }
      };
      std::sort(idx.begin(),idx.end(),CmpValIdx(vars));
      std::vector<IntVal> coeffs_simple;
      coeffs_simple.reserve(coeffs.size());
      std::vector<Val> vars_simple;
      vars_simple.reserve(vars.size());

      int ci=0;
      coeffs_simple.push_back(coeffs[idx[0]]());
      vars_simple.push_back(vars[idx[0]]);
      bool foundDuplicates = false;
      for (unsigned int i=1; i<idx.size(); i++) {
        if (vars[idx[i]].timestamp() == vars_simple[ci].timestamp()) {
          coeffs_simple[ci] += coeffs[idx[i]]();
          foundDuplicates = true;
        } else {
          coeffs_simple.push_back(coeffs[idx[i]]());
          vars_simple.push_back(vars[idx[i]]);
          ci++;
        }
      }
      if (foundDuplicates) {
        for (unsigned int i=0; i<coeffs_simple.size(); i++) {
          coeffs[i] = coeffs_simple[i];
          vars[i] = vars_simple[i];
        }
        coeffs.resize(coeffs_simple.size());
        vars.resize(vars_simple.size());
      }
    }
  }

  const std::string BytecodeProc::mode_to_string[] = { "RAW", "ROOT", "ROOT_NEG", "FUN", "FUN_NEG", "IMP", "IMP_NEG" };
  const std::string AggregationCtx::symbol_to_string[] = { "AND", "OR", "VEC", "OTHER" };

  const std::string Interpreter::status_to_string[] = {"Roger", "Aborted", "Inconsistent", "Error"};

  std::string
  Val::toString(bool trim) const {
    std::ostringstream oss;
    if (isInt()) {
      oss << (*this)();
    } else if (isDef()) {
      if (timestamp() >= 0) {
        oss << "X" << timestamp() << "(";
      }
      oss << toDef();
      if (toDef()->timestamp() >= 0) {
        oss << ")";
      }
    } else {
      oss << "X" << toVec()->timestamp() << "[";
      if (!trim || size() <= 4) {
        for (size_t i=0; i<size(); i++) {
          oss << (*this)[i].toString(trim);
          if (i<size()-1)
            oss << ",";
        }
      } else {
        oss << "<->";
      }
      oss << "]";
    }
    return oss.str();
  }

  bool Val::operator==(const Val &rhs) const {
    if ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(3)) != (reinterpret_cast<ptrdiff_t>(rhs._v) & static_cast<ptrdiff_t>(3))) {
      return false;
    } else if (isVec() && rhs.isVec()) {
      return (*toVec()) == (*rhs.toVec());
    } else {
      return reinterpret_cast<ptrdiff_t>(_v) == reinterpret_cast<ptrdiff_t>(rhs._v);
    }
  }

  Val Val::follow_alias(const Val& v, Interpreter* interpreter) {
    if (v.isDef() && v.toDef()->pred() == PrimitiveMap::ALIAS) {
      Val nval = v;
      while(nval.isDef() && nval.toDef()->pred() == PrimitiveMap::ALIAS) {
        assert(nval.toDef()->size() > 0);
        nval = nval.toDef()->arg(0);
      }
      if (interpreter && !interpreter->trail.is_trailed(v.toDef())) {
        Val& mut_v = const_cast<Val&>(v);
        mut_v.destroy(interpreter);
        mut_v._v = nval._v;
        mut_v.construct(interpreter);
      }
      return nval;
    } else {
      return v;
    }
  }

  IntVal Val::lb() const {
    if (isInt()) {
      return operator()();
    } else if (isVec()) {
      // Assume it is a set (sorted Vec of integer ranges)
      assert(operator[](0).isInt());
      return operator[](0)();
    } else {
      assert(isDef());
      return toDef()->lb();
    }
  }

  IntVal Val::ub() const {
    if (isInt()) {
      return operator()();
    } else if (isVec()) {
      // Assume it is a set (sorted Vec of integer ranges)
      assert(operator[](size()-1).isInt());
      return operator[](size()-1)();
    } else {
      assert(isDef());
      return toDef()->ub();
    }
  }

  bool Val::isFixed() const {
    if (isInt()) {
      return true;
    } else {
      assert(isDef());
      return toDef()->isFixed();
    }
  }

  CSETable::Key::Key(const std::vector<Val> &vec) {
    _size = vec.size();
    // TODO: Should CSEKeys compare arrays with the same content again?
//    for (const auto& val : vec) {
//      if (val.isVec() && val.size() <= 3) {
//        _size += val.size();
//      }
//    }
    if (_size > 0) {
     _vals = (WeakVal*) malloc(_size*sizeof(WeakVal));
      size_t i = 0;
      for (const auto& val : vec) {
//        if (val.isVec() && val.size() <= 3) {
//          _vals[i++] = WeakVal(Val(val.size()));
//          for (int j = 0; j < val.size(); ++j) {
//            assert(!val[j].isVec());
//            _vals[i++] = WeakVal(val[j]);
//          }
//        } else {
          _vals[i++] = WeakVal(val);
//        }
      }
      assert(i == _size);
    }
  }

  std::pair<Val, bool> CSETable::lookup(Interpreter* interpreter, const Key& key, BytecodeProc::Mode& mode) {
    assert(mode != BytecodeProc::RAW);
    iterator it;
    size_t i = _table.size();
    do {
      i--;
      it = _table[i].find(key);
    } while (it == _table[i].end() && i > 0);
    if (it != _table[i].end()) {
      Val val = Val::follow_alias(it->second.second, interpreter);
      BytecodeProc::Mode val_m = it->second.first;
      if (!val.exists()) {
        this->_table[i].erase(it);
        return {Val(), false};
      }
      DBG_INTERPRETER("--- CSE hit! hash(" << key.hash() << ") -> Mode: " << BytecodeProc::mode_to_string[val_m] << " Value: " << val.toString(DBG_TRIM_OUTPUT) << "\n");
      auto convert = [&interpreter, val_m, mode](Val v) {
        assert(!v.isVec());
        if (BytecodeProc::is_neg(mode) != BytecodeProc::is_neg(val_m)) {
          Val new_val;
          if (v.isInt()) {
            assert(v().toInt() == 0 || v().toInt() == 1);
            new_val = Val(1 - v().toInt());
          } else {
            Key nkey({v});
            bool found;
            auto cmode = BytecodeProc::FUN;
            std::tie(new_val, found) = interpreter->cse_lookup(PrimitiveMap::BOOLNOT, nkey, cmode);
            if (!found) {
              auto d = Definition::a(interpreter, interpreter->boolean_domain(), false, PrimitiveMap::BOOLNOT, BytecodeProc::FUN, {v}, interpreter->newIdent());
              interpreter->pushDef(d);
              new_val = Val(d);
              interpreter->cse_insert(PrimitiveMap::BOOLNOT, nkey, cmode, new_val);
            } else {
              nkey.destroy();
            }
          }
          return new_val;
        }
        return v;
      };
      if (mode == val_m){
        return std::make_pair(val, true);
      // Assumption: 'val' must be of boolean type, otherwise mode is always FUN (or RAW)
      } else if (val_m == BytecodeProc::ROOT || val_m == BytecodeProc::ROOT_NEG) {
        return {convert(val), true};
      } else if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        DBG_INTERPRETER("--- Run call in " + BytecodeProc::mode_to_string[mode] + " context\n");
        return {Val(), false};
      } else if (val_m == BytecodeProc::IMP || val_m == BytecodeProc::IMP_NEG) {
        mode = BytecodeProc::FUN;
        DBG_INTERPRETER("--- Run call in " + BytecodeProc::mode_to_string[mode] + " context\n");
        return {Val(), false};
      } else {
        return {convert(val), true};
      }
    }
    return {Val(), false};
  }

  void CSETable::insert(Interpreter* interpreter, Key& key, const BytecodeProc::Mode& mode, Val& val) {
    assert(mode != BytecodeProc::RAW);
    DBG_INTERPRETER("--- CSE add: hash(" << key.hash() << ") -> Mode: " << BytecodeProc::mode_to_string[mode] << " Value: " << val.toString(DBG_TRIM_OUTPUT) << "\n");
    // If value is reference counted, flag that it's in CSE
    val.addWeakRef(interpreter);
    auto insertion = _table.back().emplace(key, std::make_pair(mode, val));
    if (!insertion.second) {
      CSETable::iterator& it = insertion.first;
      // We are replacing another entry within the CSE table.
      assert(it->first == key && it->second.first != mode);
      it->first.destroy();
      Val& oldVal = it->second.second;
      BytecodeProc::Mode& oldMode = it->second.first;
      if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        if (oldVal.isDef()) {
          Definition* d = oldVal.toDef();
          d->alias(interpreter, BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode) ? Val(IntVal(1)) : Val(IntVal(0)));
        }
      } else if (mode == BytecodeProc::FUN || mode == BytecodeProc::FUN_NEG) {
        if (oldVal.isDef()) {
          Definition* d = oldVal.toDef();
          if (BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode)) {
            d->alias(interpreter, val);
          } else {
            Key nkey({val});
            bool found;
            auto cmode = BytecodeProc::FUN;
            Val new_val;
            std::tie(new_val, found) = interpreter->cse_lookup(PrimitiveMap::BOOLNOT, nkey, cmode);
            if (!found) {
              auto negation = Definition::a(interpreter, interpreter->boolean_domain(), false, PrimitiveMap::BOOLNOT, BytecodeProc::FUN, {val}, interpreter->newIdent());
              interpreter->pushDef(negation);
              new_val = Val(negation);
              interpreter->cse_insert(PrimitiveMap::BOOLNOT, nkey, cmode, new_val);
            } else {
              nkey.destroy();
            }
            d->alias(interpreter, new_val);
          }
        }
      }
      oldVal.removeWeakRef(interpreter);
      it->second = std::make_pair(mode, val);
    }
  }

  std::string
  BytecodeStream::toString(const std::vector<BytecodeProc>& procs) const {
    std::ostringstream oss;
    int pc = 0;
    while (pc < _bs.size()) {
      switch (instr(pc)) {
        case BytecodeStream::ADDI:
        {
          oss << "ADDI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::SUBI:
        {
          oss << "SUBI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::MULI:
        {
          oss << "MULI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DIVI:
        {
          oss << "DIVI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::MODI:
        {
          oss << "MODI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::INCI:
        {
          oss << "INCI R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DECI:
        {
          oss << "DECI R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::IMMI:
        {
          oss << "IMMI " << intval(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::LOAD_GLOBAL:
        {
          oss << "LOAD_GLOBAL " << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::STORE_GLOBAL:
        {
          oss << "STORE_GLOBAL R" << reg(pc) << " " << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::MOV:
        {
          oss << "MOV R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::JMP:
        {
          oss << "JMP " << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::JMPIF:
        {
          oss << "JMPIF R" << reg(pc) << " " << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::JMPIFNOT:
        {
          oss << "JMPIFNOT R" << reg(pc) << " " << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::EQI:
        {
          oss << "EQI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::LTI:
        {
          oss << "LTI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::LEI:
        {
          oss << "LEI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::AND:
        {
          oss << "AND R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::OR:
        {
          oss << "OR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::NOT:
        {
          oss << "NOT R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::XOR:
        {
          oss << "XOR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ISPAR:
        {
          oss << "ISPAR R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ISEMPTY:
        {
          oss << "ISEMPTY R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::LENGTH:
        {
          oss << "LENGTH R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          oss << "GET_VEC R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::LB:
        {
          oss << "LB R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::UB:
        {
          oss << "UB R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DOM:
        {
          oss << "DOM R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::MAKE_SET:
        {
          oss << "MAKE_SET R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::INTERSECTION:
        {
          oss << "INTERSECTION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::UNION:
        {
          oss << "UNION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DIFF:
        {
          oss << "DIFF R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::INTERSECT_DOMAIN:
        {
          oss << "INTERSECT_DOMAIN R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::RET:
        {
          oss << "RET\n";
        }
          break;
        case BytecodeStream::CALL:
        {
          auto m = static_cast<BytecodeProc::Mode>(chr(pc));
          int p = reg(pc);
          assert(!procs.empty());
          oss << "CALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name << " ";
          oss << procs[p].nargs;
          for (int i=0; i<procs[p].nargs; i++) {
            oss << " R" << reg(pc);
          }
          oss << "\n";
        }
          break;
        case BytecodeStream::BUILTIN:
        {
          int p = reg(pc);
          oss << "BUILTIN " << p << " ";
          oss << procs[p].nargs;
          for (int i=0; i<procs[p].nargs; i++) {
            oss << " R" << reg(pc);
          }
          oss << "\n";
        }
          break;
        case BytecodeStream::TCALL:
        {
          auto m = static_cast<BytecodeProc::Mode>(chr(pc));
          int p = reg(pc);
          
          if (procs.empty()) {
            oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << p << "\n";
          } else {
            oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name << "\n";
          }
        }
          break;
        case BytecodeStream::OPEN_AGGREGATION:
        {
          oss << "OPEN_AGGREGATION ";
          int p = chr(pc);
          switch (p) {
            case AggregationCtx::VCTX_AND:
              oss << "AND\n";
              break;
            case AggregationCtx::VCTX_OR:
              oss << "OR\n";
              break;
            case AggregationCtx::VCTX_VEC:
              oss << "VEC\n";
              break;
            case AggregationCtx::VCTX_OTHER:
              oss << "OTHER\n";
              break;
            default:
              oss << "ERROR\n";
              assert(false);
              break;
          }
        }
          break;
        case BytecodeStream::CLOSE_AGGREGATION:
        {
          oss << "CLOSE_AGGREGATION\n";
        }
          break;
        case BytecodeStream::SIMPLIFY_LIN:
        {
          oss << "SIMPLIFY_LIN R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::PUSH:
        {
          oss << "PUSH R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::POP:
        {
          oss << "POP R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::POST:
        {
          oss << "POST R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::TRACE:
        {
          oss << "TRACE R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ABORT:
        {
          oss << "ABORT\n";
        }
          break;
      }
    }
    return oss.str();
  }
  
  void
  Interpreter::pushAgg(const Val& v, int stackOffset) {
    assert(stackOffset < 0);
    assert(_agg.size()+stackOffset >= 0);
    // push value onto surrounding context
    _agg[_agg.size()+stackOffset].push(this,v);
  }
  
  void
  Interpreter::pushDef(Definition* d) {
    d->insertBefore(this, _agg.back().def_stack);
  }
  
  void
  Interpreter::pushDefs(Definition* defs) {
    defs->appendBefore(this, _agg[_agg.size()-1].def_stack);
  }
  
  void
  Interpreter::subscribe(Definition* d) {
    if (d->pred() < primitiveMap().size()) {
      primitiveMap()[d->pred()]->subscribe(*this, d);
    }
  }
  void
  Interpreter::unsubscribe(Definition* d) {
    if (d->pred() < primitiveMap().size()) {
      primitiveMap()[d->pred()]->unsubscribe(*this, d);
    }
  }
  void
  Interpreter::schedule(Definition* d, const Definition::SubscriptionEvent& ev) {
    if (!d->flag()) {
      _propQueue.push_back(d);
      d->flag(true);
    }
  }
  void
  Interpreter::deschedule(Definition* d) {
    if (d->flag()) {
      auto it = std::find(_propQueue.begin(), _propQueue.end(), d);
      if (it != _propQueue.end()) {
        _propQueue.erase(it);
      }
    }
  }
  void
  Interpreter::propagate(void) {
    while (!_propQueue.empty()) {
      Definition* d = _propQueue.front();
      _propQueue.pop_front();
      d->flag(false);
      auto ps = primitiveMap()[d->pred()]->propagate(*this, d);
      switch (ps) {
        case PrimitiveMap::Primitive::PS_OK:
          break;
        case PrimitiveMap::Primitive::PS_FAILED:
          _status = INCONSISTENT;
          return;
        case PrimitiveMap::Primitive::PS_ENTAILED:
          // TODO: remove definition
          break;
      }
    }
  }
  
  void Interpreter::run(void) {
    if (_status != ROGER) {
      return;
    }
    BytecodeFrame* frame = &_stack.back();
    for (;;) {
      DBG_INTERPRETER(frame->pc << " ");
      switch (frame->bs->instr(frame->pc)) {
        case BytecodeStream::ADDI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("ADDI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, frame->reg[r1]() + frame->reg[r2]());
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::SUBI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("SUBI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, frame->reg[r1]() - frame->reg[r2]());
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::MULI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("MULI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, frame->reg[r1]() * frame->reg[r2]());
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::DIVI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          if(frame->reg[r2]() == 0)
            frame->pc = frame->bs->size()-1;
          else
            frame->reg.assign(this, r3, frame->reg[r1]() / frame->reg[r2]());
          frame->reg.assign(this, r3, frame->reg[r1]() / frame->reg[r2]());
          DBG_INTERPRETER("DIVI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::MODI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          if(frame->reg[r2]() == 0)
            frame->pc = frame->bs->size()-1;
          else
            frame->reg.assign(this, r3, frame->reg[r1]() % frame->reg[r2]());
          DBG_INTERPRETER("MODI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::INCI:
        {
          int r1 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r1, frame->reg[r1]()+1);
          DBG_INTERPRETER("INCI R" << r1 << "(" << frame->reg[r1]() << ")" << "\n");
        }
          break;
        case BytecodeStream::DECI:
        {
          int r1 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("DECI R" << r1 << "(" << frame->reg[r1]() << ")" << "\n");
          frame->reg.assign(this, r1, frame->reg[r1]()-1);
        }
          break;
        case BytecodeStream::IMMI:
        {
          IntVal i = frame->bs->intval(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r1, i);
          DBG_INTERPRETER("IMMI " << i << " R" << r1 << "(" << frame->reg[r1]() << ")" << "\n");
        }
          break;
        case BytecodeStream::LOAD_GLOBAL:
        {
          int i = frame->bs->reg(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          globals.cp(this, i, frame->reg, r1);
          DBG_INTERPRETER("LOAD_GLOBAL " << i << " R" << r1 << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")" << "\n");
        }
          break;
        case BytecodeStream::STORE_GLOBAL:
        {
          int r1 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          frame->reg.cp(this, r1, globals, i);
          DBG_INTERPRETER("STORE_GLOBAL R" << r1 << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")" << " " << i << "\n");
        }
          break;
        case BytecodeStream::MOV:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("MOV R" << r1 << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")" );
          frame->reg.cp(this, r1,r2);
          DBG_INTERPRETER(" R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")"  << "\n");
        }
          break;
        case BytecodeStream::JMP:
        {
          int i = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("JMP " << i  << "\n");
          frame->pc = i;
        }
          break;
        case BytecodeStream::JMPIF:
        {
          int r0 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("JMPIF R" << r0 << "(" << frame->reg[r0]() << ")" << " " << i << "\n");
          if (frame->reg[r0]() != 0) {
            frame->pc = i;
          }
        }
          break;
        case BytecodeStream::JMPIFNOT:
        {
          int r0 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("JMPIFNOT R" << r0 << "(" << frame->reg[r0]() << ")" << " " << i << "\n");
          if (frame->reg[r0]() == 0) {
            frame->pc = i;
          }
        }
          break;
        case BytecodeStream::EQI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("EQI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() == frame->reg[r2]()));
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LTI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LTI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() < frame->reg[r2]()));
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LEI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LEI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() <= frame->reg[r2]()));
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::AND:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("AND R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]()!=0 && frame->reg[r2]()!=0));
          DBG_INTERPRETER(" " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::OR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("OR R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]()!=0 || frame->reg[r2]()!=0));
          DBG_INTERPRETER(" " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::NOT:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("NOT R" << r1);
          frame->reg.assign(this, r2, IntVal(frame->reg[r1]()==0));
          DBG_INTERPRETER(" R" << r2 << "(" << frame->reg[r2]() << ")" << "\n");
        }
          break;
        case BytecodeStream::XOR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("XOR R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(this, r3, IntVal( (frame->reg[r1]()!=0) ^ (frame->reg[r2]()!=0)));
          DBG_INTERPRETER(" R" << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISPAR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("ISPAR R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          if (v.isInt()) {
            frame->reg.assign(this, r2, IntVal(1));
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isInt()) {
              frame->reg.assign(this, r1, def->lb());
              frame->reg.assign(this, r2, IntVal(1));
            } else {
              frame->reg.assign(this, r2, IntVal(0));
            }
          } else {
            assert(v.isVec());
            IntVal ret = IntVal(v.toVec()->isPar());
            frame->reg.assign(this, r2, ret);
          }
          DBG_INTERPRETER(" R" << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISEMPTY:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("ISEMPTY R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          assert(frame->reg[r1].isVec());
          frame->reg.assign(this, r2, IntVal(frame->reg[r1].size()==0));
          DBG_INTERPRETER(" R" << r2  << "(" << frame->reg[r2]() << ")" << "\n");
        }
          break;
        case BytecodeStream::LENGTH:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LENGTH R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          assert(frame->reg[r1].isVec());
          frame->reg.assign(this, r2, IntVal(frame->reg[r1].size()));
          DBG_INTERPRETER(" R" << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("GET_VEC R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          assert(frame->reg[r1].isVec());
          assert(frame->reg[r2].isInt());
          assert(frame->reg[r2]() > 0 && frame->reg[r2]() <= frame->reg[r1].size());
          Val v = Val::follow_alias(frame->reg[r1][frame->reg[r2]().toInt()-1], this);
          frame->reg.assign(this, r3, v);
          DBG_INTERPRETER(" R" << r3 <<  "(" << v.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LB:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LB R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          if (v.isInt()) {
            frame->reg.assign(this, r2, v);
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->isBounded()) {
              Val lb(def->lb());
              frame->reg.assign(this, r2, lb);
              DBG_INTERPRETER(" R" << r2 <<  "(" << lb.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
            } else {
              std::cerr << "PROPAGATE " << _procs[v.toDef()->pred()].name << "!\n";
              std::cerr << v.toString() << " with domain " << v.toDef()->domain().toString() << std::endl;
              throw Error("Error: lb on unbounded variable");
            }
          } else {
            throw Error("Error: lb on invalid type");
          }
        }
          break;
        case BytecodeStream::UB:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("UB R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          if (v.isInt()) {
            frame->reg.assign(this, r2, v);
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->isBounded()) {
              Val ub(def->ub());
              frame->reg.assign(this, r2, ub);
              DBG_INTERPRETER(" R" << r2 <<  "(" << ub.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
            } else {
              throw Error("Error: ub on unbounded variable");
            }
          } else {
            throw Error("Error: ub on invalid type");
          }
        }
          break;
        case BytecodeStream::DOM:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("DOM R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          if (v.isInt()) {
            frame->reg.assign(this, r2, Val(Vec::a(this, newIdent(), {v,v})));
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isVec()) {
              frame->reg.assign(this, r2, def->domain());
              DBG_INTERPRETER(" R" << r2 <<  "(" << def->domain().toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
            } else {
              throw Error("Error: dom on unbounded variable");
            }
          } else {
            throw Error("Error: dom on invalid type");
          }
        }
          break;
        case BytecodeStream::MAKE_SET:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("MAKE_SET R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v1 = Val::follow_alias(frame->reg[r1], this);
          assert(frame->reg[r1].isVec());
          Vec* a1 = v1.toVec();

          std::vector<Val> result;
          if(a1->size() > 0) {
            std::vector<IntVal> vals(a1->size());
            for (int i=0; i<a1->size(); i++)
              vals[i] = (*a1)[i]();

            std::sort(vals.begin(), vals.end());
            IntVal l(vals[0]);
            IntVal u(vals[0]);
            for(int i = 1; i < vals.size(); ++i) {
              if(u+1 < vals[i]) {
                result.emplace_back(l);
                result.emplace_back(u);
                l = vals[i];
              }
              u = vals[i];
            }
            result.emplace_back(l);
            result.emplace_back(u);
          }
          Val result_val(Vec::a(this, newIdent(), result));
          frame->reg.assign(this, r2, result_val);
          DBG_INTERPRETER(" R" << r2 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::INTERSECTION:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("INTERSECTION R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ") R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          Val v1 = Val::follow_alias(frame->reg[r1], this);
          Val v2 = Val::follow_alias(frame->reg[r2], this);
          Val result_val;
          if (v1.isInt()) {
            result_val = v2;
          } else if (v2.isInt()) {
            result_val = v1;
          } else {
            Vec* s1 = v1.toVec();
            Vec* s2 = v2.toVec();
            VecSetRanges vsr1(s1);
            VecSetRanges vsr2(s2);
            Ranges::Inter<IntVal,VecSetRanges,VecSetRanges> inter(vsr1,vsr2);
            std::vector<Val> result;
            for (; inter(); ++inter) {
              result.emplace_back(inter.min());
              result.emplace_back(inter.max());
            }
            result_val = Val(Vec::a(this, newIdent(), result));
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::UNION:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("UNION R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ") R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          Val v1 = Val::follow_alias(frame->reg[r1], this);
          Val v2 = Val::follow_alias(frame->reg[r2], this);
          Val result_val;
          if (v1.isInt()) {
            result_val = v1;
          } else if (v2.isInt()) {
            result_val = v2;
          } else {
            Vec* s1 = v1.toVec();
            Vec* s2 = v2.toVec();
            VecSetRanges vsr1(s1);
            VecSetRanges vsr2(s2);
            Ranges::Union<IntVal,VecSetRanges,VecSetRanges> union_r(vsr1,vsr2);
            std::vector<Val> result;
            for (; union_r(); ++union_r) {
              result.emplace_back(union_r.min());
              result.emplace_back(union_r.max());
            }
            result_val = Val(Vec::a(this, newIdent(), result));
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::DIFF:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("DIFF R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ") R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          Val v1 = Val::follow_alias(frame->reg[r1], this);
          Val v2 = Val::follow_alias(frame->reg[r2], this);
          Val result_val;
          Vec* s1 = v1.toVec();
          Vec* s2 = v2.toVec();
          VecSetRanges vsr1(s1);
          VecSetRanges vsr2(s2);
          Ranges::Diff<IntVal,VecSetRanges,VecSetRanges> diff_r(vsr1,vsr2);
          std::vector<Val> result;
          for (; diff_r(); ++diff_r) {
            result.emplace_back(diff_r.min());
            result.emplace_back(diff_r.max());
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::INTERSECT_DOMAIN:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("INTERSECT_DOMAIN R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ") R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          Val v1 = Val::follow_alias(frame->reg[r1], this);
          Val v2 = Val::follow_alias(frame->reg[r2], this);

          Val dom_val;
          if (v1.isDef()) {
            dom_val = v1.toDef()->domain();
          } else {
            throw Error("Error: INTERSECT_DOMAIN on invalid type");
          }

          Val result_val;
          if (dom_val.isInt()) {
            if (v2.isVec()) {
              result_val = v2;
              v1.toDef()->domain(this, result_val, true);
            }
          } else if (v2.isVec()) {
            Vec* s1 = dom_val.toVec();
            Vec* s2 = v2.toVec();
            VecSetRanges vsr1(s1);
            VecSetRanges vsr2(s2);
            Ranges::Inter<IntVal,VecSetRanges,VecSetRanges> inter(vsr1,vsr2);
            std::vector<Val> result;
            for (; inter(); ++inter) {
              result.emplace_back(inter.min());
              result.emplace_back(inter.max());
            }
            v1.toDef()->domain(this, result, true);
            result_val = v1.toDef()->domain();
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::RET:
        {
          DBG_INTERPRETER("RET\n");
          assert(!_stack.empty());
execute_ret:
          if (_stack.size()==1) {
            // Always leave final frame on the stack
            return;
          }
          assert(!frame->cse_info.empty());
          
          for (auto& entry : frame->cse_info) {
            if (std::get<2>(entry).size() != 0) {
              if (std::get<1>(entry) == BytecodeProc::ROOT || std::get<1>(entry) == BytecodeProc::ROOT_NEG) {
                Val v = Val(1);
                cse_insert(std::get<0>(entry), std::get<2>(entry), std::get<1>(entry), v);
              } else if (std::get<3>(entry) == _agg.back().size()-1) {
                Val ret = _agg[_agg.size()-1].back();
                cse_insert(std::get<0>(entry), std::get<2>(entry), std::get<1>(entry), ret);
              } else {
                std::get<2>(entry).destroy();
              }
            }
          }
          _stack.back().destroy(this);
          _stack.pop_back();
          frame = &_stack.back();
        }
          break;
        case BytecodeStream::CALL:
        {
          char mode_c = frame->bs->chr(frame->pc);
          int code = frame->bs->reg(frame->pc);
          assert(code >= 0);
          assert(code < _procs.size());
          assert(mode_c >= 0);
          assert(mode_c <= BytecodeProc::MAX_MODE);
          auto mode = static_cast<BytecodeProc::Mode>(mode_c);
          int n = _procs[code].nargs;
          DBG_INTERPRETER("CALL " << BytecodeProc::mode_to_string[mode] << " " << code << "(" << _procs[code].name << ")");
          // TODO: See if args is created when not necessary
          std::vector<Val> args(n);
          bool cse_suited = n < 5 && mode != BytecodeProc::RAW;
          for (int i=0; i<n; i++) {
            int r = frame->bs->reg(frame->pc);
            args[i] = frame->reg[r];
            DBG_INTERPRETER(" R" << r << "(" << args[i].toString(DBG_TRIM_OUTPUT) << ")");
          }
          DBG_INTERPRETER("\n");
          CSETable::Key cse_key;
          if (cse_suited) {
            cse_key = CSETable::Key(args);
            // Lookup item in CSE
            auto lookup = cse_lookup(code, cse_key, mode);
            if (lookup.second) {
              cse_key.destroy();
              if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
                assert(lookup.first.isInt());
                if (lookup.first().toInt() != 1) {
                  _status = INCONSISTENT;
                  // Invariant: Last instruction in the frame is always an ABORT instruction
                  frame->pc = frame->bs->size()-1;
                }
              } else {
                pushAgg(lookup.first, -1);
              }
              break;
            }
          }
          if (_procs[code].mode[mode].size() == 0 || _procs[code].delay) {
            DBG_INTERPRETER((_procs[code].delay ? "--- Delayed CALL\n" : "--- FZN Builtin\n"));
            // this is a FlatZinc builtin
            int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
            Val dom;
            switch (mode) {
              case BytecodeProc::ROOT:
                dom = Val(IntVal(1));
                break;
              case BytecodeProc::ROOT_NEG:
                dom = Val(IntVal(0));
                break;
              case BytecodeProc::IMP:
              case BytecodeProc::IMP_NEG:
                dom = boolean_domain();
                break;
              default:
                dom = infinite_domain();
                break;
            }
            Definition* def = Definition::a(this,dom,false,code,mode,args,ident);
            for (const Val& arg : args) {
              if (arg.isDef()) {
                Definition* argDef = arg.toDef();
                if (!argDef->attached()) {
                  def->defs(this, argDef);
                }
              }
            }
            pushDef(def);
            if (ident >= 0) {
              pushAgg(Val(def), -1);
            }
            if (cse_suited) {
              Val v = (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) ? Val(1) : Val(def);
              cse_insert(code, cse_key, mode, v);
            }
            if (_procs[code].delay) {
              def->addWRef(this);
              delayed_calls.push_back(def);
            }
          } else {
            _stack.emplace_back(_procs[code].mode[mode]);
            BytecodeFrame* newFrame = &_stack[_stack.size()-1];
            newFrame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
            newFrame->reg.mov(this, args);
            frame = newFrame;
          }
        }
          break;
        case BytecodeStream::BUILTIN:
        {
          int code = frame->bs->reg(frame->pc);
          assert(code >= 0);
          DBG_INTERPRETER("BUILTIN " << code << "(" << _procs[code].name << ")");
          assert(code < primitiveMap().size());
          // this is a Interpreter builtin
          int n = _procs[code].nargs;
          std::vector<Val> args(n);
          for (int i=0; i<n; i++) {
            int r = frame->bs->reg(frame->pc);
            args[i].assign(this, frame->reg[r]);
            DBG_INTERPRETER(" R" << r << "(" << args[i].toString(DBG_TRIM_OUTPUT) << ")");
          }
          DBG_INTERPRETER("\n");
          primitiveMap()[code]->execute(*this, args);
        }
          break;
        case BytecodeStream::TCALL:
        {
          char mode_c = frame->bs->chr(frame->pc);
          int code = frame->bs->reg(frame->pc);
          assert(code >= 0);
          assert(code < _procs.size());
          assert(mode_c >= 0);
          assert(mode_c <= BytecodeProc::MAX_MODE);
          auto mode = static_cast<BytecodeProc::Mode>(mode_c);
          DBG_INTERPRETER("TCALL " << BytecodeProc::mode_to_string[mode] << " " << code << "(" << _procs[code].name << ")" << "\n");
          // TODO: Avoid creating the args vector
          std::vector<Val> args(_procs[mode].nargs);
          bool cse_suited = _procs[mode].nargs < 5 && mode != BytecodeProc::RAW;
          for (int i = 0; i < args.size(); ++i) {
            args[i] = frame->reg[i];
          }
          CSETable::Key cse_key;
          if (cse_suited) {
            cse_key = CSETable::Key(args);
            bool found;
            Val ret;
            std::tie(ret, found) = cse_lookup(code, cse_key, mode);
            if (found) {
              cse_key.destroy();
              // RET with CSE found value
              if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
                assert(ret.isInt());
                if (ret().toInt() != 1) {
                  _status = INCONSISTENT;
                  // Invariant: Last instruction in the frame is always an ABORT instruction
                  frame->pc = frame->bs->size()-1;
                }
              } else {
                pushAgg(ret, -1);
              }
              for (auto& entry : frame->cse_info) {
                if (std::get<2>(entry).size() != 0) {
                  cse_insert(std::get<0>(entry), std::get<2>(entry), std::get<1>(entry), ret);
                }
              }
              _stack.back().destroy(this);
              _stack.pop_back();
              frame = &_stack.back();
              break;
            }
          }
          if (_procs[code].mode[mode].size() == 0 || _procs[code].delay) {
            DBG_INTERPRETER((_procs[code].delay ? "--- Delayed CALL\n" : "--- FZN Builtin\n"));
            // this is a FlatZinc builtin
            int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
            Val dom;
            switch (mode) {
              case BytecodeProc::ROOT:
                dom = Val(IntVal(1));
                break;
              case BytecodeProc::ROOT_NEG:
                dom = Val(IntVal(0));
                break;
              case BytecodeProc::IMP:
              case BytecodeProc::IMP_NEG:
                dom = boolean_domain();
                break;
              default:
                dom = infinite_domain();
                break;
            }
            Definition* def = Definition::a(this,dom,false,code,mode,args,ident);
            for (const Val& arg : args) {
              if (arg.isDef()) {
                Definition* argDef = arg.toDef();
                if (!argDef->attached()) {
                  def->defs(this, argDef);
                }
              }
            }
            pushDef(def);
            if (ident >= 0) {
              pushAgg(Val(def), -1);
            }
            if (cse_suited) {
              Val v = (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) ? Val(1) : Val(def);
              cse_insert(code, cse_key, mode, v);
            }
            if (_procs[code].delay) {
              def->addWRef(this);
              delayed_calls.push_back(def);
            }
            goto execute_ret;
          } else {
            // Replace frame with new procedure
            frame->bs = &_procs[code].mode[mode];
            frame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
            frame->pc = 0;
          }
        }
          break;
        case BytecodeStream::TRACE:
        {
          int r = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("TRACE " << r  << "\n");
          std::cerr << frame->reg[r].toString() << "\n";
          //            std::cerr << frame->reg.e(r)->cast<StringLit>();
          //            std::cerr << *frame->reg.e(r) << "\n";
        }
          break;
        case BytecodeStream::ABORT:
        {
          DBG_INTERPRETER("ABORT\n");

          while (_stack.size() > 1) {
            _stack.back().destroy(this);
            _stack.pop_back();
          }
          // TODO: Should the Aggregation stack be emptied?

          if (_status == ROGER) {
            _status = ABORTED;
          }
          return;
        }
        case BytecodeStream::PUSH:
        {
          int r = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("PUSH R" << r << " (" << frame->reg[r].toString(DBG_TRIM_OUTPUT) << ")\n");
          assert(!_agg.empty());
          _agg.back().push(this,frame->reg[r]);
        }
          break;
        case BytecodeStream::POP:
        {
          int r = frame->bs->reg(frame->pc);
          assert(!_agg.empty());
          assert(!_agg.back().empty());
          frame->reg.assign(this, r, _agg.back().back());
          DBG_INTERPRETER("POP R" << r << " (" << frame->reg[r].toString(DBG_TRIM_OUTPUT) << ")\n");
          _agg.back().pop(this);
        }
          break;
        case BytecodeStream::POST:
        {
          int r = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("POST R" << r << " (" << frame->reg[r].toString(DBG_TRIM_OUTPUT) << ")\n");
          Val v1 = Val::follow_alias(frame->reg[r], this);
          if (v1.isInt()) {
            if (v1() == 0) {
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
            }
          } else {
            bool success = v1.toDef()->setVal(this, 1);
            if (!success) {
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
            }
            v1.toDef()->addRef(this);
          }
        }
          break;
        case BytecodeStream::OPEN_AGGREGATION:
        {
          int r = frame->bs->chr(frame->pc);
          DBG_INTERPRETER("OPEN_AGGREGATION " << AggregationCtx::symbol_to_string[r]  << ", depth "<< _agg.size() + 1 <<"\n");
          assert(r >= 0 && r <= AggregationCtx::VCTX_OTHER);
          if (r==AggregationCtx::VCTX_OTHER || r==AggregationCtx::VCTX_VEC || _agg.empty() || _agg.back().symbol != r) {
            // Push a new aggregation context
            _agg.emplace_back(this, r);
          } else {
            // Increment depth counter for current aggregation context
            _agg.back().n_symbols++;
          }
        }
          break;
        case BytecodeStream::SIMPLIFY_LIN:
        {
          DBG_INTERPRETER("SIMPLIFY_LIN");

          int r0 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER(" R" << r0 << "(" << frame->reg[r0].toString(DBG_TRIM_OUTPUT) << ")");
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);

          std::vector<Val> coeffs;
          std::vector<Val> vars;
          IntVal d;

          std::tie(coeffs, vars, d) = simplify_linexp(frame->reg[r0]);

          Val coeffs_v = Val(Vec::allocate_array(this, newIdent(), coeffs));
          Val vars_v = Val(Vec::allocate_array(this, newIdent(), vars));
          frame->reg.assign(this, r1, coeffs_v);
          frame->reg.assign(this, r2, vars_v);
          frame->reg.assign(this, r3, d);
          DBG_INTERPRETER(" R" << r1 << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          DBG_INTERPRETER(" R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          DBG_INTERPRETER(" R" << r3 << "(" << frame->reg[r3].toString(DBG_TRIM_OUTPUT) << ")\n");
        }
          break;
        case BytecodeStream::CLOSE_AGGREGATION:
        {
          DBG_INTERPRETER("CLOSE_AGGREGATION (" << AggregationCtx::symbol_to_string[_agg.back().symbol]  << ", depth "<< _agg.size() << ")\n");
          assert(!_agg.empty());
          // Decrement depth counter for current aggregation context
          _agg.back().n_symbols--;
          if (_agg.back().n_symbols==0) {
            
            // Definitions to be promoted to parent frame
            Definition* defs = nullptr;
            // Definition produced by this aggregation
            Definition* result = nullptr;
            
            // get definitions that were added during this aggregation
            if (_agg.back().def_stack->next() != _agg.back().def_stack) {
              defs = _agg.back().def_stack->next();
              // unlink definitions from aggregation, to get rid of dummy head element
              _agg.back().def_stack->unlink(this);
            }
            
            assert(_agg.size() >= 2);
            switch (_agg.back().symbol) {
              case AggregationCtx::VCTX_AND:
              {
                // Create a conjunction on the definition stack
                std::vector<Val> args;
                args.reserve(_agg.back().size());
                bool isFalse = false;
                for (int i=0; i<_agg.back().size(); i++) {
                  const Val& v = _agg.back()[i];
                  if (v.isInt()) {
                    if ( v()==0 ) {
                      // Disjunction is constant false
                      isFalse = true;
                      break;
                    }
                  } else {
                    args.push_back(v);
                  }
                }
                if (isFalse || args.empty()) {
                  // Conjunction is constant true or false
                  pushAgg(IntVal(!isFalse),-2);
                } else if (_agg.size()==2) {
                  // Push into root context
                  for (Val v : args) {
                    auto succes = v.toDef()->setVal(this, 1);
                    //FIXME: Deal with unsuccessful setVal
                    assert(succes);
                  }

                  // Why do we need a definition? If this is in ROOT, then all arguments should be true
                  /* Definition* d = Definition::a(this,boolean_domain(),false,PrimitiveMap::FORALL,BytecodeProc::ROOT, */
                  /*                               {Val(Vec::a(this,newIdent(),args))},-1); */
                  /* if (defs) { */
                  /*   d->appendBefore(this, defs); */
                  /* } else { */
                  /*   defs = d; */
                  /* } */
                } else if (args.size() == 1) {
                  pushAgg(args[0], -2);
                } else {
                  Vec* arr = Vec::allocate_array(this, newIdent(), args);
                  result = Definition::a(this,infinite_domain(),false,PrimitiveMap::MK_INTVAR,BytecodeProc::RAW,{boolean_domain()},newIdent());
                  auto ndefs = Definition::a(this,Val(1),false,PrimitiveMap::FORALL,BytecodeProc::ROOT,{Val(arr), Val(result)},newIdent());
                  result->defs(this, ndefs);

                  pushAgg(Val(result),-2);
                }
              }
                break;
              case AggregationCtx::VCTX_OR:
              {
                // Create a clause on the definition stack, and push a reference
                // to it onto the aggregation stack
                
                std::vector<Val> args;
                args.reserve(_agg.back().size());
                bool isTrue = false;
                for (int i=0; i<_agg.back().size(); i++) {
                  const Val& v = _agg.back()[i];
                  if (v.isInt()) {
                    if(v()!=0) {
                      // Disjunction is constant true
                      isTrue = true;
                      break;
                    }
                  } else {
                    args.push_back(v);
                  }
                }
                if (isTrue || args.empty()) {
                  // Disjunction is constant true or false
                  pushAgg(IntVal(isTrue),-2);
                } else if (_agg.size()==2) {
                  // Push into root context
                  Vec* arr = Vec::allocate_array(this, newIdent(), args);
                  Vec* empty = Vec::allocate_array(this, newIdent(), {});
                  Definition* d = Definition::a(this,boolean_domain(),false,PrimitiveMap::CLAUSE,BytecodeProc::ROOT,
                                                {Val(arr), Val(empty)},-1);
                  if (defs) {
                    d->appendBefore(this, defs);
                  } else {
                    defs = d;
                  }
                } else if (args.size() == 1) {
                  pushAgg(args[0],-2);
                } else {
                  Vec* arr = Vec::allocate_array(this, newIdent(), args);
                  result = Definition::a(this,infinite_domain(),false,PrimitiveMap::MK_INTVAR,BytecodeProc::RAW,{boolean_domain()},newIdent());
                  auto ndefs = Definition::a(this,Val(1),false,PrimitiveMap::EXISTS,BytecodeProc::ROOT,{Val(arr), Val(result)},newIdent());
                  result->defs(this, ndefs);

                  pushAgg(Val(result),-2);
                }
              }
                break;
              case AggregationCtx::VCTX_VEC:
              {
                // Create a vector on the aggregation stack
                _agg[_agg.size()-2].push(this,_agg.back().createVec(this,defs,newIdent()));
              }
                break;
              case AggregationCtx::VCTX_OTHER:
                // When closing a VCTX_OTHER context, it should contain at most one value
                assert(_agg.back().size()<=1);
                if (_agg.back().size()==1) {
                  if (_agg.back()[0].isDef()) {
                    result = _agg.back()[0].toDef();
                  }
                  // push value onto surrounding context
                  _agg[_agg.size()-2].push(this,_agg.back()[0]);
                }
                break;
            }
            
            _agg.back().destroyStack(this);
            if (defs && result) {
              // Aggregation produced constraints and exactly one return value, which is a definition
              if (result->timestamp() >= _agg.back().def_ident_start) {
                // the definition was produced by the current frame, so
                // attach all other defs to it
                if (result == defs) {
                  if (defs->next()==defs) {
                    defs = nullptr;
                  } else {
                    defs = defs->next();
                  }
                }
                result->unlink(this);
                // INVARIANT: The result of aggregation is not referenced by any of the registers.
                assert(std::none_of(frame->reg.cbegin(), frame->reg.cend(), [result](Val v) { return v.contains(Val(result)); }));
                result->makeUniqueReference();
                if (defs) {
                  result->defs(this, defs);
                }
                defs = result;
              }
            }
            if (defs) {
              // Move definitions to parent aggregation
              defs->appendBefore(this, _agg[_agg.size()-2].def_stack);
            }
            _agg.back().destroyDef(this);
            _agg.pop_back();
          }
        }
          break;
      }
      assert(!frame->bs->eos(frame->pc));
    }
  }

  void
  Interpreter::dumpState(std::ostream& os) {
    if (!_agg.empty()) {
      Definition::dump(_agg.back().def_stack, _procs, os, true);
    }
  }

  Model*
  Interpreter::toFZN() {
    GCLock lock;
    auto fzn = new Model();
    if (_status != ROGER) {
      std::vector<Expression*> args = {constants().boollit(true), constants().boollit(false)};
      auto fail = new Call(Location().introduce(), constants().ids.bool_eq, args);
      auto failI = new ConstraintI(Location().introduce(), fail);
      fzn->addItem(failI);
    } else if (!_agg.empty()) {
      std::unordered_map<int, VarDecl*> vdmap;
      Definition::toFZN(_agg.back().def_stack, _procs, fzn, vdmap, this);
      Env env(fzn);
      std::vector<FunctionI*> toAdd;
      for (auto ci = fzn->begin_constraints(); ci != fzn->end_constraints(); ++ci) {
        auto call = ci->e()->cast<Call>();
        FunctionI* fi = fzn->matchFn(env.envi(), call, false);
        if (!fi) {
          std::vector<VarDecl*> args;
          for (int i = 0; i < call->n_args(); ++i) {
            TypeInst* ti;
            if (call->arg(i)->type().dim() > 0) {
              auto al = eval_array_lit(env.envi(), call->arg(i));
              std::vector<TypeInst*> ranges(al->dims());
              for (auto& range : ranges) {
                range = new TypeInst(Location().introduce(), Type::parint(), nullptr);
              }
              ti = new TypeInst(Location().introduce(), call->arg(i)->type(), ranges, nullptr);
            } else {
              ti = new TypeInst(Location().introduce(), call->arg(i)->type(), nullptr);
            }
            args.push_back(new VarDecl(Location().introduce(), ti, i));
          }
          auto ti = new TypeInst(Location().introduce(), Type::varbool());
          fi = new FunctionI(Location().introduce(), call->id().str(), ti, args, nullptr);
          fzn->registerFn(env.envi(), fi);
          toAdd.push_back(fi);
        }
        call->decl(fi);
      }
      env.model(nullptr);
      for (const auto& j : toAdd) {
        fzn->addItem(j);
      }
    }

    // TODO: What solve item should we add?
    fzn->addItem(SolveI::sat(Location().introduce()));
    return fzn;
  }
  
  Interpreter::~Interpreter(void) {
    globals.destroy(this);
    for (auto& f : _stack) {
      f.destroy(this);
    }
    for (auto& a : _agg) {
      a.destroyStack(this);
      a.destroyDef(this);
    }
    for (auto &table : cse) {
      table.destroy(this);
    }
    RefCountedObject::rmRef(this, infinite_dom);
    RefCountedObject::rmRef(this, boolean_dom);
  }
  
  void
  Interpreter::call(int code, const BytecodeProc::Mode& mode0, const std::vector<Val>& args0, bool delayed) {
    if (_status != ROGER) {
      return;
    }
    BytecodeProc::Mode mode = mode0;
    std::vector<Val> args = args0;
    assert(code >= 0);
    assert(code < _procs.size());
    int n = _procs[code].nargs;
    DBG_INTERPRETER("Interpreter::call " << BytecodeProc::mode_to_string[mode] << " " << code << "(" << _procs[code].name << ")" << "\n");
    // TODO: See if args is created when not necessary
    assert(n == args.size());
    bool cse_suited = n < 5 && mode != BytecodeProc::RAW && !delayed;
    CSETable::Key cse_key;
    if (cse_suited) {
      cse_key = CSETable::Key(args);
      // Lookup item in CSE
      auto lookup = cse_lookup(code, cse_key, mode);
      if (lookup.second) {
        cse_key.destroy();
        if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
          assert(lookup.first.isInt());
          if (lookup.first().toInt() != 1) {
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            _stack.back().pc = _stack.back().bs->size()-1;
          }
        } else {
          pushAgg(lookup.first, -1);
        }
        return;
      }
    }
    if (_procs[code].mode[mode].size() == 0) {
      DBG_INTERPRETER("--- FZN Builtin\n");
      // this is a FlatZinc builtin
      int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
      Val dom;
      switch (mode) {
        case BytecodeProc::ROOT:
          dom = Val(IntVal(1));
          break;
        case BytecodeProc::ROOT_NEG:
          dom = Val(IntVal(0));
          break;
        case BytecodeProc::IMP:
        case BytecodeProc::IMP_NEG:
          dom = boolean_domain();
          break;
        default:
          dom = infinite_domain();
          break;
      }
      Definition* def = Definition::a(this,dom,false,code,mode,args,ident);
      pushDef(def);
      if (cse_suited) {
        Val v = (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) ? Val(1) : Val(def);
        cse_insert(code, cse_key, mode, v);
      }
      if (ident >= 0) {
        pushAgg(Val(def), -1);
      }
      return;
    } else {
      // Ensure the last RET is next on the program counter
      _stack.back().pc--;
      _stack.emplace_back(_procs[code].mode[mode]);
      BytecodeFrame* newFrame = &_stack[_stack.size()-1];
      newFrame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
      newFrame->reg.mov(this, args);
      return run();
    }
  }

  bool Interpreter::runDelayed() {
    std::vector<Definition*> wave = std::move(delayed_calls);
    delayed_calls.clear();
    for (auto def : wave) {
      if (def->exists()) {
        auto mode = static_cast<BytecodeProc::Mode>(def->mode());
        std::vector<Val> args(def->size());
        for (int i = 0; i < def->size(); ++i) {
          args[i] = def->arg(i);
        }
        call(def->pred(), mode, args, true);
        if (_status != ROGER) {
          break;
        }
        Val ret(1);
        if (mode != BytecodeProc::ROOT && mode != BytecodeProc::ROOT_NEG) {
          ret = _agg.back().back();
          assert(ret.isDef() || ret.isInt());
        }
        def->alias(this, ret);
      }
      RefCountedObject::rmWRef(this, def);
    }
    return !delayed_calls.empty();
  }

  size_t Trail::save_state(MiniZinc::Interpreter* interpreter) {
    trail_size.emplace_back(hedge_trail.size(), obj_trail.size(), alias_trail.size(), domain_trail.size());
    timestamp_trail.push_back(interpreter->_identCount);
    for (auto &table : interpreter->cse) {
      table.push(interpreter, !last_operation_pop);
    }
    last_operation_pop = false;
    return len();
  }

  void Trail::untrail(MiniZinc::Interpreter* interpreter) {
    assert(len() > 0);
    assert(interpreter->_stack.size() == 1);
    size_t ht_size, ot_size, at_size, dt_size;
    std::tie(ht_size, ot_size, at_size, dt_size) = trail_size.back(); trail_size.pop_back();
    int timestamp = timestamp_trail.back(); timestamp_trail.pop_back();
    Definition* stack = interpreter->_agg[0].def_stack; // Stack head (empty object)
    Definition* back = stack->prev(); // Current last element on the stack;
    assert(back->pred() != 0);
    // Reconstruct destroyed items
    while(obj_trail.size() > ot_size) {
      auto obj = obj_trail.back();
      switch (obj->rcoType()) {
        case RefCountedObject::DEF:
          static_cast<Definition*>(obj)->reconstruct(interpreter);
          break;
        case RefCountedObject::VEC:
          static_cast<Vec*>(obj)->reconstruct(interpreter);
          break;
        default:
          assert(false);
      }
      obj_trail.pop_back();
    }
    // Restore hedge pointers back to their previous versions
    while (hedge_trail.size() > ht_size) {
      auto entry = hedge_trail.back();
      *entry.first = entry.second;
      hedge_trail.pop_back();
    }
    // Restore original definitions for created aliases
    while (alias_trail.size() > at_size) {
      Definition* def;
      int proc, size;
      Val arg0;
      std::tie(def, proc, size, arg0) = alias_trail.back();
      def->unalias(interpreter, proc, size, arg0);
      arg0.removeWeakRef(interpreter);
      alias_trail.pop_back();
    }
    // Restore original domains
    while (domain_trail.size() > dt_size) {
      Definition* def;
      Val dom;
      std::tie(def, dom) = domain_trail.back();
      def->_domain.destroy(interpreter);
      def->_domain = dom;
      def->_domain.construct(interpreter);
      dom.removeWeakRef(interpreter);
      domain_trail.pop_back();
    }
    // Remove all additions/changes to the CSE table
    for (auto &table : interpreter->cse) {
      table.pop(interpreter);
    }
    // Remove all newly created definitions
    if (stack->prev() != back) {  // If last element on the stack changed
      do {
        Definition* rem = back;
        back = back->prev();
        rem->destroy(interpreter);
        free(rem);
      } while (back->next() != back);
    }
    // TODO: Should we remove newly created propagators??
    // Reset the timestamp count to its previous value
    interpreter->_identCount = timestamp;
    last_operation_pop = true;
  }
  
  void
  Interpreter::optimize(void) {
  }
  
}
