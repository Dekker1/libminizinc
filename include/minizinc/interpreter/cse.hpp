/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/interpreter/cse.hh>
#include <minizinc/interpreter/primitives.hh>

namespace MiniZinc {

template <class Key>
inline std::pair<Val, bool> CSETable<Key>::find(Interpreter& interpreter, const Key& key,
                                                BytecodeProc::Mode& mode) {
  size_t i = _table.size();
  auto it = _table[i].end();
  do {
    i--;
    it = _table[i].find(key);
  } while (it == _table[i].end() && i > 0);
  if (it != _table[i].end()) {
    // FIXME: This can be a permanent replacement, but the change needs to be stored in the CSETable
    // and we are removing a weak reference, not a strong reference.
    Val val = Val::follow_alias(it->second.second);
    BytecodeProc::Mode val_m = it->second.first;
    if (!val.exists()) {
      this->_table[i].erase(it);
      return {Val(), false};
    }
    DBG_INTERPRETER("--- CSE hit! hash(" << key.hash()
                                         << ") -> Mode: " << BytecodeProc::mode_to_string[val_m]
                                         << " Value: " << val.toString(DBG_TRIM_OUTPUT) << "\n");
    auto convert = [&interpreter, val_m, mode](Val v) {
      assert(!v.isVec());
      if (BytecodeProc::is_neg(mode) != BytecodeProc::is_neg(val_m)) {
        if (v.isInt()) {
          assert(v == 0 || v == 1);
          return 1 - v;
        } else {
          FixedKey<1> nkey(interpreter, {v});
          Val new_val;
          bool found;
          auto cmode = BytecodeProc::FUN;
          std::tie(new_val, found) = interpreter.cse_find(PrimitiveMap::OP_NOT, nkey, cmode);
          if (!found) {
            Variable* new_var = Variable::a(&interpreter, interpreter.boolean_domain(), true,
                                            interpreter.newIdent());
            new_val = Val(new_var);
            auto c = Constraint::a(&interpreter, PrimitiveMap::BOOLNOT, BytecodeProc::ROOT,
                                   {v, new_val});
            assert(c.first);
            new_var->addRef(&interpreter);
            new_var->addDefinition(&interpreter, c.first);
            interpreter.cse_insert(PrimitiveMap::OP_NOT, nkey, cmode, new_val);
            RefCountedObject::rmRef(&interpreter, new_var);
          } else {
            nkey.destroy(interpreter);
          }
          return new_val;
        }
      }
      return v;
    };
    if (mode == val_m) {
      return std::make_pair(val, true);
      // Assumption: 'val' must be of boolean type, otherwise mode is always FUN
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

template <class Key>
inline void CSETable<Key>::insert(Interpreter& interpreter, Key& key,
                                  const BytecodeProc::Mode& mode, Val& val) {
  DBG_INTERPRETER("--- CSE add: hash(" << key.hash()
                                       << ") -> Mode: " << BytecodeProc::mode_to_string[mode]
                                       << " Value: " << val.toString(DBG_TRIM_OUTPUT) << "\n");
  // If value is reference counted, flag that it's in CSE
  val.addMemRef(&interpreter);
  auto insertion = _table.back().emplace(key, std::make_pair(mode, val));
  if (!insertion.second) {
    auto& it = insertion.first;
    // We are replacing another entry within the CSE table.
    assert(it->first == key);
    key.destroy(interpreter);
    if (it->second.first != mode) {
      Val oldVal = Val::follow_alias(it->second.second);
      BytecodeProc::Mode& oldMode = it->second.first;
      if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        if (oldVal.isVar()) {
          Variable* v = oldVal.toVar();
          v->alias(&interpreter,
                   BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode) ? 1 : 0);
        }
      } else if (mode == BytecodeProc::FUN || mode == BytecodeProc::FUN_NEG) {
        if (oldVal.isVar()) {
          Variable* v = oldVal.toVar();
          if (BytecodeProc::is_neg(oldMode) == BytecodeProc::is_neg(mode)) {
            // Value might have already been aliased earlier in the call stack
            if (val != oldVal) {
              v->alias(&interpreter, val);
            }
          } else {
            FixedKey<1> nkey(interpreter, {val});
            Val new_val;
            bool found;
            auto cmode = BytecodeProc::FUN;
            std::tie(new_val, found) = interpreter.cse_find(PrimitiveMap::OP_NOT, nkey, cmode);
            if (!found) {
              Variable* new_var = Variable::a(&interpreter, interpreter.boolean_domain(), true,
                                              interpreter.newIdent());
              new_val = Val(new_var);
              auto c = Constraint::a(&interpreter, PrimitiveMap::BOOLNOT, BytecodeProc::ROOT,
                                     {val, new_val});
              assert(c.first);
              new_var->addRef(&interpreter);
              new_var->addDefinition(&interpreter, c.first);
              interpreter.cse_insert(PrimitiveMap::OP_NOT, nkey, cmode, new_val);
              RefCountedObject::rmRef(&interpreter, new_var);
            } else {
              nkey.destroy(interpreter);
            }
            if (new_val != oldVal) {
              v->alias(&interpreter, new_val);
            }
          }
        }
      }
      it->second.second.rmMemRef(&interpreter);
      it->second = std::make_pair(mode, val);
    }
  }
}

}  // namespace MiniZinc
