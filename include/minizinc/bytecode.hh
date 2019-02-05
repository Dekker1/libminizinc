/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_BYTECODE_HH__
#define __MINIZINC_BYTECODE_HH__

#include <vector>
#include <iostream>

#include <minizinc/values.hh>

namespace MiniZinc {

  class BytecodeStream {
  protected:
    /// The bytecode stream
    std::vector<char> bs;
  public:
    enum Instr {
      ADDI, // R1, R2 -> R3
      SUBI, // R1, R2 -> R3
      MULI, // R1, R2 -> R3
      DIVI, // R1, R2 -> R3
      MODI, // R1, R2 -> R3
      INCI, // R1
      DECI, // R1
      
      IMMI, // I, R : Load immediate integer to register
      MOV,  // R1 -> R2

      JMP,  // i : pc+=i
      JMPIF,     // R, i: if R then pc+=i
      JMPIFNOT,  // R, i: if not R then pc+=i
      
      EQI,
      LTI,
      LEI,
      
      AND,
      OR,
      NOT,
      XOR,
      
      ISPAR,
      ISVAR,
      ISABSENT,
      ISOPT,
      
      RET, // R: return value from register R
      CALL, // i, n, R1, ..., Rn
      TCALL, // i : call code i (arguments are assumed to be in correct registers already)
      
      TRACE, // output string
      ABORT, // abort execution
      
      NEW_VEC_I, // R: allocate new integer vector in R
      DEL_VEC_I, // R: delete integer vector in R
      GET_VEC_I, // R1, R2 -> R3: get element R2 from integer vector in R1 into R3
      PUT_VEC_I, // R1, R2, R3: put R1 into element R2 of vector in R3
      NEW_VEC_E, // R: allocate new expression vector in R
      DEL_VEC_E, // R: delete expression vector in R
      GET_VEC_E, // R1, R2 -> R3: get element R2 from expression vector in R1 into R3
      PUT_VEC_E, // R1, R2, R3: put R1 into element R2 of vector in R3
      
      MK_ARRAY_I, // R1 -> R2: Make array literal from integer vector in R1
      MK_ARRAY_E, // R1 -> R2: Make array literal from expression vector in R1
    };
    
    /// Get instruction at \a pc and increment \a pc
    Instr instr(int& pc) const { assert(pc < bs.size()); return static_cast<Instr>(bs[pc++]); }
    IntVal intval(int& pc) const { assert(pc < bs.size()); const IntVal* iv = reinterpret_cast<const IntVal*>(&bs[pc]); pc += sizeof(IntVal); return *iv; }
    Expression* expr(int& pc) { assert(pc < bs.size()); Expression** e = reinterpret_cast<Expression**>(&bs[pc]); pc += sizeof(Expression*); return *e; }
    int reg(int& pc) const { assert(pc < bs.size()); const int* iv = reinterpret_cast<const int*>(&bs[pc]); pc += sizeof(int); return *iv;}

    int size(void) const { return bs.size(); }
    bool eos(int pc) const { return pc >= bs.size(); }
    
    void patchAddress(int pc, int addr) {
      const char* cp = reinterpret_cast<const char*>(&addr);
      for (int i=0; i<sizeof(int); i++) {
        bs[pc+i] = cp[i];
      }
    }
    
    void addInstr(const Instr& i) { bs.push_back(i); }
    void addReg(int iv) {
      const char* cp = reinterpret_cast<const char*>(&iv);
      for (int i=0; i<sizeof(int); i++) {
        bs.push_back(cp[i]);
      }
    }
    void addIntVal(const IntVal& iv) {
      const char* cp = reinterpret_cast<const char*>(&iv);
      for (int i=0; i<sizeof(IntVal); i++) {
        bs.push_back(cp[i]);
      }
    }
    void addExpr(Expression* e) {
      const char* cp = reinterpret_cast<const char*>(e);
      for (int i=0; i<sizeof(Expression*); i++) {
        bs.push_back(cp[i]);
      }
    }

    std::string toString(void) const;
    
  };

  class RegisterFile {
  protected:
    union RegContent {
      Expression* e;
      IntVal i;
      std::vector<IntVal>* iv;
      std::vector<Expression*>* ev;
      RegContent() : i() {}
    };
    std::vector<RegContent> _r;
  public:
    Expression* e(int reg) { return _r[reg].e; }
    IntVal i(int reg) { return _r[reg].i; }
    std::vector<IntVal>* iv(int reg) { return _r[reg].iv; }
    std::vector<Expression*>* ev(int reg) { return _r[reg].ev; }
    void e(Expression* exp, int reg) { if (reg >= _r.size()) _r.resize(reg+1); _r[reg].e=exp; }
    void i(IntVal iv, int reg) { if (reg >= _r.size()) _r.resize(reg+1); _r[reg].i=iv; }
    void new_iv(int reg) { if (reg >= _r.size()) _r.resize(reg+1); _r[reg].iv = new std::vector<IntVal>(); }
    void delete_iv(int reg) { delete _r[reg].iv; _r[reg].iv=NULL; }
    void new_ev(int reg) { if (reg >= _r.size()) _r.resize(reg+1); _r[reg].ev = new std::vector<Expression*>(); }
    void delete_ev(int reg) { delete _r[reg].ev; _r[reg].ev=NULL; }
    void mov(int r1, int r2) {
      if (r1 >= _r.size()) _r.resize(r1+1);
      if (r2 >= _r.size()) _r.resize(r2+1);
      _r[r2] = _r[r1];
    }
    void mov(int r1, RegisterFile& rf, int r2) {
      if (r1 >= _r.size()) _r.resize(r1+1);
      if (r2 >= rf._r.size()) rf._r.resize(r2+1);
      rf._r[r2] = _r[r1];
    }
  };
  
  class BytecodeFrame {
  public:
    RegisterFile reg;
    const BytecodeStream* bs;
    int pc;
    BytecodeFrame(const BytecodeStream& bs0) : bs(&bs0), pc(0) {}
  };
  
  class Interpreter {
  protected:
    std::vector<BytecodeFrame> _stack;
    const std::vector<BytecodeStream>& _procs;
  public:
    Interpreter(const std::vector<BytecodeStream>& procs, const BytecodeFrame& f) : _procs(procs) {
      _stack.push_back(f);
    }
    void run(void);
  };
  
  void testBytecode();

  std::vector<BytecodeStream> parse(const std::string& s);

}

#endif
