/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/bytecode.hh>

#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>


#include <iostream>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <streambuf>

#define DBG_INTERPRETER(msg) std::cerr << msg
//#define DBG_INTERPRETER(msg) do {} while(0)

namespace MiniZinc {

  PrimitiveMap::PrimitiveMap(void)
  : _s({ {"clause",CLAUSE}, {"forall",FORALL}, {"lin_exp",LINEXP} }) {
    _n.resize(_s.size());
    for (auto& entry : _s) {
      _n[entry.second] = entry.first;
    }
  }

  const PrimitiveMap::Primitive PrimitiveMap::ALL[] = { CLAUSE, FORALL, LINEXP };
  
  const std::string BytecodeProc::mode_to_string[] = { "RAW", "ROOT", "ROOT_NEG", "FUN", "FUN_NEG", "IMP", "IMP_NEG" };
  
  std::string
  Val::toString(void) const {
    std::ostringstream oss;
    if (isInt()) {
      oss << (*this)();
    } else if (isRef()) {
      oss << "X" << r()();
    } else {
      oss << "[";
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
    } else if (isVec()) {
      return (*toVec()) == (*rhs.toVec());
    } else {
      return reinterpret_cast<ptrdiff_t>(_v) == reinterpret_cast<ptrdiff_t>(rhs._v);
    }
  }

  std::vector<WeakVal> WeakVal::flat_vector(const std::vector<Val> &vec) {
    std::vector<WeakVal> nvec;
    nvec.reserve(vec.size());
    for (const auto& val : vec) {
      if (val.isVec()) {
        Vec* vv = val.toVec();
        nvec.reserve(nvec.size() + vv->size() + 1);
        nvec.emplace_back(Val(vv->size()));
        for (int i = 0; i < vv->size(); ++i) {
          nvec.emplace_back((*vv)[i]);
        }
      } else {
        nvec.emplace_back(val);
      }
    }
    return nvec;
  }

  void cmb_hash(size_t& incumbent, const size_t h) {
    incumbent ^= h + 0x9e3779b9 + (incumbent << 6) + (incumbent >> 2);
  }
  size_t CSEHasher::operator()(const std::vector<WeakVal> &vec) const {
    size_t hash = 0;
    for(const auto& val : vec) {
      cmb_hash(hash, val.hash());
    }
    return hash;
  }

  std::pair<Val, bool> BytecodeProc::CSETable::lookup(const std::vector<Val>& args, const BytecodeProc::Mode& mode) {
    if (mode == RAW) {
      return std::pair<Val, bool>(Val(), false);
    }
    auto key = WeakVal::flat_vector(args);
    auto it = _table.find(key);
    if (it != _table.end()) {
      // TODO: Convert depending on Mode!
      Val v = it->second.second.to_val();
      DBG_INTERPRETER("--- CSE hit! hash(" << CSEHasher()(key) << ") -> Mode: " << mode << " Value: " << v.toString() << "\n");
      return std::make_pair(v, true);
    }
    return std::pair<Val, bool>(Val(), false);
  }

  void BytecodeProc::CSETable::insert(const std::vector<Val>& args, const BytecodeProc::Mode& mode, const Val& val) {
    auto key = WeakVal::flat_vector(args);
    DBG_INTERPRETER("--- CSE add: hash(" << CSEHasher()(key) << ") -> Mode: " << mode << " Value: " << val.toString() << "\n");
    _table.insert({key, std::make_pair(mode, val)});
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
        case BytecodeStream::RET:
        {
          oss << "RET\n";
        }
          break;
        case BytecodeStream::CALL:
        {
          BytecodeProc::Mode m = static_cast<BytecodeProc::Mode>(chr(pc));
          int p = reg(pc);
          if (procs.empty()) {
            oss << "CALL " << BytecodeProc::mode_to_string[m] << " " << p << " ";
          } else {
            oss << "CALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name << " ";
          }
          int n=reg(pc);
          oss << n;
          for (int i=0; i<n; i++) {
            oss << " R" << reg(pc);
          }
          oss << "\n";
        }
          break;
        case BytecodeStream::BUILTIN:
        {
          int p = reg(pc);
          oss << "BUILTIN " << p << " ";
          int n=reg(pc);
          oss << n;
          for (int i=0; i<n; i++) {
            oss << " R" << reg(pc);
          }
        }
          break;
        case BytecodeStream::TCALL:
        {
          BytecodeProc::Mode m = static_cast<BytecodeProc::Mode>(chr(pc));
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
            case AggregationCtx::VCTX_LIN:
              oss << "LIN\n";
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
  Interpreter::push(const Val& v, int stackOffset) {
    assert(stackOffset < 0);
    assert(_agg.size()+stackOffset >= 0);
    if (_agg[_agg.size()+stackOffset].symbol==AggregationCtx::VCTX_LIN) {
      // add coefficient to surrounding linear context
      _agg[_agg.size()+stackOffset].stack.push_back(IntVal(1));
    }
    // push value onto surrounding context
    _agg[_agg.size()+stackOffset].stack.push_back(v);
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
          frame->reg.assign(r3, frame->reg[r1]() + frame->reg[r2]());
          DBG_INTERPRETER("ADDI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::SUBI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, frame->reg[r1]() - frame->reg[r2]());
          DBG_INTERPRETER("SUBI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::MULI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, frame->reg[r1]() * frame->reg[r2]());
          DBG_INTERPRETER("MULI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
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
          DBG_INTERPRETER("INCI " << r1 << "\n");
          frame->reg.assign(r1, frame->reg[r1]()+1);
        }
          break;
        case BytecodeStream::DECI:
        {
          int r1 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("DECI " << r1 << "\n");
          frame->reg.assign(r1, frame->reg[r1]()-1);
        }
          break;
        case BytecodeStream::IMMI:
        {
          IntVal i = frame->bs->intval(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          frame->reg.assign(r1, i);
          DBG_INTERPRETER("IMMI " << i << " " << r1 << "(" << frame->reg[r1]() << ")" << "\n");
        }
          break;
        case BytecodeStream::LOAD_GLOBAL:
        {
          int i = frame->bs->reg(frame->pc);
          int r1 = frame->bs->reg(frame->pc);
          _stack[0].reg.cp(i, frame->reg, r1);
          DBG_INTERPRETER("LOAD_GLOBAL " << i << " " << r1 << "(" << frame->reg[r1]() << ")" << "\n");
        }
          break;
        case BytecodeStream::STORE_GLOBAL:
        {
          int r1 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          frame->reg.cp(r1, _stack[0].reg, i);
          DBG_INTERPRETER("STORE_GLOBAL R" << r1 << "(" << frame->reg[r1]() << ")" << " " << i << "\n");
        }
          break;
        case BytecodeStream::MOV:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("MOV " << r1 << " " << r2 << "\n");
          frame->reg.cp(r1,r2);
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
          DBG_INTERPRETER("JMPIF " << r0 << "(" << frame->reg[r0]() << ")" << " " << i << "\n");
          if (frame->reg[r0]() != 0) {
            frame->pc = i;
          }
        }
          break;
        case BytecodeStream::JMPIFNOT:
        {
          int r0 = frame->bs->reg(frame->pc);
          int i = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("JMPIFNOT " << r0 << " " << i << "\n");
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
          frame->reg.assign(r3, IntVal(frame->reg[r1]() == frame->reg[r2]()));
          DBG_INTERPRETER("EQI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LTI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, IntVal(frame->reg[r1]() < frame->reg[r2]()));
          DBG_INTERPRETER("LTI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LEI:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, IntVal(frame->reg[r1]() <= frame->reg[r2]()));
          DBG_INTERPRETER("LEI " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::AND:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, IntVal(frame->reg[r1]()!=0 && frame->reg[r2]()!=0));
          DBG_INTERPRETER("AND " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::OR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, IntVal(frame->reg[r1]()!=0 || frame->reg[r2]()!=0));
          DBG_INTERPRETER("OR " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::NOT:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("NOT " << r1 << " " << r2 << "\n");
          frame->reg.assign(r2, IntVal(frame->reg[r1]()==0));
        }
          break;
        case BytecodeStream::XOR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          frame->reg.assign(r3, IntVal( (frame->reg[r1]()!=0) ^ (frame->reg[r2]()!=0)));
          DBG_INTERPRETER("XOR " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" << " " << r3 <<  "(" << frame->reg[r3]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISPAR:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          if (frame->reg[r1].isInt()) {
            frame->reg.assign(r2, IntVal(1));
          } else if (frame->reg[r1].isRef()) {
            int r = frame->reg[r1].r()();
            assert(r >= 0 && r < _defstack.size());
            if (_defstack[r].domain.isInt()) {
              frame->reg.assign(r1, _defstack[r].domain);
              frame->reg.assign(r2, IntVal(1));
            } else {
              frame->reg.assign(r2, IntVal(0));
            }
          } else {
            frame->reg.assign(r2, IntVal(0));
          }
          DBG_INTERPRETER("ISPAR " << r1  << "(" << frame->reg[r1]() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::ISEMPTY:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          frame->reg.assign(r2, IntVal(frame->reg[r1].size()==0));
          DBG_INTERPRETER("ISEMPTY " << r1  << "(" << frame->reg[r1].toString() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::LENGTH:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          frame->reg.assign(r2, IntVal(frame->reg[r1].size()));
          DBG_INTERPRETER("LENGTH " << r1  << "(" << frame->reg[r1].toString() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          int r1 = frame->bs->reg(frame->pc);
          int r2 = frame->bs->reg(frame->pc);
          int r3 = frame->bs->reg(frame->pc);
          assert(frame->reg[r1].isVec());
          assert(frame->reg[r2].isInt());
          assert(frame->reg[r2]() < frame->reg[r1].size());
          DBG_INTERPRETER("GET_VEC " << r1  << "(" << frame->reg[r1].toString() << ")" << " " << r2  << "(" << frame->reg[r2]() << ")");
          frame->reg.assign(r3, frame->reg[r1][frame->reg[r2]().toInt()]);
          DBG_INTERPRETER(" " << r3 <<  "(" << frame->reg[r3].toString() << ")" <<  "\n");
        }
          break;
        case BytecodeStream::RET:
        {
          DBG_INTERPRETER("RET\n");
          if (_stack.size()==1) {
            // Always leave final frame on the stack
            return;
          }
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
          int n = frame->bs->reg(frame->pc);
          if (_procs[code].mode[mode].size()==0) {
            DBG_INTERPRETER("CALL fzn builtin " << code  << " " << n << "\n");
            // this is a FlatZinc builtin
            std::vector<Val> args(n);
            for (int i=0; i<n; i++) {
              int r = frame->bs->reg(frame->pc);
              args[i] = frame->reg[r];
            }
            // Lookup item in CSE
            auto cse = _procs[code].cse.lookup(args, mode);
            if (cse.second) {
              push(cse.first, -1);
            } else {
              _defstack.emplace_back(IntVal(0),code,mode,Val(Vec::a(args)));
              Val ret = Ref(_defstack.size()-1);
              _procs[code].cse.insert(args, mode, ret);
              push(ret,-1);
            }
          } else {
            DBG_INTERPRETER("CALL " << code  << " " << n << "\n");
            _stack.emplace_back(_procs[code].mode[mode]);
            BytecodeFrame* oldFrame = &_stack[_stack.size()-2];
            BytecodeFrame* newFrame = &_stack[_stack.size()-1];
            for (int i=0; i<n; i++) {
              int r = oldFrame->bs->reg(oldFrame->pc);
              oldFrame->reg.cp(r, newFrame->reg, i);
            }
            frame = newFrame;
          }
        }
          break;
        case BytecodeStream::BUILTIN:
        {
          int code = frame->bs->reg(frame->pc);
          assert(code >= 0);
          assert(code < _builtins.size());
          // this is a FlatZinc builtin
          int n = frame->bs->reg(frame->pc);
          std::vector<Val> args(n);
          for (int i=0; i<n; i++) {
            int r = frame->bs->reg(frame->pc);
            args[i] = frame->reg[r];
          }
          _builtins[code](*this, args);
        }
          break;
        case BytecodeStream::TCALL:
        {
          char mode_c = frame->bs->chr(frame->pc);
          int code = frame->bs->reg(frame->pc);
          DBG_INTERPRETER("TCALL " << code  << "\n");
          assert(code >= 0);
          assert(code < _procs.size());
          assert(mode_c >= 0);
          assert(mode_c <= BytecodeProc::MAX_MODE);
          BytecodeProc::Mode mode = static_cast<BytecodeProc::Mode>(mode_c);
          frame->bs = &_procs[code].mode[mode];
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
          _agg.back().stack.push_back(frame->reg[r]);
        }
          break;
        case BytecodeStream::POP:
        {
          int r = frame->bs->reg(frame->pc);
          assert(!_agg.empty());
          assert(!_agg.back().stack.empty());
          frame->reg.assign(r, _agg.back().stack.back());
          DBG_INTERPRETER("POP R" << r << " (" << frame->reg[r].toString() << ")\n");
          _agg.back().stack.pop_back();
        }
          break;
        case BytecodeStream::OPEN_AGGREGATION:
        {
          int r = frame->bs->chr(frame->pc);
          DBG_INTERPRETER("OPEN_AGGREGATION " << r  << "\n");
          assert(r >= 0 && r <= AggregationCtx::VCTX_OTHER);
          if (r==AggregationCtx::VCTX_OTHER || r==AggregationCtx::VCTX_VEC || _agg.empty() || _agg.back().symbol != r) {
            // Push a new aggregation context
            _agg.push_back(AggregationCtx(r, _defstack.size()));
          } else {
            // Increment depth counter for current aggregation context
            _agg.back().n_symbols++;
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
            assert(_agg.size() >= 2);
            switch (_agg.back().symbol) {
              case AggregationCtx::VCTX_AND:
              {
                // Create a conjunction on the definition stack
                std::vector<Val> args;
                args.reserve(_agg.back().stack.size());
                bool isFalse = false;
                for (unsigned int i=0; i<_agg.back().stack.size(); i++) {
                  Val& v = _agg.back().stack[i];
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
                  // Remove all elements from definition stack
                  _defstack.resize(_agg.back().def_stack_depth);
                  push(IntVal(!isFalse),-2);
                } else {
                  _defstack.push_back(Definition(IntVal(0),PrimitiveMap::FORALL,BytecodeProc::FUN,Val(Vec::a(args))));
                  push(Ref(_defstack.size()-1),-2);
                }
              }
                break;
              case AggregationCtx::VCTX_OR:
              {
                // Create a clause on the definition stack, and push a reference
                // to it onto the aggregation stack
                
                std::vector<Val> pos;
                pos.reserve(_agg.back().stack.size());
                std::vector<Val> neg;
                neg.reserve(_agg.back().stack.size());
                bool isTrue = false;
                for (unsigned int i=0; i<_agg.back().stack.size(); i+=2) {
                  IntVal sign = _agg.back().stack[i]();
                  Val& v = _agg.back().stack[i+1];
                  if (v.isInt()) {
                    if ( (sign==0 && v()==0) || (sign!=0 && v()!=0) ) {
                      // Disjunction is constant true
                      isTrue = true;
                      break;
                    }
                  } else {
                    if (sign==0) {
                      neg.push_back(v);
                    } else {
                      pos.push_back(v);
                    }
                  }
                }
                if (isTrue || (pos.empty() && neg.empty())) {
                  // Disjunction is constant true or false
                  // Remove all elements from definition stack
                  _defstack.resize(_agg.back().def_stack_depth);
                  push(IntVal(isTrue),-2);
                } else {
                  _defstack.push_back(Definition(IntVal(0),PrimitiveMap::CLAUSE,BytecodeProc::FUN,Val(Vec::a({Val(Vec::a(pos)),Val(Vec::a(neg))}))));
                  push(Ref(_defstack.size()-1),-2);
                }
              }
                break;
              case AggregationCtx::VCTX_LIN:
              {
                // Create a linear expression on the aggregation stack
                // This will leave the coefficient vector, the variable vector, and a constant
                // in the surrounding context
                assert(_agg[_agg.size()-2].symbol==AggregationCtx::VCTX_OTHER);
                assert(_agg.back().stack.size() % 2 == 0);
                std::vector<Val> coeffs;
                coeffs.reserve(_agg.back().stack.size());
                std::vector<Val> vars;
                vars.reserve(_agg.back().stack.size());
                IntVal d = 0;
                for (unsigned int i=0; i<_agg.back().stack.size(); i+=2) {
                  Val& ci = _agg.back().stack[i];
                  Val& vi = _agg.back().stack[i+1];
                  if (ci() != 0) {
                    if (vi.isInt()) {
                      d += ci()*vi();
                    } else {
                      coeffs.push_back(ci);
                      vars.push_back(vi);
                    }
                  }
                }
                _agg[_agg.size()-2].stack.push_back(Val(Vec::a(coeffs)));
                _agg[_agg.size()-2].stack.push_back(Val(Vec::a(vars)));
                _agg[_agg.size()-2].stack.push_back(d);
              }
                break;
              case AggregationCtx::VCTX_VEC:
                // Create a vector on the aggregation stack
                assert(_agg[_agg.size()-2].symbol==AggregationCtx::VCTX_OTHER);
                _agg[_agg.size()-2].stack.push_back(Val(Vec::a(_agg.back().stack)));
                break;
              case AggregationCtx::VCTX_OTHER:
                // When closing a VCTX_OTHER context, it should contain at most one value
                assert(_agg.back().stack.size()<=1);
                if (_agg.back().stack.size()==1) {
                  if (_agg[_agg.size()-2].symbol==AggregationCtx::VCTX_LIN) {
                    // add coefficient to surrounding linear context
                    _agg[_agg.size()-2].stack.push_back(IntVal(1));
                  }
                  // push value onto surrounding context
                  _agg[_agg.size()-2].stack.push_back(_agg.back().stack[0]);
                }
                break;
            }
            _agg.pop_back();
          }
        }
          break;
      }
      assert(!frame->bs->eos(frame->pc));
    }
  }
    
  bool startsWith(const std::string s, const std::string t) {
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
    std::string n2 = nn.substr(0,n.find(' '));
    r2 = std::stoi(n2);
    std::string n3 = nn.substr(nn.find(" R")+2);
    r3 = std::stoi(n3);
    return true;
  }

  std::vector<BytecodeProc> parse(const std::string& s) {

    std::vector<BytecodeProc> codes;
    std::unordered_map<std::string, int> procs;
    
    struct Patch {
      int code;
      BytecodeProc::Mode mode;
      std::vector<std::pair<int,std::string>> patch;
      Patch(int code0, BytecodeProc::Mode mode0, const std::vector<std::pair<int,std::string>>& patch0)
      : code(code0), mode(mode0), patch(patch0) {}
    };
    
    std::vector<Patch> toPatch;

    PrimitiveMap pm;
    // Initialise first slots with
    for (const PrimitiveMap::Primitive& p : PrimitiveMap::ALL) {
      BytecodeProc bcp;
      bcp.name = pm[p];
      std::cerr << "add primitive " << bcp.name << " " << p << "\n";
      codes.push_back(bcp);
      procs.insert({bcp.name,p});
    }

    std::istringstream iss(s);
    std::string cur_proc;
    BytecodeProc::Mode cur_mode;
    BytecodeStream cur_code;
    std::vector<std::pair<int,std::string> > cur_toPatch;
    std::vector<std::pair<int,std::string> > cur_labels;
    std::unordered_map<std::string, int> labels;
    for (std::string line; std::getline(iss,line); ) {
      if (line.size() == 0 || line[0]=='%')
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

          std::unordered_map<std::string, int>::iterator it = procs.find(cur_proc);
          if (it != procs.end()) {
            BytecodeProc& bcp = codes[it->second];
            if (bcp.mode[cur_mode].size() > 0) {
              throw Error("Error: procedure "+cur_proc+" already defined before with the same mode\n");
            }
            bcp.mode[cur_mode] = cur_code;
            toPatch.push_back(Patch(it->second,cur_mode,cur_toPatch));
          } else {
            BytecodeProc bcp;
            bcp.name = cur_proc;
            bcp.mode[cur_mode] = cur_code;
            procs[cur_proc] = codes.size();
            toPatch.push_back(Patch(codes.size(),cur_mode,cur_toPatch));
            codes.push_back(bcp);
            cur_code = BytecodeStream();
            cur_toPatch.clear();
          }
        }
        size_t finalColon = line.find(':',1);
        cur_proc = line.substr(1,finalColon-1);
        std::string newMode = line.substr(finalColon+1);
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
      int r1, r2, r3;
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
        cur_labels.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addSmallInt(0); // placeholder
      } else if (instrRS(line,"JMPIF",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIF);
        cur_code.addReg(r1);
        cur_labels.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addSmallInt(0); // placeholder
      } else if (instrRS(line,"JMPIFNOT",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIFNOT);
        cur_code.addReg(r1);
        cur_labels.push_back(std::make_pair(cur_code.size(),rs));
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
        std::string n0 = n.substr(n.find(' ')+1);
        std::string rs = n0.substr(0, n0.find(' '));
        cur_toPatch.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addSmallInt(0); // placeholder
        std::string n1 = n0.substr(n0.find(' ')+1);
        int n_args = std::stoi(n1.substr(0,n1.find(' ')));
        cur_code.addSmallInt(n_args);
        for (int i=0; i<n_args; i++) {
          n1 = n1.substr(n1.find(" R")+2);
          int r = std::stoi(n1.substr(0,n1.find(' ')));
          cur_code.addReg(r);
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
        std::string rs = n0.substr(0, n0.find(' '));
        cur_toPatch.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addSmallInt(0); // placeholder
      } else if (instrR(line,"TRACE",r1)) {
        cur_code.addInstr(BytecodeStream::TRACE);
        cur_code.addReg(r1);
      } else if (instrS(line,"OPEN_AGGREGATION",rs)) {
        cur_code.addInstr(BytecodeStream::OPEN_AGGREGATION);
        if (rs=="AND") {
          cur_code.addCharVal(AggregationCtx::VCTX_AND);
        } else if (rs=="OR") {
          cur_code.addCharVal(AggregationCtx::VCTX_OR);
        } else if (rs=="LIN") {
          cur_code.addCharVal(AggregationCtx::VCTX_LIN);
        } else if (rs=="VEC") {
          cur_code.addCharVal(AggregationCtx::VCTX_VEC);
        } else if (rs=="OTHER") {
          cur_code.addCharVal(AggregationCtx::VCTX_OTHER);
        } else {
          throw Error("Error: illegal context\n"+line);
        }
      } else if (line=="CLOSE_AGGREGATION") {
        cur_code.addInstr(BytecodeStream::CLOSE_AGGREGATION);
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
      
      std::unordered_map<std::string, int>::iterator it = procs.find(cur_proc);
      if (it != procs.end()) {
        BytecodeProc& bcp = codes[it->second];
        if (bcp.mode[cur_mode].size() > 0) {
          throw Error("Error: procedure "+cur_proc+" already defined before with the same mode\n");
        }
        bcp.mode[cur_mode] = cur_code;
      } else {
        BytecodeProc bcp;
        bcp.name = cur_proc;
        bcp.mode[cur_mode] = cur_code;
        procs[cur_proc] = codes.size();
        toPatch.push_back(Patch(codes.size(),cur_mode,cur_toPatch));
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
  
}
