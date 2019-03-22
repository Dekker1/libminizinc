/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Graeme Gange <graeme.gange@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_CODEGEN_INTERNAL_HPP__
#define __MINIZINC_CODEGEN_INTERNAL_HPP__
// Helper definitions for assorted bytecode generation stuff.
#include <minizinc/codegen.hh>

namespace MiniZinc {

#if 1
template<class V>
void PUSH_INSTR_OPERAND(CG_Instr& i, std::vector<V>& vec) {
  for(auto v : vec)
    PUSH_INSTR_OPERAND(i, v);
}
void PUSH_INSTR_OPERAND(CG_Instr& i, CG_Value x) {
  i.params.push_back(x);
}
void PUSH_INSTR_OPERAND(CG_Instr& i, CG_ProcID p) {
  i.params.push_back(CG_Value::proc(p.p));
}
void PUSH_INSTR_OPERAND(CG_Instr& i, BytecodeProc::Mode m) {
  i.params.push_back(CG::i((int) m));
}
void PUSH_INSTR_OPERAND(CG_Instr& i, AggregationCtx::Symbol s) {
  i.params.push_back(CG::i((int) s));
}
void PUSH_INSTR_OPERANDS(CG_Instr& i) { }

template<class T, typename ...Args>
void PUSH_INSTR_OPERANDS(CG_Instr& i, T x, Args... args) {
  PUSH_INSTR_OPERAND(i, x);
  PUSH_INSTR_OPERANDS(i, args...);
}

template<typename... Args>
void PUSH_INSTR(CG_Builder& cg, BytecodeStream::Instr i, Args... args) {
  cg.instrs.push_back(CG_Instr::instr(i));
  PUSH_INSTR_OPERANDS(cg.instrs.back(), args...);
}

void PUSH_LABEL(CG_Builder& frag, unsigned int label) { frag.instrs.push_back(CG_Instr::label(label)); }
/*
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, BytecodeStream::Instr i, Args... args) {
  std::cerr << "  " << instr_name(i);
  // cg.bytecode[cg.current_proc].addInstr(i);
  PUSH_INSTR_BODY(cg, args...);
}
template<class V, typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, std::vector<V>& vec, Args... args) {
  std::cerr << " " << vec.size();
  for(const V& v : vec)
    PUSH_INSTR_BODY(cg, v);
    // std::cerr << " " << v;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, CG_ProcID p, Args... args) {
  std::cerr << " proc[" << p.p << "]";
    // std::cerr << " " << v;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, CG_Value x, Args... args) {
  std::cerr << " " << x.value;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, BytecodeProc::Mode m, Args... args) {
  std::cerr << " " << mode_name(m);
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, AggregationCtx::Symbol ctx, Args... args) {
  std::cerr << " " << agg_name(ctx);
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR(CG_Builder& cg, Args... args) {
  PUSH_INSTR_BODY(cg, args...);
  std::cerr << std::endl;
}
void PUSH_LABEL(CG_Builder& frag, unsigned int label) { std::cerr << label << ":" << std::endl; }
*/

#else

void PUSH_INSTR_BODY(CG_Builder& cg) { }

template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, BytecodeStream::Instr i, Args... args) {
  std::cerr << "  " << instr_name(i);
  // cg.bytecode[cg.current_proc].addInstr(i);
  PUSH_INSTR_BODY(cg, args...);
}
template<class V, typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, std::vector<V>& vec, Args... args) {
  std::cerr << " " << vec.size();
  for(const V& v : vec)
    PUSH_INSTR_BODY(cg, v);
    // std::cerr << " " << v;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, CG_ProcID p, Args... args) {
  std::cerr << " proc[" << p.p << "]";
    // std::cerr << " " << v;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, CG_Value x, Args... args) {
  std::cerr << " " << x.value;
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, BytecodeProc::Mode m, Args... args) {
  std::cerr << " " << mode_name(m);
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR_BODY(CG_Builder& cg, AggregationCtx::Symbol ctx, Args... args) {
  std::cerr << " " << agg_name(ctx);
  PUSH_INSTR_BODY(cg, args...);
}
template<typename... Args>
void PUSH_INSTR(CG_Builder& cg, Args... args) {
  PUSH_INSTR_BODY(cg, args...);
  std::cerr << std::endl;
}
void PUSH_LABEL(CG_Builder& frag, unsigned int label) { std::cerr << label << ":" << std::endl; }
#endif

// Basic generator manipulation.
inline int GET_LABEL(CodeGen& cg) { return cg.current_label_count++; }
inline int GET_REG(CodeGen& cg) { return cg.current_reg_count++; }
inline int TEMP_REG(CodeGen& cg) {
  if(cg.temporary_reg == (unsigned int) -1)
    cg.temporary_reg = GET_REG(cg);
  return cg.temporary_reg;
}

struct REG {
  REG(int _r) : r(_r) { }
  void operator()(CodeGen& cg, CG_Builder& frag) { PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r)); }
  int r;
};

struct LOC {
  LOC(Loc _l) : l(_l) { }
  void operator()(CodeGen& cg, CG_Builder& frag) {
    int r;
    if(!l.is_global()) {
      r = l.index();
    } else {
      r = TEMP_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(l.index()), CG::r(r));
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r)); 
  }
  Loc l;
};

// Combinators for slightly safer code generation.
struct PUSH_REG {
  PUSH_REG(int _r) : r(_r) { }
  void operator()(CodeGen& cg, CG_Builder& frag) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  }
  int r;
};
struct PUSH {
  PUSH(void) { }
  PUSH_REG operator()(int r) { return PUSH_REG(r); }
};

template<class T>
struct Retn {
  Retn(T _x) : x(_x) { }

  T operator()(CodeGen& cg, CG_Builder& frag) const { return x; }
  T x;
};
struct RETN {
  RETN() { }

  template<class T>
  Retn<T> operator()(T x) { return Retn<T>(x); }
};

template<class V, class E>
struct _LET {
  V v;
  E e; 
  auto operator()(CodeGen& cg, CG_Builder& frag) -> decltype(e(0)(cg)) {
    // 
    int reg(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER);
    v(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(reg));
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
    return e(reg)(cg, frag);
  }
};
template<class V, class E>
_LET<V, E> LET(V&& v, E&& e) { return _LET<V, E> { std::move(v), std::move(e) }; }

// Iterating over various things -- vectors, interleaved vectors, and sets.
template<class V, class E>
struct _FOREACH {
  V v;
  E e;
  auto operator()(CodeGen& cg, CG_Builder& frag) -> decltype(e(0)(cg, frag)) {
    int r(v(cg, frag)); // Get the register for v.
    int rB(GET_REG(cg));
    int rE(GET_REG(cg));
    int rV(GET_REG(cg));
    int lblH(GET_LABEL(cg));
    int lblE(GET_LABEL(cg));
    // Set up the iterators
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(rB));
    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r), CG::r(rE));
    // Check if the vec is non-empty
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rB), CG::r(rE), CG::r(rV));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rV), CG::l(lblE));
    PUSH_LABEL(frag, lblH);
    // Dereference the iterator
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(rB), CG::r(rV));
    // Emit code for the body
    e(rV)(cg, frag);
    // Now increment and loop back.
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rB));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rB), CG::r(rE), CG::r(rV));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(rV), CG::l(lblH));
    PUSH_LABEL(frag, lblE);
  }
};
template<class V, class E>
_FOREACH<V, E> FOREACH(V&& v, E&& e) { return _FOREACH<V, E> { std::move(v), std::move(e) }; }

// Same as FOREACH, but when working with a vector of pairs (i.e. sets)
template<class V, class E>
struct _FOREACH2 {
  V v;
  E e;
  auto operator()(CodeGen& cg, CG_Builder& frag) -> decltype(e(0,0)(cg, frag)) {
    int r(v(cg, frag)); // Get the register for v.
    int rB(GET_REG(cg));
    int rE(GET_REG(cg));
    int rV1(GET_REG(cg));
    int rV2(GET_REG(cg));
    int lblH(GET_LABEL(cg));
    int lblE(GET_LABEL(cg));
    // Set up the iterators
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(rB));
    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r), CG::r(rE));
    // Check if the vec is non-empty
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rB), CG::r(rE), CG::r(rV1));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rV1), CG::l(lblE));
    PUSH_LABEL(frag, lblH);
    // Dereference the iterator
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(rB), CG::r(rV1));
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rB));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(rB), CG::r(rV2));
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rB));
    // Emit code for the body
    e(rV1, rV2)(cg, frag);
    // Now increment and loop back.
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rB), CG::r(rE), CG::r(rV1));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(rV1), CG::l(lblH));
    PUSH_LABEL(frag, lblE);
  }
};
template<class V, class E>
_FOREACH2<V, E> FOREACH2(V v, E e) { return _FOREACH2<V, E> { v, e }; }

template<class E>
struct _FORRANGE {
  int rL;
  int rU;
  E e;
  _FORRANGE(int _rL, int _rU, E _e) : rL(_rL), rU(_rU), e(_e) { }

  auto operator()(CodeGen& cg, CG_Builder& frag) -> decltype(e(0)(cg, frag)) {
    int lblH(GET_LABEL(cg));
    int lblE(GET_LABEL(cg));
    int rV(GET_REG(cg));
    int rC(GET_REG(cg));
    // Check if the range is non-empty.
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rL), CG::r(rU), CG::r(rC));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblE));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(rL), CG::r(rV));
    PUSH_LABEL(frag, lblH);
    // Emit code for the body
    e(rV)(cg, frag);
    // Now increment and loop back.
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rV));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rV), CG::r(rU), CG::r(rC));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(rC), CG::l(lblH));
    PUSH_LABEL(frag, lblE);
  }
};
template<class E>
_FORRANGE<E> FORRANGE(int rL, int rU, E e) { return _FORRANGE<E>(rL, rU, e); }


// We can use FOREACH2 and FORRANGE to iterate over sets.
template<class V, class E>
struct _FORSET {
  V v;
  E e;
  _FORSET(V&& _v, E&& _e) : v(_v), e(_e) { }
  
  void operator()(CodeGen& cg, CG_Builder& frag) {
    FOREACH2(v, [this](int rL, int rU) { return FORRANGE(rL, rU, e); })(cg, frag);
  }
};
template<class V, class E>
_FORSET<V, E> FORSET(V&& v, E&& e) { return _FORSET<V, E>(std::move(v), std::move(e)); }

// Non-combinator versions of the iteration generators.
// Less safe, because they don't automatically resolve containment, but more convenient
// if, say, we need unbounded 
struct Foreach {
  Foreach(CodeGen& cg, int _r, int k = 1)
    : lblH(GET_LABEL(cg)), lblE(GET_LABEL(cg))
    , r(_r), rB(GET_REG(cg)), rE(GET_REG(cg)) {
    assert(k > 0);
    for(int ii = 0; ii < k; ++ii)
      rVS.push_back(GET_REG(cg));
  }
  
  void emit_pre(CG_Builder& frag) {
    // Set up the iterators
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(rB));
    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r), CG::r(rE));
    // Check if the vec is non-empty
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(rB), CG::r(rE), CG::r(rVS[0]));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rVS[0]), CG::l(lblE));
    PUSH_LABEL(frag, lblH);
    // Dereference the iterator
    for(int rv : rVS) {
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(rB), CG::r(rv));
      PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rB));
    }
  }

  void emit_post(CG_Builder& frag) {
    // Now increment and loop back.
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(rB), CG::r(rE), CG::r(rVS[0]));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(rVS[0]), CG::l(lblH));
    PUSH_LABEL(frag, lblE);
  }

  int val(int i) const { return rVS[i]; }

  int lblH;
  int lblE;
  int r;
  int rB;
  int rE;
  std::vector<int> rVS; 
};

struct Forrange {
  Forrange(CodeGen& _cg, int _rL, int _rU)
    : cg(_cg)
    , lblH(GET_LABEL(cg)), lblE(GET_LABEL(cg)), lblCont(-1)
    , rL(_rL), rU(_rU)
    , rV(GET_REG(cg)), rC(GET_REG(cg)) {

  }
  
  void emit_pre(CG_Builder& frag) {
     // Check if the range is non-empty.
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rL), CG::r(rU), CG::r(rC));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblE));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(rL), CG::r(rV));
    PUSH_LABEL(frag, lblH);
  }

  void emit_post(CG_Builder& frag) {
    // Now increment and loop back.
    if(lblCont != -1)
      PUSH_LABEL(frag, lblCont);
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(rV));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(rV), CG::r(rU), CG::r(rC));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(rC), CG::l(lblH));
    PUSH_LABEL(frag, lblE);
  }
  int val(void) const { return rV; }
  int cont(void) {
    if(lblCont == -1)
      lblCont = GET_LABEL(cg);
    return lblCont;
  }

  CodeGen& cg;
  int lblH;
  int lblE;
  int lblCont;
  int rL;
  int rU;
  int rV;
  int rC;
};

struct Forset {
  Forset(CodeGen& cg, int r)
    : ranges(cg, r, 2), values(cg, ranges.val(0), ranges.val(1)) { }

  void emit_pre(CG_Builder& frag) {
    ranges.emit_pre(frag);
    values.emit_pre(frag);
  }
  void emit_post(CG_Builder& frag) {
    values.emit_post(frag);
    ranges.emit_post(frag);
  }

  int val(void) const { return values.val(); }
  int cont(void) { return values.cont(); }

  Foreach ranges;
  Forrange values;
};

};

#endif
