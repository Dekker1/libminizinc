/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace MiniZinc {
inline std::pair<Val, bool> Interpreter::cse_find(int proc, const CSEKey& key,
                                                  BytecodeProc::Mode& mode) {
  std::pair<Val, bool> result;
  DTRACE2(CSE_FIND_START, (uintptr_t)this, _procs[proc].nargs);
  switch (_procs[proc].nargs) {
    case 1: {
      auto fkey = static_cast<const FixedKey<1>&>(key);
      auto table = static_cast<CSETable<FixedKey<1>>*>(cse[proc]);
      result = table->find(*this, fkey, mode);
      break;
    }
    case 2: {
      auto fkey = static_cast<const FixedKey<2>&>(key);
      auto table = static_cast<CSETable<FixedKey<2>>*>(cse[proc]);
      result = table->find(*this, fkey, mode);
      break;
    }
    case 3: {
      auto fkey = static_cast<const FixedKey<3>&>(key);
      auto table = static_cast<CSETable<FixedKey<3>>*>(cse[proc]);
      result = table->find(*this, fkey, mode);
      break;
    }
    case 4: {
      auto fkey = static_cast<const FixedKey<4>&>(key);
      auto table = static_cast<CSETable<FixedKey<4>>*>(cse[proc]);
      result = table->find(*this, fkey, mode);
      break;
    }
    default: {
      auto vkey = static_cast<const VariadicKey&>(key);
      auto table = static_cast<CSETable<VariadicKey>*>(cse[proc]);
      result = table->find(*this, vkey, mode);
      break;
    }
  }
  DTRACE2(CSE_FIND_END, (uintptr_t)this, result.second);
  return result;
}
inline void Interpreter::cse_insert(int proc, CSEKey& key, BytecodeProc::Mode& mode, Val& val) {
  DTRACE2(CSE_INSERT_START, (uintptr_t)this, _procs[proc].nargs);
  switch (_procs[proc].nargs) {
    case 1: {
      auto fkey = static_cast<FixedKey<1>&>(key);
      auto table = static_cast<CSETable<FixedKey<1>>*>(cse[proc]);
      table->insert(*this, fkey, mode, val);
      break;
    }
    case 2: {
      auto fkey = static_cast<FixedKey<2>&>(key);
      auto table = static_cast<CSETable<FixedKey<2>>*>(cse[proc]);
      table->insert(*this, fkey, mode, val);
      break;
    }
    case 3: {
      auto fkey = static_cast<FixedKey<3>&>(key);
      auto table = static_cast<CSETable<FixedKey<3>>*>(cse[proc]);
      table->insert(*this, fkey, mode, val);
      break;
    }
    case 4: {
      auto fkey = static_cast<FixedKey<4>&>(key);
      auto table = static_cast<CSETable<FixedKey<4>>*>(cse[proc]);
      table->insert(*this, fkey, mode, val);
      break;
    }
    default: {
      auto vkey = static_cast<VariadicKey&>(key);
      auto table = static_cast<CSETable<VariadicKey>*>(cse[proc]);
      table->insert(*this, vkey, mode, val);
      break;
    }
  }
  DTRACE1(CSE_INSERT_END, (uintptr_t)this);
}
}  // namespace MiniZinc
