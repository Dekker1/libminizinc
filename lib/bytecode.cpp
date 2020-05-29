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

//class DebugStream {
//  static const int _max_size = 20;
//  std::vector<std::string> _stream;
//  int _i;
//public:
//
//  DebugStream(void) : _stream(_max_size), _i(0) {}
//
//  void put(const std::string& s) {
//    _stream[_i++]=s;
//    if (_i==_max_size) {
//      _i=0;
//    }
//  }
//
//  void dump(void);
//};
//
//void DebugStream::dump(void) {
//  for (int i=_i+1; i<_stream.size(); i++) {
//    std::cerr << _stream[i];
//  }
//  for (int i=0; i<_i; i++) {
//    std::cerr << _stream[i];
//  }
//}
//
//static DebugStream* __debugstream = new DebugStream;
//
//#define DBG_INTERPRETER(msg) do { \
//  std::ostringstream oss;         \
//  oss << msg;                     \
//  __debugstream->put(oss.str());\
//} while(0)

//#define DBG_INTERPRETER(msg) std::cerr << msg
#define DBG_INTERPRETER(msg) do {} while(0)
#define DBG_TRIM_OUTPUT true

namespace MiniZinc {

  Val
  AggregationCtx::createVec(Interpreter* interpreter, int timestamp) const {
    return Val(Vec::a(interpreter, timestamp, stack));
  }

  AggregationCtx::~AggregationCtx(void) {
    /// TODO
  }

  Vec* Vec::allocate_array(Interpreter* interpreter, int timestamp, const std::vector<Val>& v) {
    Vec* values = a(interpreter, interpreter->newIdent(), v);
    Vec* idx = a(interpreter, interpreter->newIdent(), {IntVal(1), IntVal(v.size())});
    Vec* nv = a(interpreter, timestamp, {Val(values), Val(idx)});
    return nv;
  }

  bool
  Vec::isPar() const {
    assert(alive()); 
    for (int i = 0; i < this->size(); ++i) {
      Val v = this->operator[](i);
      if (v.isVec() && (!v.toVec()->isPar())) {
        return false;
      }
      if (v.isVar()) {
        return false;
      }
    }
    return true;
  }

  void
  Constraint::destroy(Interpreter* interpreter) {
    for (unsigned int i=0; i<_size; i++) {
      _args[i].destroy(interpreter);
    }
  }

  void
  Constraint::reconstruct(Interpreter* interpreter) {
    for (unsigned int i=0; i<_size; i++) {
      _args[i].construct(interpreter);
    }
  }

  Constraint::Constraint(Interpreter* interpreter,int pred,char mode,const std::vector<Val>& args,Val ann,Val defines)
    : _pred(pred), _mode(mode), _size(args.size()), _scheduled(0)
  {
    _ann.construct(interpreter);
    for (unsigned int i=0; i<args.size(); i++) {
      new (&_args[i]) Val(args[i]);
      _args[i].construct(interpreter);
      if (pred > PrimitiveMap::MAX_LIN) {
        _args[i].finalizeLin(interpreter);
      }
    }
  }

  std::pair<Constraint*, bool> Constraint::a(Interpreter* interpreter, int pred, char mode, const std::vector<Val>& args, Val defines, Val ann) {
    Constraint* c = static_cast<Constraint*>(::malloc(sizeof(Constraint)+sizeof(Val)*(std::max(0,static_cast<int>(args.size())-1))));
    c = new (c) Constraint(interpreter,pred,mode,args,ann,defines);
    PropStatus ps = interpreter->subscribe(c);
    if (ps == PS_ENTAILED || ps == PS_FAILED) {
      interpreter->unsubscribe(c);
      c->destroy(interpreter);
      Constraint::free(c);
      return {nullptr, ps == PS_ENTAILED};
    }
    return {c, true};
  }

  Variable::Variable(Interpreter* interpreter, Val domain, int ident)
    : RefCountedObject(RefCountedObject::VAR,ident), _prev(this), _next(this), _domain(domain), _binding(true), _aliased(false) {
    assert(_domain.isVec());
    _domain.construct(interpreter);
    _ann.construct(interpreter);
    addRef(interpreter);
  }

  Variable::Variable(Interpreter* interpreter, Val domain, bool binding, int ident, Val ann)
    : RefCountedObject(RefCountedObject::VAR,ident), _prev(this), _next(this), _domain(domain), _ann(ann), _binding(binding), _aliased(false) {
    assert(_domain.isVec());
    _domain.construct(interpreter);
    _ann.construct(interpreter);
    if (binding)
      addRef(interpreter);
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

  bool Variable::setMin(Interpreter* interpreter, IntVal i, bool binding) {
    assert(!aliased());
    assert(_domain.isVec());
    assert(_domain.size() % 2 == 0);
    size_t j = 0;
    while (j < _domain.size() && _domain[j]() < i) {
      ++j;
    }
    if (j == 0) {
      return true;
    }
    if (j == _domain.size()) {
      Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
      ndom.construct(interpreter);
      domain(interpreter, ndom, binding);
      ndom.destroy(interpreter);
      return false;
    }
    std::vector<Val> dom;
    if (j % 2 == 1) {
      dom.emplace_back(i);
    }
    for (; j < _domain.size(); ++j) {
      dom.push_back(_domain[j]);
    }
    Val ndom(Vec::a(interpreter, interpreter->newIdent(),dom));
    ndom.construct(interpreter);
    domain(interpreter, ndom, binding);
    ndom.destroy(interpreter);
    return true;
  }

  bool Variable::setMax(Interpreter* interpreter, IntVal i, bool binding) {
    assert(!aliased());
    assert(_domain.isVec());
    assert(_domain.size() % 2 == 0);
    size_t j = _domain.size() - 1;
    while (j >= 0 && _domain[j]() > i) {
      --j;
    }
    if (j == _domain.size() - 1) {
      return true;
    }
    if (j < 0 ) {
      Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
      ndom.construct(interpreter);
      domain(interpreter, ndom, binding);
      ndom.destroy(interpreter);
      return false;
    }
    std::vector<Val> dom;
    for (size_t k = 0; k <= j; ++k) {
      dom.push_back(_domain[k]);
    }
    if (j % 2 == 0) {
      dom.emplace_back(i);
    }
    Val ndom(Vec::a(interpreter, interpreter->newIdent(),dom));
    ndom.construct(interpreter);
    domain(interpreter, ndom, binding);
    ndom.destroy(interpreter);
    return true;
  }

  bool Variable::setVal(Interpreter* interpreter, IntVal i, bool binding) {
    assert(!aliased());
    assert(_domain.isVec());
    assert(_domain.size() % 2 == 0);
    for (int j = 0; j < _domain.size(); j+=2) {
      if (_domain[j]() <= i && i <= _domain[j+1]()) {
        this->binding(interpreter, binding);
        alias(interpreter, Val(i));
        return true;
      }
    }
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), {}));
    ndom.construct(interpreter);
    domain(interpreter, ndom, binding);
    ndom.destroy(interpreter);
    return false;
  }

  bool Variable::intersectDom(Interpreter* interpreter, const std::vector<Val>& dom, bool binding) {
    if (!isBounded()) {
      Val ndom(Vec::a(interpreter, interpreter->newIdent(), dom));
      ndom.construct(interpreter);
      domain(interpreter, ndom, binding);
      ndom.destroy(interpreter);
      return true;
    }
    assert(!aliased());
    assert(_domain.isVec());
    assert(_domain.size() % 2 == 0);
    VecSetRanges vsr1(_domain.toVec());
    StdVecSetRanges vsr2(&dom);
    Ranges::Inter<IntVal,VecSetRanges,StdVecSetRanges> inter(vsr1,vsr2);
    std::vector<Val> result;
    for (; inter(); ++inter) {
      result.emplace_back(inter.min());
      result.emplace_back(inter.max());
    }
    Val ndom(Vec::a(interpreter, interpreter->newIdent(), result));
    ndom.construct(interpreter);
    domain(interpreter, ndom, binding);
    ndom.destroy(interpreter);
    return !result.empty();
  }

  bool Variable::intersectDom(Interpreter* interpreter, Val dom, bool binding) {
    assert(!dom.isVar());
    if (dom.isInt()) {
      return setVal(interpreter, dom());
    }
    // TODO: Allocation is not really necessary;
    std::vector<Val> vdom(dom.size());
    for (int i = 0; i < dom.size(); ++i) {
      vdom[i] = dom[i];
    }
    return intersectDom(interpreter, vdom, binding);
  }

  void
  Variable::domain(Interpreter* interpreter, const Val& newDomain, bool binding0) {
    assert(!aliased());
    assert(!newDomain.isRCO() || newDomain.toRCO()->alive());
    assert(_domain.isVec());
    interpreter->trail.trail_domain(interpreter, this, _domain.toVec());
    SubscriptionEvent sev;
    if (newDomain.isInt()) {
      alias(interpreter, newDomain);
      sev = SEV_VAL;
    } else if (newDomain.size() == 2 && newDomain[0]() == newDomain[1]()) {
      alias(interpreter, newDomain[0]);
      sev = SEV_VAL;
    } else {
      Val nd = newDomain;
      nd.construct(interpreter);
      _domain.destroy(interpreter);
      _domain = newDomain;
      sev = SEV_DOM;
    }
    for (auto& s : _subscriptions) {
      if (sev==SEV_VAL || s.second==SES_ANY) {
        interpreter->schedule(s.first, sev);
      }
    }
    binding(interpreter,binding0);
  }
  void
  Variable::domain(Interpreter* interpreter, const std::vector<Val>& newDomain, bool binding0) {
    if (!isBounded()) {
      Val ndom(Vec::a(interpreter, interpreter->newIdent(), newDomain));
      ndom.construct(interpreter);
      domain(interpreter, ndom, binding0);
      ndom.destroy(interpreter);
    } else {
      bool did_update = false;
      if (newDomain.size() != _domain.size()) {
        did_update = true;
      } else {
        for (int i=0; i<newDomain.size(); i++) {
          if (newDomain[i]() != _domain[i]()) {
            did_update = true;
            break;
          }
        }
      }
      if (did_update) {
        Val ndv(Vec::a(interpreter, interpreter->newIdent(), newDomain));
        ndv.construct(interpreter);
        domain(interpreter, ndv, binding0);
        ndv.destroy(interpreter);
      }
    }
  }

  void Variable::destroy(MiniZinc::Interpreter* interpreter)  {
    _ref_count = (1u<<31u)-1u;
    _domain.destroy(interpreter);
    _ann.destroy(interpreter);
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
    _ref_count = 0;

  }

  void Variable::reconstruct(Interpreter* interpreter) {
    /// TODO: what about subscriptions?
    assert(_ref_count == 0);
    _ann.construct(interpreter);
    _domain.construct(interpreter);
    for (auto c : _definitions) {
      c->reconstruct(interpreter);
    }
    _ref_count = 0;
  }

  void Variable::addDefinition(Interpreter* interpreter, Constraint* c) {
    _definitions.push_back(c);
    if (this != interpreter->root()) {
      // Remove reference counts for this variable from each argument in c
      for (int i=0; i<c->size(); i++) {
        if (c->arg(i).isVar()) {
          if (c->arg(i).timestamp()==_timestamp) {
            RefCountedObject::rmRef(interpreter, this);
          }
        } else if (c->arg(i).isVec()) {
          assert(c->arg(i).size()==2);
          assert(c->arg(i)[0].isVec());
          bool hasVar = false;
          for (int j=0; j<c->arg(i)[0].size(); j++) {
            if (c->arg(i)[0][j].isVar() && c->arg(i)[0][j].timestamp()==_timestamp) {
              hasVar = true;
              break;
            }
          }
          if (hasVar) {
            if (!c->arg(i).unique() || !c->arg(i)[0].unique()) {
              // make vectors unique so that we can safely decrement reference counts
              std::vector<Val> vals(c->arg(i)[0].size());
              for (int i=0; i<vals.size(); i++) {
                vals[i] = c->arg(i)[0][i];
              }
              Vec* vv = Vec::a(interpreter, interpreter->newIdent(), vals);
              Vec* v = Vec::a(interpreter, interpreter->newIdent(), {Val(vv), c->arg(i)[1]});
              c->arg(interpreter, i, Val(v));
            }
            for (int j=0; j<c->arg(i)[0].size(); j++) {
              if (c->arg(i)[0][j].timestamp()==_timestamp) {
                RefCountedObject::rmRef(interpreter, this);
              }
            }
          }
        }
      }
    }
    interpreter->trail.trail_add_def(this,c);
  }

  void Variable::alias(Interpreter* interpreter, Val v) {
    assert(!_aliased);
    assert(!v.isVar() || v.toVar()!=this);
    // Move defining constraints to current context
    
    for (Constraint* c : _definitions) {
      for (int i = 0; i < c->size(); ++i) {
        Val arg = c->arg(i);
        if (arg.isVec()) {
          _ref_count += arg.toVec()->count(Val(this));
        } else {
          _ref_count += (arg == Val(this));
        }
      }
      interpreter->trail.trail_rm_def(this,c);
      interpreter->pushConstraint(c);
    }
    _definitions.clear();

    // Destroy old domain
    _domain.destroy(interpreter);
    _ann.destroy(interpreter);
    _ann = Val(IntVal(0));

    // Transfer subscriptions to new value and schedule propagators
    for (auto& s : _subscriptions) {
      if (v.isVar()) {
        v.toVar()->subscribe(s.first, s.second);
      }
      if (s.second==SES_VALUNIFY || s.second==SES_ANY) {
        interpreter->schedule(s.first, SEV_UNIFY);
      }
    }
    _subscriptions.clear();
        
    // Set Alias
    interpreter->trail.trail_alias(interpreter, this);
    _aliased = true;
    _domain = v;
    v.construct(interpreter);
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

  void
  Variable::binding(Interpreter* interpreter, bool f) {
    if (!_binding && f) {
      addRef(interpreter);
    } else if (_binding && !f) {
      RefCountedObject::rmRef(interpreter, this);
    }
    _binding = f;
  }

  void
  Variable::subscribe(Constraint* c, const SubscriptionEventSet& events) {
    Variable* sub = this;
    while (sub && sub->aliased()) {
      if (sub->_domain.isVar()) {
        sub = sub->_domain.toVar();
      } else {
        sub = nullptr;
      }
    }
    if (sub) {
      sub->_subscriptions.insert(std::make_pair(c,events));
    }
  }
  void
  Variable::unsubscribe(Constraint* c) {
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

  void
  Variable::dump(Variable* head, const std::vector<BytecodeProc>& bs, std::ostream& os) {
    Variable* d = head;
    do {
      d = d->next();
      if (d != head) {
        if (d->timestamp() >=0) {
          os << d->timestamp() << "(";
        }
        os << d << "." << d->_ref_count;
        if (d->timestamp() >=0) {
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
        for (int i=0; i<c->size(); i++) {
          os << c->arg(i).toString();
          if (i<c->size()-1)
            os << ", ";
        }
        os << ")\n";
      }
    } while (d != head);
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
      if (coeffs[j]() != 0) {
        defs.emplace_back(coeffs[j](), vars[j]);
      }
    }
    coeffs.clear();
    vars.clear();

    std::vector<int> idx;
    while (!defs.empty()) {
      IntVal coeff = defs.back().first;
      Val stacktop = Val::follow_alias(defs.back().second);
      defs.pop_back();
      if (coeff==0)
        continue;
      if (stacktop.isInt()) {
        d += coeff*stacktop();
      } else {
        Variable* cur = stacktop.toVar();
        if (Constraint* defby = cur->defined_by()) {
          switch (defby->pred()) {
            case PrimitiveMap::INT_PLUS: {
              assert(stacktop == Val::follow_alias(defby->arg(2)));
              for (int i = 0; i < 2; ++i) {
                Val arg = Val::follow_alias(defby->arg(i));
                if (arg.isInt()) {
                  d += coeff * arg();
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
                  d += coeff * lhs();
              } else {
                defs.emplace_back(coeff, lhs);
              }
              Val rhs = Val::follow_alias(defby->arg(1));
              if (rhs.isInt()) {
                d += coeff * -rhs();
              } else {
                defs.emplace_back(-coeff, rhs);
              }
              continue;
            }
            case PrimitiveMap::INT_SUM: {
              assert(stacktop == Val::follow_alias(defby->arg(1)));
              Val arr = defby->arg(0)[0];
              for (int i = 0; i < arr.size(); ++i) {
                Val arg = Val::follow_alias(arr[i]);
                if (arg.isInt()) {
                  d += coeff * arg();
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
                  d += coeff * lhs() * rhs();
                } else {
                  defs.emplace_back(coeff * lhs(), rhs);
                }
                continue;
              }
              if (rhs.isInt()) {
                defs.emplace_back(coeff * rhs(), lhs);
                continue;
              }
              break;
            }
            default: {}
          }
        }
        if (coeff != 0) {
          coeffs.emplace_back(coeff);
          vars.emplace_back(cur);
          idx.push_back(idx.size());
        }
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
        int j=0;
        for (unsigned int i=0; i<coeffs_simple.size(); i++) {
          if (coeffs_simple[i] != 0) {
            coeffs[j++] = coeffs_simple[i];
            vars[j++] = vars_simple[i];
          }
        }
        coeffs.resize(j);
        vars.resize(j);
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
    } else if (isVar()) {
      if (timestamp() >= 0) {
        oss << "X" << timestamp() << "(";
      }
      oss << toVar();
      if (toVar()->timestamp() >= 0) {
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
    if (v.isVar() && v.toVar()->aliased()) {
      Val nval = v;
      while(nval.isVar() && nval.toVar()->aliased()) {
        nval = nval.toVar()->alias();
      }
      if (interpreter && !interpreter->trail.is_trailed(v.toVar())) {
        Val& mut_v = const_cast<Val&>(v);
        nval.construct(interpreter);
        mut_v.destroy(interpreter);
        mut_v._v = nval._v;
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
      return toVar()->lb();
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
      IntVal d = 0;
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

        Vec* ncoeffs = Vec::allocate_array(interpreter, interpreter->newIdent(), coeffs);
        Vec* nvars = Vec::allocate_array(interpreter, interpreter->newIdent(), vars);
        bool b;
        std::tie(nc, b) = Constraint::a(interpreter, PrimitiveMap::INT_LIN_EQ, BytecodeProc::ROOT, {Val(ncoeffs), Val(nvars), Val(-d)});
        assert(nc);
      }

      this->toVar()->_definitions.clear();
      if (nc) {
        this->toVar()->addDefinition(interpreter, nc);
      }

      // FIXME: c->destroy will remove a non-existing reference to this.
      this->toVar()->addRef(interpreter);
      c->destroy(interpreter);
      Constraint::free(c);
    }
  } else {
    this->toVec()->finalizeLin(interpreter);
  }
}

  CSETable::Key::Key(const std::vector<Val> &vec) {
    _size = vec.size();
    // TODO: Should CSEKeys compare arrays with the same content again?
    for (const auto& val : vec) {
      if (val.isVec() && val.size()==2 && val[0].isVec() && val[1].isVec() && val[0].size() <= 5) {
        _size += val[0].size() + val[1].size() + 1;
      }
    }
    if (_size > 0) {
     _vals = (WeakVal*) malloc(_size*sizeof(WeakVal));
      size_t i = 0;
      for (const auto& val : vec) {
        if (val.isVec() && val.size()==2 && val[0].isVec() && val[1].isVec() && val[0].size() <= 5) {
          _vals[i++] = WeakVal(Val(val[0].size()));
          for (int j = 0; j < val[0].size(); ++j) {
            assert(!val[0][j].isVec());
            _vals[i++] = WeakVal(val[0][j]);
          }
          _vals[i++] = WeakVal(Val(val[1].size()));
          for (int j = 0; j < val[1].size(); ++j) {
            assert(!val[1][j].isVec());
            _vals[i++] = WeakVal(val[1][j]);
          }
        } else {
          _vals[i++] = WeakVal(val);
        }
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
      // FIXME: This can be a permanent replacement, but the change needs to be stored in the CSETable and we are removing a weak reference, not a strong reference.
      Val val = Val::follow_alias(it->second.second);
      BytecodeProc::Mode val_m = it->second.first;
      if (!val.exists()) {
        this->_table[i].erase(it);
        return {Val(), false};
      }
      DBG_INTERPRETER("--- CSE hit! hash(" << key.hash() << ") -> Mode: " << BytecodeProc::mode_to_string[val_m] << " Value: " << val.toString(DBG_TRIM_OUTPUT) << "\n");
      auto convert = [&interpreter, val_m, mode](Val v) {
        assert(!v.isVec());
        if (BytecodeProc::is_neg(mode) != BytecodeProc::is_neg(val_m)) {
          if (v.isInt()) {
            assert(v().toInt() == 0 || v().toInt() == 1);
            return Val(1 - v().toInt());
          } else {
            Key nkey({v});
            Val new_val;
            bool found;
            auto cmode = BytecodeProc::FUN;
            std::tie(new_val, found) = interpreter->cse_lookup(PrimitiveMap::OP_NOT, nkey, cmode);
            if (!found) {
              Variable* new_var = Variable::a(interpreter, interpreter->boolean_domain(), true, interpreter->newIdent());
              new_val = Val(new_var);
              auto c = Constraint::a(interpreter, PrimitiveMap::BOOLNOT, BytecodeProc::ROOT, {v, new_val});
              assert(c.first);
              new_var->addRef(interpreter);
              new_var->addDefinition(interpreter, c.first);
              interpreter->cse_insert(PrimitiveMap::OP_NOT, nkey, cmode, new_val);
              RefCountedObject::rmRef(interpreter, new_var);
            } else {
              nkey.destroy();
            }
            return new_val;
          }
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
      key.destroy();
      Val oldVal = Val::follow_alias(it->second.second);
      BytecodeProc::Mode& oldMode = it->second.first;
      if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        if (oldVal.isVar()) {
          Variable* v = oldVal.toVar();
          v->alias(interpreter, BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode) ? Val(IntVal(1)) : Val(IntVal(0)));
        }
      } else if (mode == BytecodeProc::FUN || mode == BytecodeProc::FUN_NEG) {
        if (oldVal.isVar()) {
          Variable* v = oldVal.toVar();
          if (BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode)) {
            // Value might have already been aliased earlier in the call stack
            if (val != oldVal) {
              v->alias(interpreter, val);
            }
          } else {
            Key nkey({val});
            Val new_val;
            bool found;
            auto cmode = BytecodeProc::FUN;
            std::tie(new_val, found) = interpreter->cse_lookup(PrimitiveMap::OP_NOT, nkey, cmode);
            if (!found) {
              Variable* new_var = Variable::a(interpreter, interpreter->boolean_domain(), true, interpreter->newIdent());
              new_val = Val(new_var);
              auto c = Constraint::a(interpreter, PrimitiveMap::BOOLNOT, BytecodeProc::ROOT, {val, new_val});
              assert(c.first);
              new_var->addRef(interpreter);
              new_var->addDefinition(interpreter, c.first);
              interpreter->cse_insert(PrimitiveMap::OP_NOT, nkey, cmode, new_val);
              RefCountedObject::rmRef(interpreter, new_var);
            } else {
              nkey.destroy();
            }
            if (new_val != oldVal) {
              v->alias(interpreter, new_val);
            }
          }
        }
      }
      it->second.second.removeWeakRef(interpreter);
      it->second = std::make_pair(mode, val);
    }
  }

  std::string
  BytecodeStream::toString(const std::vector<BytecodeProc>& procs) const {
    std::ostringstream oss;
    int pc = 0;
    while (pc < _bs.size()) {
      int cur_pc = pc;
      switch (instr(pc)) {
        case BytecodeStream::ADDI:
        {
          oss << "ADDI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::SUBI:
        {
          oss << "SUBI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::MULI:
        {
          oss << "MULI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::DIVI:
        {
          oss << "DIVI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::MODI:
        {
          oss << "MODI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::INCI:
        {
          oss << "INCI R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::DECI:
        {
          oss << "DECI R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::IMMI:
        {
          oss << "IMMI " << intval(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::CLEAR:
        {
          oss << "CLEAR " << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::LOAD_GLOBAL:
        {
          oss << "LOAD_GLOBAL " << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::STORE_GLOBAL:
        {
          oss << "STORE_GLOBAL R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::MOV:
        {
          oss << "MOV R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::JMP:
        {
          oss << "JMP " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::JMPIF:
        {
          oss << "JMPIF R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::JMPIFNOT:
        {
          oss << "JMPIFNOT R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::EQI:
        {
          oss << "EQI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::LTI:
        {
          oss << "LTI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::LEI:
        {
          oss << "LEI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::AND:
        {
          oss << "AND R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::OR:
        {
          oss << "OR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::NOT:
        {
          oss << "NOT R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::XOR:
        {
          oss << "XOR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ISPAR:
        {
          oss << "ISPAR R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ISEMPTY:
        {
          oss << "ISEMPTY R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::LENGTH:
        {
          oss << "LENGTH R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          oss << "GET_VEC R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::GET_VEC_NDIM:
        {
          oss << "GET_VEC_NDIM ";
          IntVal n=intval(pc);
          oss << n;
          for (int i=0; i < (n + 3); ++i) {
            oss << " R" << reg(pc);
          }
          oss << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::LB:
        {
          oss << "LB R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::UB:
        {
          oss << "UB R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::DOM:
        {
          oss << "DOM R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::MAKE_SET:
        {
          oss << "MAKE_SET R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::INTERSECTION:
        {
          oss << "INTERSECTION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::UNION:
        {
          oss << "UNION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::DIFF:
        {
          oss << "DIFF R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::INTERSECT_DOMAIN:
        {
          oss << "INTERSECT_DOMAIN R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::RET:
        {
          oss << "RET" <<  " % " << cur_pc << "\n";
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
          oss << " % " << cur_pc << "\n";
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
          oss << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::TCALL:
        {
          auto m = static_cast<BytecodeProc::Mode>(chr(pc));
          int p = reg(pc);
          
          if (procs.empty()) {
            oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << p << " % " << cur_pc << "\n";
          } else {
            oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name << " % " << cur_pc << "\n";
          }
        }
          break;
        case BytecodeStream::ITER_VEC:
        {
          oss << "ITER_VEC " << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ITER_RANGE:
        {
          oss << "ITER_RANGE " << reg(pc) << " " << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ITER_NEXT:
        {
          oss << "ITER_NEXT " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ITER_BREAK:
        {
          oss << "ITER_NEXT " << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::OPEN_AGGREGATION:
        {
          oss << "OPEN_AGGREGATION ";
          int p = chr(pc);
          switch (p) {
            case AggregationCtx::VCTX_AND:
              oss << "AND";
              break;
            case AggregationCtx::VCTX_OR:
              oss << "OR";
              break;
            case AggregationCtx::VCTX_VEC:
              oss << "VEC";
              break;
            case AggregationCtx::VCTX_OTHER:
              oss << "OTHER";
              break;
            default:
              oss << "ERROR";
              assert(false);
              break;
          }
          oss << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::CLOSE_AGGREGATION:
        {
          oss << "CLOSE_AGGREGATION" << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::SIMPLIFY_LIN:
        {
          oss << "SIMPLIFY_LIN R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::PUSH:
        {
          oss << "PUSH R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::POP:
        {
          oss << "POP R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::POST:
        {
          oss << "POST R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::TRACE:
        {
          oss << "TRACE R" << reg(pc) << " % " << cur_pc << "\n";
        }
          break;
        case BytecodeStream::ABORT:
        {
          oss << "ABORT" << " % " << cur_pc << "\n";
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
  Interpreter::pushConstraint(Constraint* c) {
    _agg.back().constraints.push_back(c);
  }
    
  PropStatus
  Interpreter::subscribe(Constraint* c) {
    if (c->pred() < primitiveMap().size()) {
      return primitiveMap()[c->pred()]->subscribe(*this, c);
    }
    return PS_OK;
  }
  void
  Interpreter::unsubscribe(Constraint* c) {
    if (c->pred() < primitiveMap().size()) {
      primitiveMap()[c->pred()]->unsubscribe(*this, c);
    }
  }
  void
  Interpreter::schedule(Constraint* c, const Variable::SubscriptionEvent& ev) {
    if (!c->scheduled()) {
      _propQueue.push_back(c);
      c->scheduled(true);
    }
  }
  void
  Interpreter::deschedule(Constraint* c) {
    /// TODO: is this required? seems to be unused
    if (c->scheduled()) {
      auto it = std::find(_propQueue.begin(), _propQueue.end(), c);
      if (it != _propQueue.end()) {
        _propQueue.erase(it);
      }
    }
  }
  void
  Interpreter::propagate(void) {
    while (!_propQueue.empty()) {
      Constraint* c = _propQueue.front();
      _propQueue.pop_front();
      c->scheduled(false);
      auto ps = primitiveMap()[c->pred()]->propagate(*this, c);
      switch (ps) {
        case PS_OK:
          break;
        case PS_FAILED:
          _status = INCONSISTENT;
          return;
        case PS_ENTAILED:
          // TODO: remove constraint
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
        case BytecodeStream::CLEAR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          assert(r1<=r2);
          for (int i=r1; i<=r2; i++) {
            frame->reg.assign(this, i, IntVal(0));
          }
          DBG_INTERPRETER("CLEAR " << " R" << r1 << " " << r2 << "\n");
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
          } else if (v.isVar()) {
            frame->reg.assign(this, r2, IntVal(0));
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
          assert(frame->reg[r1].isInt() || frame->reg[r1].isVec());
          frame->reg.assign(this, r2, IntVal(frame->reg[r1].isInt() || frame->reg[r1].size()==0));
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
        case BytecodeStream::GET_VEC_NDIM:
        {
          IntVal n = frame->bs->intval(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("GET_VEC_NDIM " << n << "R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          std::vector<IntVal> idx(n.toInt());
          for (int i=0; i<n; i++) {
            int rr = frame->bs->reg(frame->pc);
            DBG_INTERPRETER(" R" << rr  << "(" << frame->reg[rr].toString(DBG_TRIM_OUTPUT) << ")");
            idx[i] = frame->reg[rr]();
          }
          int r_res = frame->bs->reg(frame->pc);
          int r_cond = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          assert(frame->reg[r1].size()==2);
          assert(frame->reg[r1][0].isVec());
          assert(frame->reg[r1][1].isVec());

          std::vector<std::pair<IntVal,IntVal>> dimensions;
          IntVal realdim = 1;
          for (int i=0; i<frame->reg[r1][1].size(); i+=2) {
            IntVal a = frame->reg[r1][1][i]();
            IntVal b = frame->reg[r1][1][i+1]();
            dimensions.emplace_back(a,b);
            realdim *= b-a+1;
          }

          bool success = true;
          IntVal realidx = 0;
          for (int i=0; i<idx.size(); i++) {
            IntVal ix = idx[i];
            if (ix < dimensions[i].first || ix > dimensions[i].second) {
              success = false;
              break;
            }
            realdim /= dimensions[i].second-dimensions[i].first+1;
            realidx += (ix-dimensions[i].first)*realdim;
          }
          assert(realidx >= 0 && realidx < frame->reg[r1][0].size());

          Val v(IntVal(0));
          if (success) {
            v = Val::follow_alias(frame->reg[r1][0][realidx.toInt()], this);
          }

          frame->reg.assign(this, r_res, v);
          frame->reg.assign(this, r_cond, Val(success));
          DBG_INTERPRETER(" R" << r_res <<  "(" << v.toString(DBG_TRIM_OUTPUT) << ") R" << r_cond<< "(" << Val(success).toString(DBG_TRIM_OUTPUT) << ")"<<  "\n");
        }
          break;
        case BytecodeStream::LB:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LB R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          frame->reg.assign(this, r2, v.lb());
          DBG_INTERPRETER(" R" << r2 <<  "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::UB:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("UB R" << r1  << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          Val v = Val::follow_alias(frame->reg[r1], this);
          frame->reg.assign(this, r2, v.ub());
          DBG_INTERPRETER(" R" << r2 <<  "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
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
          } else if (v.isVar()) {
            Variable* var = v.toVar();
            frame->reg.assign(this, r2, Val(var->domain()));
          } else {
            throw Error("Error: dom on invalid type");
          }
          DBG_INTERPRETER(" R" << r2 <<  "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
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
          result_val = Val(Vec::a(this, newIdent(), result));
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
          Val v2 = frame->reg[r2];
          assert(v2.isVec());

          Val result_val;
          Vec* s2 = v2.toVec();
          if (v1.isVar()) {
            Vec* s1 = v1.toVar()->domain();
            VecSetRanges vsr1(s1);
            VecSetRanges vsr2(s2);
            Ranges::Inter<IntVal,VecSetRanges,VecSetRanges> inter(vsr1,vsr2);
            std::vector<Val> result;
            for (; inter(); ++inter) {
              result.emplace_back(inter.min());
              result.emplace_back(inter.max());
            }
            if (result.empty()) {
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
              break;
            }
            v1.toVar()->domain(this, result, true);
            Val v1a = Val::follow_alias(v1);
            if (v1a.isVar()) {
              result_val = Val(v1a.toVar()->domain());
            } else {
              result_val = Val(Vec::a(this, newIdent(), {v1a(),v1a()}));
            }
          } else {
            Ranges::Const<IntVal> vsr1(v1(),v1());
            VecSetRanges vsr2(s2);
            if (Ranges::subset(vsr1, vsr2)) {
              result_val = Val(Vec::a(this, newIdent(), {v1(),v1()}));
            } else {
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
              break;
            }
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
        }
          break;
        case BytecodeStream::RET:
        {
          DBG_INTERPRETER("RET");
          if (!frame->cse_info.empty()) {
            DBG_INTERPRETER(" %-- " << std::get<0>(frame->cse_info.back()) << " (" << _procs[std::get<0>(frame->cse_info.back())].name << ") " << BytecodeProc::mode_to_string[std::get<1>(frame->cse_info.back())]);
          }
          DBG_INTERPRETER("\n");
          assert(!_stack.empty());
execute_ret:
          if (_stack.size()==1) {
            // Always leave final frame on the stack
            // Copy remaining constraints into toplevel
            assert(_agg.size()==1);
            for (Constraint* c : _agg[0].constraints) {
              root()->addDefinition(this, c);
            }
            _agg[0].constraints.clear();
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
            if (mode==BytecodeProc::RAW) {
              assert(code==PrimitiveMap::MK_INTVAR); // The only RAW primitive!
              Variable* v = Variable::a(this, args[0], true, newIdent());
              pushAgg(Val(v), -1);
            } else {
              assert(mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG);
              auto c = Constraint::a(this, code, mode, args);
              if (!(c.first || c.second)) {
                // Propagation failed
                _status = INCONSISTENT;
                // Invariant: Last instruction in the frame is always an ABORT instruction
                frame->pc = frame->bs->size()-1;
                break;
              }
              if (c.first){
                pushConstraint(c.first);
              }
              if (cse_suited) {
                Val ret(c.second);
                cse_insert(code, cse_key, mode, ret);
              }
              /// TODO: delayed calls
  //            if (_procs[code].delay) {
  //              delayed_calls.push_back(c);
  //            }
            }
          } else {
            _stack.emplace_back(_procs[code].mode[mode], code, mode);
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
          std::vector<Val> args(_procs[code].nargs);
          bool cse_suited = _procs[code].nargs < 5 && mode != BytecodeProc::RAW;
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
            assert(mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG);
            auto c = Constraint::a(this, code, mode, args);
            if (!(c.first || c.second)) {
              // Propagation failed
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
              break;
            }
            if (c.first){
              pushConstraint(c.first);
            }
            if (cse_suited) {
              Val ret(c.second);
              cse_insert(code, cse_key, mode, ret);
            }
            /// TODO: delayed calls
//            if (_procs[code].delay) {
//              delayed_calls.push_back(def);
//            }
            goto execute_ret;
          } else {
            // Replace frame with new procedure
            frame->bs = &_procs[code].mode[mode];
            frame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
            frame->pc = 0;
          }
        }
          break;
        case BytecodeStream::ITER_VEC:
        {
          int r1 = frame->bs->reg(frame->pc);
          int l = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("ITER_VEC " << r1  << " " << l << "\n");
          assert(frame->reg[r1].isVec());
          Vec* v(frame->reg[r1].toVec()); 
          _loops.push_back(LoopState(v, l));
        }
          break;
        case BytecodeStream::ITER_RANGE:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int lbl = frame->bs->reg(frame->pc);

          Val lb = Val::follow_alias(frame->reg[r1], this);
          Val ub = Val::follow_alias(frame->reg[r2], this);
          assert(lb.isInt());
          assert(ub.isInt());
          DBG_INTERPRETER("ITER_RANGE " << r1  << " " << r2 << " " << lbl << "\n");
          _loops.push_back(LoopState(lb().toInt(), ub().toInt(), lbl));
        }
          break;
        case BytecodeStream::ITER_NEXT:
        {
          int r1 = frame->bs->reg(frame->pc); 
          DBG_INTERPRETER("ITER_NEXT " << r1  << "\n");
          assert(_loops.size() > 0);
          LoopState& outer(_loops.back());
          if(outer.pos < outer.end) {
            Val v;
            if(outer.is_range) {
              v = IntVal(outer.pos);
              outer.pos++;
            } else {
              Val* ptr(reinterpret_cast<Val*>(outer.pos));
              v = Val::follow_alias(*ptr, this);
              outer.pos += sizeof(Val*);
            }
            frame->reg.assign(this, r1, v);
            DBG_INTERPRETER(" R" << r1 <<  "(" << v.toString(DBG_TRIM_OUTPUT) << ")" <<  "\n");
          } else {
            frame->pc = outer.exit_pc;
            _loops.pop_back();
          }
        }
          break;
        case BytecodeStream::ITER_BREAK:
        {
          int num = frame->bs->reg(frame->pc); 
          DBG_INTERPRETER("ITER_BREAK " << num  << "\n");
          assert(_loops.size() >= num);
          auto it(_loops.end() - num);
          frame->pc = it->exit_pc;
          _loops.erase(it, _loops.end());
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
            bool success = v1.toVar()->setVal(this, 1);
            if (!success) {
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame->pc = frame->bs->size()-1;
            }
            /// TODO: check why this is adding a ref
            /// Shouldn't be necessary
//            v1.toVar()->addRef(this);
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
          int r1 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER(" R" << r0 << "(" << frame->reg[r0].toString(DBG_TRIM_OUTPUT) << ")");
          DBG_INTERPRETER(" R" << r1 << "(" << frame->reg[r1].toString(DBG_TRIM_OUTPUT) << ")");
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          int r4 = frame->bs->reg(frame->pc);

          std::vector<Val> coeffs;
          std::vector<Val> vars;
          IntVal d;

          std::tie(coeffs, vars, d) = simplify_linexp(frame->reg[r0]);
          d = frame->reg[r1]()-d;

          if (coeffs.size()==0) {
            frame->reg.assign(this, r2, IntVal(0));
            frame->reg.assign(this, r3, IntVal(0));
          } else {
            Val coeffs_v = Val(Vec::allocate_array(this, newIdent(), coeffs));
            Val vars_v = Val(Vec::allocate_array(this, newIdent(), vars));
            frame->reg.assign(this, r2, coeffs_v);
            frame->reg.assign(this, r3, vars_v);
          }
          frame->reg.assign(this, r4, d);
          DBG_INTERPRETER(" R" << r2 << "(" << frame->reg[r2].toString(DBG_TRIM_OUTPUT) << ")");
          DBG_INTERPRETER(" R" << r3 << "(" << frame->reg[r3].toString(DBG_TRIM_OUTPUT) << ")\n");
          DBG_INTERPRETER(" R" << r4 << "(" << frame->reg[r4].toString(DBG_TRIM_OUTPUT) << ")\n");
        }
          break;
        case BytecodeStream::CLOSE_AGGREGATION:
        {
          DBG_INTERPRETER("CLOSE_AGGREGATION (" << AggregationCtx::symbol_to_string[_agg.back().symbol]  << ", depth "<< _agg.size() << ")\n");
          assert(!_agg.empty());
          // Decrement depth counter for current aggregation context
          _agg.back().n_symbols--;
          if (_agg.back().n_symbols==0) {
            // Result produced by this aggregation
            Variable* result = nullptr;

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
                  /// TODO: check, what if _agg.size()==2 as below?
                  // Conjunction is constant true or false
                  pushAgg(IntVal(!isFalse),-2);
                } else if (_agg.size()==2) {
                  // Push into root context
                  for (Val v : args) {
                    auto success = v.toVar()->setVal(this, 1);
                    //FIXME: Deal with unsuccessful setVal
                    assert(success);
                  }
                  pushAgg(IntVal(1), -2);

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
                  result = Variable::a(this,boolean_domain(),false, newIdent());
                  auto def_c = Constraint::a(this, PrimitiveMap::FORALL, BytecodeProc::ROOT, {Val(arr), Val(result)});
                  assert(def_c.first);
                  result->addRef(this);
                  result->addDefinition(this, def_c.first);
                  pushAgg(Val(result),-2);
                  RefCountedObject::rmRef(this, result);
                }
              }
                break;
              case AggregationCtx::VCTX_OR:
              {
                // Create a clause on the definition stack, and push a reference
                // to it onto the aggregation stack
                std::vector<Val> pos;
                std::vector<Val> neg;
                bool isTrue = false;
                for (int i=0; i<_agg.back().size(); i++) {
                  const Val& v = Val::follow_alias(_agg.back()[i]);
                  if (v.isInt()) {
                    if(v()!=0) {
                      // Disjunction is constant true
                      isTrue = true;
                      break;
                    }
                  } else {
                    Variable* cur = v.toVar();
                    if (Constraint* defby = cur->defined_by()) {
                      if (defby->pred() == PrimitiveMap::BOOLNOT) {
                        if (Val::follow_alias(defby->arg(0)) == v) {
                          neg.push_back(Val::follow_alias(defby->arg(1)));
                        } else {
                          neg.push_back(Val::follow_alias(defby->arg(0)));
                        }
                        continue;
                      }
                    }
                    pos.push_back(v);
                  }
                }
                if (isTrue || (pos.empty() && neg.empty())) {
                  // Disjunction is constant true or false
                  pushAgg(IntVal(isTrue),-2);
                } else if (pos.size() == 1 && neg.empty()) {
                  if (_agg.size()==2) {
                    auto success = pos[0].toVar()->setVal(this, 1);
                    //FIXME: Deal with unsuccessful setVal
                    assert(success);
                    pushAgg(IntVal(1), -2);
                  } else {
                    pushAgg(pos[0],-2);
                  }
                } else {
                  // Push into root context
                  Vec* vpos = Vec::allocate_array(this, newIdent(), pos);
                  Vec* vneg = Vec::allocate_array(this, newIdent(), neg);
                  /// TODO: check, why not EXISTS? Is it guaranteed that this will be processed further?
                  if (_agg.size() == 2) {
                    Vec* empty = Vec::allocate_array(this, newIdent(), {});
                    auto c = Constraint::a(this, PrimitiveMap::CLAUSE, BytecodeProc::ROOT, {Val(vpos), Val(vneg)});
                    assert(c.first);
                    root()->addDefinition(this, c.first);
                    pushAgg(IntVal(1), -2);
                  } else {
                    result = Variable::a(this,boolean_domain(),false, newIdent());
                    auto def_c = Constraint::a(this, PrimitiveMap::CLAUSE_REIF, BytecodeProc::ROOT, {Val(vpos), Val(vneg), Val(result)});
                    assert(def_c.first);
                    result->addRef(this);
                    result->addDefinition(this, def_c.first);
                    pushAgg(Val(result),-2);
                    RefCountedObject::rmRef(this, result);
                  }
                }
              }
                break;
              case AggregationCtx::VCTX_VEC:
              {
                // Create a vector on the aggregation stack
                _agg[_agg.size()-2].push(this,_agg.back().createVec(this,newIdent()));
              }
                break;
              case AggregationCtx::VCTX_OTHER:
                // When closing a VCTX_OTHER context, it should contain at most one value
                assert(_agg.back().size()<=1);
                if (_agg.back().size()==1) {
                  if (_agg.back()[0].isVar()) {
                    result = _agg.back()[0].toVar();
                  }
                  // push value onto surrounding context
                  pushAgg(_agg.back()[0],-2);
                }
                break;
            }
            
            _agg.back().destroyStack(this);
            if (result && !_agg.back().constraints.empty()) {
              // Aggregation produced constraints and exactly one return value, which is a variable
              if (result->timestamp() >= _agg.back().def_ident_start) {
                // the definition was produced by the current frame, so
                // attach all constraints to it
                for (Constraint* c : _agg.back().constraints) {
                  result->addDefinition(this, c);
                }
                _agg.back().constraints.clear();
                // INVARIANT: The result of aggregation is not referenced by any of the registers.
                assert(std::none_of(frame->reg.cbegin(), frame->reg.cend(), [result](Val v) { return v.contains(Val(result)); }));
                result->binding(this, false);
              }
            }
            if (!_agg.back().constraints.empty()) {
              if (_agg.size()==2) {
                // only one frame left, so move all constraints into root context
                for (Constraint* c : _agg.back().constraints) {
                  root()->addDefinition(this, c);
                }
              } else {
                // Move definitions to parent aggregation
                for (Constraint* c : _agg.back().constraints) {
                  _agg[_agg.size()-2].constraints.push_back(c);
                }
              }
              _agg.back().constraints.clear();
            }
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
    Variable::dump(root(), _procs, os);
//    if (!_agg.empty()) {
//      Definition::dump(_agg.back().def_stack, _procs, os, true);
//    }
  }
  void
  Interpreter::dumpState() { dumpState(std::cerr); }

  Interpreter::~Interpreter(void) {
    globals.destroy(this);
    for (auto& f : _stack) {
      f.destroy(this);
    }
    for (auto& a : _agg) {
      a.destroyStack(this);
//      a.destroyDef(this); /// TODO: replace with what? Just delete all constraints?
    }
    for (auto &table : cse) {
      table.destroy(this);
    }
    RefCountedObject::rmRef(this, infinite_dom);
    RefCountedObject::rmRef(this, boolean_dom);
    RefCountedObject::rmRef(this, true_dom);
  }
  
  void
  Interpreter::call(int code, const BytecodeProc::Mode& mode0, const std::vector<Val>& args0, bool delayed) {
    /// TODO!
//    if (_status != ROGER) {
//      return;
//    }
//    BytecodeProc::Mode mode = mode0;
//    std::vector<Val> args = args0;
//    assert(code >= 0);
//    assert(code < _procs.size());
//    int n = _procs[code].nargs;
//    DBG_INTERPRETER("Interpreter::call " << BytecodeProc::mode_to_string[mode] << " " << code << "(" << _procs[code].name << ")" << "\n");
//    // TODO: See if args is created when not necessary
//    assert(n == args.size());
//    bool cse_suited = n < 5 && mode != BytecodeProc::RAW && !delayed;
//    CSETable::Key cse_key;
//    if (cse_suited) {
//      cse_key = CSETable::Key(args);
//      // Lookup item in CSE
//      auto lookup = cse_lookup(code, cse_key, mode);
//      if (lookup.second) {
//        cse_key.destroy();
//        if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
//          assert(lookup.first.isInt());
//          if (lookup.first().toInt() != 1) {
//            _status = INCONSISTENT;
//            // Invariant: Last instruction in the frame is always an ABORT instruction
//            _stack.back().pc = _stack.back().bs->size()-1;
//          }
//        } else {
//          pushAgg(lookup.first, -1);
//        }
//        return;
//      }
//    }
//    if (_procs[code].mode[mode].size() == 0) {
//      DBG_INTERPRETER("--- FZN Builtin\n");
//      // this is a FlatZinc builtin
//      int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
//      assert (mode == BytecodeProc::RAW || mode == BytecodeProc::ROOT);
//      Definition* def = Definition::a(this,nullptr,false,code,mode,args,ident);
//      pushDef(def);
//      if (cse_suited) {
//        Val v = (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) ? Val(1) : Val(def);
//        cse_insert(code, cse_key, mode, v);
//      }
//      if (ident >= 0) {
//        pushAgg(Val(def), -1);
//      }
//      return;
//    } else {
//      // Ensure the last RET is next on the program counter
//      _stack.back().pc--;
//      _stack.emplace_back(_procs[code].mode[mode]);
//      BytecodeFrame* newFrame = &_stack[_stack.size()-1];
//      newFrame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
//      newFrame->reg.mov(this, args);
//      return run();
//    }
  }

  bool Interpreter::runDelayed() {
    /// TODO!
//    std::vector<Definition*> wave = std::move(delayed_calls);
//    delayed_calls.clear();
//    for (auto def : wave) {
//      if (def->exists()) {
//        auto mode = static_cast<BytecodeProc::Mode>(def->mode());
//        std::vector<Val> args(def->size());
//        for (int i = 0; i < def->size(); ++i) {
//          args[i] = def->arg(i);
//        }
//        call(def->pred(), mode, args, true);
//        if (_status != ROGER) {
//          break;
//        }
//        Val ret(1);
//        if (mode != BytecodeProc::ROOT && mode != BytecodeProc::ROOT_NEG) {
//          ret = _agg.back().back();
//          assert(ret.isDef() || ret.isInt());
//        }
//        def->alias(this, ret);
//      }
//      RefCountedObject::rmWRef(this, def);
//    }
//    return !delayed_calls.empty();
    return false;
  }

  size_t Trail::save_state(MiniZinc::Interpreter* interpreter) {
    trail_size.emplace_back(var_list_trail.size(), obj_trail.size(), alias_trail.size(), domain_trail.size(), def_trail.size());
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
    size_t vlt_size, ot_size, at_size, dt_size, deft_size;
    std::tie(vlt_size, ot_size, at_size, dt_size, deft_size) = trail_size.back(); trail_size.pop_back();
    int timestamp = timestamp_trail.back(); timestamp_trail.pop_back();
    // Reconstruct destroyed items
    while(obj_trail.size() > ot_size) {
      auto obj = obj_trail.back();
      switch (obj->rcoType()) {
        case RefCountedObject::VAR:
          static_cast<Variable*>(obj)->reconstruct(interpreter);
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
    while (var_list_trail.size() > vlt_size) {
      auto entry = var_list_trail.back();
      *entry.first = entry.second;
      var_list_trail.pop_back();
    }
    // Restore original definitions for created aliases
    while (alias_trail.size() > at_size) {
      Variable* var;
      Val dom;
      std::tie(var, dom) = alias_trail.back();
      var->unalias(interpreter, dom);
      dom.removeWeakRef(interpreter);
      alias_trail.pop_back();
    }
    // Restore original domains
    while (domain_trail.size() > dt_size) {
      Variable* var;
      Vec* dom;
      std::tie(var, dom) = domain_trail.back();
      Val nd(dom);
      nd.construct(interpreter);
      var->_domain.destroy(interpreter);
      var->_domain = nd;
      RefCountedObject::rmWRef(interpreter, dom);
      domain_trail.pop_back();
    }
    // Remove all additions/changes to the CSE table
    for (auto &table : interpreter->cse) {
      table.pop(interpreter);
    }
    /// TODO: Remove all newly created variables
//    if (stack->prev() != back) {  // If last element on the stack changed
//      do {
//        Variable* rem = back;
//        back = back->prev();
//        rem->destroy(interpreter);
//        Variable::free(rem);
//      } while (back->next() != back);
//    }
    // TODO: Should we remove newly created propagators??
    // Reset the timestamp count to its previous value
    interpreter->_identCount = timestamp;
    last_operation_pop = true;
  }
  
  void
  Interpreter::optimize(void) {
  }
  
}
