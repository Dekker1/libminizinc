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

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <streambuf>

//#define DBG_INTERPRETER(msg) std::cerr << msg
#define DBG_INTERPRETER(msg) do {} while(0)

namespace MiniZinc {

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
      for (unsigned int i=0; i<d->size(); i++) {
        os << d->arg(i).toString();
        if (i<d->size()-1)
          os << ", ";
      }
      os << ")";
      os << " domain: " << d->domain().toString() << "\n";
      if (!d->subscriptions().empty()) {
        os << "   subscriptions: ";
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

  Definition::Definition(Interpreter* interpreter, Val domain,int pred,char mode,const std::vector<Val>& args,int ident,Val ann)
  : RefCountedObject(RefCountedObject::DEF,ident), _prev(this), _next(this),
  _domain(domain), _ann(ann), _defs(nullptr), _pred(pred), _size(args.size()), _flag(0), _mode(mode) {
    _domain.construct(interpreter);
    _ann.construct(interpreter);
    for (unsigned int i=0; i<args.size(); i++) {
      new (&_args[i]) Val(args[i]);
      _args[i].construct(interpreter);
    }
    interpreter->subscribe(this);
  }

  
  void Definition::destroy(MiniZinc::Interpreter* interpreter)  {
    assert(_ref_count == 0);
    _ref_count = (1u<<31u)-1u;
    if (_defs) {
      Definition* cur = _defs->next();
      while (cur != _defs) {
        if (cur->_ref_count > 0) {
          // Promote cur to parent level
          cur->unlink(interpreter);
          cur->insertBefore(interpreter, this->next());
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
        cur = cur->next();
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
      {"int_eq", "int_ne"}
  };

  Model* Definition::toFZN(Interpreter* interpreter, Definition* head, const std::vector<BytecodeProc>& bs, Model* model) {
    GCLock lock;
    auto fzn = model ? model : new Model();
    if (head->next()==head)
      return fzn;
    Definition* d = head->next(); // Ignore dummy head
    std::map<int, VarDecl*> vdmap;
    while (d != head) {
      if (d->pred() == 0) {
        d = d->next();
        continue;
      }
      BytecodeProc proc = bs[d->pred()];
      auto mode = static_cast<BytecodeProc::Mode>(d->mode());
      std::string name = proc.name;
      if (BytecodeProc::is_neg(mode)) {
        auto it = negated_constraints.find(name);
        assert(it != negated_constraints.end());
        name = it->second;
      }
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
      if (proc.name == "mk_intvar") {
        // Construct domain
        auto ti = new TypeInst(Location().introduce(), Type::varint(), dom_set);
        auto vd = new VarDecl(Location().introduce(), ti, d->timestamp());
        vd->addAnnotation(constants().ann.output_var);
        auto vdi = new VarDeclI(Location().introduce(), vd);
        auto ret = vdmap.emplace(d->timestamp(), vd);
        fzn->addItem(vdi);
      } else if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        std::vector<Expression*> args(proc.nargs);
        for (int i = 0; i < proc.nargs; ++i) {
          Val v = Val::follow_alias(interpreter, d->arg(i));
          args[i] = v.toFZN(vdmap);
        }
        auto c = new Call(Location().introduce(), name, args);
        auto ci = new ConstraintI(Location().introduce(), c);
        fzn->addItem(ci);
      } else {
        auto ti = new TypeInst(Location().introduce(), Type::varint(), dom_set);
        auto vd = new VarDecl(Location().introduce(), ti, d->timestamp());
        fzn->addItem(new VarDeclI(Location().introduce(), vd));
        auto ret = vdmap.emplace(d->timestamp(), vd);

        std::vector<Expression*> args(proc.nargs + 1);
        for (int i = 0; i < proc.nargs; ++i) {
          Val v = Val::follow_alias(interpreter, d->arg(i));
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
        fzn->addItem(new ConstraintI(Location().introduce(), c));
      }
      if (d->defs())
        toFZN(interpreter, d->defs(), bs, fzn);
      d = d->next();
    }
    return fzn;
  }

  void Definition::alias(Interpreter* interpreter, Val v) {
    assert(size() >= 1);
    // Destroy old definition
    auto ref_count = _ref_count;
    _ref_count = (1u<<31u)-1u;
    if (_defs) {
      Definition* cur = _defs->next();
      while (cur != _defs) {
        if (cur->_ref_count > 0) {
          // Promote cur to parent level
          cur->unlink(interpreter);
          cur->insertBefore(interpreter, this->next());
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
        cur = cur->next();
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
    for (unsigned int i=0; i<_size; i++) {
      _args[i].destroy(interpreter);
    }
    _ref_count = ref_count;

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
    interpreter->trail.trail_alias(this);
    _pred = PrimitiveMap::ALIAS;
    _size = 1;
    _args[0] = v;
    v.construct(interpreter);
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
  Definition::domain(Interpreter* interpreter, Val newDomain) {
    _domain.destroy(interpreter);
    _domain = newDomain;
    _domain.construct(interpreter);
    bool assigned = newDomain.toVec()->size()==2 && (*newDomain.toVec())[0]()==(*newDomain.toVec())[1]();
    interpreter->schedule(this, assigned ? Definition::SEV_VAL : Definition::SEV_DOM);
  }
  void
  Definition::domain(Interpreter* interpreter, const std::vector<Val>& newDomain) {
    if (_domain.isInt()) {
      domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), newDomain)));
    } else {
      bool did_update = false;
      Vec* d = _domain.toVec();
      if (newDomain.size() != d->size()) {
        did_update = true;
      } else {
        for (unsigned int i=0; i<newDomain.size(); i++) {
          if (newDomain[i]() != (*d)[i]()) {
            did_update = true;
            break;
          }
        }
      }
      if (did_update) {
        domain(interpreter, Val(Vec::a(interpreter, interpreter->newIdent(), newDomain)));
      }
    }
  }
  
  const std::string BytecodeProc::mode_to_string[] = { "RAW", "ROOT", "ROOT_NEG", "FUN", "FUN_NEG", "IMP", "IMP_NEG" };
  
  std::string
  Val::toString(void) const {
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
      for (unsigned int i=0; i<size(); i++) {
        oss << (*this)[i].toString();
        if (i<size()-1)
          oss << ",";
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

  Val Val::follow_alias(Interpreter* interpreter, const Val& v) {
    if (v.isDef() && v.toDef()->pred() == PrimitiveMap::ALIAS) {
      Val nval = v;
      while(nval.isDef() && nval.toDef()->pred() == PrimitiveMap::ALIAS) {
        assert(nval.toDef()->size() > 0);
        nval = nval.toDef()->arg(0);
      }
      auto mut_v = const_cast<Val&>(v);
      mut_v.destroy(interpreter);
      mut_v._v = nval._v;
      mut_v.construct(interpreter);
      return nval;
    } else {
      return v;
    }
  }


    CSETable::Key::Key(const std::vector<Val> &vec) {
    _size = vec.size();
    for (const auto& val : vec) {
      if (val.isVec() && val.size() <= 3) {
        _size += val.size();
      }
    }
    if (_size > 0) {
     _vals = (WeakVal*) malloc(_size*sizeof(WeakVal));
      size_t i = 0;
      for (const auto& val : vec) {
        if (val.isVec() && val.size() <= 3) {
          _vals[i++] = WeakVal(Val(val.size()));
          for (int j = 0; j < val.size(); ++j) {
            assert(!val[j].isVec());
            _vals[i++] = WeakVal(val[j]);
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
      Val val = Val::follow_alias(interpreter, it->second.second);
      BytecodeProc::Mode val_m = it->second.first;
      if (!val.exists()) {
        this->_table[i].erase(it);
        return {Val(), false};
      }
      DBG_INTERPRETER("--- CSE hit! hash(" << key.hash() << ") -> Mode: " << BytecodeProc::mode_to_string[val_m] << " Value: " << val.toString() << "\n");
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
              auto d = Definition::a(interpreter, IntVal(0), PrimitiveMap::BOOLNOT, BytecodeProc::FUN, {v}, interpreter->newIdent());
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
    DBG_INTERPRETER("--- CSE add: hash(" << key.hash() << ") -> Mode: " << BytecodeProc::mode_to_string[mode] << " Value: " << val.toString() << "\n");
    // If value is reference counted, flag that it's in CSE
    val.addToCSE(interpreter);
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
              auto negation = Definition::a(interpreter, IntVal(0), PrimitiveMap::BOOLNOT, BytecodeProc::FUN, {val}, interpreter->newIdent());
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
      oldVal.removeFromCSE(interpreter);
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
          // TODO: fail entire interpreter state
          break;
        case PrimitiveMap::Primitive::PS_ENTAILED:
          // TODO: remove definition
          break;
      }
    }
  }
  
  void
  Interpreter::run(void) {
    BytecodeFrame* frame = &_stack.back();
    for (;;) {
      DBG_INTERPRETER(frame->pc << " ");
      switch (frame->bs->instr(frame->pc)) {
        case BytecodeStream::ADDI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, frame->reg[r1]() + frame->reg[r2]());
          DBG_INTERPRETER("ADDI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::SUBI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, frame->reg[r1]() - frame->reg[r2]());
          DBG_INTERPRETER("SUBI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::MULI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, frame->reg[r1]() * frame->reg[r2]());
          DBG_INTERPRETER("MULI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::DIVI:
        {
          assert(false);
        }
          break;
        case BytecodeStream::MODI:
        {
          assert(false);
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
          DBG_INTERPRETER("LOAD_GLOBAL " << i << " R" << r1 << "(" << frame->reg[r1].toString() << ")" << "\n");
        }
          break;
        case BytecodeStream::STORE_GLOBAL:
        {
          int r1 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          frame->reg.cp(this, r1, globals, i);
          DBG_INTERPRETER("STORE_GLOBAL R" << r1 << "(" << frame->reg[r1].toString() << ")" << " " << i << "\n");
        }
          break;
        case BytecodeStream::MOV:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("MOV R" << r1 << " R" << r2 << "\n");
          frame->reg.cp(this, r1,r2);
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
          DBG_INTERPRETER("JMPIFNOT R" << r0 << " " << i << "\n");
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
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() == frame->reg[r2]()));
          DBG_INTERPRETER("EQI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LTI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() < frame->reg[r2]()));
          DBG_INTERPRETER("LTI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LEI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]() <= frame->reg[r2]()));
          DBG_INTERPRETER("LEI R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::AND:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]()!=0 && frame->reg[r2]()!=0));
          DBG_INTERPRETER("AND R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::OR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, IntVal(frame->reg[r1]()!=0 || frame->reg[r2]()!=0));
          DBG_INTERPRETER("OR R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::NOT:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r2, IntVal(frame->reg[r1]()==0));
          DBG_INTERPRETER("NOT R" << r1 << " R" << r2 << "(" << frame->reg[r2]() << ")" << "\n");
        }
          break;
        case BytecodeStream::XOR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(this, r3, IntVal( (frame->reg[r1]()!=0) ^ (frame->reg[r2]()!=0)));
          DBG_INTERPRETER("XOR R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISPAR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          Val v = Val::follow_alias(this, frame->reg[r1]);
          if (v.isInt()) {
            frame->reg.assign(this, r2, IntVal(1));
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isVec() && def->domain().toVec()->size()==2 &&
                (*def->domain().toVec())[0]==(*def->domain().toVec())[1]) {
              frame->reg.assign(this, r1, (*def->domain().toVec())[0]);
              frame->reg.assign(this, r2, IntVal(1));
            } else {
              frame->reg.assign(this, r2, IntVal(0));
            }
          } else {
            assert(v.isVec());
            IntVal ret = IntVal(1);
            for (int i = 0; i < v.size(); ++i) {
              assert(!v[i].isVec());
              if (v[i].isDef()) {
                // TODO: Fix domain check when we have domains!
                ret = IntVal(0);
                break;
              }
            }
            frame->reg.assign(this, r2, ret);
          }
          DBG_INTERPRETER("ISPAR R" << r1  << "(" << frame->reg[r1]() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISEMPTY:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          frame->reg.assign(this, r2, IntVal(frame->reg[r1].size()==0));
          DBG_INTERPRETER("ISEMPTY R" << r1  << "(" << frame->reg[r1].toString() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LENGTH:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          frame->reg.assign(this, r2, IntVal(frame->reg[r1].size()));
          DBG_INTERPRETER("LENGTH R" << r1  << "(" << frame->reg[r1].toString() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          assert(frame->reg[r2].isInt());
          assert(frame->reg[r2]() > 0 && frame->reg[r2]() <= frame->reg[r1].size());
          DBG_INTERPRETER("GET_VEC R" << r1  << "(" << frame->reg[r1].toString() << ")" << " R" << r2  << "(" << frame->reg[r2]() << ")");
          Val v = Val::follow_alias(this, frame->reg[r1][frame->reg[r2]().toInt()-1]);
          frame->reg.assign(this, r3, v);
          DBG_INTERPRETER(" R" << r3 <<  "(" << v.toString() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LB:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("LB R" << r1  << "(" << frame->reg[r1].toString() << ")");
          Val v = Val::follow_alias(this, frame->reg[r1]);
          if (v.isInt()) {
            frame->reg.assign(this, r2, v);
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isVec()) {
              Val lb = (*def->domain().toVec())[0];
              frame->reg.assign(this, r2, lb);
              DBG_INTERPRETER(" R" << r2 <<  "(" << lb.toString() << ")" <<  "\n");
            } else {
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
          DBG_INTERPRETER("UB R" << r1  << "(" << frame->reg[r1].toString() << ")");
          Val v = Val::follow_alias(this, frame->reg[r1]);
          if (v.isInt()) {
            frame->reg.assign(this, r2, v);
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isVec()) {
              Val ub = (*def->domain().toVec())[def->domain().toVec()->size()-1];
              frame->reg.assign(this, r2, ub);
              DBG_INTERPRETER(" R" << r2 <<  "(" << ub.toString() << ")" <<  "\n");
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
          DBG_INTERPRETER("DOM R" << r1  << "(" << frame->reg[r1].toString() << ")");
          Val v = Val::follow_alias(this, frame->reg[r1]);
          if (v.isInt()) {
            frame->reg.assign(this, r2, Val(Vec::a(this, newIdent(), {v,v})));
          } else if (v.isDef()) {
            Definition* def = v.toDef();
            if (def->domain().isVec()) {
              frame->reg.assign(this, r2, def->domain());
              DBG_INTERPRETER(" R" << r2 <<  "(" << def->domain().toString() << ")" <<  "\n");
            } else {
              throw Error("Error: dom on unbounded variable");
            }
          } else {
            throw Error("Error: dom on invalid type");
          }
        }
          break;
        case BytecodeStream::INTERSECTION:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("INTERSECTION R" << r1  << "(" << frame->reg[r1].toString() << ") R" << r2 << "(" << frame->reg[r2].toString() << ")");
          Val v1 = Val::follow_alias(this, frame->reg[r1]);
          Val v2 = Val::follow_alias(this, frame->reg[r2]);
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
              result.push_back(inter.min());
              result.push_back(inter.max());
            }
            result_val = Val(Vec::a(this, newIdent(), result));
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::UNION:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("UNION R" << r1  << "(" << frame->reg[r1].toString() << ") R" << r2 << "(" << frame->reg[r2].toString() << ")");
          Val v1 = Val::follow_alias(this, frame->reg[r1]);
          Val v2 = Val::follow_alias(this, frame->reg[r2]);
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
              result.push_back(union_r.min());
              result.push_back(union_r.max());
            }
            result_val = Val(Vec::a(this, newIdent(), result));
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::INTERSECT_DOMAIN:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("INTERSECT_DOMAIN R" << r1  << "(" << frame->reg[r1].toString() << ") R" << r2 << "(" << frame->reg[r2].toString() << ")");
          Val v1 = Val::follow_alias(this, frame->reg[r1]);
          Val v2 = Val::follow_alias(this, frame->reg[r2]);

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
              v1.toDef()->domain(this, result_val);
            }
          } else if (v2.isVec()) {
            Vec* s1 = dom_val.toVec();
            Vec* s2 = v2.toVec();
            VecSetRanges vsr1(s1);
            VecSetRanges vsr2(s2);
            Ranges::Inter<IntVal,VecSetRanges,VecSetRanges> inter(vsr1,vsr2);
            std::vector<Val> result;
            for (; inter(); ++inter) {
              result.push_back(inter.min());
              result.push_back(inter.max());
            }
            v1.toDef()->domain(this, result);
            result_val = v1.toDef()->domain();
          }
          frame->reg.assign(this, r3, result_val);
          DBG_INTERPRETER(" R" << r3 <<  "(" << result_val.toString() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::RET:
        {
          DBG_INTERPRETER("RET\n");
          assert(!_stack.empty());
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
          DBG_INTERPRETER("CALL " << BytecodeProc::mode_to_string[mode] << " " << code << "(" << _procs[code].name << ")" << "\n");
          // TODO: See if args is created when not necessary
          std::vector<Val> args(n);
          bool cse_suited = n < 5 && mode != BytecodeProc::RAW;
          for (int i=0; i<n; i++) {
            int r = frame->bs->reg(frame->pc);
            args[i] = frame->reg[r];
          }
          CSETable::Key cse_key;
          if (cse_suited) {
            cse_key = CSETable::Key(args);
            // Lookup item in CSE
            auto cse = cse_lookup(code, cse_key, mode);
            if (cse.second) {
              cse_key.destroy();
              if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
                assert(cse.first.isInt());
                if (cse.first().toInt() != 1) {
                  // TODO: The model is inconsistent!
                  throw Error("Error: Model Inconsistent!");
                }
              } else {
                pushAgg(cse.first, -1);
              }
              break;
            }
          }
          if (_procs[code].mode[mode].size() == 0 || _procs[code].delay) {
            DBG_INTERPRETER((_procs[code].delay ? "--- Delayed CALL\n" : "--- FZN Builtin\n"));
            // this is a FlatZinc builtin
            int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
            Definition* def = Definition::a(this,IntVal(0),code,mode,args,ident);
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
          assert(code < primitiveMap().size());
          // this is a FlatZinc builtin
          int n = _procs[code].nargs;
          std::vector<Val> args(n);
          for (int i=0; i<n; i++) {
            int r = frame->bs->reg(frame->pc);
            args[i].assign(this, frame->reg[r]);
          }
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
                  // TODO: The model is inconsistent!
                  throw Error("Error: Model Inconsistent!");
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
          // Replace frame with new procedure
          frame->bs = &_procs[code].mode[mode];
          frame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
          frame->pc = 0;
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
          return;
        }
        case BytecodeStream::PUSH:
        {
          int r = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("PUSH R" << r << " (" << frame->reg[r].toString() << ")\n");
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
          DBG_INTERPRETER("POP R" << r << " (" << frame->reg[r].toString() << ")\n");
          _agg.back().pop(this);
        }
          break;
        case BytecodeStream::OPEN_AGGREGATION:
        {
          int r = frame->bs->chr(frame->pc);
          DBG_INTERPRETER("OPEN_AGGREGATION " << r  << "\n");
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
          DBG_INTERPRETER("SIMPLIFY_LIN\n");

          int r0 = frame->bs->reg(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          if (frame->reg[r0].isInt()) {
            Val result = frame->reg[r0];
            frame->reg.assign(this, r3, result);
            frame->reg.assign(this, r1, Val(Vec::a(this, newIdent(), {})));
            frame->reg.assign(this, r2, Val(Vec::a(this, newIdent(), {})));
          } else {
            std::vector<Val> coeffs;
            std::vector<Val> vars;
            std::vector<int> idx;
            IntVal d = 0;
            std::vector<std::pair<IntVal,Val>> defs({std::make_pair(IntVal(1),frame->reg[r0])});
            while (!defs.empty()) {
              IntVal coeff = defs.back().first;
              Val stacktop = defs.back().second;
              defs.pop_back();
              if (stacktop.isInt()) {
                d += coeff*stacktop();
              } else {
                Definition* cur = stacktop.toDef();
                switch (cur->pred()) {
                  case PrimitiveMap::LINEXP:
                  {
                    for (unsigned int i=0; i<cur->arg(0).size(); i++) {
                      defs.emplace_back(coeff*cur->arg(0)[i](), cur->arg(1)[i]);
                    }
                    d += coeff*cur->arg(2)();
                  }
                    break;
                  case PrimitiveMap::INT_SUM:
                    for (unsigned int i=0; i<cur->arg(0).size(); i++) {
                      defs.emplace_back(coeff,cur->arg(0)[i]);
                    }
                    break;
                  case PrimitiveMap::INT_TIMES:
                    if (cur->arg(0).isInt()) {
                      if (cur->arg(1).isInt()) {
                        // both constants, compute result
                        d += coeff*cur->arg(0)()*cur->arg(1)();
                      } else {
                        defs.emplace_back(coeff*cur->arg(0)(), cur->arg(1));
                      }
                    } else if (cur->arg(1).isInt()) {
                      if (cur->arg(0).isInt()) {
                        // both constants, compute result
                        d += coeff*cur->arg(0)()*cur->arg(1)();
                      } else {
                        defs.emplace_back(coeff*cur->arg(1)(), cur->arg(0));
                      }
                    } else {
                      // Variable multiplication, don't aggregate
                      coeffs.emplace_back(coeff);
                      vars.emplace_back(cur);
                      idx.push_back(idx.size());
                    }
                    break;
                  default:
                    coeffs.emplace_back(coeff);
                    vars.emplace_back(cur);
                    idx.push_back(idx.size());
                    break;
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
                for (unsigned int i=0; i<coeffs_simple.size(); i++) {
                  coeffs[i] = coeffs_simple[i];
                  vars[i] = vars_simple[i];
                }
                coeffs.resize(coeffs_simple.size());
                vars.resize(vars_simple.size());
              }
            }
            
            Val coeffs_v = Val(Vec::a(this, newIdent(), coeffs));
            Val vars_v = Val(Vec::a(this, newIdent(), vars));
            frame->reg.assign(this, r1, coeffs_v);
            frame->reg.assign(this, r2, vars_v);
            frame->reg.assign(this, r3, d);
          }
        }
          break;
        case BytecodeStream::CLOSE_AGGREGATION:
        {
          DBG_INTERPRETER("CLOSE_AGGREGATION\n");
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
                for (unsigned int i=0; i<_agg.back().size(); i++) {
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
                } else {
                  result = Definition::a(this,IntVal(0),PrimitiveMap::FORALL,BytecodeProc::FUN,
                                         {Val(Vec::a(this,newIdent(),args))},newIdent());
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
                for (unsigned int i=0; i<_agg.back().size(); i++) {
                  const Val& v = _agg.back()[i];
                  if (v.isInt() && v()!=0) {
                    // Disjunction is constant true
                    isTrue = true;
                    break;
                  } else {
                    args.push_back(v);
                  }
                }
                if (isTrue || args.empty()) {
                  // Disjunction is constant true or false
                  pushAgg(IntVal(isTrue),-2);
                } else {
                  result = Definition::a(this,IntVal(0),PrimitiveMap::EXISTS,BytecodeProc::FUN,
                                         {Val(Vec::a(this,newIdent(),args))},newIdent());
                  pushAgg(Val(result),-2);
                }
              }
                break;
              case AggregationCtx::VCTX_VEC:
                // Create a vector on the aggregation stack
                assert(_agg[_agg.size()-2].symbol==AggregationCtx::VCTX_OTHER);
                _agg[_agg.size()-2].push(this,_agg.back().toVec(this,newIdent()));
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
                result->defs(this, defs);
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
    
  bool startsWith(const std::string& s, const std::string& t) {
    if (s.size()<t.size())
      return false;
    for (unsigned int i=0; i<t.size(); i++) {
      if (s[i] != t[i])
        return false;
    }
    return true;
  }
  
  bool instrR(const std::string& line, const std::string& op, int& r1) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    r1 = std::stoi(n);
    return true;
  }
  bool instrI(const std::string& line, const std::string& op, int& r1) {
    if (!startsWith(line, op+" "))
      return false;
    std::string n = line.substr(op.size()+1);
    r1 = std::stoi(n);
    return true;
  }
  bool instrS(const std::string& line, const std::string& op, std::string& r1) {
    if (!startsWith(line, op+" "))
      return false;
    r1 = line.substr(op.size()+1);
    return true;
  }
  bool instrRR(const std::string& line, const std::string& op, int& r1, int& r2) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    std::string n2 = n.substr(n.find(" R")+2);
    r2 = std::stoi(n2);
    return true;
  }
  bool instrIR(const std::string& line, const std::string& op, int& r1, int& r2) {
    if (!startsWith(line, op+" "))
      return false;
    std::string n = line.substr(op.size()+1);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    std::string n2 = n.substr(n.find(" R")+2);
    r2 = std::stoi(n2);
    return true;
  }
  bool instrRI(const std::string& line, const std::string& op, int& r1, int& r2) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    std::string n2 = n.substr(n.find(' ')+1);
    r2 = std::stoi(n2);
    return true;
  }
  bool instrRS(const std::string& line, const std::string& op, int& r1, std::string& rs) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    rs = n.substr(n.find(' ')+1);
    return true;
  }
  bool instrRRR(const std::string& line, const std::string& op, int& r1, int& r2, int& r3) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    std::string nn = n.substr(n.find(" R")+2);
    std::string n2 = nn.substr(0,nn.find(' '));
    r2 = std::stoi(n2);
    std::string n3 = nn.substr(nn.find(" R")+2);
    r3 = std::stoi(n3);
    return true;
  }
  bool instrRRRR(const std::string& line, const std::string& op, int& r1, int& r2, int& r3, int& r4) {
    if (!startsWith(line, op+" R"))
      return false;
    std::string n = line.substr(op.size()+2);
    std::string n1 = n.substr(0,n.find(' '));
    r1 = std::stoi(n1);
    std::string nn = n.substr(n.find(" R")+2);
    std::string n2 = nn.substr(0,nn.find(' '));
    r2 = std::stoi(n2);
    std::string nnn = nn.substr(nn.find(" R")+2);
    std::string n3 = nnn.substr(0,nnn.find(' '));
    r3 = std::stoi(n3);
    std::string n4 = nnn.substr(nnn.find(" R")+2);
    r4 = std::stoi(n4);
    return true;
  }

  std::vector<BytecodeProc> parse(const std::string& s) {

    std::vector<BytecodeProc> codes;
    std::unordered_map<std::string, int> procs;
    
    struct Patch {
      int code;
      BytecodeProc::Mode mode;
      std::vector<std::pair<int,std::string>> patch;
      Patch(int code0, BytecodeProc::Mode mode0, std::vector<std::pair<int, std::string>> patch0)
      : code(code0), mode(mode0), patch(std::move(patch0)) {}
    };
    
    std::vector<Patch> toPatch;

    // Initialise first slots with
    for (PrimitiveMap::Primitive* p : primitiveMap()) {
      BytecodeProc bcp;
      bcp.name = p->name();
      bcp.nargs = p->n_args();
      bcp.delay = false;
      DBG_INTERPRETER("add primitive " << bcp.name << " " << p->ident() << " " << p->n_args() << "\n");
      codes.push_back(bcp);
      procs.emplace(bcp.name, p->ident());
    }

    std::istringstream iss(s);
    std::string cur_proc;
    BytecodeProc::Mode cur_mode = BytecodeProc::RAW;
    BytecodeStream cur_code;
    int cur_proc_nargs = 0;
    bool cur_proc_delay = false;
    std::vector<std::pair<int,std::string> > cur_toPatch;
    std::vector<std::pair<int,std::string> > cur_labels;
    std::unordered_map<std::string, int> labels;
    for (std::string line; std::getline(iss,line); ) {
      if (line.empty() || line[0]=='%')
        continue;
      
      if (line[0]==':') {
        // this is the start of a new procedure
        if (!cur_proc.empty()) {
          // patch jumps with recorded labels
          for (auto& cl : cur_labels) {
            if (labels.find(cl.second)==labels.end())
              throw Error("Error: label "+cl.second+" not found\n");
            cur_code.patchAddress(cl.first, labels[cl.second]);
          }
          labels.clear();
          cur_labels.clear();

          auto it = procs.find(cur_proc);
          if (it != procs.end()) {
            BytecodeProc& bcp = codes[it->second];
            if (bcp.mode[cur_mode].size() > 0) {
              throw Error("Error: procedure "+cur_proc+" already defined before with the same mode\n");
            }
            if (bcp.nargs != cur_proc_nargs) {
              throw Error("Error: procedure "+cur_proc+" already defined before with different number of arguments\n");
            }
            bcp.mode[cur_mode] = cur_code;
            toPatch.emplace_back(it->second, cur_mode, cur_toPatch);
            cur_code = BytecodeStream();
            cur_toPatch.clear();
          } else {
            BytecodeProc bcp;
            bcp.name = cur_proc;
            bcp.mode[cur_mode] = cur_code;
            bcp.nargs = cur_proc_nargs;
            bcp.delay = cur_proc_delay;
            procs[cur_proc] = codes.size();
            toPatch.emplace_back(codes.size(), cur_mode, cur_toPatch);
            codes.push_back(bcp);
            cur_code = BytecodeStream();
            cur_toPatch.clear();
          }
        }
        size_t finalColon = line.find(':',1);
        cur_proc = line.substr(1,finalColon-1);
        size_t space = line.find(' ');
        std::string newMode = line.substr(finalColon+1, space-(finalColon+1));
        if (newMode=="RAW") {
          cur_mode = BytecodeProc::RAW;
        } else if (newMode=="ROOT") {
          cur_mode = BytecodeProc::ROOT;
        } else if (newMode=="ROOT_NEG") {
          cur_mode = BytecodeProc::ROOT_NEG;
        } else if (newMode=="FUN") {
          cur_mode = BytecodeProc::FUN;
        } else if (newMode=="FUN_NEG") {
          cur_mode = BytecodeProc::FUN_NEG;
        } else if (newMode=="IMP") {
          cur_mode = BytecodeProc::IMP;
        } else if (newMode=="IMP_NEG") {
          cur_mode = BytecodeProc::IMP_NEG;
        } else {
          cur_mode = BytecodeProc::FUN;
        }
        size_t space2 = line.find(' ', space+1);
        cur_proc_nargs = std::stoi(line.substr(space+1, space2-(space+1)));
        cur_proc_delay = false;
        if (space2 != std::string::npos) {
          std::string d = line.substr(space2+1);
          cur_proc_delay = (d == "d") || (d == "D");
          assert(cur_proc_delay);
        }
        continue;
      }
      
      size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::string label = line.substr(0,colon);
        labels[label] = cur_code.size();
        line = line.substr(colon+2);
      }
      if (cur_proc.empty()) {
        throw Error("Error: not in a procedure yet\n");
      }
      int r1, r2, r3, r4;
      std::string rs;
      if (instrRRR(line,"ADDI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::ADDI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"SUBI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::SUBI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"MULI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::MULI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"DIVI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::DIVI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"MODI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::MODI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrR(line,"INCI",r1)) {
        cur_code.addInstr(BytecodeStream::INCI);
        cur_code.addReg(r1);
      } else if (instrR(line,"DECI",r1)) {
        cur_code.addInstr(BytecodeStream::DECI);
        cur_code.addReg(r1);
      } else if (instrIR(line,"IMMI",r1,r2)) {
        cur_code.addInstr(BytecodeStream::IMMI);
        cur_code.addIntVal(r1);
        cur_code.addReg(r2);
      } else if (instrIR(line,"LOAD_GLOBAL",r1,r2)) {
        cur_code.addInstr(BytecodeStream::LOAD_GLOBAL);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRI(line,"STORE_GLOBAL",r1,r2)) {
        cur_code.addInstr(BytecodeStream::STORE_GLOBAL);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"MOV",r1,r2)) {
        cur_code.addInstr(BytecodeStream::MOV);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrS(line,"JMP",rs)) {
        cur_code.addInstr(BytecodeStream::JMP);
        cur_labels.emplace_back(cur_code.size(), rs);
        cur_code.addSmallInt(0); // placeholder
      } else if (instrRS(line,"JMPIF",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIF);
        cur_code.addReg(r1);
        cur_labels.emplace_back(cur_code.size(), rs);
        cur_code.addSmallInt(0); // placeholder
      } else if (instrRS(line,"JMPIFNOT",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIFNOT);
        cur_code.addReg(r1);
        cur_labels.emplace_back(cur_code.size(), rs);
        cur_code.addSmallInt(0); // placeholder
      } else if (instrRRR(line,"EQI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::EQI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"LTI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::LTI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"LEI",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::LEI);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"AND",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::AND);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"OR",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::OR);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRR(line,"NOT",r1,r2)) {
        cur_code.addInstr(BytecodeStream::NOT);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRRR(line,"XOR",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::XOR);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRR(line,"ISPAR",r1,r2)) {
        cur_code.addInstr(BytecodeStream::ISPAR);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"ISEMPTY",r1,r2)) {
        cur_code.addInstr(BytecodeStream::ISEMPTY);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"LENGTH",r1,r2)) {
        cur_code.addInstr(BytecodeStream::LENGTH);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRRR(line,"GET_VEC",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::GET_VEC);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRR(line,"LB",r1,r2)) {
        cur_code.addInstr(BytecodeStream::LB);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"UB",r1,r2)) {
        cur_code.addInstr(BytecodeStream::UB);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"DOM",r1,r2)) {
        cur_code.addInstr(BytecodeStream::DOM);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRRR(line,"INTERSECTION",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::INTERSECTION);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"UNION",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::UNION);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"INTERSECT_DOMAIN",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::INTERSECT_DOMAIN);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (line=="RET") {
        cur_code.addInstr(BytecodeStream::RET);
      } else if (startsWith(line,"CALL ")) {
        size_t cur_pos = line.find(' ');
        std::string n = line.substr(cur_pos+1);
        std::string mode = n.substr(0, n.find(' '));
        cur_code.addInstr(BytecodeStream::CALL);
        if (mode=="RAW") {
          cur_code.addCharVal(BytecodeProc::RAW);
        } else if (mode=="ROOT") {
          cur_code.addCharVal(BytecodeProc::ROOT);
        } else if (mode=="ROOT_NEG") {
          cur_code.addCharVal(BytecodeProc::ROOT_NEG);
        } else if (mode=="FUN") {
          cur_code.addCharVal(BytecodeProc::FUN);
        } else if (mode=="FUN_NEG") {
          cur_code.addCharVal(BytecodeProc::FUN_NEG);
        } else if (mode=="IMP") {
          cur_code.addCharVal(BytecodeProc::IMP);
        } else if (mode=="IMP_NEG") {
          cur_code.addCharVal(BytecodeProc::IMP_NEG);
        } else {
          throw Error("Invalid mode:\n"+line+"\n");
        }
        n = n.substr(n.find(' ')+1);
        std::string rs = n.substr(0, n.find(' '));
        cur_toPatch.emplace_back(cur_code.size(), rs);
        cur_code.addSmallInt(0); // placeholder
        n = n.substr(n.find(' '));
        std::vector<int> args;
        size_t pos = n.find(" R");
        while (pos != std::string::npos) {
          n = n.substr(pos+2);
          args.push_back(std::stoi(n.substr(0,n.find(' '))));
          pos = n.find(" R");
        }
        for(auto arg : args) {
          cur_code.addReg(arg);
        }
      } else if (startsWith(line,"TCALL ")) {
        cur_code.addInstr(BytecodeStream::TCALL);
        size_t cur_pos = line.find(' ');
        std::string n = line.substr(cur_pos+1);
        std::string mode = n.substr(0, n.find(' '));
        if (mode=="ROOT") {
          cur_code.addCharVal(BytecodeProc::ROOT);
        } else if (mode=="ROOT_NEG") {
          cur_code.addCharVal(BytecodeProc::ROOT_NEG);
        } else if (mode=="FUN") {
          cur_code.addCharVal(BytecodeProc::FUN);
        } else if (mode=="FUN_NEG") {
          cur_code.addCharVal(BytecodeProc::FUN_NEG);
        } else if (mode=="IMP") {
          cur_code.addCharVal(BytecodeProc::IMP);
        } else if (mode=="IMP_NEG") {
          cur_code.addCharVal(BytecodeProc::IMP_NEG);
        } else {
          throw Error("Invalid mode:\n"+line+"\n");
        }
        std::string n0 = n.substr(n.find(' ')+1);
        std::string rs1 = n0.substr(0, n0.find(' '));
        cur_toPatch.emplace_back(cur_code.size(), rs1);
        cur_code.addSmallInt(0); // placeholder
      } else if (startsWith(line,"BUILTIN ")) {
        cur_code.addInstr(BytecodeStream::BUILTIN);
        size_t cur_pos = line.find(' ');
        std::string n = line.substr(cur_pos+1);
        std::string rs1 = n.substr(0, n.find(' '));
        cur_toPatch.emplace_back(cur_code.size(), rs1);
        cur_code.addSmallInt(0); // placeholder
        n = n.substr(n.find(' '));
        std::vector<int> args;
        size_t pos = n.find(" R");
        while (pos != std::string::npos) {
          n = n.substr(pos+2);
          args.push_back(std::stoi(n.substr(0,n.find(' '))));
          pos = n.find(" R");
        }
        for(auto arg : args) {
          cur_code.addReg(arg);
        }
      } else if (instrR(line,"TRACE",r1)) {
        cur_code.addInstr(BytecodeStream::TRACE);
        cur_code.addReg(r1);
      } else if (instrS(line,"OPEN_AGGREGATION",rs)) {
        cur_code.addInstr(BytecodeStream::OPEN_AGGREGATION);
        if (rs=="AND") {
          cur_code.addCharVal(AggregationCtx::VCTX_AND);
        } else if (rs=="OR") {
          cur_code.addCharVal(AggregationCtx::VCTX_OR);
        } else if (rs=="VEC") {
          cur_code.addCharVal(AggregationCtx::VCTX_VEC);
        } else if (rs=="OTHER") {
          cur_code.addCharVal(AggregationCtx::VCTX_OTHER);
        } else {
          throw Error("Error: illegal context\n"+line);
        }
      } else if (line=="CLOSE_AGGREGATION") {
        cur_code.addInstr(BytecodeStream::CLOSE_AGGREGATION);
      } else if (instrRRRR(line, "SIMPLIFY_LIN", r1, r2, r3, r4)) {
        cur_code.addInstr(BytecodeStream::SIMPLIFY_LIN);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
        cur_code.addReg(r4);
      } else if (instrR(line,"PUSH",r1)) {
        cur_code.addInstr(BytecodeStream::PUSH);
        cur_code.addReg(r1);
      } else if (instrR(line,"POP",r1)) {
        cur_code.addInstr(BytecodeStream::POP);
        cur_code.addReg(r1);
      } else if (line=="ABORT") {
        cur_code.addInstr(BytecodeStream::ABORT);
      } else {
        throw Error("Error: illegal line\n"+line);
      }
    }
    if (!cur_proc.empty()) {
      // patch jumps with recorded labels
      for (auto& cl : cur_labels) {
        if (labels.find(cl.second)==labels.end())
          throw Error("Error: label "+cl.second+" not found\n");
        cur_code.patchAddress(cl.first, labels[cl.second]);
      }
      labels.clear();
      cur_labels.clear();

      auto it = procs.find(cur_proc);
      if (it != procs.end()) {
        BytecodeProc& bcp = codes[it->second];
        if (bcp.mode[cur_mode].size() > 0) {
          throw Error("Error: procedure "+cur_proc+" already defined before with the same mode\n");
        }
        bcp.mode[cur_mode] = cur_code;
        if (bcp.nargs != cur_proc_nargs) {
          throw Error("Error: procedure "+cur_proc+" already defined before with different number of arguments\n");
        }
        cur_code = BytecodeStream();
        cur_toPatch.clear();
      } else {
        BytecodeProc bcp;
        bcp.name = cur_proc;
        bcp.mode[cur_mode] = cur_code;
        bcp.nargs = cur_proc_nargs;
        bcp.delay = cur_proc_delay;
        procs[cur_proc] = codes.size();
        toPatch.emplace_back(codes.size(), cur_mode, cur_toPatch);
        codes.push_back(bcp);
        cur_code = BytecodeStream();
        cur_toPatch.clear();
      }
    }

    for (auto& p : toPatch) {
      int code = p.code;
      BytecodeProc::Mode mode = p.mode;
      for (auto& patch : p.patch) {
        codes[code].mode[mode].patchAddress(patch.first,procs[patch.second]);
      }
    }
    
    return codes;
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
    if (!_agg.empty()) {
      auto fzn = Definition::toFZN(this, _agg.back().def_stack, _procs);
      // TODO: What solve item should we add?
      fzn->addItem(SolveI::sat(Location().introduce()));
      return fzn;
    }
    return nullptr;
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
  }
  
  void
  Interpreter::call(int code, const BytecodeProc::Mode& mode0, const std::vector<Val>& args0, bool delayed) {
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
      auto cse = cse_lookup(code, cse_key, mode);
      if (cse.second) {
        cse_key.destroy();
        if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
          assert(cse.first.isInt());
          if (cse.first().toInt() != 1) {
            // TODO: The model is inconsistent!
            throw Error("Error: Model Inconsistent!");
          }
        } else {
          pushAgg(cse.first, -1);
        }
        return;
      }
    }
    if (_procs[code].mode[mode].size() == 0) {
      DBG_INTERPRETER("--- FZN Builtin\n");
      // this is a FlatZinc builtin
      int ident = (mode==BytecodeProc::ROOT || mode==BytecodeProc::ROOT_NEG) ? -1 : newIdent();
      Definition* def = Definition::a(this,IntVal(0),code,mode,args,ident);
      pushDef(def);
      if (cse_suited) {
        Val v = (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) ? Val(1) : Val(def);
        cse_insert(code, cse_key, mode, v);
      }
      if (ident >= 0) {
        pushAgg(Val(def), -1);
      }
    } else {
      // Ensure the last RET is next on the program counter
      _stack.back().pc--;
      _stack.emplace_back(_procs[code].mode[mode]);
      BytecodeFrame* newFrame = &_stack[_stack.size()-1];
      newFrame->cse_info.emplace_back(code, mode, cse_key, _agg.back().size());
      newFrame->reg.mov(this, args);
      run();
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
    trail_size.emplace_back(hedge_trail.size(), obj_trail.size(), alias_trail.size());
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
    size_t ht_size, ot_size, at_size;
    std::tie(ht_size, ot_size, at_size) = trail_size.back(); trail_size.pop_back();
    int timestamp = timestamp_trail.back(); timestamp_trail.pop_back();
    Definition* back = interpreter->_agg[0].def_stack;
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
      def->_pred = proc;
      def->_size = size;
      def->_args[0] = arg0;
      alias_trail.pop_back();
    }
    // Remove all additions/changes to the CSE table
    for (auto &table : interpreter->cse) {
      table.pop(interpreter);
    }
    // Remove all newly created definitions
    while (back->timestamp() > timestamp) {
      Definition* rem = back;
      back = back->prev();
      rem->destroy(interpreter);
      free(rem);
    }
    // Reset the timestamp count to its previous value
    interpreter->_identCount = timestamp;
    last_operation_pop = true;
  }
  
  void
  Interpreter::optimize(void) {
  }
  
}
