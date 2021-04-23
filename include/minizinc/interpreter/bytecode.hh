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

#include <minizinc/interpreter/values.hh>

namespace MiniZinc {

class BytecodeProc;

class BytecodeStream {
protected:
  /// The bytecode stream
  std::vector<char> _bs;
  /// The largest register used by this code
  int _max_reg;

public:
  /// Constructor
  BytecodeStream(void) : _max_reg(0) {}

  enum Instr {
    ADDI,  // R1, R2 -> R3
    SUBI,  // R1, R2 -> R3
    MULI,  // R1, R2 -> R3
    DIVI,  // R1, R2 -> R3
    MODI,  // R1, R2 -> R3
    INCI,  // R1
    DECI,  // R1

    IMMI,          // I, R : Load immediate integer into register
    CLEAR,         // R1, Rn : Clear all registers R1, R2, ..., Rn
    LOAD_GLOBAL,   // i -> R : Load global i into register R (globals are registers of the bottom
                   // stack frame)
    STORE_GLOBAL,  // R -> i : Store register R into global i
    MOV,           // R1 -> R2

    JMP,       // i : pc=i
    JMPIF,     // R, i: if R then pc=i
    JMPIFNOT,  // R, i: if not R then pc=i

    EQI,
    LTI,
    LEI,

    AND,
    OR,
    NOT,
    XOR,

    ISPAR,      // R1 -> R2: put whether value in R1 is not a variable into R2
    ISEMPTY,    // R1 -> R2: put whether vector in R1 is empty into R2
    LENGTH,     // R1 -> R2: put length of array in R1 into R2
    GET_VEC,    // R1, R2 -> R3: put element R2 of vector in R1 into R3
    GET_ARRAY,  // n, R1, R2, ... Rn -> Rn+1 Rn+2: put element [R2,...,Rn] of n-dimensional vector
                // in R1 into Rn+1 with success signal Rn+2

    LB,   // R1 -> R2: put lower bound of value in R1 into R2
    UB,   // R1 -> R2: put upper bound of value in R1 into R2
    DOM,  // R1 -> R2: put domain of value in R1 into R2

    MAKE_SET,      // R1 -> R2: turn a vector (of values) into a set
    INTERSECTION,  // R1, R2 -> R3: put intersection of sets in R1 and R2 into R3
    UNION,         // R1, R2 -> R3: put union of sets in R1 and R2 into R3
    DIFF,          // R1, R2 -> R3: put difference of sets in R1 and R2 into R3

    INTERSECT_DOMAIN,  // R1, R2 -> R3: Update domain of R1 with set R2, place result in R3

    OPEN_AGGREGATION,   // i: Create a new aggregation context with symbol i
    CLOSE_AGGREGATION,  // Close current aggregation context, put result onto context above

    SIMPLIFY_LIN,  // R1, R2, i -> R3, R4, R5: simplify linear expression (R1-R2-i), return
                   // coefficients (R3), variables (R4), constant (R5)

    PUSH,  // R: push R onto value stack
    POP,   // R: pop from value stack into R
    POST,  // R: post constraint in R

    RET,   // return from call
    CALL,  // m, i, cse, R1, ..., Rn: call code i in mode m with n arguments. The 0/1 flag 'cse' is
           // used to signal if a CSE lookup should be performed
    BUILTIN,  // i, n, R1, ..., Rn : call builtin function i
    TCALL,    // m, i, cse : call code i in mode m (arguments are assumed to be in correct registers
              // already). The 0/1 flag 'cse' is used to signal if a CSE lookup should be performed

    ITER_ARRAY,  // R, l: Iterate over array in R, jump to l when finished.
    ITER_VEC,    // R, l: Iterate over vector in R, jump to l when finished.
    ITER_RANGE,  // R1, R2, l: Iterate over values in [R1, R2]
    ITER_NEXT,  // R: increment the topmost loop, binding the result to R. Pop and jump to loop exit
                // if finished.
    ITER_BREAK,  // i : drop i iterators, jump to the exit of the last.

    TRACE,  // R: output string representation of R
    ABORT,  // abort execution

  };

  /// Get instruction at \a pc and increment \a pc
  Instr instr(int& pc) const {
    assert(pc < _bs.size());
    return static_cast<Instr>(_bs[pc++]);
  }
  Val intval(int& pc) const {
    assert(pc < _bs.size());
    const Val* iv = reinterpret_cast<const Val*>(&_bs[pc]);
    pc += sizeof(Val);
    return *iv;
  }
  Expression* expr(int& pc) {
    assert(pc < _bs.size());
    Expression** e = reinterpret_cast<Expression**>(&_bs[pc]);
    pc += sizeof(Expression*);
    return *e;
  }
  int reg(int& pc) const {
    assert(pc < _bs.size());
    const int* iv = reinterpret_cast<const int*>(&_bs[pc]);
    pc += sizeof(int);
    return *iv;
  }
  char chr(int& pc) const {
    assert(pc < _bs.size());
    return _bs[pc++];
  }
  const char* str(int& pc) const {
    assert(pc < _bs.size());
    int n = reg(pc);
    const char* ret = &_bs[pc];
    pc += n + 1;
    return ret;
  }

  int size(void) const { return _bs.size(); }
  bool eos(int pc) const { return pc >= _bs.size(); }

  void patchAddress(int pc, int addr) {
    const char* cp = reinterpret_cast<const char*>(&addr);
    for (int i = 0; i < sizeof(int); i++) {
      _bs[pc + i] = cp[i];
    }
  }

  void addInstr(const Instr& i) { _bs.push_back(i); }
  void addReg(int iv, bool global = false) {
    if (!global) {
      _max_reg = std::max(_max_reg, iv);
    }
    const char* cp = reinterpret_cast<const char*>(&iv);
    for (int i = 0; i < sizeof(int); i++) {
      _bs.push_back(cp[i]);
    }
  }
  void addSmallInt(int iv) {
    const char* cp = reinterpret_cast<const char*>(&iv);
    for (int i = 0; i < sizeof(int); i++) {
      _bs.push_back(cp[i]);
    }
  }
  void addIntVal(const IntVal& iv) {
    Val v = Val::fromIntVal(iv);
    const char* cp = reinterpret_cast<const char*>(&v);
    for (int i = 0; i < sizeof(Val); i++) {
      _bs.push_back(cp[i]);
    }
  }
  void addCharVal(const char c) { _bs.push_back(c); }
  void addStr(const std::string& s) {
    addReg(s.size());
    for (char c : s) _bs.push_back(c);
    _bs.push_back(0);
  }

  void setNArgs(int n) { _max_reg = std::max(_max_reg, n); }

  std::string toString(const std::vector<BytecodeProc>& procs = std::vector<BytecodeProc>()) const;
  int maxRegister(void) const { return _max_reg; }
};

class BytecodeProc {
public:
  /// The name of this procedure
  std::string name;
  /// Number of arguments
  int nargs;
  /// Delayed execution
  bool delay;
  /// Modes
  enum Mode { ROOT, ROOT_NEG, FUN, FUN_NEG, IMP, IMP_NEG, MAX_MODE = IMP_NEG };
  static const std::string mode_to_string[MAX_MODE + 1];
  static const bool is_neg(const Mode& mode) {
    return mode == ROOT_NEG || mode == FUN_NEG || mode == IMP_NEG;
  }
  static const Mode negate(const Mode& mode) {
    switch (mode) {
      case ROOT:
        return ROOT_NEG;
      case IMP:
        return IMP_NEG;
      case FUN:
        return FUN_NEG;
      case ROOT_NEG:
        return ROOT;
      case IMP_NEG:
        return IMP;
      case FUN_NEG:
        return FUN;
      default:
        assert(false);
    }
  }
  /// The code for different modes
  BytecodeStream mode[MAX_MODE + 1];
};

std::vector<BytecodeProc> parse(const std::string& s);
}  // namespace MiniZinc
