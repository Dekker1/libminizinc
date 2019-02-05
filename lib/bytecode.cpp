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

namespace MiniZinc {

  std::string
  BytecodeStream::toString(void) const {
    std::ostringstream oss;
    int pc = 0;
    while (pc < bs.size()) {
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
          oss << "JMP " << intval(pc) << "\n";
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
        case BytecodeStream::ISVAR:
        {
          oss << "ISVAR R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ISABSENT:
        {
          oss << "ISABSENT R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ISOPT:
        {
          oss << "ISOPT R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::RET:
        {
          oss << "RET R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::CALL:
        {
          oss << "CALL " << reg(pc) << " ";
          int n=reg(pc);
          oss << n;
          for (int i=0; i<n; i++) {
            oss << " R" << reg(pc);
          }
          oss << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::TCALL:
        {
          oss << "TCALL " << reg(pc) << "\n";
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
        case BytecodeStream::NEW_VEC:
        {
          oss << "NEW_VEC R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DEL_VEC:
        {
          oss << "DEL_VEC R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::GET_VEC:
        {
          oss << "GET_VEC R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::PUT_VEC:
        {
          oss << "PUT_VEC R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::MK_ARRAY:
        {
          oss << "MK_ARRAY R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
      }
    }
    return oss.str();
  }
  
//#define DBG_INTERPRETER(msg) std::cerr << msg
#define DBG_INTERPRETER(msg) do {} while(0)
  
  void
  Interpreter::run(void) {
    GCLock lock; /// TODO: make stack part of GC root set
  interpreter_start:
    while (!_stack.empty()) {
      BytecodeFrame& frame = _stack.back();
      DBG_INTERPRETER("run frame " << _stack.size()-1 << "\n");
      for (;;) {
        DBG_INTERPRETER(frame.pc << " ");
        switch (frame.bs->instr(frame.pc)) {
          case BytecodeStream::ADDI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)+frame.reg.i(r2),r3);
            DBG_INTERPRETER("ADDI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::SUBI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)-frame.reg.i(r2),r3);
            DBG_INTERPRETER("SUBI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::MULI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)*frame.reg.i(r2),r3);
            DBG_INTERPRETER("MULI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::DIVI:
          {
            assert(false);
//            int r1 = frame.bs->reg(frame.pc);
//            int r2 = frame.bs->reg(frame.pc);
//            int r3 = frame.bs->reg(frame.pc);
//            frame.reg.i(frame.reg.i(r1)+frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::MODI:
          {
            assert(false);
//            int r1 = frame.bs->reg(frame.pc);
//            int r2 = frame.bs->reg(frame.pc);
//            int r3 = frame.bs->reg(frame.pc);
            //            frame.reg.i(frame.reg.i(r1)+frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::INCI:
          {
            int r1 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("INCI " << r1 << "\n");
            frame.reg.i(frame.reg.i(r1)+1,r1);
          }
            break;
          case BytecodeStream::DECI:
          {
            int r1 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("DECI " << r1 << "\n");
            frame.reg.i(frame.reg.i(r1)-1,r1);
          }
            break;
          case BytecodeStream::IMMI:
          {
            IntVal i = frame.bs->intval(frame.pc);
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.i(i,r1);
            DBG_INTERPRETER("IMMI " << i << " " << r1 << "(" << frame.reg.i(r1) << ")" << "\n");
          }
            break;
          case BytecodeStream::LOAD_GLOBAL:
          {
            int i = frame.bs->reg(frame.pc);
            int r1 = frame.bs->reg(frame.pc);
            _stack[0].reg.mov(i, frame.reg, r1);
            DBG_INTERPRETER("LOAD_GLOBAL " << i << " " << r1 << "(" << frame.reg.i(r1) << ")" << "\n");
          }
            break;
          case BytecodeStream::STORE_GLOBAL:
          {
            int r1 = frame.bs->reg(frame.pc);
            int i = frame.bs->reg(frame.pc);
            frame.reg.mov(r1, _stack[0].reg, i);
            DBG_INTERPRETER("STORE_GLOBAL R" << r1 << "(" << frame.reg.i(r1) << ")" << " " << i << "\n");
          }
            break;
          case BytecodeStream::MOV:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("MOV " << r1 << " " << r2 << "\n");
            frame.reg.mov(r1,r2);
          }
            break;
          case BytecodeStream::JMP:
          {
            int i = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("JMP " << i  << "\n");
            frame.pc = i;
          }
            break;
          case BytecodeStream::JMPIF:
          {
            int r0 = frame.bs->reg(frame.pc);
            int i = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("JMPIF " << r0 << "(" << frame.reg.i(r0) << ")" << " " << i << "\n");
            if (frame.reg.i(r0) != 0) {
              frame.pc = i;
            }
          }
            break;
          case BytecodeStream::JMPIFNOT:
          {
            int r0 = frame.bs->reg(frame.pc);
            int i = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("JMPIFNOT " << r0 << " " << i << "\n");
            if (frame.reg.i(r0) == 0) {
              frame.pc = i;
            }
          }
            break;
          case BytecodeStream::EQI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)==frame.reg.i(r2),r3);
            DBG_INTERPRETER("EQI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::LTI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)<frame.reg.i(r2),r3);
            DBG_INTERPRETER("LTI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::LEI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)<=frame.reg.i(r2),r3);
            DBG_INTERPRETER("LEI " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::AND:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1) != 0 && frame.reg.i(r2) != 0,r3);
            DBG_INTERPRETER("AND " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::OR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1) != 0 || frame.reg.i(r2) != 0,r3);
            DBG_INTERPRETER("OR " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::NOT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("NOT " << r1 << " " << r2 << "\n");
            frame.reg.i(frame.reg.i(r1) == 0,r2);
          }
            break;
          case BytecodeStream::XOR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i((frame.reg.i(r1) != 0) ^ (frame.reg.i(r2) != 0),r3);
            DBG_INTERPRETER("XOR " << r1  << "(" << frame.reg.i(r1) << ")" << " " << r2  << "(" << frame.reg.i(r2) << ")" << " " << r3 <<  "(" << frame.reg.i(r3) << ")" <<  "\n");
          }
            break;
          case BytecodeStream::ISPAR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("ISPAR " << r1 << " " << r2 << "\n");
            frame.reg.i(frame.reg.e(r1)->type().ispar(),r2);
          }
            break;
          case BytecodeStream::ISVAR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("ISVAR " << r1 << " " << r2 << "\n");
            frame.reg.i(frame.reg.e(r1)->type().isvar(),r2);
          }
            break;
          case BytecodeStream::ISABSENT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("ISABSENT " << r1 << " " << r2 << "\n");
            frame.reg.i(frame.reg.e(r1) == constants().absent,r2);
          }
            break;
          case BytecodeStream::ISOPT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("ISOPT " << r1 << " " << r2 << "\n");
            frame.reg.i(frame.reg.e(r1)->type().isopt(),r2);
          }
            break;
          case BytecodeStream::RET:
          {
            int r = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("RET " << r << "\n");
            // Invariant: if there is a frame below, its pc is on the return register
            if (_stack.size() > 1) {
              BytecodeFrame& caller = _stack[_stack.size()-2];
              int ret_r = caller.bs->reg(caller.pc);
              frame.reg.mov(r, caller.reg, ret_r);
            }
            _stack.pop_back();
            goto interpreter_start;
          }
            break;
          case BytecodeStream::CALL:
          {
            int code = frame.bs->reg(frame.pc);
            assert(code >= 0);
            assert(code < _procs.size());
            _stack.emplace_back(_procs[code]);
            BytecodeFrame& oldFrame = _stack[_stack.size()-2];
            BytecodeFrame& newFrame = _stack[_stack.size()-1];
            int n = oldFrame.bs->reg(oldFrame.pc);
            for (int i=0; i<n; i++) {
              int r = oldFrame.bs->reg(oldFrame.pc);
              oldFrame.reg.mov(r, newFrame.reg, i);
            }
            DBG_INTERPRETER("CALL " << code  << " " << n << "\n");
            goto interpreter_start;
          }
            break;
          case BytecodeStream::TCALL:
          {
            int code = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("TCALL " << code  << "\n");
            assert(code >= 0);
            assert(code < _procs.size());
            frame.bs = &_procs[code];
            frame.pc = 0;
          }
            break;
          case BytecodeStream::TRACE:
          {
            int r = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("TRACE " << r  << "\n");
//            std::cerr << frame.reg.e(r)->cast<StringLit>();
            std::cerr << *frame.reg.e(r) << "\n";
          }
            break;
          case BytecodeStream::ABORT:
          {
            DBG_INTERPRETER("ABORT\n");
            return;
          }
          case BytecodeStream::NEW_VEC:
          {
            int r1 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("NEW_VEC " << r1  << "\n");
            frame.reg.new_ev(r1);
          }
            break;
          case BytecodeStream::DEL_VEC:
          {
            int r1 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("DEL_VEC " << r1  << "\n");
            frame.reg.delete_ev(r1);
          }
            break;
          case BytecodeStream::GET_VEC:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("GET_VEC " << r1 << " " << r2 << " " << r3  << "\n");
            std::vector<Expression*>& ev = *frame.reg.ev(r1);
            IntVal idx = frame.reg.i(r2);
            frame.reg.e(ev[idx.toInt()], r3);
          }
            break;
          case BytecodeStream::PUT_VEC:
          {
            int r1 = frame.bs->reg(frame.pc);
            Expression *val = frame.reg.e(r1);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("PUT_VEC " << r1 << " " << r2 << " " << r3  << "\n");
            std::vector<Expression*>& ev = *frame.reg.ev(r3);
            IntVal idx = frame.reg.i(r2);
            if (ev.size() <= idx)
              ev.resize(idx.toInt()+1);
            ev[idx.toInt()] = val;
          }
            break;
          case BytecodeStream::MK_ARRAY:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            DBG_INTERPRETER("MK_ARRAY " << r1 << " " << r2  << "\n");
            GCLock lock;
            std::vector<Expression*>& ev = *frame.reg.ev(r1);
            frame.reg.e(new ArrayLit(Location().introduce(), ev), r2);
          }
            break;

        }
        assert(!frame.bs->eos(frame.pc));
      }
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

  std::vector<BytecodeStream> parse(const std::string& s) {
    std::vector<BytecodeStream> codes;
    std::vector<std::vector<std::pair<int,std::string> > > toPatch;
    
    std::unordered_map<std::string, int> procs;
    
    std::istringstream iss(s);
    std::string cur_proc;
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

          if (procs.find(cur_proc) != procs.end())
            throw Error("Error: procedure "+cur_proc+" already defined before\n");
          procs[cur_proc] = codes.size();
          codes.push_back(cur_code);
          toPatch.push_back(cur_toPatch);
          cur_code = BytecodeStream();
          cur_toPatch.clear();
          
          
        }
        cur_proc = line.substr(1,line.find(':',1)-1);
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
        cur_code.addReg(0); // placeholder
      } else if (instrRS(line,"JMPIF",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIF);
        cur_code.addReg(r1);
        cur_labels.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addReg(0); // placeholder
      } else if (instrRS(line,"JMPIFNOT",r1,rs)) {
        cur_code.addInstr(BytecodeStream::JMPIFNOT);
        cur_code.addReg(r1);
        cur_labels.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addReg(0); // placeholder
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
      } else if (instrRR(line,"ISVAR",r1,r2)) {
        cur_code.addInstr(BytecodeStream::ISVAR);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"ISABSENT",r1,r2)) {
        cur_code.addInstr(BytecodeStream::ISABSENT);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"ISOPT",r1,r2)) {
        cur_code.addInstr(BytecodeStream::ISOPT);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrR(line,"RET",r1)) {
        cur_code.addInstr(BytecodeStream::RET);
        cur_code.addReg(r1);
      } else if (startsWith(line,"CALL ")) {
        size_t cur_pos = line.find(' ');
        std::string n = line.substr(cur_pos+1);
        std::string rs = n.substr(0, n.find(' '));
        cur_code.addInstr(BytecodeStream::CALL);
        cur_toPatch.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addReg(0); // placeholder
        std::string n1 = n.substr(n.find(' ')+1);
        int n_args = std::stoi(n1.substr(0,n1.find(' ')));
        cur_code.addReg(n_args);
        for (int i=0; i<n_args+1; i++) {
          n1 = n1.substr(n1.find(" R")+2);
          int r = std::stoi(n1.substr(0,n1.find(' ')));
          cur_code.addReg(r);
        }
      } else if (instrS(line,"TCALL",rs)) {
        cur_code.addInstr(BytecodeStream::TCALL);
        cur_toPatch.push_back(std::make_pair(cur_code.size(),rs));
        cur_code.addReg(0); // placeholder
      } else if (instrR(line,"TRACE",r1)) {
        cur_code.addInstr(BytecodeStream::TRACE);
        cur_code.addReg(r1);
      } else if (line=="ABORT") {
        cur_code.addInstr(BytecodeStream::ABORT);
      } else if (instrR(line,"NEW_VEC",r1)) {
        cur_code.addInstr(BytecodeStream::NEW_VEC);
        cur_code.addReg(r1);
      } else if (instrR(line,"DEL_VEC",r1)) {
        cur_code.addInstr(BytecodeStream::DEL_VEC);
        cur_code.addReg(r1);
      } else if (instrRRR(line,"GET_VEC",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::GET_VEC);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"PUT_VEC",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::PUT_VEC);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRR(line,"MK_ARRAY",r1,r2)) {
        cur_code.addInstr(BytecodeStream::MK_ARRAY);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else {
        throw Error("Error: illegal line\n"+line);
      }
    }
    if (!cur_proc.empty()) {
      if (procs.find(cur_proc) != procs.end())
        throw Error("Error: procedure "+cur_proc+" already defined before\n");
      procs[cur_proc] = codes.size();
      codes.push_back(cur_code);
      toPatch.push_back(cur_toPatch);
    }

    for (unsigned int i=0; i<codes.size(); i++) {
      for (auto& patch : toPatch[i]) {
        codes[i].patchAddress(patch.first,procs[patch.second]);
      }
    }
    
    return codes;
  }
  
}
