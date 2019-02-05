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

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <streambuf>

#define DBGOUT std::cerr

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
          oss << "TRACE R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::ABORT:
        {
          oss << "ABORT\n";
        }
          break;
        case BytecodeStream::NEW_VEC_I:
        {
          oss << "NEW_VEC_I R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DEL_VEC_I:
        {
          oss << "DEL_VEC_I R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::GET_VEC_I:
        {
          oss << "GET_VEC_I R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::PUT_VEC_I:
        {
          oss << "PUT_VEC_I R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::NEW_VEC_E:
        {
          oss << "NEW_VEC_E R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::DEL_VEC_E:
        {
          oss << "DEL_VEC_E R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::GET_VEC_E:
        {
          oss << "GET_VEC_E R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::PUT_VEC_E:
        {
          oss << "PUT_VEC_E R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc)  << "\n";
        }
          break;
        case BytecodeStream::MK_ARRAY_I:
        {
          oss << "MK_ARRAY_I R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
        case BytecodeStream::MK_ARRAY_E:
        {
          oss << "MK_ARRAY_E R" << reg(pc) << " R" << reg(pc) << "\n";
        }
          break;
      }
    }
    return oss.str();
  }

  
  void
  Interpreter::run(void) {
    GCLock lock; /// TODO: make stack part of GC root set
  interpreter_start:
    while (!_stack.empty()) {
      BytecodeFrame& frame = _stack.back();
      for (;;) {
        switch (frame.bs->instr(frame.pc)) {
          case BytecodeStream::ADDI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)+frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::SUBI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)-frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::MULI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)*frame.reg.i(r2),r3);
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
            frame.reg.i(frame.reg.i(r1)+1,r1);
          }
            break;
          case BytecodeStream::DECI:
          {
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)-1,r1);
          }
            break;
          case BytecodeStream::IMMI:
          {
            IntVal i = frame.bs->intval(frame.pc);
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.i(i,r1);
          }
            break;
          case BytecodeStream::MOV:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.mov(r1,r2);
          }
            break;
          case BytecodeStream::JMP:
          {
            int i = frame.bs->reg(frame.pc);
            frame.pc += i;
          }
            break;
          case BytecodeStream::JMPIF:
          {
            int r0 = frame.bs->reg(frame.pc);
            int i = frame.bs->reg(frame.pc);
            if (frame.reg.i(r0) != 0) {
              frame.pc += i;
            }
          }
            break;
          case BytecodeStream::JMPIFNOT:
          {
            int r0 = frame.bs->reg(frame.pc);
            int i = frame.bs->reg(frame.pc);
            if (frame.reg.i(r0) == 0) {
              frame.pc += i;
            }
          }
            break;
          case BytecodeStream::EQI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)==frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::LTI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)<frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::LEI:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1)<=frame.reg.i(r2),r3);
          }
            break;
          case BytecodeStream::AND:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1) != 0 && frame.reg.i(r2) != 0,r3);
          }
            break;
          case BytecodeStream::OR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1) != 0 || frame.reg.i(r2) != 0,r3);
          }
            break;
          case BytecodeStream::NOT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.i(r1) == 0,r2);
          }
            break;
          case BytecodeStream::XOR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            frame.reg.i((frame.reg.i(r1) != 0) ^ (frame.reg.i(r2) != 0),r3);
          }
            break;
          case BytecodeStream::ISPAR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.e(r1)->type().ispar(),r2);
          }
            break;
          case BytecodeStream::ISVAR:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.e(r1)->type().isvar(),r2);
          }
            break;
          case BytecodeStream::ISABSENT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.e(r1) == constants().absent,r2);
          }
            break;
          case BytecodeStream::ISOPT:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            frame.reg.i(frame.reg.e(r1)->type().isopt(),r2);
          }
            break;
          case BytecodeStream::RET:
          {
            // Invariant: if there is a frame below, its pc is on the return register
            if (_stack.size() > 1) {
              BytecodeFrame& caller = _stack[_stack.size()-2];
              int ret_r = caller.bs->reg(caller.pc);
              int r = frame.bs->reg(frame.pc);
              frame.reg.mov(r, caller.reg, ret_r);
            }
            _stack.pop_back();
            goto interpreter_start;
          }
            break;
          case BytecodeStream::CALL:
          {
            BytecodeFrame& oldFrame = frame;
            int code = oldFrame.bs->reg(oldFrame.pc);
            assert(code >= 0);
            assert(code < _procs.size());
            _stack.emplace_back(_procs[code]);
            BytecodeFrame& newFrame = _stack.back();
            int n = oldFrame.bs->reg(oldFrame.pc);
            for (int i=0; i<n; i++) {
              int r = oldFrame.bs->reg(oldFrame.pc);
              oldFrame.reg.mov(r, newFrame.reg, i);
            }
            goto interpreter_start;
          }
            break;
          case BytecodeStream::TCALL:
          {
            int code = frame.bs->reg(frame.pc);
            assert(code >= 0);
            assert(code < _procs.size());
            frame.bs = &_procs[code];
            frame.pc = 0;
          }
            break;
          case BytecodeStream::TRACE:
          {
            int r = frame.bs->reg(frame.pc);
            std::cerr << frame.reg.e(r)->cast<StringLit>();
          }
            break;
          case BytecodeStream::ABORT:
          {
            return;
          }
          case BytecodeStream::NEW_VEC_I:
          {
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.new_iv(r1);
          }
            break;
          case BytecodeStream::DEL_VEC_I:
          {
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.delete_iv(r1);
          }
            break;
          case BytecodeStream::GET_VEC_I:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            std::vector<IntVal>& iv = *frame.reg.iv(r1);
            IntVal idx = frame.reg.i(r2);
            frame.reg.i(iv[idx.toInt()], r3);
          }
            break;
          case BytecodeStream::PUT_VEC_I:
          {
            int r1 = frame.bs->reg(frame.pc);
            IntVal val = frame.reg.i(r1);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            std::vector<IntVal>& iv = *frame.reg.iv(r3);
            IntVal idx = frame.reg.i(r2);
            if (iv.size() < idx)
              iv.resize(idx.toInt()+1);
            iv[idx.toInt()] = val;
          }
            break;
          case BytecodeStream::NEW_VEC_E:
          {
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.new_ev(r1);
          }
            break;
          case BytecodeStream::DEL_VEC_E:
          {
            int r1 = frame.bs->reg(frame.pc);
            frame.reg.delete_ev(r1);
          }
            break;
          case BytecodeStream::GET_VEC_E:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            std::vector<Expression*>& ev = *frame.reg.ev(r1);
            IntVal idx = frame.reg.i(r2);
            frame.reg.e(ev[idx.toInt()], r3);
          }
            break;
          case BytecodeStream::PUT_VEC_E:
          {
            int r1 = frame.bs->reg(frame.pc);
            Expression *val = frame.reg.e(r1);
            int r2 = frame.bs->reg(frame.pc);
            int r3 = frame.bs->reg(frame.pc);
            std::vector<Expression*>& ev = *frame.reg.ev(r3);
            IntVal idx = frame.reg.i(r2);
            if (ev.size() < idx)
              ev.resize(idx.toInt()+1);
            ev[idx.toInt()] = val;
          }
            break;
          case BytecodeStream::MK_ARRAY_I:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
            GCLock lock;
            std::vector<IntVal>& iv = *frame.reg.iv(r1);
            std::vector<Expression*> a(iv.size());
            for (unsigned int i=0; i<iv.size(); i++) {
              a[i] = IntLit::a(iv[i]);
            }
            frame.reg.e(new ArrayLit(Location().introduce(), a), r2);
          }
            break;
          case BytecodeStream::MK_ARRAY_E:
          {
            int r1 = frame.bs->reg(frame.pc);
            int r2 = frame.bs->reg(frame.pc);
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
  
  void testBytecode() {
    std::ifstream t("test.mza", std::ifstream::in);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    try {
      auto bs = parse(str);
      for (auto& b : bs) {
        std::cerr << "---------------\n";
        std::cerr << b.toString();
      }
      std::cerr << "Run last proc:\n";
      BytecodeFrame frame(bs.back());
      Interpreter interpreter(bs, frame);
      interpreter.run();
    } catch (Error& e) {
      std::cerr << e.msg() << "\n";
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
    for (std::string line; std::getline(iss,line); ) {
      if (line.size() == 0 || line[0]=='%')
        continue;
      size_t colon = line.find(':');
      if (colon != std::string::npos) {
        if (!cur_proc.empty()) {
          if (procs.find(cur_proc) != procs.end())
            throw Error("Error: procedure "+cur_proc+" already defined before\n");
          procs[cur_proc] = codes.size();
          codes.push_back(cur_code);
          toPatch.push_back(cur_toPatch);
          cur_code = BytecodeStream();
          cur_toPatch.clear();
        }
        cur_proc = line.substr(0,colon);
        continue;
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
      } else if (instrRR(line,"MOV",r1,r2)) {
        cur_code.addInstr(BytecodeStream::MOV);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrI(line,"JMP",r1)) {
        cur_code.addInstr(BytecodeStream::JMP);
        cur_code.addReg(r1);
      } else if (instrRI(line,"JMPIF",r1,r2)) {
        cur_code.addInstr(BytecodeStream::JMPIF);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRI(line,"JMPIFNOT",r1,r2)) {
        cur_code.addInstr(BytecodeStream::JMPIFNOT);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
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
      } else if (instrR(line,"NEW_VEC_I",r1)) {
        cur_code.addInstr(BytecodeStream::NEW_VEC_I);
        cur_code.addReg(r1);
      } else if (instrR(line,"DEL_VEC_I",r1)) {
        cur_code.addInstr(BytecodeStream::DEL_VEC_I);
        cur_code.addReg(r1);
      } else if (instrRRR(line,"GET_VEC_I",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::GET_VEC_I);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"PUT_VEC_I",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::PUT_VEC_I);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrR(line,"NEW_VEC_E",r1)) {
        cur_code.addInstr(BytecodeStream::NEW_VEC_E);
        cur_code.addReg(r1);
      } else if (instrR(line,"DEL_VEC_E",r1)) {
        cur_code.addInstr(BytecodeStream::DEL_VEC_E);
        cur_code.addReg(r1);
      } else if (instrRRR(line,"GET_VEC_E",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::GET_VEC_E);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRRR(line,"PUT_VEC_E",r1,r2,r3)) {
        cur_code.addInstr(BytecodeStream::PUT_VEC_E);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
        cur_code.addReg(r3);
      } else if (instrRR(line,"MK_ARRAY_I",r1,r2)) {
        cur_code.addInstr(BytecodeStream::MK_ARRAY_I);
        cur_code.addReg(r1);
        cur_code.addReg(r2);
      } else if (instrRR(line,"MK_ARRAY_E",r1,r2)) {
        cur_code.addInstr(BytecodeStream::MK_ARRAY_E);
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
