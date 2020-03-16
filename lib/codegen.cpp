#include <functional>
#include <set>
#include <minizinc/flatten.hh>
#include <minizinc/eval_par.hh>
#include <minizinc/copy.hh>
#include <minizinc/hash.hh>
#include <minizinc/astexception.hh>
#include <minizinc/optimize.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/output.hh>
#include <minizinc/prettyprinter.hh>

#include <minizinc/bytecode.hh>
#include <minizinc/codegen.hh>
#include <../lib/codegen/codegen_internal.hpp>
#include <../lib/codegen/analysis.hpp>

namespace MiniZinc {


// Known limitations:
// - Comprehensions and generator expressions are assumed to be total, so are evaluated in root context.
// Expression evaluation currently runs in two modes:
// - eval, which places the result onto the value stack on the enclosing context, or
// - locate, which places the result into a register, and returns the register.
// this is done to avoid a unnecessary push/pop sequences, when a result is already bound to
// a register, and is needed for e.g. a call.

#define ENABLE_PLUS 1

using Mode = CG::Mode;

struct builtin_t {
  std::function<CG_Cond::T(Call*, Mode, CodeGen&, CG_Builder&)> boolean;
  std::function<CG::Binding(Call*, Mode, CodeGen&, CG_Builder&)> general;
};

typedef std::unordered_map<ASTString, builtin_t> builtin_table;

const char* instr_names[] = {
      "ADDI",
      "SUBI",
      "MULI",
      "DIVI",
      "MODI",
      "INCI",
      "DECI",
      
      "IMMI",
      "LOAD_GLOBAL",
      "STORE_GLOBAL",
      "MOV",

      "JMP",
      "JMPIF",
      "JMPIFNOT",
      
      "EQI",
      "LTI",
      "LEI",
      
      "AND",
      "OR",
      "NOT",
      "XOR",

      "ISPAR",
      "ISEMPTY",
      "LENGTH",
      "GET_VEC",

      "LB",
      "UB",
      "DOM",

      "MAKE_SET",
      "INTERSECTION",
      "UNION",

      "INTERSECT_DOMAIN",
      
      "OPEN_AGGREGATION",
      "CLOSE_AGGREGATION",
      "SIMPLIFY_LIN",
      
      "PUSH",
      "POP",
      "POST",
      
      "RET",
      "CALL",
      "BUILTIN",
      "TCALL",
      
      "TRACE",
      "ABORT",
    };


const char* mode_names[] = {
  "RAW",
  "ROOT",
  "ROOT_NEG",
  "FUN",
  "FUN_NEG",
  "IMP",
  "IMP_NEG",
  "MAX_MODE",
};

const char* agg_names[] = {
  "AND",
  "OR",
  "VEC",
  "OTHER"
};
const char* instr_name(BytecodeStream::Instr i) {
  return instr_names[i];
}

const char* agg_name(AggregationCtx::Symbol s) {
  return agg_names[s];
}
const char* mode_name(BytecodeProc::Mode m) {
  return mode_names[m];
}

inline void TODO(void) {
  throw InternalError("Not yet implemented!");
}

void CodeGen::register_builtins(void) {
  // Solver Built-ins
  register_builtin("mk_intvar", 1);

//  register_builtin("bool_not", 1);
//  register_builtin("bool_clause", 2);
//
//  register_builtin("int_eq", 2);
//  register_builtin("int_lt", 2);
//  register_builtin("int_le", 2);
//  register_builtin("set_in", 2);
//
//  register_builtin("int_lin_le", 3);
//  register_builtin("int_lin_eq", 3);
//
//  register_builtin("int_sum", 1);
//  register_builtin("int_plus", 2);
//  register_builtin("int_min", 2);
//  register_builtin("int_times", 2);
//  register_builtin("int_pow", 2);
//  register_builtin("int_div", 2);
//
//  register_builtin("int_element", 2);
//  register_builtin("bool_element", 2);
//
//  register_builtin("float_div", 2);

  register_builtin("absent", 1);

  // Interpreter Built-ins
  register_builtin("uniform", 2);
  register_builtin("sol", 1);
  register_builtin("sort_by", 2);
  register_builtin("floor", 1);
  register_builtin("ceil", 1);
}

void OPEN_AGG(CodeGen& cg, CG_Builder& frag, AggregationCtx::Symbol ctx) {
  cg.env_push();
  cg.reg_trail.push_back(cg.current_reg_count);
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, ctx);
}
void CLOSE_AGG(CodeGen& cg, CG_Builder& frag) {
  assert(cg.reg_trail.size() > 0);
  int old_reg_count = cg.current_reg_count;
  cg.current_reg_count = cg.reg_trail.back();
  cg.reg_trail.pop_back();
  for(int ii = cg.current_reg_count; ii < old_reg_count; ++ii)
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(ii));
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  cg.env_pop();
}

void OPEN_AND(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_AND); }
void OPEN_OR(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OR); }
void OPEN_OTHER(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OTHER); }
void OPEN_VEC(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_VEC); }

CG_ProcID CodeGen::register_builtin(std::string s, unsigned int arity) {
  auto it(_proc_map.find(s));
  if(it != _proc_map.end()) {
    // throw InternalError(std::string("Builtin already registered: ") + s);
    std::cerr << "WARNING: Builtin " << s << " already registered." << std::endl;
    return it->second;
  }

  CG_ProcID id(CG_ProcID::builtin(_builtins.size()));
  _builtins.push_back(std::make_pair(s, arity));
  _proc_map.insert(std::make_pair(s, id));
  return id;
}

CG_ProcID CodeGen::find_builtin(std::string s) {
  auto it(_proc_map.find(s));
  assert(it != _proc_map.end());
  return (*it).second;
}

int bind_cst(int x, CodeGen& cg, CG_Builder& frag);

void call_binop(CodeGen& cg, CG_Builder& frag, Mode ctx, BinOpType op, int r_lhs, int r_rhs) {
  GCLock lock;
  switch(op) {
    // Actual builtins
    case BOT_EQ: {
      auto fun = find_call_fun(cg, {"op_equals"}, Type::varbool(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_LQ:
    {
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = bind_cst(0, cg, frag);
      OPEN_OTHER(cg, frag);
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
      assert(fun.second == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(c));
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(c), CG::r(c), CG::r(x), CG::r(k));
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(z), CG::r(k), CG::r(k));
      fun = find_call_fun(cg, {"int_lin_le"}, Type::varbool(), {Type::parint(1), Type::varint(1), Type::parint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(c), CG::r(x), CG::r(k));
      return;
    }
    case BOT_LE:
    {
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = bind_cst(-1, cg, frag);
      OPEN_OTHER(cg, frag);
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
      assert(fun.second == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(c));
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(c), CG::r(c), CG::r(x), CG::r(k));
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(z), CG::r(k), CG::r(k));
      fun = find_call_fun(cg, {"int_lin_le"}, Type::varbool(), {Type::parint(1), Type::varint(1), Type::parint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(c), CG::r(x), CG::r(k));
      return;
    }
    case BOT_IN: {
      auto fun = find_call_fun(cg, {"'in'"}, Type::varint(), {Type::varint(), Type::varsetint()}, BytecodeProc::FUN);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_PLUS: {
      if (ENABLE_PLUS) {
        auto fun = find_call_fun(cg, {"op_plus"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
        assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
        PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      } else {
        OPEN_OTHER(cg, frag);
        OPEN_VEC(cg, frag);
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
        CLOSE_AGG(cg, frag);
        CLOSE_AGG(cg, frag);
        int r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        auto fun = find_call_fun(cg, {"sum"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
        assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
        PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r));
      }
      return;
    }
    case BOT_MINUS: {
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_MULT: {
      auto fun = find_call_fun(cg, {"op_times"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_IDIV: {
      auto fun = find_call_fun(cg, {"op_int_division"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_DIV: {
      auto fun = find_call_fun(cg, {"op_float_division"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    // Normalisation
    case BOT_NQ:
      call_binop(cg, frag, -ctx, BOT_EQ, r_lhs, r_rhs);
      return;
    case BOT_GR:
      call_binop(cg, frag, ctx, BOT_LE, r_rhs, r_lhs);
      return;
    case BOT_GQ:
      call_binop(cg, frag, ctx, BOT_LQ, r_rhs, r_lhs);
      return;

    case BOT_XOR:
      call_binop(cg, frag, -ctx, BOT_EQUIV, r_lhs, r_rhs);
      return;
    case BOT_RIMPL:
      call_binop(cg, frag, ctx, BOT_IMPL, r_rhs, r_lhs);
      return;
    case BOT_DOTDOT:
      // The values in r_lhs and r_rhs had better be IMMIs.
      OPEN_OTHER(cg, frag);
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      return;
    case BOT_PLUSPLUS:
    {
      OPEN_OTHER(cg, frag);
      OPEN_VEC(cg, frag);

      Foreach iter_lhs(cg, r_lhs);
      iter_lhs.emit_pre(frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(iter_lhs.val()));
      iter_lhs.emit_post(frag);

      Foreach iter_rhs(cg, r_rhs);
      iter_rhs.emit_pre(frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(iter_rhs.val()));
      iter_rhs.emit_post(frag);

      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      return;
    }
    default:
      TODO();
    // BOT_PLUS, BOT_MINUS, BOT_MULT, BOT_DIV, BOT_IDIV, BOT_MOD, BOT_POW,
    // BOT_LE, BOT_LQ, BOT_GR, BOT_GQ, BOT_EQ, BOT_NQ,
    // BOT_IN, BOT_SUBSET, BOT_SUPERSET, BOT_UNION, BOT_DIFF, BOT_SYMDIFF,
    // BOT_INTERSECT,
    // BOT_PLUSPLUS,
    // BOT_EQUIV, BOT_IMPL, BOT_RIMPL, BOT_OR, BOT_AND, BOT_XOR,
    // BOT_DOTDOT
  }
}

int bind_binop_par(CodeGen& cg, CG_Builder& frag, BinOpType op, int r_lhs, int r_rhs) {
  int r;
  switch(op) {
    // Actual builtins
    case BOT_EQ:
    case BOT_EQUIV:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_NQ:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
      return r;
    case BOT_LE:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_LQ:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_GR:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
      return r;
    case BOT_GQ:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
      return r;
    case BOT_XOR:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::XOR, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
      return r;
    case BOT_AND:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::AND, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_OR:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_IMPL:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_lhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_RIMPL:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_rhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::XOR, CG::r(r_lhs), CG::r(r), CG::r(r));
      return r;
    case BOT_PLUS:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_MINUS:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_MULT:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_IDIV:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::DIVI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_MOD:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::MODI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_DOTDOT:
      // The values in r_lhs and r_rhs had better be IMMIs.
      OPEN_OTHER(cg, frag);
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    case BOT_IN: {
      OPEN_OTHER(cg, frag);
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::INTERSECTION, CG::r(r), CG::r(r_rhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r), CG::r(r));
      int l(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r));
      PUSH_LABEL(frag, l);
      }
      return r;
    case BOT_UNION:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_INTERSECT:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::INTERSECTION, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    // BOT_IN, BOT_SUBSET, BOT_SUPERSET, BOT_UNION, BOT_DIFF, BOT_SYMDIFF,
    // BOT_INTERSECT,
    default:
      TODO();
      return 0xdead;
    /*
    // BOT_PLUS, BOT_MINUS, BOT_MULT, BOT_DIV, BOT_IDIV, BOT_MOD, BOT_POW,
    // BOT_LE, BOT_LQ, BOT_GR, BOT_GQ, BOT_EQ, BOT_NQ,
    // BOT_IN, BOT_SUBSET, BOT_SUPERSET, BOT_UNION, BOT_DIFF, BOT_SYMDIFF,
    // BOT_INTERSECT,
    // BOT_PLUSPLUS,
    */
  }
}

CG_Cond::T binop_cond(CodeGen& cg, BinOpType op, Mode ctx, int r_lhs, int r_rhs) {
  GCLock lock;
  switch(op) {
    // Actual builtins
    case BOT_EQ: {
      auto fun = find_call_fun(cg, {"op_equals"}, Type::varbool(), {Type::varint(), Type::varint()}, ctx);
      assert(ctx == fun.second);
      return CG_Cond::call(fun.first, ctx, CG::r(r_lhs), CG::r(r_rhs));
    }
    case BOT_LQ:
    {
      auto fun = find_call_fun(cg, {"op_lessorequals"}, Type::varbool(), {Type::varint(), Type::varint()}, ctx);
      assert(ctx == fun.second);
      return CG_Cond::call(fun.first, ctx, CG::r(r_lhs), CG::r(r_rhs));
    }
    case BOT_LE:
    {
      auto fun = find_call_fun(cg, {"op_less"}, Type::varbool(), {Type::varint(), Type::varint()}, ctx);
      assert(ctx == fun.second);
      return CG_Cond::call(fun.first, ctx, CG::r(r_lhs), CG::r(r_rhs));
    }
    case BOT_IN: {
      auto fun = find_call_fun(cg, {"set_in"}, Type::varbool(), {Type::varint(), Type::varsetint()}, ctx);
      assert(ctx == fun.second);
      return CG_Cond::call(fun.first, ctx, CG::r(r_lhs), CG::r(r_rhs));
    }
    // Normalisation
    case BOT_NQ:
      return ~binop_cond(cg, BOT_EQ, -ctx, r_lhs, r_rhs);
    case BOT_GR:
      return binop_cond(cg, BOT_LE, ctx, r_rhs, r_lhs);
    case BOT_GQ:
      return binop_cond(cg, BOT_LQ, ctx, r_rhs, r_lhs);
    case BOT_XOR:
      return ~binop_cond(cg, BOT_EQUIV, -ctx, r_lhs, r_rhs);
    case BOT_RIMPL:
      return binop_cond(cg, BOT_IMPL, ctx, r_rhs, r_lhs);
    default:
      TODO();
    // BOT_PLUS, BOT_MINUS, BOT_MULT, BOT_DIV, BOT_IDIV, BOT_MOD, BOT_POW,
    // BOT_LE, BOT_LQ, BOT_GR, BOT_GQ, BOT_EQ, BOT_NQ,
    // BOT_IN, BOT_SUBSET, BOT_SUPERSET, BOT_UNION, BOT_DIFF, BOT_SYMDIFF,
    // BOT_INTERSECT,
    // BOT_PLUSPLUS,
    // BOT_EQUIV, BOT_IMPL, BOT_RIMPL, BOT_OR, BOT_AND, BOT_XOR,
    // BOT_DOTDOT
  }
  throw InternalError("Unexpected fall-through in binop_cond.");
}

CG_Cond::T linear_cond(CodeGen& cg, CG_Builder& frag, BinOpType op, Mode ctx, int r_lhs, int r_rhs) {
  GCLock lock;
  switch(op) {
    // Actual builtins
    case BOT_EQ: {
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = bind_cst(0, cg, frag);
      OPEN_OTHER(cg, frag);
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
      assert(fun.second == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(c));
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(c), CG::r(c), CG::r(x), CG::r(k));
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(z), CG::r(k), CG::r(k));
      fun = find_call_fun(cg, {"int_lin_eq"}, Type::varbool(), {Type::parint(1), Type::varint(1), Type::parint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      return CG_Cond::call(fun.first, fun.second, CG::r(c), CG::r(x), CG::r(k));
    }
    case BOT_LQ:
    {
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = bind_cst(0, cg, frag);
      OPEN_OTHER(cg, frag);
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
      assert(fun.second == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(c));
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(c), CG::r(c), CG::r(x), CG::r(k));
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(z), CG::r(k), CG::r(k));
      fun = find_call_fun(cg, {"int_lin_le"}, Type::varbool(), {Type::parint(1), Type::varint(1), Type::parint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      return CG_Cond::call(fun.first, fun.second, CG::r(c), CG::r(x), CG::r(k));
    }
    case BOT_LE:
    {
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = bind_cst(-1, cg, frag);
      OPEN_OTHER(cg, frag);
      auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
      assert(fun.second == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_lhs), CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(c));
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(c), CG::r(c), CG::r(x), CG::r(k));
      PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(z), CG::r(k), CG::r(k));
      fun = find_call_fun(cg, {"int_lin_le"}, Type::varbool(), {Type::parint(1), Type::varint(1), Type::parint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
      return CG_Cond::call(fun.first, fun.second, CG::r(c), CG::r(x), CG::r(k));
    }
    case BOT_NQ:
      return ~binop_cond(cg, BOT_EQ, -ctx, r_lhs, r_rhs);
    case BOT_GR:
      return binop_cond(cg, BOT_LE, ctx, r_rhs, r_lhs);
    case BOT_GQ:
      return binop_cond(cg, BOT_LQ, ctx, r_rhs, r_lhs);
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in linear_cond.");
}

CG_ProcID CodeGen::resolve_fun(FunctionI* fun) {
  auto it(fun_bodies.find(fun));
  if(it != fun_bodies.end())
    return it->second;
  
  GCLock lock;

  std::cerr << "%%%% Resolving: "; debugprint(fun);
  int p_idx = bytecode.size();
  CG_ProcID p_id(CG_ProcID::proc(p_idx));
  ASTExprVec<VarDecl> params(fun->params());

  std::stringstream ss;
  if (fun->e()) {
    ss << "f_" << fun->id().str();
    for (auto& param : params) {
      ss << "_";
      if (param->type().dim() > 0) {
        ss << "d" << param->type().dim();
      } else if (param->type().dim() < 0) {
        ss << "d$";
      }
      if (param->type().isvar()) {
        ss << "v";
      }
      switch (param->type().bt()) {
        case Type::BT_BOOL: {
          ss << "b";
          break;
        }
        case Type::BT_INT: {
          ss << "i";
          break;
        }
        case Type::BT_FLOAT: {
          ss << "f";
          break;
        }
        case Type::BT_STRING: {
          ss << "s";
          break;
        }
        case Type::BT_ANN: {
          ss << "a";
          break;
        }
        case Type::BT_TOP: {
          ss << "t";
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
    }
  } else {
    ss << fun->id().str();
  }

  bytecode.emplace_back(ss.str(), params.size());
  fun_bodies.insert(std::make_pair(fun, p_id));
  return p_id;
}

struct dispatch_node {
  int label;
  int level;
  uint64_t sig;
};

std::pair<CG_ProcID, BytecodeProc::Mode> find_call_fun(CodeGen& cg, const ASTString& ident, const Type& ret_type, std::vector<Type> arg_types, BytecodeProc::Mode m) {
  for (auto& arg_type : arg_types) {
    arg_type.ti(Type::TI_PAR);
  }
  int sz = arg_types.size();
  CallSig sig(ident, arg_types);
  auto it(cg.dispatch.find(sig));

  BytecodeProc::Mode call_mode(ret_type.isbool() ? m : BytecodeProc::FUN);
  BytecodeProc::Mode def_mode(ret_type.isbool() ? m : BytecodeProc::ROOT);

  if(it != cg.dispatch.end()) {
    CG_ProcID d_proc(it->second);
    if(d_proc.is_builtin() || cg.bytecode[d_proc.id()].is_available(call_mode))
      return {d_proc, call_mode};
  }

  GCLock lock;
  auto bodies = std::move(cg.fun_map.get_bodies(ident, arg_types));
  assert(!bodies.empty());

  // TODO: Consider negated contexts.
  if (ret_type.isbool() && call_mode != BytecodeProc::ROOT) {
    bool valid = false;
    if (call_mode == BytecodeProc::IMP) {
      valid = cg.fun_map.defines_mode(ident, arg_types, BytecodeProc::IMP);
      if (!valid) {
        valid = cg.fun_map.defines_mode(ident, arg_types, BytecodeProc::FUN);
        if (valid) {
          call_mode = BytecodeProc::FUN;
          def_mode = BytecodeProc::FUN;
        }
      }
    } else if (call_mode == BytecodeProc::FUN) {
      valid = cg.fun_map.defines_mode(ident, arg_types, BytecodeProc::FUN);
    }
    for (auto & body : bodies) {
      if (body->e()) {
        valid = true;
        break;
      }
    }
    if (!valid) {
      throw InternalError(ident.str() + " is used in a reified context, but no reification is available.");
    }
  }

  std::vector<CG_ProcID> procs;
  for(FunctionI* b : bodies) {
    CG_ProcID body(cg.resolve_fun(b));
    // Force the body to be created
    procs.push_back(body);
    if(!cg.bytecode[body.id()].is_available(call_mode)) {
      cg.bytecode[body.id()].body(call_mode);
      cg.pending_bodies.emplace_back(b, std::make_pair(call_mode, def_mode));
    }
  }
  
  // If there's a unique candidate, go for it.
  if(procs.size() == 1) {
    if(it == cg.dispatch.end())
      cg.dispatch.insert(std::make_pair(sig, procs[0]));
    return {procs[0], call_mode};
  }
  
  CG_ProcID d_proc(CG_ProcID::builtin(0));
  if(it != cg.dispatch.end()) {
    d_proc = it->second;
  } else {
    // Otherwise, generate the dispatch function.
    int p_idx = cg.bytecode.size();

    std::stringstream ss;
    ss << "d_" << ident.str();
    for (auto& type : arg_types) {
      ss << "_";
      if (type.dim() > 0) {
        ss << "d" << type.dim();
      } else if (type.dim() < 0) {
        ss << "d$";
      }
      switch (type.bt()) {
        case Type::BT_BOOL: {
          ss << "b";
          break;
        }
        case Type::BT_INT: {
          ss << "i";
          break;
        }
        case Type::BT_FLOAT: {
          ss << "f";
          break;
        }
        case Type::BT_STRING: {
          ss << "s";
          break;
        }
        case Type::BT_ANN: {
          ss << "a";
          break;
        }
        case Type::BT_TOP: {
          ss << "t";
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
    }

    d_proc = CG_ProcID::proc(p_idx);
    cg.bytecode.emplace_back(ss.str(), arg_types.size());
    cg.dispatch.insert(std::make_pair(sig, d_proc));
  }

  // Now generate the dispatch body.
  CG_Builder frag;
  std::vector<uint64_t> var_sig(sz);
  std::vector<uint64_t> def_sig(bodies.size());
  for(int bi = 0; bi < bodies.size(); ++bi) {
    ASTExprVec<VarDecl> params(bodies[bi]->params());
    for(int ii = 0; ii < params.size(); ++ii) {
      if(params[ii]->type().isvar()) {
        var_sig[ii] |= 1ull << bi;
        def_sig[bi] |= 1ull << ii;
      }
    }
  }

  std::vector< std::unordered_map<uint64_t, int> > sig_table(sz+1);
  std::vector<dispatch_node> nodes;

  nodes.push_back(dispatch_node { -1, 0, (1ull << bodies.size())-1 });
  
  for(int ii = 0; ii < nodes.size(); ii++) {
    dispatch_node d(nodes[ii]);
    if(d.label != -1)
      PUSH_LABEL(frag, d.label);
    if(!d.sig) {
      PUSH_INSTR(frag, BytecodeStream::ABORT);
    } else if(d.level == sz) {
      // Find the best candidate, and emit a call.
      uint64_t candidates(d.sig);
      unsigned int best(find_lsb(candidates));
      uint64_t best_sig(def_sig[best]);
      candidates ^= 1ull<<best;
      while(candidates) {
        unsigned int curr(find_lsb(candidates)); 
        candidates ^= 1ull<<curr;
        uint64_t sig(def_sig[curr]);
        if(!(sig & ~best_sig)) {
          // At least as good as the incumbent
          best = curr;
          best_sig = sig;
        }
      }
      CG_ProcID p_id(procs[best]);
      PUSH_INSTR(frag, BytecodeStream::TCALL, call_mode, p_id);
    } else {
      int par_label;
      auto p_it(sig_table[d.level+1].find(d.sig));
      if(p_it != sig_table[d.level+1].end()) {
        par_label = p_it->second;
      } else {
        par_label = GET_LABEL(cg);
        // int idx = nodes.size();
        sig_table[d.level+1].insert(std::make_pair(d.sig, par_label));
        nodes.push_back(dispatch_node { par_label, d.level+1, d.sig});
      }
      uint64_t v_sig(d.sig & var_sig[d.level]);
      if(v_sig == d.sig) {
        PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(par_label));
      } else {
        int var_label;
        auto v_it(sig_table[d.level+1].find(v_sig));
        if(v_it != sig_table[d.level+1].end()) {
          var_label = v_it->second;
        } else {
          var_label = GET_LABEL(cg);
          // int idx = nodes.size();
          sig_table[d.level+1].insert(std::make_pair(v_sig, var_label));
          nodes.push_back(dispatch_node { var_label, d.level+1, v_sig});
        }

        PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(d.level), CG::r(sz));
        PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(sz), CG::l(par_label));
        PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(var_label));
      }
    }
  }

  cg.append(d_proc.id(), call_mode, frag);

  return {d_proc, call_mode};
}


std::pair<CG_ProcID, BytecodeProc::Mode> find_call_fun(CodeGen& cg, Call* call, BytecodeProc::Mode m) {
  std::vector<Type> arg_types;
  int sz = call->n_args();
  for(int ii = 0; ii < sz; ++ii) {
    Type t(call->arg(ii)->type());
    arg_types.push_back(t);
  }
  return find_call_fun(cg, call->id(), call->type(), arg_types, m);
}
/*
CG_ProcID find_call_pred(CodeGen& cg, Call* c) {
  return CG_ProcID::proc(0xbead);

}
*/

// Analyse an expression (and sub-expressions) for partiality
#if 0
struct ClearFlags : public EVisitor {
  bool enter(Expression* e) {
    if(!e->isUnboxedVal() && e->user_flag0()) {
      e->user_flag0(0);
      e->user_flag1(0);
      return true;
    }
    return false;
  }
  // FIXME: We need to identify call bodies.
  void vCall(const Call&) {}

  static void clear(Expression* e) {
    ClearFlags cf;
    TopDownIterator<ClearFlags> td(cf);
    td.run(e);
  }
};

struct Partiality {
  Partiality(CodeGen& _cg) : cg(_cg) { }

  // We use _flag_3 to track whether something is already on the
  // stack, and _flag_4 to track whether it is eliminated as true.
  bool is_partial(Expression* e) {
    if(e->isUnboxedVal())
      return false;
    // Either on the call stack and still open, or completed.
    // In either case, check whether the partiality-flag is set.
    if(e->user_flag0())
      return e->user_flag1();
    // Otherwise, mark it as pending, and enter it.
    e->user_flag0(1);
    bool p = _is_partial(e);
    // Record the result, and return.
    e->user_flag1(p);
    return p;
  }

  bool _is_partial(Expression* e) {
    // First, Boolean expressions are always total.
    if(e->type().isbool())
      return false;
    // Otherwise, look at the 
    switch(e->eid()) {
      case Expression::E_INTLIT:
      case Expression::E_FLOATLIT:
      case Expression::E_SETLIT:
      case Expression::E_BOOLLIT:
      case Expression::E_STRINGLIT:
      case Expression::E_ID:
        return false;
      case Expression::E_ARRAYLIT: {
        ArrayLit* a(e->template cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii) {
          if(is_partial((*a)[ii]))
            return true;
        }
        return true;
      }
      case Expression::E_ARRAYACCESS: {
        /*
        ArrayAccess* a(e->template cast<ArrayAccess>());
        if(is_partial(a->v()))
          return true;
        ASTExprVec<Expression> idx(a->idx());
        int sz(idx.size());
        for(int ii = 0; ii < sz; ++ii) {
          if(is_partial(idx[ii]))
            return true;
        }
        break;
        */
        // FIXME: Needs an analysis to determine whether
        // dom(a->v) subseteq index_set(A).
        return true;
      }
      case Expression::E_COMP: {
        // A comprehension is total if all its generators
        // and its body are total.
        // Don't need to look in the where clauses, because
        // they're Boolean, and therefore total.
        Comprehension* c(e->template cast<Comprehension>());
        int sz = c->n_generators();
        for(int g = 0; g < c->n_generators(); ++g) {
          if(is_partial(c->in(g)))
            return true;
        }
        return is_partial(c->e());
      }
      case Expression::E_ITE: {
        // The conditions are Boolean, so must be total.
        // Look at the values.
        ITE* ite(e->template cast<ITE>());
        int sz(ite->size());
        if(is_partial(ite->e_else()))
          return false;
        for(int ii = 0; ii < sz; ++ii) {
          if(is_partial(ite->e_then(ii)))
            return false;
        }
        return true;
      }
      case Expression::E_BINOP: {
        BinOp* b(e->template cast<BinOp>());
        // First, check if the op is itself partial.
        // TODO: (Eventually) add a pass to determine whether we can exclude
        // 0 from the domain of b->rhs().
        if(b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD)
          return true;
        return is_partial(b->lhs()) || is_partial(b->rhs());
      }
      case Expression::E_UNOP:
        return is_partial(e->template cast<UnOp>()->e());

      case Expression::E_CALL: {
        Call* call(e->template cast<Call>());
        int sz = call->n_args();
        // Check if any of its arguments are partial.
        for(int ii = 0; ii < sz; ++ii) {
          if(is_partial(call->arg(ii)))
            return true;
        }
        // FIXME: Identify the relevant call body, recursively
        // check for partiality.
        // return false;
        for(FunctionI* b : cg.fun_map.get_bodies(call)) {
          // Boolean-typed values are always total
          if(b->ti()->type().isbool())
            continue;
          if(!b->e())
            return true;
        }
        return false;
      }
      case Expression::E_LET: {
        Let* let(e->template cast<Let>());
        
        // Check if any of the expressions are partial.
        ASTExprVec<Expression> bindings(let->let());
        for(Expression* item : bindings) {
          if (VarDecl* vd = e->dyn_cast<VarDecl>()) {
            if(vd->e()) {
              // If both a domain and a definition are given,
              // the domain might be constraining.
              if (vd->ti()->domain())
                return true;
              if(is_partial(vd->e()))
                return true;
            }
          } else {
            // If there's some item that isn't a binding, it must be a constriant
            return true;
          }
        }
        return is_partial(let->in());
      }
      case Expression::E_ANON:
      case Expression::E_VARDECL:
      case Expression::E_TI:
      case Expression::E_TIID:
        throw InternalError("Bytecode generator encountered unexpected expression type.");
    }
  }

  void reset_flags(Expression* e) {
    /*
    if(!e->isUnboxedVal()) {
      if(e->_flag_3) {
        e->_flag_3 = e->_flag_4 = 0;
      }
    }
    */
  }
   
  CodeGen& cg;

};
#endif

ASTStSet CodeGen::scope(Expression* e) {
  // Is it already cached?
  auto it(_exp_scope.find(e));
  if(it != _exp_scope.end())
    return (*it).second;

  // Otherwise, dispatch on the type.
  ASTStSet r;
  switch (e->eid()) {
  case Expression::E_INTLIT:
  case Expression::E_FLOATLIT:
  case Expression::E_SETLIT:
  case Expression::E_BOOLLIT:
  case Expression::E_STRINGLIT:
    break;
  case Expression::E_ID: {
    Id* id(e->template cast<Id>());
    r.insert(id->v());
    break;
  }
  case Expression::E_ARRAYLIT: {
    ArrayLit* a(e->template cast<ArrayLit>());
    int sz(a->size());
    for(int ii = 0; ii < sz; ++ii) {
      ASTStSet rr(scope((*a)[ii]));
      r.insert(rr.begin(), rr.end());
    }
    break;
  }
  case Expression::E_ARRAYACCESS: {
    ArrayAccess* a(e->template cast<ArrayAccess>());
    r = scope(a->v());

    ASTExprVec<Expression> idx(a->idx());
    int sz(idx.size());
    for(int ii = 0; ii < sz; ++ii) {
      ASTStSet rr(scope(idx[ii]));
      r.insert(rr.begin(), rr.end());
    }
    break;
  }
  case Expression::E_COMP: {
    // Work from the inner expression out.
    Comprehension* c(e->template cast<Comprehension>());
    r = scope(c->e());
    int sz = c->n_generators();
    for(int g = sz-1; g >= 0; --g) {
      ASTStSet r_in(scope(c->in(g)));

      if(c->where(g)) {
        ASTStSet r_where(scope(c->where(g)));
        r.insert(r_where.begin(), r_where.end());
        r.insert(r_in.begin(), r_in.end());
      }

      // Now remove the variables that were bound.
      for(int d = 0; d < c->n_decls(g); ++d) {
        VarDecl* vd(c->decl(g, d));
        r.erase(vd->id()->str());
      }
    }
    break;
  }
  case Expression::E_ITE: {
    ITE* ite(e->template cast<ITE>());
    int sz(ite->size());
    r = scope(ite->e_else());
    for(int ii = 0; ii < sz; ++ii) {
      ASTStSet rr(scope(ite->e_if(ii)));
      r.insert(rr.begin(), rr.end());
      rr = scope(ite->e_then(ii));
      r.insert(rr.begin(), rr.end());
    }
    break;
  }
  case Expression::E_BINOP: {
    BinOp* b(e->template cast<BinOp>());
    r = scope(b->lhs());
    ASTStSet rr(scope(b->rhs()));
    r.insert(rr.begin(), rr.end());
    break;
  }
  case Expression::E_UNOP:
    r = scope(e->template cast<UnOp>()->e());
    break;
  case Expression::E_CALL: {
    Call* call(e->template cast<Call>());
    int sz = call->n_args();
    for(int ii = 0; ii < sz; ++ii) {
      ASTStSet rr(scope(call->arg(ii)));
      r.insert(rr.begin(), rr.end());
    }
    break;
  }
  case Expression::E_LET: {
    Let* let(e->template cast<Let>());
    ASTExprVec<Expression> bindings(let->let());
    int sz(bindings.size());
    r = scope(let->in());
    for(int ii = sz-1; ii >= 0; --ii) {
      if (VarDecl* vd = bindings[ii]->dyn_cast<VarDecl>()) {
        r.erase(vd->id()->str());
        if(vd->e()) {
          ASTStSet r_d(scope(vd->e()));
          r.insert(r_d.begin(), r_d.end());
        }
      } else {
        ASTStSet r_e(scope(bindings[ii]));
        r.insert(r_e.begin(), r_e.end());
      }
    }
    break;
  }
  case Expression::E_ANON:
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
  }

  _exp_scope.insert(std::make_pair(e, r));
  return r;
}

CodeGen::Binding CodeGen::cache_lookup(Expression* e) {
  return env().cache_lookup(e, scope(e));
}
void CodeGen::cache_store(Expression* e, CodeGen::Binding l) {
  env().cache_store(e, scope(e), l); 
}

void _debugcond(CG_Cond::T c) {
  CG_Cond::_T* p(c.get());
  if(c.sign())
    std::cerr << "~";
  if(!p) {
    std::cerr << "T";
  } else {
    switch(p->kind()) {
      case CG_Cond::CC_Reg:
        std::cerr << "R" << p->reg[0].reg;
        break;
      case CG_Cond::CC_Call:
        std::cerr << "<Call>";
        break;
      case CG_Cond::CC_And:
        std::cerr << "(and";
        for(CG_Cond::T child : static_cast<CG_Cond::C_And*>(p)->children) {
          std::cerr << " ";
          _debugcond(child);
        }
        break;
    }
  }
}
void debugcond(CG_Cond::T c) {
  _debugcond(c);
  std::cerr << std::endl;
}

int bind_cst(int x, CodeGen& cg, CG_Builder& frag) {
    CG::Binding b;
    if(cg.env().cache_lookup_cst(x, b))
      return b.first;
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
    cg.env().cache_store_cst(x, std::make_pair(r, CG_Cond::ttt()));
    return r;
}


// Given CG_Cond cond, collect the disjuncts having positive or negative values.
// Returns false if the conjunction is a contradiction.
bool collect_prod(std::vector<int>& pos, std::vector<int>& neg, std::vector<CG_Cond::C_And*>& delayed, CG_Cond::T cond, CodeGen& cg, CG_Builder& frag) {
  assert(cond.get());
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  
  if(p->reg[1 - sign].is_seen || p->reg[1 - sign].is_root)
    return false;
  if(p->reg[sign].is_seen || p->reg[sign].is_root)
    return true;
  p->reg[sign].is_seen = true;

  if(p->reg[sign].has_reg()) {
    pos.push_back(p->reg[sign].reg);
  } else if(p->reg[1 - sign].has_reg()) {
    neg.push_back(p->reg[1 - sign].reg);
  } else if(p->kind() == CG_Cond::CC_And && !sign) {
    // Recursively collect the children.
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for(CG_Cond::T c : children) {
      if(!collect_prod(pos, neg, delayed, c, cg, frag))
        return false;
    }
  } else if(p->kind() == CG_Cond::CC_And) {
    // How do we decide which way to compile the remaining conditions?
    delayed.push_back(static_cast<CG_Cond::C_And*>(p));
  } else {
    assert(p->kind() == CG_Cond::CC_Call);
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    OPEN_OTHER(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::CALL, call->m, call->p, call->params);
    CLOSE_AGG(cg, frag);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    if(!sign)
      pos.push_back(r);
    else
      neg.push_back(r);
  }
  return true;
}

// Given CG_Cond cond, collect the disjuncts having positive or negative values.
// Returns true if the disjunction is a tautology.
bool collect_disj(std::vector<int>& pos, std::vector<int>& neg, std::vector<CG_Cond::C_And*>& delayed, CG_Cond::T cond, CodeGen& cg, CG_Builder& frag) {
  assert(cond.get());
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  
  if(p->reg[1 - sign].is_seen || p->reg[sign].is_root)
    return true;
  if(p->reg[sign].is_seen || p->reg[1 - sign].is_root)
    return false;
  p->reg[sign].is_seen = true;

  if(p->reg[sign].has_reg()) {
    pos.push_back(p->reg[sign].reg);
  } else if(p->reg[1 - sign].has_reg()) {
    neg.push_back(p->reg[1 - sign].reg);
  } else if(p->kind() == CG_Cond::CC_And && sign) {
    // Recursively collect the children.
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for(CG_Cond::T c : children) {
      if(collect_disj(pos, neg, delayed, ~c, cg, frag))
        return true;
    }
  } else if(p->kind() == CG_Cond::CC_And) {
    delayed.push_back(static_cast<CG_Cond::C_And*>(p));
  } else {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    OPEN_OTHER(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::CALL, call->m, call->p, call->params);
    CLOSE_AGG(cg, frag);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    if(!sign)
      pos.push_back(r);
    else
      neg.push_back(r);
  }
  return false;
}

void force_and_leaves(std::vector<int>& leaves, CG_Cond::T child, CodeGen& cg, CG_Builder& frag) {
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  if(p->reg[sign].has_reg()) {
    leaves.push_back(p->reg[sign].reg);
  } else if(p->reg[1 - sign].has_reg()) {
    // Create the negation 
    OPEN_OTHER(cg, frag);
    auto fun = find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(p->reg[1 - sign].reg));
    CLOSE_AGG(cg, frag);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));   
    p->reg[sign].reg = r;
    leaves.push_back(r);
  } else if(p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for(CG_Cond::T c : children)
      force_and_leaves(leaves, c, cg, frag);
  } else {
    leaves.push_back(CG::force(child, cg, frag));
  }
}

// Pushing the _negation_ of child.
void force_or_leaves(std::vector<int>& leaves, CG_Cond::T child, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  if(p->reg[1 - sign].has_reg()) {
    leaves.push_back(p->reg[1 - sign].reg);
  } else if(p->reg[sign].has_reg()) {
    // Create the negation 
    OPEN_OTHER(cg, frag);
    auto fun = find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(p->reg[sign].reg));
    CLOSE_AGG(cg, frag);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));   
    p->reg[1 - sign].reg = r;
    leaves.push_back(r);
  } else if(p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for(CG_Cond::T c : children)
      force_or_leaves(leaves, c, cg, frag);
  } else {
    leaves.push_back(CG::force(~child, cg, frag));
  }
}

int _force_cond(CG_Cond::T cond, CodeGen& cg, CG_Builder& frag) {
  CG_Cond::_T* p(cond.get());
  bool negated(cond.sign());
  if(!p) {
    // Either true or false.
    // return CG::locate_immi(1 - cond.sign(), cg, frag);
    return bind_cst(1 - cond.sign(), cg, frag);
  }
  if(p->kind() == CG_Cond::CC_Reg) {
    // return cond->reg[negated].reg;
    throw InternalError("_force_cond called on value in register.");
    return 0;
  } else if(p->kind() == CG_Cond::CC_Call) {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    int r;
    CG::Mode m(call->m);
    if(m != BytecodeProc::ROOT && m != BytecodeProc::ROOT_NEG) {
      OPEN_OTHER(cg, frag);
      Mode call_m(m.strength(), negated);
      PUSH_INSTR(frag, BytecodeStream::CALL, call_m, call->p, call->params);
      CLOSE_AGG(cg, frag);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    } else {
      /*
      PUSH_INSTR(frag, BytecodeStream::CALL, negated ? -m : m, call->p, call->params);
      // r = CG::locate_immi(1, cg, frag);
      */
      CG::Mode call_m(negated ? BytecodeProc::ROOT_NEG : BytecodeProc::ROOT);
      assert(m == call_m);
      PUSH_INSTR(frag, BytecodeStream::CALL, call_m, call->p, call->params);
      r = bind_cst(1, cg, frag);
    }
    return r;
  } else if(p->kind() == CG_Cond::CC_And && !cond.sign()) {
    std::vector<int> leaves;
    OPEN_OTHER(cg, frag);
    force_and_leaves(leaves, cond, cg, frag);
    OPEN_AND(cg, frag);
    for(int r_c : leaves)
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    return r;
  } else {
    assert(p->kind() == CG_Cond::CC_And && cond.sign());
    std::vector<int> leaves;
    OPEN_OTHER(cg, frag);
    force_or_leaves(leaves, ~cond, cg, frag);
    OPEN_OR(cg, frag);
    for(int r_c : leaves)
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    return r;
  }
}
int CG::force(CG_Cond::T cond, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  // Check if the condition is already forced.
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  if(!p) {
    return bind_cst(!sign, cg, frag);
  }
  if(p->reg[sign].has_reg())
    return p->reg[sign].reg;
  if(p->reg[1 - sign].has_reg()) {
    // Emit the negation
    int r_neg(p->reg[1 - sign].reg);
    OPEN_OTHER(cg, frag);
    auto fun = find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_neg));
    CLOSE_AGG(cg, frag);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    return p->reg[cond.sign()].reg = r;
  }
  return p->reg[cond.sign()].reg = _force_cond(cond, cg, frag);
}


class EnvInit : public ItemVisitor {
private:
  friend class ItemIter<EnvInit>;

  EnvInit(CodeGen& _cg)
    : cg(_cg) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(!vd->type().isann()) {
      if(vd->type().ispar()) {
        // std::cerr << "%%%% Binding " << vd->id()->str() << " at g" << slot << std::endl;
        // std::cerr << "%%%% "; debugprint(vd);
        if(!vd->e()) {
          // debugprint(vd);
          // cg.env().bind(vd->id()->v(), Loc::global(cg.num_globals));
          cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
          std::cout << "%% " << vd->id()->v() << " ~> " << cg.num_globals << std::endl;
          // FIXME
          ++cg.num_globals;
        }
      } else {
        // If it's a var with a body, feed it into the mode analyser.
        // TODO: Because mode analysis is not interprocedural, we have to
        // assume global params may be used in any context.
        modes.def(vd, BytecodeProc::ROOT);
        modes.use(vd, BytecodeProc::FUN);
      }
    }
  }
  void vConstraintI(ConstraintI* c) {
    modes.use(c->e(), BytecodeProc::ROOT);
  }
  /// Visit assign item
  void vAssignI(AssignI* ass) {
    debugprint(ass); 
  }

  void vFunctionI(FunctionI* f) {
    // std::cout << "%% F: "; debugprint(f);
    /*
    if(!f->e() && !f->from_stdlib()) {
      std::cerr << "%% F: "; debugprint(f);
    }
    */
    cg.register_function(f);
  }

  CodeGen& cg;
  ModeAnalysis modes;
public:
  static void run(CodeGen& cg, Model* m) {
    EnvInit eb(cg);
    iterItems(eb, m);
    cg.mode_map = std::move(eb.modes.extract());
    for(auto p : cg.mode_map) {
      std::cerr << mode_name(p.second) << "[" << p.first << "] "; debugprint(p.first);
    }
  }
};

struct ShowVal {
  ShowVal(CodeGen& _cg, CG_Value _v)
    : cg(_cg), v(_v) { }  

  CodeGen& cg;
  CG_Value v;
};
std::ostream& operator<<(std::ostream& o, ShowVal s) {
  switch(s.v.kind) {
    case CG_Value::V_Immi:
      o << s.v.value;
      break;
    case CG_Value::V_Global:
      o << s.v.value;
      break;
    case CG_Value::V_Reg:
      o << "R" << s.v.value;
      break;
    case CG_Value::V_Proc:
      o << "p" << s.v.value;
      break;
    case CG_Value::V_Label:
      o << "l" << s.v.value;
  }
  return o;
}

template<class O>
void show_frag(O& out, CodeGen& cg, std::vector<CG_Instr>& frag) {
  auto show = [&cg](CG_Value v) { return ShowVal(cg, v); };

  int num_agg = 0;
  for(CG_Instr& i : frag) {
    auto op(static_cast<BytecodeStream::Instr>(i.tag>>1));
    if (op == BytecodeStream::CLOSE_AGGREGATION) {
      num_agg--;
    }
    for (int j = 0; j < num_agg; ++j) {
      out << "  ";
    }
    if(i.tag&1) {
      out << "l" << (i.tag>>1) << ": ";
      continue;
    }

    out << instr_name(op);
    switch(op) {
      case BytecodeStream::OPEN_AGGREGATION:
        out << " " << agg_name((AggregationCtx::Symbol) i.params[0].value);
        num_agg++;
        break;
      case BytecodeStream::BUILTIN: {
        CG_ProcID p(CG_ProcID::of_val(i.params[0]));
        assert(p.is_builtin());
        out << " " << cg._builtins[p.id()].first;
        for(int ii = 1; ii < i.params.size(); ++ii) {
          out << " " << show(i.params[ii]);
        }
        break;
      }
      case BytecodeStream::CALL: {
        out << " " << mode_name((BytecodeProc::Mode) i.params[0].value);
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if(p.is_builtin())
          out << " " << cg._builtins[p.id()].first;
        else
          // out << " #P" << p.id(); 
          out << " " << cg.bytecode[p.id()].ident;
        for(int ii = 2; ii < i.params.size(); ++ii) {
          out << " " << show(i.params[ii]);
        }
        break;
      }
      case BytecodeStream::TCALL: {
        out << " " << mode_name((BytecodeProc::Mode) i.params[0].value);
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if(p.is_builtin())
          out << " " << cg._builtins[p.id()].first;
        else
          // out << " #P" << p.id(); 
          out << " " << cg.bytecode[p.id()].ident;
        break;
      }
      default:
        for(CG_Value p : i.params)
          out << " " << show(p);
    }
    out << std::endl;
  }
}

template<class O>
void show(O& out, CodeGen& cg) {
  for(auto b : cg._builtins) {
    out << ":" << b.first << ": " << b.second << std::endl;
  }
  for(auto& p : cg.bytecode) {
    for(BytecodeProc::Mode m : p) {

      out << ":" << p.ident << ":" << mode_name(m) << " " << p.arity << std::endl;
      show_frag(out, cg, p.body(m));
    }
  }
}

/*
void post_cond(CodeGen& cg, CG_Builder& frag, CG_Cond::T cond) {
  if(!cond.get()) {
    assert(!cond.sign());
    return;
  }

  std::vector<int> leaves;
  force_and_leaves(leaves, cond, cg, frag);
  for(int r_c : leaves)
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(r_c));
}
*/
void post_cond(CodeGen& cg, CG_Builder& frag, CG_Cond::T cond) {
  GCLock lock;
  if(!cond.get()) {
    if(cond.sign())
      PUSH_INSTR(frag, BytecodeStream::POST, CG::r(bind_cst(0, cg, frag)));
    return;
  }
  CG_Cond::_T* p(cond.get()); 
  bool sign(cond.sign()); 
  if(p->reg[sign].is_root)
    return;
  if(p->reg[1 - sign].is_root) {
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(bind_cst(0, cg, frag)));
    return;
  }

  if(p->reg[sign].has_reg()) {
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(p->reg[sign].reg));
  } else if(p->reg[1 - sign].has_reg()) {
    OPEN_OTHER(cg, frag);
    auto fun = find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(p->reg[1 - sign].reg));
    CLOSE_AGG(cg, frag);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(r));
    p->reg[sign] = r;
  } else if(p->kind() == CG_Cond::CC_Call) {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    CG::Mode call_m(CG::Mode::Root, sign);
    PUSH_INSTR(frag, BytecodeStream::CALL, call_m, call->p, call->params);
  } else {
    assert(p->kind() == CG_Cond::CC_And);
    CG_Cond::C_And* conj(reinterpret_cast<CG_Cond::C_And*>(p));
    if(!sign) {
      // Recurse.    
      for(CG_Cond::T child : conj->children)
        post_cond(cg, frag, child);
    } else {
      PUSH_INSTR(frag, BytecodeStream::POST, CG::r(CG::force(cond, cg, frag)));
    }
  }
  p->reg[sign].is_root = true;
}

// FIXME: This always forces calls outside the aggregation.
void aggregate_cond(CodeGen& cg, CG_Builder& frag, CG_Cond::T cond) {
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  if(!p) {
    // int r_val(CG::locate_immi(1 - sign, cg, frag));
    int r_val(bind_cst(1 - sign, cg, frag));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_val));
    return;
  }
  if(p->reg[sign].has_reg()) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(p->reg[sign].reg));
    return;
  }
  std::vector<int> leaves;
  if(p->kind() == CG_Cond::CC_And) {
    if(!sign) {
      force_and_leaves(leaves, cond, cg, frag);
      OPEN_AND(cg, frag);
      for(auto r_l : leaves)
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_l));
      CLOSE_AGG(cg, frag);
    } else {
      force_or_leaves(leaves, ~cond, cg, frag);
      OPEN_OR(cg, frag);
      for(auto r_l : leaves)
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_l));
      CLOSE_AGG(cg, frag);
    }
  } else {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(CG::force(cond, cg, frag)));
  }
}

int locate_range(int l, int u, CodeGen& cg, CG_Builder& frag) {
  CG::Binding b;
  if(cg.env().cache_lookup_range(l, u, b))
    return b.first;

  OPEN_VEC(cg, frag); 
  int r = GET_REG(cg);
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(l), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  CLOSE_AGG(cg, frag);
  r = GET_REG(cg);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  cg.env().cache_store_range(l, u, std::make_pair(r, CG_Cond::ttt()));
  return r;
}

CG::Binding bind_domain(VarDecl* vd, CodeGen& cg, CG_Builder& frag) {
  if(vd->type().isbool()) {
    return CG::Binding(locate_range(0, 1, cg, frag), CG_Cond::ttt()); 
  } else {
    Expression* d(vd->ti()->domain());
    assert(d); // Unbounded domains not yet supported.
    CG::Binding b(CG::bind(d, cg, frag));
    // Ignoring partiality here.
    return b;
  }
}

class Compile : public ItemVisitor {
private:
  friend class ItemIter<Compile>;

  Compile(CodeGen& _cg) : cg(_cg) /*, bool_dom(-1) */ { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(vd->type().isann())
      return;
    if(vd->type().isopt())
      return;
    std::cerr << "%%%% Binding " << vd->id()->str() << std::endl;
    if(vd->type().isvar()) {
      // In whatever case, we're going to create something,
      // and dump it in a register.
      int r_var;
      if(vd->e()) {
        // Defined
        // Evaluate the definition in root context,
        // add it to a register
        // r_var = CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);
        CG::Binding b_var(CG::bind(vd->e(), cg, root_frag));
        r_var = b_var.first;
        // TODO: Special case for root stuff.
        post_cond(cg, root_frag, b_var.second);

        if(Expression* d = vd->ti()->domain()) {
          if(!vd->ti()->isarray()) {
            CG::Binding b_d = CG::bind(d, cg, root_frag);
            int r_dp = GET_REG(cg);
            PUSH_INSTR(root_frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(r_var), CG::r(b_d.first), CG::r(r_dp));
            post_cond(cg, root_frag, b_d.second);
          }
        }
      } else {
        CG::Binding b_d(bind_domain(vd, cg, root_frag));
        post_cond(cg, root_frag, b_d.second);
        int r_d = b_d.first;

        if(vd->ti()->isarray()) {
          // Open nested iterators
          std::vector<int> r_regs;
          for(Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            CG::Binding b_reg(CG::bind(dim, cg, root_frag));
            post_cond(cg, root_frag, b_reg.second);
            r_regs.push_back(b_reg.first);
          }
          std::vector<Forset> nesting;
          OPEN_VEC(cg, root_frag);
          for(int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
          for(int r_i = r_regs.size()-1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(root_frag);
          }
          CLOSE_AGG(cg, root_frag);
        } else {
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
        }
        r_var = GET_REG(cg);
        PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(r_var));
      }
      // Now copy it into a global, and add it to the env.
      PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r_var), CG::g(cg.num_globals));
      std::cout << "%% " << vd->id()->v() << cg.num_globals << std::endl;
      cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
      ++cg.num_globals;

      // Since it's still in a register, add it to the current env as well.
      cg.env().bind(vd->id()->str(), CG::Binding(r_var, CG_Cond::ttt()));
    } else {
      // For par identifiers with definitions, we evaluate them.
      if(vd->e() && !vd->type().isann()) {
        // Evaluate the definition.
        // int r = vd->type().ispar() ? CG::locate_par(vd->e(), cg, root_frag) : CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);
        int r;
        if(vd->type().isbool()) {
          r = CG::force(CG::compile(vd->e(), cg, root_frag), cg, root_frag);
        } else {
          // Par expressions may still introduce constraints.
          CG::Binding b_d = CG::bind(vd->e(), cg, root_frag);
          post_cond(cg, root_frag, b_d.second);
          r = b_d.first;
        }
        PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(cg.num_globals));
        cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));

        ++cg.num_globals;

        cg.env().bind(vd->id()->str(), CG::Binding(r, CG_Cond::ttt()));
      }
    }
  }

  /// Visit assign item
  void vAssignI(AssignI* ass) {
    // std::cerr << "%%%% Assign: "; debugprint(ass); 
  }

  void vConstraintI(ConstraintI* c) {
    // std::cerr << "%%%% "; debugprint(c->e());
    // CG::eval(c->e(), BytecodeProc::ROOT, cg, root_frag);
    post_cond(cg, root_frag, CG::compile(c->e(), cg, root_frag));
  }

  void vFunctionI(FunctionI* f) {
    GCLock l;
    if(f->ann().contains(constants().ann._export)) {
      CG_ProcID body(cg.resolve_fun(f));
      // Force the body to be created
      if(!body.is_builtin()) {
        assert(body.id() < cg.bytecode.size());
        if(!cg.bytecode[body.id()].is_available(BytecodeProc::ROOT)) {
          cg.bytecode[body.id()].body(BytecodeProc::ROOT);
          cg.pending_bodies.push_back(std::make_pair(f, std::make_pair(BytecodeProc::ROOT, BytecodeProc::ROOT)));
        }
      }
    }
  }

  // FIXME: This method of saving the CodeGen state is pretty icky.
  // CodeGen should probably be split into two objects.
  void compile_fun(CG_Builder& frag, const ASTExprVec<VarDecl>& params, Expression* e) {
    // Save the codegen state.
    auto saved_env = cg.current_env;
    cg.current_env = CG_Env<CG::Binding>::spawn(nullptr);
    int saved_regs = cg.current_reg_count;

    cg.current_reg_count = params.size();

    cg._exp_scope.clear();
    cg.mode_map.clear();

    // Rerun mode analysis
    ModeAnalysis modes;
    // FIXME: Currently assuming all functions are total.
    modes.def(e, BytecodeProc::ROOT);
    modes.use(e, BytecodeProc::ROOT);
    cg.mode_map = std::move(modes.extract());

    // Set up the new env.
    {
    GCLock l;
    for(int ii = 0; ii < params.size(); ++ii) {
      cg.env().bind(params[ii]->id()->str(), CG::Binding(ii, CG_Cond::ttt()));
    }
    }
    
    // Now compile the result.
    OPEN_OTHER(cg, frag);
    CG::Binding b_res = CG::bind(e, cg, frag);
    if(b_res.second.p) {
      CG::force(b_res.second, cg, frag);
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b_res.first));
    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::RET);

    // now restore everything
    cg.current_reg_count = saved_regs;

    delete cg.current_env;
    cg.current_env = saved_env;
  }

  void compile_pred(CG_Builder& frag, const ASTExprVec<VarDecl>& params, Mode m, Expression* e) {
    // Save the codegen state.
    auto saved_env = cg.current_env;
    cg.current_env = CG_Env<CG::Binding>::spawn(nullptr);
    int saved_regs = cg.current_reg_count;

    cg.current_reg_count = params.size();

    cg._exp_scope.clear();
    cg.mode_map.clear();

    // Rerun mode analysis
    ModeAnalysis modes;
    modes.use(e, m);
    cg.mode_map = std::move(modes.extract());

    // Set up the new env.
    { GCLock l;
    for(int ii = 0; ii < params.size(); ++ii) {
      cg.env().bind(params[ii]->id()->str(), CG::Binding(ii, CG_Cond::ttt()));
    }
    }
    
    // Now compile the result. 
    CG_Cond::T cond(CG::compile(e, cg, frag));
    if(m == BytecodeProc::ROOT) 
      post_cond(cg, frag, cond);
    else
      aggregate_cond(cg, frag, CG::compile(e, cg, frag));
    PUSH_INSTR(frag, BytecodeStream::RET);

    cg.current_reg_count = saved_regs;
    // then restore everything.
    delete cg.current_env;
    cg.current_env = saved_env;
  }
  CodeGen& cg;
  CG_Builder root_frag;

  int globals_count;
  // int bool_dom;
public:
  static void run(CodeGen& cg, Model* m) {
    Compile c(cg);
    OPEN_OTHER(cg, c.root_frag);
    iterItems(c, m);
    PUSH_INSTR(c.root_frag, BytecodeStream::RET);

    // Now generate procedures for any necessary function/predicate bodies.
    while(!cg.pending_bodies.empty()) {
      GCLock lock;
      auto p(cg.pending_bodies.back());
      cg.pending_bodies.pop_back();
      
      FunctionI* fun(p.first);
      debugprint(fun);
      Mode call_mode(p.second.first);
      Mode def_mode(p.second.second);
      annotate_total(fun);
      // Find the body.
      if (call_mode == BytecodeProc::IMP || call_mode == BytecodeProc::FUN) {
        std::vector<Type> arg_types (fun->params().size());
        for (int j = 0; j < arg_types.size(); ++j) {
          arg_types[j] = fun->params()[j]->type();
        }
        bool reif_exists = cg.fun_map.defines_mode(fun->id(), arg_types, call_mode);
        if (reif_exists) {
          CG_ProcID proc(cg.resolve_fun(fun));
          CG_Builder frag;
          std::vector<Expression*> args;
          args.reserve(fun->params().size() + 1);
          for (int i = 0; i < fun->params().size(); ++i) {
            VarDecl* vd = fun->params()[i];
            args.emplace_back(vd->id());
          }
          TypeInst var_bool(Location().introduce(), Type::varbool());
          VarDecl new_var(Location().introduce(), &var_bool, "b");
          args.emplace_back(new_var.id());
          Call call(
            Location().introduce(),
            call_mode == BytecodeProc::FUN ? fun->id().str() + "_reif" : fun->id().str() + "_imp",
            args
          );
          call.type(Type::varbool());
          Let let(Location().introduce(), {&new_var}, &call);
          let.type(Type::varbool());
          c.compile_pred(frag, fun->params(), BytecodeProc::ROOT, &let);
          cg.append(proc.id(), call_mode, frag);
          continue;
        }
      }

      if (fun->e()) {
        CG_ProcID proc(cg.resolve_fun(fun));
        CG_Builder frag;
        if(fun->e()->type().isbool()) {
          c.compile_pred(frag, fun->params(), def_mode, fun->e());
        } else {
          assert(def_mode == BytecodeProc::ROOT);
          c.compile_fun(frag, fun->params(), fun->e());
        }
        cg.append(proc.id(), call_mode, frag);
      } else {
        assert(call_mode == BytecodeProc::ROOT);
        CG_ProcID proc(cg.resolve_fun(fun));
        CG_Builder frag;
        cg.append(proc.id(), call_mode, frag);
      }
    }

    // And finally, add the entry function.
    int main_proc = cg.bytecode.size();
    cg.bytecode.emplace_back("main", 0);
    cg.append(main_proc, BytecodeProc::ROOT, c.root_frag);
 
    show(std::cout, cg);
  }
};

Mode open_conj(Mode ctx, CG_Builder& frag) {
  if(ctx == BytecodeProc::ROOT)
    return ctx;
  
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, ctx.is_neg() ? AggregationCtx::VCTX_OR : AggregationCtx::VCTX_AND);
  return +ctx;
}
void close_conj(Mode ctx, CG_Builder& frag) {
  if(ctx != BytecodeProc::ROOT)
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
Mode open_disj(Mode ctx, CG_Builder& frag) {
  if(ctx == BytecodeProc::ROOT_NEG)
    return ctx;
  
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, ctx.is_neg() ? AggregationCtx::VCTX_AND : AggregationCtx::VCTX_OR);
  return +ctx;
}
void close_disj(Mode ctx, CG_Builder& frag) {
  if(ctx != BytecodeProc::ROOT_NEG)
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

Mode left_child_ctx(Mode ctx, BinOpType b) {
  switch(b) {
    // Only Boolean operators change the mode.
    case BOT_AND:
      return ctx.is_neg() ? +ctx : ctx; 
    case BOT_OR:
    case BOT_RIMPL:
      return ctx.is_neg() ? ctx : +ctx;
    case BOT_IMPL:
      return ctx.is_neg() ? -ctx : -(+ctx);
    case BOT_EQUIV:
    case BOT_XOR:
      return *ctx;
    default:
      return ctx;
  }
}
Mode right_child_ctx(Mode ctx, BinOpType b) {
  switch(b) {
    // Only Boolean operators change the mode.
    case BOT_AND:
      return ctx.is_neg() ? +ctx : ctx; 
    case BOT_OR:
    case BOT_IMPL:
      return ctx.is_neg() ? ctx : +ctx;
    case BOT_RIMPL:
      return ctx.is_neg() ? -ctx : -(+ctx);
    case BOT_EQUIV:
    case BOT_XOR:
      return *ctx;
    default:
      return ctx;
  }
}
Mode child_ctx(Mode ctx, UnOpType u) {
  switch(u) {
    case UOT_NOT:
      return -ctx;  
    default:
      return ctx;
  }
}

void CG::run(CodeGen& cg, Model* m) {
  // First, collect the model parameters, and assign them
  // global slots.
  EnvInit::run(cg, m);
  Compile::run(cg, m); 
}

// For a non-Boolean value, place it in a register and collect its partiality.
std::pair<int, CG_Cond::T> _bind(Expression* e, CodeGen& cg, CG_Builder& frag) {
  // Look up the mode we need to compile e in.
  /*
  debugprint(e);
  */
  // FIXME: Figure out why some expressions don't have a mode attached.
  CG::Mode ctx(BytecodeProc::FUN);
  try {
    ctx = cg.mode_map.at(e);
  } catch(const std::out_of_range& exn) { }

  switch (e->eid()) {
  case Expression::E_INTLIT:
    // return std::make_pair(CG::locate_immi(e->template cast<IntLit>()->v().toInt(), cg, frag), CG_Cond::ttt());
    return std::make_pair(bind_cst(e->template cast<IntLit>()->v().toInt(), cg, frag), CG_Cond::ttt());
  case Expression::E_FLOATLIT:
    // return std::make_pair(CG::locate_par(e->template cast<FloatLit>(), cg, frag), nullptr);
    TODO();
    return CG::Binding(0, CG_Cond::ttt());
  case Expression::E_SETLIT:
    return CG::bind(e->template cast<SetLit>(), ctx, cg, frag);
  case Expression::E_BOOLLIT:
    throw InternalError("bind called on Boolean expression.");
  case Expression::E_STRINGLIT:
    TODO();
    return CG::Binding(0, CG_Cond::ttt());
  case Expression::E_ID:
    return CG::bind(e->template cast<Id>(), ctx, cg, frag);
  case Expression::E_ANON:
    throw InternalError("bind reached unexpected expression type: E_ANON.");
  case Expression::E_ARRAYLIT:
    return CG::bind(e->template cast<ArrayLit>(), ctx, cg, frag);
  case Expression::E_ARRAYACCESS:
    return CG::bind(e->template cast<ArrayAccess>(), ctx, cg, frag);
  case Expression::E_COMP:
    return CG::bind(e->template cast<Comprehension>(), ctx, cg, frag);
  case Expression::E_ITE:
    return CG::bind(e->template cast<ITE>(), ctx, cg, frag);
  case Expression::E_BINOP:
    return CG::bind(e->template cast<BinOp>(), ctx, cg, frag);
  case Expression::E_UNOP:
    return CG::bind(e->template cast<UnOp>(), ctx, cg, frag);
  case Expression::E_CALL:
    return CG::bind(e->template cast<Call>(), ctx, cg, frag);
  case Expression::E_LET:
    return CG::bind(e->template cast<Let>(), ctx, cg, frag);
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type in bind.");
  }
}

CG::Binding CG::bind(Expression* e, CodeGen& cg, CG_Builder& frag) {
  try {
    return cg.cache_lookup(e);
  } catch(const CG_Env<CG::Binding>::NotFound& exn) {
    CG::Binding b(_bind (e, cg, frag));
    cg.cache_store(e, b);
    return b;
  }
  // return _bind(e, cg, frag); // FIXME
}

//
int CG::locate_immi(int x, CodeGen& cg, CG_Builder& frag) {
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
  return r;
}

// Modified version of execute-comprehension, but using bind.
void execute_comprehension_bind(Comprehension* c, Mode ctx, CodeGen& cg, CG_Builder& frag) {
   // Build up the object to build the generator.
  std::vector<EmitPost*> nesting;
  
  int g = c->n_generators();
  int n_gen = c->n_generators();
  for(int g = 0; g < n_gen; ++g) {
    // Bind the in-expression to a register.
    // assert(c->in(g)->type().ispar());
    Expression* in(c->in(g));
    int r(CG::bind(in, cg, frag).first);
    // std::cout << "Binding R" << r << " to: "; debugprint(in);
    // Open the bindings.
    cg.env_push();
    if(in->type().is_set()) {
      assert(in->type().ispar());
      for(int d = 0; d < c->n_decls(g); ++d) {
        Forset* iter(new Forset(cg, r));
        nesting.push_back(iter);
        iter->emit_pre(frag);
        // Bind vd->id() to iter.val()
        VarDecl* vd(c->decl(g, d));
        ASTString id(vd->id()->str());
        // cg.env().bind(id, Loc::reg(iter.val()));
        cg.env().bind(id, CodeGen::Binding(iter->val(), CG_Cond::ttt()));
      }
    } else {
      assert(in->type().isboolarray() || in->type().isintarray());
      for(int d = 0; d < c->n_decls(g); ++d) {
        Foreach* iter(new Foreach(cg, r));
        nesting.push_back(iter);
        iter->emit_pre(frag);

        VarDecl* vd(c->decl(g, d));
        ASTString id(vd->id()->str());
        cg.env().bind(id, CodeGen::Binding(iter->val(), CG_Cond::ttt()));
      }
    }
    // Now emit the where-clause
    Expression* where(c->where(g));
    if(where) {
      int lblCont(nesting.back()->cont());
      assert(where->type().ispar());
      int rC = CG::force(CG::compile(where, cg, frag), cg, frag);
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblCont));
    }
  }
  // We're now in the deepest scope. Generate code for the body.
  int r_e;
  if(c->e()->type().isbool()) {
    r_e = CG::force(CG::compile(c->e(), cg, frag), cg, frag);
  } else {
    r_e = CG::bind(c->e(), cg, frag).first; // FIXME: Discarding partiality
  }

  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_e));

  // Now close the iterators _in reverse order_, and restore the environment.
  for(int ii = nesting.size()-1; ii >= 0; --ii) {
    nesting[ii]->emit_post(frag);
    delete nesting[ii];
  }
  for(int ii = 0; ii < n_gen; ++ii)
    cg.env_pop();
}

// Special case implementation of folds where body is a generator.
CG_Cond::T eval_forall(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        std::vector<CG_Cond::T> conj;
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          conj.push_back(CG::compile((*a)[ii], cg, frag));
        return CG_Cond::forall(ctx, conj);
      }
      break;
      /*
    case Expression::E_COMP: {
      OPEN_OTHER(cg, frag);
      OPEN_AND(cg, frag);
      execute_comprehension_compile(param->cast<Comprehension>(), c_ctx, cg, frag);
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      int r(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return CG_Cond::reg(r);
      }
      break;
      */
    default:
      {
        OPEN_OTHER(cg, frag);
        CG::Binding b_param(CG::bind(param, cg, frag));
        int r_A(b_param.first);
        std::vector<int> p_A;
        if(!b_param.second.get()) {
          if(b_param.second.sign())
            return CG_Cond::T::fff();
        } else  {
          force_and_leaves(p_A, b_param.second, cg, frag);
        }
        OPEN_AND(cg, frag);
        // First, push the constraints attached to A.
        for(int r_c : p_A) {
          PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
        }
        FOREACH(RETN()(r_A), PUSH())(cg, frag);
        CLOSE_AGG(cg, frag);
        CLOSE_AGG(cg, frag);
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        return CG_Cond::reg(r);
      }
  }
}
// Same as forall, but producing an or-context.
CG_Cond::T eval_exists(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        std::vector<CG_Cond::T> disj;
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii) {
          CG_Cond::T elt(CG::compile((*a)[ii], cg, frag));
          disj.push_back(elt);
        }
        return CG_Cond::exists(ctx, disj);
      }
      break;
      /*
    case Expression::E_COMP: {
      Comprehension* c(param->cast<Comprehension>());
      execute_comprehension(c, c_ctx, cg, frag);
      }
      break;
      */
    default:
      {
        // Otherwise, get the result into a register...
        CG::Binding b_A(CG::bind(param, cg, frag));
        int r_A(b_A.first);
        // and push every element.
        OPEN_OTHER(cg, frag);
        OPEN_OR(cg, frag);
        FOREACH(RETN()(r_A), PUSH())(cg, frag);
        CLOSE_AGG(cg, frag);
        CLOSE_AGG(cg, frag);
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        return CG_Cond::forall(ctx, b_A.second, CG_Cond::reg(r));
      }
  }
}

CG_Cond::T eval_isfixed_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* e(call->arg(0));
  
  if(e->type().ispar())
    return CG_Cond::T::ttt();

  int r_e;
  if(e->type().isbool()) {
    CG_Cond::T b_e(CG::compile(e, cg, frag));
    if(!b_e.get())
      return CG_Cond::T::ttt();
    r_e = CG::force(b_e, cg, frag);
  } else {
    CG::Binding b(CG::bind(e, cg, frag));
    r_e = b.first;
  }
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(r_e), CG::r(r));
  return CG_Cond::reg(r);
}

CG_Cond::T eval_error_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in general context.");
  return CG_Cond::ttt();
}
CG::Binding bind_error_g(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in Boolean context.");
}
CG::Binding bind_sum(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  std::cerr << "%%%% Evaluating sum" << std::endl;
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO: Specialise for literals and comprehensions.
  CG::Binding b_elts(CG::bind(e, cg, frag));
  int r_A(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_elts.first), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
  if(e->type().ispar()) {
    int r_sum(GET_REG(cg));   
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_sum));
    Foreach iter(cg, r_A);
    iter.emit_pre(frag);
    PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_sum), CG::r(iter.val()), CG::r(r_sum));
    iter.emit_post(frag);
    return CG::Binding(r_sum, b_elts.second);
  }

  if (ENABLE_PLUS) {
    OPEN_OTHER(cg, frag);
    // int r_one(CG::locate_immi(1, cg, frag));
    int r_one(bind_cst(1, cg, frag));
    int r_sz(GET_REG(cg));
    int r_elt(GET_REG(cg));
    int r_acc(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
    int l_hd(GET_LABEL(cg));
    int l_end(GET_LABEL(cg));

    // Slightly cheeky here -- if this fails, we know the accumulator should be zero.
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_one), CG::r(r_sz), CG::r(r_acc));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_acc), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_sz), CG::r(r_acc));
    // Second element.
    PUSH_INSTR(frag, BytecodeStream::DECI, CG::r(r_sz));
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_one), CG::r(r_sz), CG::r(r_elt));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_elt), CG::l(l_end));
    PUSH_LABEL(frag, l_hd);

    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_sz), CG::r(r_elt));
    // OPEN_OTHER(cg, frag); // maybe needed?
    auto fun = find_call_fun(cg, {"op_plus"}, Type::varint(), {Type::varint(), Type::varint()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_elt), CG::r(r_acc));
    // CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_acc));

    PUSH_INSTR(frag, BytecodeStream::DECI, CG::r(r_sz));
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_one), CG::r(r_sz), CG::r(r_elt));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_elt), CG::l(l_hd));
    PUSH_LABEL(frag, l_end);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_acc));
    CLOSE_AGG(cg, frag);
  } else {
    OPEN_OTHER(cg, frag);
    int r_one(bind_cst(1, cg, frag));
    int r_sz(GET_REG(cg));
    int r_res(GET_REG(cg));

    int l_eq0(GET_LABEL(cg));
    int l_eq1(GET_LABEL(cg));
    int l_end(GET_LABEL(cg));

    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sz), CG::l(l_eq0));
    PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_one), CG::r(r_sz), CG::r(r_res));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_res), CG::l(l_eq1));

    // Sum n arguments
    auto fun = find_call_fun(cg, {"sum"}, Type::varint(), {Type::varint(1)}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_A));
    PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));

    // Sum zero arguments (result must be 0)
    PUSH_LABEL(frag, l_eq0);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_sz));
    PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));

    // Sum 1 arguments (result == a[1])
    PUSH_LABEL(frag, l_eq1);
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_sz), CG::r(r_res));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_res));

    // End of sum
    PUSH_LABEL(frag, l_end);
    CLOSE_AGG(cg, frag);
  }
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));
  return CG::Binding(r_ret, b_elts.second);
}

CG::Binding bind_set2array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  // Bind set
  Expression* e = call->arg(0);
  CG::Binding b_elts(CG::bind(e, cg, frag));

  // Create array
  OPEN_VEC(cg, frag);
  Forset iter(cg, b_elts.first);
  iter.emit_pre(frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(iter.val()));
  iter.emit_post(frag);
  CLOSE_AGG(cg, frag);
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  // Create index set
  int rI(GET_REG(cg));
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_ret), CG::r(rI));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

  // Combine array and index
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_ret));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  return {r_ret, b_elts.second};
}

CG::Binding bind_dom_bounds_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r_A(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
  // Open the context here, so we can recover all the registers after.
  OPEN_VEC(cg, frag);
  int r_lb(GET_REG(cg));
  int r_ub(GET_REG(cg));
  
  // Do the iteration
  int r_i(GET_REG(cg));
  int r_sz(GET_REG(cg)); 
  int r_test(GET_REG(cg));
  int r_elt(GET_REG(cg));
  int r_bound(GET_REG(cg));
  int l_fst(GET_LABEL(cg));
  int l_hd(GET_LABEL(cg));
  int l_ub(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
  // FIXME: Deal with zero case correctly
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_fst));
  PUSH_INSTR(frag, BytecodeStream::ABORT);

  PUSH_LABEL(frag, l_fst);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_elt), CG::r(r_lb));
  PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_elt), CG::r(r_ub));
   
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));

  // Body.
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_elt), CG::r(r_bound));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_lb), CG::r(r_bound), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_ub));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_bound), CG::r(r_lb));
  PUSH_LABEL(frag, l_ub);
  PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_elt), CG::r(r_bound));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_bound), CG::r(r_ub), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_hd));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_bound), CG::r(r_ub));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));

  PUSH_LABEL(frag, l_tl); 
  // Now collect the elements
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lb));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_ub));
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return CG::Binding(r, b_A.second);
}

CG_Cond::T eval_assert_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->arg(0)->type().ispar());
  // Evaluate the assertion, abort if it fails.
  /*
  int r_cond = CG::force(CG::compile(call->arg(0), cg, frag), cg, frag);
  int l_okay(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_cond), CG::l(l_okay));
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  PUSH_LABEL(frag, l_okay);
  */
  // Otherwise, evaluate as usual.
  if(call->n_args() == 2) {
    return CG_Cond::ttt();
  } else {
    assert(call->n_args() == 3);
    return CG::compile(call->arg(2), cg, frag);
  }
}

CG::Binding bind_assert_g(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 3);
  assert(call->arg(0)->type().ispar());
  /*
  int r_cond = CG::force(CG::compile(call->arg(0), cg, frag), cg, frag);
  int l_okay(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_cond), CG::l(l_okay));
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  PUSH_LABEL(frag, l_okay);
  */
  return CG::bind(call->arg(2), cg, frag);
}

template<int X>
CG::Binding bind_arrayXd(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == X + 1);
  std::vector<CG_Cond::T> cond;

  // Bind all index sets
  std::vector<int> r_index(X);
  for(int ii = 0; ii < X; ++ii) {
    CG::Binding b(CG::bind(call->arg(ii), cg, frag));
    r_index[ii] = b.first;
    cond.push_back(b.second);
  }

  // Bind array expression
  int rA(GET_REG(cg));
  CG::Binding arr = CG::bind(call->arg(X), cg, frag);
  cond.push_back(arr.second);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(arr.first), CG::r(bind_cst(1, cg, frag)), CG::r(rA));

  int rI(GET_REG(cg));
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
    for (int ii = 0; ii < X; ++ii) {
      // TODO: Ensure the index set is a range (2-elements)
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_index[ii]), CG::r(bind_cst(1, cg, frag)), CG::r(rI));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_index[ii]), CG::r(bind_cst(2, cg, frag)), CG::r(rI));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    }
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

  // TODO: Ensure that the new index sets match the size of the array
  // Combine array and index
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rA));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rA));

  return {rA, CG_Cond::forall(ctx, cond)};
}

  CG::Binding bind_array1d(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if(call->n_args() == 1) {
    // Index set: 1..len
    int rA(GET_REG(cg));
    int rI(GET_REG(cg));

    // Bind array expression
    CG::Binding arr = CG::bind(call->arg(0), cg, frag);
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(arr.first), CG::r(bind_cst(1, cg, frag)), CG::r(rA));

    // Add index set
    OPEN_OTHER(cg, frag);
    OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(1, cg, frag)));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(rA), CG::r(rI));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

    // Combine array and index
    OPEN_OTHER(cg, frag);
    OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rA));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rA));

    return {rA, arr.second};
  } else {
    // Index set given by the user
    assert(call->n_args() == 2);
    return bind_arrayXd<1>(call, ctx, cg, frag);
  }
}

CG::Binding bind_array_union(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::cerr << "%%%% Evaluating array_union" << std::endl;
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO: Specialise for literals and comprehensions.
  CG::Binding b_elts(CG::bind(e, cg, frag));
  int r_A(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_elts.first), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));

  assert (e->type().ispar()); // TODO: var case
  int r_union(GET_REG(cg));
  int r_sz(GET_REG(cg));
  int r_one(bind_cst(1, cg, frag));
  int r_elt(GET_REG(cg));

  int l_hd(GET_LABEL(cg));
  int l_eq0(GET_LABEL(cg));
  int l_end(GET_LABEL(cg));

  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sz), CG::l(l_eq0));

  // Sum n arguments
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_sz), CG::r(r_union));

  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::DECI, CG::r(r_sz));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_one), CG::r(r_sz), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_elt), CG::l(l_end));

  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_sz), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r_union), CG::r(r_elt), CG::r(r_union));

  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));

  // Union zero arguments (abort)
  PUSH_LABEL(frag, l_eq0);
  /*
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  */
  // Make an empty vector.
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_union));
  
  // End of union
  PUSH_LABEL(frag, l_end);

  return {r_union, b_elts.second};
}

CG::Binding bind_length(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b.first), CG::r(r));
  return CG::Binding(r, b.second);
}

CG::Binding bind_lb(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(b.first), CG::r(r));
  return CG::Binding(r, b.second);
}
CG::Binding bind_ub(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::UB, CG::r(b.first), CG::r(r));
  return CG::Binding(r, b.second);
}
CG::Binding bind_dom(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::DOM, CG::r(b.first), CG::r(r));
  return CG::Binding(r, b.second);
}
CG::Binding bind_lb_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r_A(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
  // Check for emptiness
  int r_agg(GET_REG(cg));
  int r_test(GET_REG(cg));
  int r_i(GET_REG(cg));
  int r_Ai(GET_REG(cg));
  int r_sz(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
  int l_hd(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_agg), CG::l(l_tl));
  // Initialize the accumulator
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_agg), CG::r(r_agg));
  // Check if there's a next element.
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));
  // Main loop body.
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_Ai)); 
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_Ai), CG::r(r_Ai));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_Ai), CG::r(r_agg), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_hd));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_Ai), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  return CG::Binding(r_agg, b_A.second);
}

CG::Binding bind_ub_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r_A(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
  // Check for emptiness
  int r_agg(GET_REG(cg));
  int r_test(GET_REG(cg));
  int r_i(GET_REG(cg));
  int r_Ai(GET_REG(cg));
  int r_sz(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_A), CG::r(r_sz));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
  int l_hd(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_agg), CG::l(l_tl));
  // Initialize the accumulator
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_agg), CG::r(r_agg));
  // Check if there's a next element.
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));
  // Main loop body.
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_i), CG::r(r_Ai)); 
  PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_Ai), CG::r(r_Ai));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_agg), CG::r(r_Ai), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_hd));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_Ai), CG::r(r_agg));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  return CG::Binding(r_agg, b_A.second);
}

template<int X, int Y>
CG::Binding bind_index_set_XofY(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  OPEN_OTHER(cg, frag);
  CG::Binding b_arg(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  {
    OPEN_VEC(cg, frag);
    int r_tmp(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_arg.first), CG::r(bind_cst(2, cg, frag)), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(bind_cst((X-1)*2+1, cg, frag)), CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r), CG::r(bind_cst((X-1)*2+2, cg, frag)), CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_tmp));
    CLOSE_AGG(cg, frag);
  }
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return {r, b_arg.second};
}

CG::Binding bind_bool2int(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  int r_e(CG::force(CG::compile(call->arg(0), cg, frag), cg, frag));
  return CG::Binding(r_e, CG_Cond::ttt());
}

CG::Binding bind_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
CG_Cond::T compile_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);

CG::Binding bind_max(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if(call->type().ispar()) {
    if(call->n_args() == 2) {
      CG::Binding b_lhs(CG::bind(call->arg(0), cg, frag));
      CG::Binding b_rhs(CG::bind(call->arg(1), cg, frag));
      // Again, assuming this is total
      int r(GET_REG(cg));
      int l_r(GET_LABEL(cg));
      int l_e(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(b_lhs.first), CG::r(b_rhs.first), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_r));
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_rhs.first), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_e));

      PUSH_LABEL(frag, l_r);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_lhs.first), CG::r(r));
      PUSH_LABEL(frag, l_e);
      return CG::Binding(r, CG_Cond::ttt());
    } else {
      assert(call->n_args() == 1); 
      CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
      int r(GET_REG(cg));
      int r_i(GET_REG(cg));
      int r_sz(GET_REG(cg)); 
      int r_test(GET_REG(cg));
      int r_elt(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
      int l_fst(GET_LABEL(cg));
      int l_hd(GET_LABEL(cg));
      int l_tl(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_fst));
      PUSH_INSTR(frag, BytecodeStream::ABORT);
      PUSH_LABEL(frag, l_fst);

      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r));
      
      PUSH_LABEL(frag, l_hd);
      PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));
      
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_elt));
      PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r), CG::r(r_elt), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_hd));
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));
      PUSH_LABEL(frag, l_tl);
      return CG::Binding(r, CG_Cond::ttt());
    }
  }
  return bind_call(call, ctx, cg, frag);
}

CG::Binding bind_arg_max(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1); 
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_v(GET_REG(cg));
  int r_i(GET_REG(cg));
  int r_sz(GET_REG(cg)); 
  int r_test(GET_REG(cg));
  int r_elt(GET_REG(cg));

  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
  int l_fst(GET_LABEL(cg));
  int l_hd(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_fst));
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  PUSH_LABEL(frag, l_fst);

  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_v));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r));
  
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));
  
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_v), CG::r(r_elt), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_hd));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r_v));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_i), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  return CG::Binding(r, CG_Cond::ttt());
  return bind_call(call, ctx, cg, frag);
}

CG::Binding bind_min(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if(call->type().ispar()) {
    if(call->n_args() == 2) {
      CG::Binding b_lhs(CG::bind(call->arg(0), cg, frag));
      CG::Binding b_rhs(CG::bind(call->arg(1), cg, frag));
      // Again, assuming this is total
      int r(GET_REG(cg));
      int l_r(GET_LABEL(cg));
      int l_e(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(b_lhs.first), CG::r(b_rhs.first), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_r));
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_lhs.first), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_e));

      PUSH_LABEL(frag, l_r);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_rhs.first), CG::r(r));
      PUSH_LABEL(frag, l_e);
      return CG::Binding(r, CG_Cond::ttt());
    } else {
      assert(call->n_args() == 1); 
      CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
      int r(GET_REG(cg));
      int r_i(GET_REG(cg));
      int r_sz(GET_REG(cg)); 
      int r_test(GET_REG(cg));
      int r_elt(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
      int l_fst(GET_LABEL(cg));
      int l_hd(GET_LABEL(cg));
      int l_tl(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_fst));
      PUSH_INSTR(frag, BytecodeStream::ABORT);
      PUSH_LABEL(frag, l_fst);

      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r));
      
      PUSH_LABEL(frag, l_hd);
      PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_tl));
      
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_elt));
      PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_elt), CG::r(r), CG::r(r_test));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_hd));
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_hd));
      PUSH_LABEL(frag, l_tl);
      return CG::Binding(r, CG_Cond::ttt());
    }
  }
  return bind_call(call, ctx, cg, frag);
}

CG::Binding bind_card(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->type().ispar());
  assert(call->n_args() == 1);

  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_i(GET_REG(cg));
  int r_sz(GET_REG(cg)); 
  int r_e(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));

  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
  int l_hd(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_i), CG::r(r_sz), CG::r(r_e));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_e), CG::l(l_tl));

  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_e));
  PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r), CG::r(r_e), CG::r(r)); 
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_i), CG::r(r_e));
  PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r), CG::r(r_e), CG::r(r));

  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_i), CG::r(r_sz), CG::r(r_e));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_e), CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding bind_internal(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::string name = call->decl()->id().str();
  CG_ProcID proc = cg.find_builtin(name);
  int r_res(GET_REG(cg));
  OPEN_OTHER(cg, frag);

  std::vector<CG_Value> r_args(call->n_args());
  for (int i = 0; i < call->n_args(); ++i) {
    CG::Binding b_arg(CG::bind(call->arg(i), cg, frag));
    r_args[i] = CG::r(b_arg.first);
    // TODO: What about the CG_Cond (how do they aggregate for builtin calls?)
  }

  // Push BUILTIN instruction with the correct id
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, proc);
  // Append instruction with register arguments
  CG_Instr &i = frag.instrs.back();
  PUSH_INSTR_OPERAND(i, r_args);

  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_res));

  return {r_res, CG_Cond::ttt()};
}

CG_Cond::T eval_context_is_root(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: What do we do with the argument?

  if (ctx == BytecodeProc::ROOT) {
    return CG_Cond::ttt();
  } else {
    return CG_Cond::fff();
  }
}

CG_Cond::T eval_has_bounds(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: Actual implementation!

  return CG_Cond::ttt();
}

builtin_table init_builtins(void) {
  builtin_table tbl;
  Constants& c(constants());
  tbl.insert(std::make_pair(c.ids.sum, builtin_t { eval_error_b, bind_sum } ));
  tbl.insert(std::make_pair(c.ids.exists, builtin_t { eval_exists, bind_error_g } ));
  tbl.insert(std::make_pair(c.ids.forall, builtin_t { eval_forall, bind_error_g } ));
  tbl.insert(std::make_pair(c.ids.assert, builtin_t { eval_assert_b, bind_assert_g } ));
  tbl.insert(std::make_pair("array1d", builtin_t { eval_error_b, bind_array1d } ));
  tbl.insert(std::make_pair("array2d", builtin_t { eval_error_b, bind_arrayXd<2> } ));
  tbl.insert(std::make_pair("array3d", builtin_t { eval_error_b, bind_arrayXd<3> } ));
  tbl.insert(std::make_pair("array4d", builtin_t { eval_error_b, bind_arrayXd<4> } ));
  tbl.insert(std::make_pair("array5d", builtin_t { eval_error_b, bind_arrayXd<5> } ));
  tbl.insert(std::make_pair("array6d", builtin_t { eval_error_b, bind_arrayXd<6> } ));
  tbl.insert(std::make_pair("array7d", builtin_t { eval_error_b, bind_arrayXd<7> } ));
  tbl.insert(std::make_pair("array8d", builtin_t { eval_error_b, bind_arrayXd<8> } ));
  tbl.insert(std::make_pair("array9d", builtin_t { eval_error_b, bind_arrayXd<9> } ));
  tbl.insert(std::make_pair("array_union", builtin_t { eval_error_b, bind_array_union } ));
  tbl.insert(std::make_pair("index_set", builtin_t { eval_error_b, bind_index_set_XofY<1, 1>} ));
  tbl.insert(std::make_pair("index_set_1of2", builtin_t { eval_error_b, bind_index_set_XofY<1, 2>} ));
  tbl.insert(std::make_pair("index_set_2of2", builtin_t { eval_error_b, bind_index_set_XofY<2, 2>} ));
  tbl.insert(std::make_pair("index_set_1of3", builtin_t { eval_error_b, bind_index_set_XofY<1, 3>} ));
  tbl.insert(std::make_pair("index_set_2of3", builtin_t { eval_error_b, bind_index_set_XofY<2, 3>} ));
  tbl.insert(std::make_pair("index_set_3of3", builtin_t { eval_error_b, bind_index_set_XofY<3, 3>} ));
  tbl.insert(std::make_pair("index_set_1of4", builtin_t { eval_error_b, bind_index_set_XofY<1, 4>} ));
  tbl.insert(std::make_pair("index_set_2of4", builtin_t { eval_error_b, bind_index_set_XofY<2, 4>} ));
  tbl.insert(std::make_pair("index_set_3of4", builtin_t { eval_error_b, bind_index_set_XofY<3, 4>} ));
  tbl.insert(std::make_pair("index_set_4of4", builtin_t { eval_error_b, bind_index_set_XofY<4, 4>} ));
  tbl.insert(std::make_pair("length", builtin_t { eval_error_b, bind_length } ));
  tbl.insert(std::make_pair("lb", builtin_t { eval_error_b, bind_lb } ));
  tbl.insert(std::make_pair("ub", builtin_t { eval_error_b, bind_ub } ));
  tbl.insert(std::make_pair("dom", builtin_t { eval_error_b, bind_dom } ));
  tbl.insert(std::make_pair("lb_array", builtin_t { eval_error_b, bind_lb_array } ));
  tbl.insert(std::make_pair("ub_array", builtin_t { eval_error_b, bind_ub_array } ));
  tbl.insert(std::make_pair("set2array", builtin_t { eval_error_b, bind_set2array } ));
  tbl.insert(std::make_pair("dom_bounds_array", builtin_t { eval_error_b, bind_dom_bounds_array } ));
  tbl.insert(std::make_pair("max", builtin_t { eval_error_b, bind_max } ));
  tbl.insert(std::make_pair("arg_max", builtin_t { eval_error_b, bind_arg_max } ));
  tbl.insert(std::make_pair("min", builtin_t { eval_error_b, bind_min } ));
  tbl.insert(std::make_pair("card", builtin_t { eval_error_b, bind_card } ));
  tbl.insert(std::make_pair(c.ids.bool2int, builtin_t { eval_error_b, bind_bool2int } ));
  tbl.insert(std::make_pair("uniform", builtin_t { eval_error_b, bind_internal } ));
  tbl.insert(std::make_pair("sol", builtin_t { eval_error_b, bind_internal } ));
  tbl.insert(std::make_pair("sort_by", builtin_t { eval_error_b, bind_internal } ));
  tbl.insert(std::make_pair("floor", builtin_t { eval_error_b, bind_internal } ));
  tbl.insert(std::make_pair("ceil", builtin_t { eval_error_b, bind_internal } ));
  tbl.insert(std::make_pair("mzn_in_root_context", builtin_t { eval_context_is_root, bind_error_g } ));
  tbl.insert(std::make_pair("has_bounds", builtin_t { eval_has_bounds, bind_error_g } ));
  return tbl;
}
builtin_table& builtins(void) {
  static builtin_table tbl(init_builtins());
  return tbl;
}

CG::Binding CG::bind(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    return CG::Binding(cg.env().lookup(x->v()).first, CG_Cond::ttt());
  } catch(const CG_Env<CodeGen::Binding>::NotFound& exn) {
    debugprint(x);
    int g = cg.globals_env.at(x->v());
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG::Binding(r, CG_Cond::ttt());
  }
}

CG::Binding CG::bind(SetLit* l, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  bool is_vec(false);
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  if(IntSetVal* s = l->isv()) {
    int r_t(GET_REG(cg));
    for(int ii = 0; ii < s->size(); ++ii) {
      int l(s->min(ii).toInt());
      int u(s->max(ii).toInt());
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(l), CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u), CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
    }
  } else {
    is_vec = true;
    int sz = l->v().size();
    int r_t(GET_REG(cg));
    for(int ii = 0; ii < sz; ii++) {
      // Assumes the set is sorted.
      /*
      IntLit* k(l->v()[ii]->template cast<IntLit>());
      assert(k);
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(k->v().toInt()), CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      */
      CG::Binding b(CG::bind(l->v()[ii], cg, frag)); // Ignores any partiality.
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b.first));
    }
  }
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  if(is_vec)
    PUSH_INSTR(frag, BytecodeStream::MAKE_SET, CG::r(r), CG::r(r));
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding CG::bind(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Build up the array.
  std::vector<int> r_vec;
  std::vector<CG_Cond::T> p_vec;

  int sz(a->size());
  /*
  int r_one(-1);
  int r_zero(-1);
  */
  for(int ii = 0; ii < sz; ++ii) {
    if((*a)[ii]->type().isbool()) {
      CG_Cond::T c(CG::compile((*a)[ii], cg, frag));
      if(!c.get()) {
        if(!c.sign()) {
          r_vec.push_back(bind_cst(1, cg, frag));
        } else {
          r_vec.push_back(bind_cst(0, cg, frag));
        }
      } else {
        r_vec.push_back(CG::force(c, cg, frag));
      }
    } else {
      Binding b_ii(CG::bind((*a)[ii], cg, frag));
      r_vec.push_back(b_ii.first);
      p_vec.push_back(b_ii.second);
    }
  }

//  Build Array
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  for(int r_c : r_vec)
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  int rA(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rA));

//  Build index sets
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  for (int ii = 0; ii < a->dims(); ++ii) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(a->min(ii), cg, frag)));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(a->max(ii), cg, frag)));
  }
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  int rI(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

// Combine array and index sets
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rA));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rA));

  return {rA, CG_Cond::forall(ctx, p_vec)};
}

CG::Binding CG::bind(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());

  bool is_var = false;
  for(int ii = 0; ii < sz; ++ii) {
    is_var = is_var || idx[ii]->type().isvar();
  }

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T> cond;

  if(is_var) {
    OPEN_OTHER(cg, frag);
  }
  // Now evaluate the array body, and emit the indices.
  std::vector<int> r_idxs(sz);
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    cond.push_back(b.second);
  }

  Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  cond.push_back(b_A.second);

  int r = GET_REG(cg);

  if(is_var) {
    std::vector<Type> types(sz+1, Type::varint());
    types[sz] = Type::varint(sz);
    auto fun = find_call_fun(cg, {"element"}, Type::varint(), types, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);

    std::vector<CG_Value> r_args(sz+1);
    for (int ii = 0; ii < sz; ++ii) {
      r_args[ii] = CG::r(r_idxs[ii]);
    }
    r_args[sz] = CG::r(r_A);

    // Push CALL instruction with the correct id
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first);
    // Append instruction with register arguments
    CG_Instr &i = frag.instrs.back();
    PUSH_INSTR_OPERAND(i, r_args);

    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  } else {
    // Just read the vector, and get the appropriate element.
    int r_I(GET_REG(cg));
    int r_index(GET_REG(cg));
    int r_mult(GET_REG(cg));
    int r_min(GET_REG(cg));
    int r_max(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(bind_cst(2, cg, frag)), CG::r(r_I));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(1, cg, frag)), CG::r(r_min));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_idxs[0]), CG::r(r_index));
    PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_index), CG::r(r_min), CG::r(r_index));
    if (sz > 1) {
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_mult));
      for (int idxs = 1; idxs < sz; ++idxs) {
        PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(idxs*2+1, cg, frag)), CG::r(r_min));
        PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(idxs*2+2, cg, frag)), CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_max), CG::r(r_min), CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_mult), CG::r(r_max), CG::r(r_mult));

        PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_index), CG::r(r_mult), CG::r(r_index));

        PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_index), CG::r(r_idxs[idxs]), CG::r(r_index));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_index), CG::r(r_min), CG::r(r_index));
      }
    }
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_index)); // Indexes are 1 indexed.

    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_index), CG::r(r));
  }
  return {r, CG_Cond::forall(ctx, cond)};
}

int make_vec(CodeGen& cg, CG_Builder& frag, const std::vector<int>& regs) {
  OPEN_OTHER(cg, frag);
  OPEN_VEC(cg, frag);
  for(int r : regs)
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  CLOSE_AGG(cg, frag);
  CLOSE_AGG(cg, frag);
  int r_vec(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_vec));
  return r_vec;
}

void call_clause(CodeGen& cg, CG_Builder& frag, Mode ctx, const std::vector<int>& pos, const std::vector<int>& neg) {
  GCLock lock;
  // Build the arguments vectors.
  int r_pos(make_vec(cg, frag, pos));
  int r_neg(make_vec(cg, frag, neg));
  auto fun = find_call_fun(cg, {"clause"}, Type::varbool(), {Type::varbool(1), Type::varbool(1)}, ctx);
  assert (BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
  PUSH_INSTR(frag, BytecodeStream::CALL, fun.second, fun.first, CG::r(r_pos), CG::r(r_neg));
}

int deinterlace(CodeGen& cg, CG_Builder& frag, int vec, int width, int offset) {
  OPEN_VEC(cg, frag);
  // int r_i(CG::locate_immi(1 + offset, cg, frag));
  // int r_step(CG::locate_immi(width, cg, frag));
  int r_i(bind_cst(1 + offset, cg, frag));
  int r_step(bind_cst(width, cg, frag));
  int r_sz(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(vec), CG::r(r_sz));
  int r_elt(GET_REG(cg));

  int l_hd(GET_LABEL(cg));
  int l_tl(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_elt), CG::l(l_tl));
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(vec), CG::r(r_i), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_i), CG::r(r_step), CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_i), CG::r(r_sz), CG::r(r_elt));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_elt), CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  CLOSE_AGG(cg, frag);
  int r_slice(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_slice));
  return r_slice;
}

/*
inline int reg_k(int k, CodeGen& cg, CG_Builder& frag, int& r) {
  if(r == -1)
    r = CG::locate_immi(1, cg, frag);
  return r;
}
*/

CG::Binding CG::bind(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<int> r_cond;
  std::vector<int> p_res;
  std::vector<int> r_res;

  /*
  int r_one(-1);
  int r_zero(-1);
  */
  int r_one(bind_cst(1, cg, frag));

  bool is_total = true;
  int sz(ite->size());
  for(int ii = 0; ii < sz; ++ii) {
    r_cond.push_back(CG::force(CG::compile(ite->e_if(ii), cg, frag), cg, frag));
    CG::Binding b_res(CG::bind(ite->e_then(ii), cg, frag));
    r_res.push_back(b_res.first);
    // Make sure b_res.second is evaluated _outside_ the aggregation.
    if(b_res.second.get()) {
      is_total = false;
      p_res.push_back(CG::force(b_res.second, cg, frag));
    } else {
      if(b_res.second.sign())
        /* p_res.push_back(reg_k(0, cg, frag, r_zero)); */
        p_res.push_back(bind_cst(0, cg, frag));
      else
        /* p_res.push_back(reg_k(1, cg, frag, r_one)); */
        p_res.push_back(r_one);
    }
  }
  Binding b_final = CG::bind(ite->e_else(), cg, frag);
  r_res.push_back(b_final.first);
  if(b_final.second.get()) {
    is_total = false;
    p_res.push_back(CG::force(b_final.second, cg, frag));
  } else {
    if(b_final.second.sign())
      // p_res.push_back(reg_k(0, cg, frag, r_zero));
      p_res.push_back(bind_cst(0, cg, frag));
    else
      // p_res.push_back(reg_k(1, cg, frag, r_one));
      p_res.push_back(bind_cst(1, cg, frag));
  }

  // Build an array of interleaved selectors and conditions.
  int r_UB = CG::locate_immi(1, cg, frag);
  int r_test(GET_REG(cg));
  OPEN_VEC(cg, frag);
  int l_end(GET_LABEL(cg));
  for(int ii = 0; ii < sz; ++ii) {
    int l_cont(GET_LABEL(cg));
    PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(r_cond[ii]), CG::r(r_test));
    // PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_test)); // ISPAR is broken for now.
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_cont));
    int l_skip(GET_LABEL(cg));
    // Condition is par. If the condition is false, we skip this one.
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_cond[ii]), CG::l(l_skip));
    // If it's true, we push this last element, and go to the end.
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_res[ii]));
    if(!is_total)
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(p_res[ii]));
    PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));
    PUSH_LABEL(frag, l_cont);
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_UB));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_res[ii]));
    if(!is_total)
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(p_res[ii]));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_cond[ii]));
    PUSH_LABEL(frag, l_skip);
  }
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_res[sz]));
  if(!is_total)
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(p_res[sz]));
  PUSH_LABEL(frag, l_end);
  CLOSE_AGG(cg, frag);
  // We now have a vector of the form [then(0), p[then(0)], if(0)|then(1), p[then(1)], if(1)|...|else,p[else]].
  // With size r_UB.
  int r_VEC(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_VEC));

  CG_Cond::T part(CG_Cond::ttt()); 

  // Check how many values there are.
  int l_fin(GET_LABEL(cg));
  int l_sel(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_one), CG::r(r_UB), CG::r(r_test));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_sel));
  {
  OPEN_OTHER(cg, frag);
  // If only one, we just return it.
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_VEC), CG::r(r_one), CG::r(r_UB));
  if(!is_total) {
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(2), CG::r(r_one));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_VEC), CG::r(r_one), CG::r(r_UB));
    part = CG_Cond::reg(r_UB);
  }
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_UB));
  CLOSE_AGG(cg, frag);
  }
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_fin));
  
  PUSH_LABEL(frag, l_sel);
  // Create the selector variable.
  // It's not nested inside the result variable,
  // because we'll also need it for the partiality.
  // Create the selector variable in a nested context.
  {
  OPEN_OTHER(cg, frag);
  // Create the selector domain.
  OPEN_VEC(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_one));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_UB));
  CLOSE_AGG(cg, frag);
  int r_d(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_d));

  // And the variable
  PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
  int r_idx(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_idx));
  
  // Now add the constraints on the selector.
  int r_SEL = deinterlace(cg, frag, r_VEC, 3 - is_total, 2 - is_total);
  int r_i(CG::locate_immi(1, cg, frag));
  int l_hd(GET_LABEL(cg));
  int l_ex(GET_LABEL(cg));
  int r_sel(GET_REG(cg));
  int r_pre(GET_REG(cg));
  int r_curr(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_i), CG::r(r_UB), CG::r(r_sel));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sel), CG::l(l_ex));
  // First case, cond[i] <-> x <= i.
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_SEL), CG::r(r_i), CG::r(r_sel));
  call_binop(cg, frag, BytecodeProc::FUN, BOT_LQ, r_idx, r_i);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_curr));

  call_clause(cg, frag, BytecodeProc::ROOT, std::vector<int> { r_curr }, std::vector<int> { r_sel });
  call_clause(cg, frag, BytecodeProc::ROOT, std::vector<int> { r_sel }, std::vector<int> { r_curr });

  // Increment to the second element. If we got here, there definitely was one.
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_LABEL(frag, l_hd);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_SEL), CG::r(r_i), CG::r(r_sel));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_curr), CG::r(r_pre));
  call_binop(cg, frag, BytecodeProc::FUN, BOT_LQ, r_idx, r_i);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_curr));

  // Inner case:
  // ~cond[i] \/ x <= i.
  call_clause(cg, frag, BytecodeProc::ROOT, std::vector<int> { r_curr }, std::vector<int> { r_sel });
  // cond[i] \/ x <= (i-1) \/ x > i.
  call_clause(cg, frag, BytecodeProc::ROOT, std::vector<int> { r_sel, r_pre }, std::vector<int> { r_curr });

  // Now increment the loop.
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_i), CG::r(r_UB), CG::r(r_sel));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_sel), CG::l(l_hd));
  PUSH_LABEL(frag, l_ex);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_idx));
  CLOSE_AGG(cg, frag);
  }
  int r_idx(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_idx));

  // Now that we've got the selector, compile the conditional part, and the result.
  if(!is_total) {
    GCLock lock;
    int r_part(deinterlace(cg, frag, r_VEC, 3, 1));
    auto fun = find_call_fun(cg, {"element"}, Type::varbool(), {Type::varbool(), Type::varint(1)}, ctx);
    assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
    part = CG_Cond::call(fun.first, fun.second, CG::r(r_idx), CG::r(r_part));
  }

  {
    GCLock lock;
    OPEN_OTHER(cg, frag);
    int r_A(deinterlace(cg, frag, r_VEC, 3 - is_total, 0));
    auto fun = find_call_fun(cg, {"element"}, Type::varint(), {Type::varint(), Type::varint(1)}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(r_idx), CG::r(r_A));
    CLOSE_AGG(cg, frag);
  }

  // On either branch, part is set, and the result is on the stack.
  PUSH_LABEL(frag, l_fin);
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));
  
  return CG::Binding(r_ret, part);
}

CG::Binding CG::bind(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  Binding b_lhs(CG::bind(b->lhs(), cg, frag));
  Binding b_rhs(CG::bind(b->rhs(), cg, frag));

  std::vector<CG_Cond::T> partial;
  partial.push_back(b_lhs.second);
  partial.push_back(b_rhs.second);
  int r;
  if(b->type().ispar()) {
    r = bind_binop_par(cg, frag, b->op(), b_lhs.first, b_rhs.first);
  } else {
    OPEN_OTHER(cg, frag);
    call_binop(cg, frag, BytecodeProc::FUN, b->op(), b_lhs.first, b_rhs.first);
    CLOSE_AGG(cg, frag);
    r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  }

  if(b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
    // These all require rhs() != 0. Which means we need to evaluate the rhs,
    // and put it in a register.
    // int r_zero(CG::locate_immi(0, cg, frag));
    int r_zero(bind_cst(0, cg, frag));
    if(b->rhs()->type().ispar()) {
      /*
      int r_nz(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(b_rhs.first), CG::r(r_zero), CG::r(r_nz));
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_nz), CG::r(r_nz));
      partial.push_back(CG_Cond::reg(r_nz));
      */
      // FIXME: Will currently abort if div/mod is called with a zero rhs.
    } else {
      GCLock lock;
      auto fun = find_call_fun(cg, {"op_equals"}, Type::varbool(), {Type::varint(), Type::varint()}, -ctx);
      assert(BytecodeProc::is_neg(-ctx) == BytecodeProc::is_neg(fun.second));
      partial.push_back(~CG_Cond::call(fun.first, fun.second, CG::r(b_rhs.first), CG::r(r_zero)));
    }
  }
  return CG::Binding(r, CG_Cond::forall(ctx, partial));
}

CG::Binding CG::bind(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Unop is easy.
  switch(u->op()) {
    case UOT_NOT:
      throw InternalError("Non-Boolean bind called on boolean operator.");
    case UOT_PLUS:
      return CG::bind(u->e(), cg, frag);
    case UOT_MINUS: {
      Binding b_e(CG::bind(u->e(), cg, frag));
      int r;
      if(u->type().ispar()) {
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r), CG::r(b_e.first), CG::r(r));
      } else {
        GCLock lock;
        OPEN_OTHER(cg, frag);
        auto fun = find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint()}, BytecodeProc::FUN);
        assert(fun.second == BytecodeProc::FUN);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, CG::r(b_e.first));
        CLOSE_AGG(cg, frag);
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      }
      return CG::Binding(r, b_e.second);
    }
  }
}

CG::Binding bind_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Collects the partiality of the arguments
  std::vector<CG_Cond::T> p_arg;

  OPEN_OTHER(cg, frag);
  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  for(int ii = 0; ii < sz; ++ii) {
    if(call->arg(ii)->type().isbool()) {
      int r_i(CG::force(CG::compile(call->arg(ii), cg, frag), cg, frag));
      r_arg[ii] = CG::r(r_i);
    } else {
      CG::Binding r_bind(CG::bind(call->arg(ii), cg, frag));
      r_arg[ii] = CG::r(r_bind.first);
      p_arg.push_back(r_bind.second);
    }
  }
  // p_arg.push_back(CG_Cond::call(find_call_pred(cg, call), ctx, r_arg));

  // Bind the value part.
  auto fun = find_call_fun(cg, call, BytecodeProc::ROOT);
  PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, fun.first, r_arg);
  CLOSE_AGG(cg, frag);
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  return std::make_pair(r_ret, CG_Cond::forall(ctx, p_arg));
}

CG::Binding CG::bind(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      return (*it).second.general(call, ctx, cg, frag);
    }
  }

  return bind_call(call, ctx, cg, frag);
}

CG::Binding CG::bind(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  ASTExprVec<Expression> bindings(let->let());
  std::vector<CG_Cond::T> partial;
  cg.env_push();
  for(Expression* e : bindings) {
    if (VarDecl* vd = e->dyn_cast<VarDecl>()) {
      // Create the new name
      int r_v;
      if(vd->e()) {
        // FIXME: Assuming domain isn't constraining.
        CG::Binding b_v(CG::bind(vd->e(), cg, frag));
        r_v = b_v.first;
        partial.push_back(b_v.second);
        if(Expression* d = vd->ti()->domain()) {
          if(!vd->ti()->isarray()) {
            CG::Binding b_d = CG::bind(d, cg, frag); // Discarding any constraints on the set.
            partial.push_back(b_d.second);
            int r_dp = GET_REG(cg);
            PUSH_INSTR(frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(r_v), CG::r(b_d.first), CG::r(r_dp));
          }
        }
      } else {
        // Variable declaration. Assumes is total and nonempty.
        CG::Binding b_d(bind_domain(vd, cg, frag));
        int r_d = b_d.first; // Ignoring constraints introduced by domain.
        partial.push_back(b_d.second);
        if(!vd->ti()->isarray()) {
          OPEN_OTHER(cg, frag);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
          CLOSE_AGG(cg, frag);
        } else {
          // Open nested iterators
          std::vector<int> r_regs;
          for(Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            CG::Binding b_reg(CG::bind(dim, cg, frag));
            // FIXME: Ignoring conditions
            // post_cond(cg, frag, b_reg.second);
            r_regs.push_back(b_reg.first);
          }
          std::vector<Forset> nesting;
          OPEN_VEC(cg, frag);
          for(int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(frag);
          }
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(b_d.first));
          for(int r_i = r_regs.size()-1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(frag);
          }
          CLOSE_AGG(cg, frag);
        }
        r_v = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));
      }
      cg.env().bind(vd->id()->v(), CodeGen::Binding(r_v, CG_Cond::ttt()));
    } else {
      // Must be a constraint
      partial.push_back(CG::compile(e, cg, frag));
    }
  }
  CG::Binding b_in(CG::bind(let->in(), cg, frag));
  partial.push_back(b_in.second);
  cg.env_pop();
  return CG::Binding(b_in.first, CG_Cond::forall(ctx, partial));
}

CG::Binding CG::bind(Comprehension* comp, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Lift out the ourter-most comprehension, since it will always
  // be executed.
  CG::bind(comp->in(0), cg, frag);

  cg.env_push();
  OPEN_VEC(cg, frag);
  execute_comprehension_bind(comp, ctx, cg, frag);
  CLOSE_AGG(cg, frag);
  cg.env_pop();
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  if(comp->type().is_set()) {
    PUSH_INSTR(frag, BytecodeStream::MAKE_SET, CG::r(r), CG::r(r));
  } else {
    assert(comp->type().dim() > 0);
    int rI(GET_REG(cg));
//  Add index set
    OPEN_OTHER(cg, frag);
    OPEN_VEC(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(1, cg, frag)));
    PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r), CG::r(rI));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

// Combine array and index sets
    OPEN_OTHER(cg, frag);
    OPEN_VEC(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    CLOSE_AGG(cg, frag);
    CLOSE_AGG(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  }
  return CG::Binding(r, CG_Cond::ttt());
}

CG_Cond::T _compile(Expression* e, CodeGen& cg, CG_Builder& frag) {
  // Look up the mode we need to compile e in.
  // debugprint(e);
  CG::Mode ctx(BytecodeProc::FUN);
  try {
    ctx = cg.mode_map.at(e);
  } catch(const std::out_of_range& exn) {
    std::cerr << "%% Missing mode for: "; debugprint(e);  
  }

  switch (e->eid()) {
  case Expression::E_INTLIT:
  case Expression::E_FLOATLIT:
  case Expression::E_SETLIT:
  case Expression::E_STRINGLIT:
  case Expression::E_ARRAYLIT:
  case Expression::E_COMP:
    throw InternalError("compile called on non-Boolean expression.");
  case Expression::E_BOOLLIT:
      return (e->template cast<BoolLit>()->v()) ? CG_Cond::ttt() : CG_Cond::fff();
  case Expression::E_ID:
    return CG::compile(e->template cast<Id>(), ctx, cg, frag);
  case Expression::E_ANON:
    throw InternalError("compile reached unexpected expression type: E_ANON.");
  case Expression::E_ARRAYACCESS:
    return CG::compile(e->template cast<ArrayAccess>(), ctx, cg, frag);
  case Expression::E_ITE:
    return CG::compile(e->template cast<ITE>(), ctx, cg, frag);
  case Expression::E_BINOP:
    return CG::compile(e->template cast<BinOp>(), ctx, cg, frag);
  case Expression::E_UNOP:
    return CG::compile(e->template cast<UnOp>(), ctx, cg, frag);
  case Expression::E_CALL:
    return CG::compile(e->template cast<Call>(), ctx, cg, frag);
  case Expression::E_LET:
    return CG::compile(e->template cast<Let>(), ctx, cg, frag);
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type in compile.");
  }
}

CG_Cond::T CG::compile(Expression* e, CodeGen& cg, CG_Builder& frag) {
  assert(e->type().isbool());
  try {
    CG_Cond::T r(cg.cache_lookup(e).second);
    return r;
  } catch(const CG_Env<CG::Binding>::NotFound& exn) {
    CG_Cond::T cond(_compile(e, cg, frag));
    CG::Binding b(0xdeadbeef, cond);
    cg.cache_store(e, b);
    return cond;
  }
  // return _compile(e, cg, frag); // FIXME: Update env representation.
}

CG_Cond::T CG::compile(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    return CG_Cond::reg(cg.env().lookup(x->v()).first);
  } catch(const CG_Env<Binding>::NotFound& exn) {
    // debugprint(x);
    int g = cg.globals_env.at(x->v());
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG_Cond::reg(r);
  }
}

CG_Cond::T compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());

  bool is_var = false;
  for(int ii = 0; ii < sz; ++ii) {
    is_var = is_var || idx[ii]->type().isvar();
  }

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T> cond;

  // Now evaluate the array body, and emit the indices.
  std::vector<int> r_idxs(sz);
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    cond.push_back(b.second);
  }

  CG::Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  cond.push_back(b_A.second);

  if(is_var) {
    GCLock lock;
    std::vector<Type> types(sz+1, Type::varint());
    types[sz] = Type::varbool(sz);
    auto fun = find_call_fun(cg, {"element"}, Type::varbool(), types, -ctx);
    assert(BytecodeProc::is_neg(-ctx) == BytecodeProc::is_neg(fun.second));

    std::vector<CG_Value> r_args(sz+1);
    for (int ii = 0; ii < sz; ++ii) {
      r_args[ii] = CG::r(r_idxs[ii]);
    }
    r_args[sz] = CG::r(r_A);

    cond.push_back(CG_Cond::call(fun.first, fun.second, r_args));
  } else {
    // Just read the vector, and get the appropriate element.
    int r(GET_REG(cg));
    int r_I(GET_REG(cg));
    int r_index(GET_REG(cg));
    int r_mult(GET_REG(cg));
    int r_min(GET_REG(cg));
    int r_max(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(bind_cst(2, cg, frag)), CG::r(r_I));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(1, cg, frag)), CG::r(r_min));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_idxs[0]), CG::r(r_index));
    PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_index), CG::r(r_min), CG::r(r_index));
    if (sz > 1) {
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_mult));
      for (int idxs = 1; idxs < sz; ++idxs) {
        PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(idxs*2+1, cg, frag)), CG::r(r_min));
        PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_I), CG::r(bind_cst(idxs*2+2, cg, frag)), CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_max), CG::r(r_min), CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_max));
        PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_mult), CG::r(r_max), CG::r(r_mult));

        PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_index), CG::r(r_mult), CG::r(r_index));

        PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_index), CG::r(r_idxs[idxs]), CG::r(r_index));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_index), CG::r(r_min), CG::r(r_index));
      }
    }
    PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_index)); // Indexes are 1 indexed.

    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(bind_cst(1, cg, frag)), CG::r(r_A));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_index), CG::r(r));
    cond.push_back(CG_Cond::reg(r));
  }
  return CG_Cond::forall(ctx, cond);
}

CG_Cond::T CG::compile(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int sz(ite->size());
   
  // Check whether all the conditions are par.
  bool all_par = true;
  for(int ii = 0; ii < sz; ++ii) {
    if(!ite->e_if(ii)->type().ispar()) {
      all_par = false;
      break;
    }
  }

  if(all_par) {
    // We need to interfere with the env, here, because stuff may not be available.
    // Except, ite->e_if(0) will always be available.
    int l_end(GET_LABEL(cg));
    int r_ret(GET_REG(cg));
    for(int ii = 0; ii < sz; ++ii) {
      int r_sel = CG::force(CG::compile(ite->e_if(ii), cg, frag), cg, frag);
      int l_cont(GET_LABEL(cg));  
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sel), CG::l(l_cont));
      cg.env_push();
      int r_val = CG::force(CG::compile(ite->e_then(ii), cg, frag), cg, frag);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_val), CG::r(r_ret));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));
      cg.env_pop();
      cg.env_push();
      PUSH_LABEL(frag, l_cont);
    }
    // Else case.
    int r_val = CG::force(CG::compile(ite->e_else(), cg, frag), cg, frag);
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_val), CG::r(r_ret));
    PUSH_LABEL(frag, l_end);
    // Now kill the availability of all the expressions.
    for(int ii = 0; ii < sz; ++ii)
      cg.env_pop();
    return CG_Cond::reg(r_ret);
  } else {
    // Put the conditions in registers, and compile the results.
    std::vector<int> r_if;
    std::vector<CG_Cond::T> c_then;

    for(int ii = 0; ii < sz; ++ii) {
      r_if.push_back(CG::force(CG::compile(ite->e_if(ii), cg, frag), cg, frag)); 
      c_then.push_back(CG::compile(ite->e_then(ii), cg, frag));
    }
    TODO();
    /*
    int r(GET_REG(cg));
    int l_exit(GET_LABEL(cg));
    for(int ii = 0; ii < sz; ++ii) {
      int l_cont(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_if[ii]), CG::l(l_cont));
      PUSH_INSTR(frag, BytecodeStream::MOV, 
    }
    */
    return CG_Cond::ttt();
  }
}

int force_or_bind(Expression* e, std::vector<CG_Cond::T>& cond, CodeGen& cg, CG_Builder& frag) {
  if(e->type().isbool()) {
    return CG::force(CG::compile(e, cg, frag), cg, frag);
  } else {
    CG::Binding b(CG::bind(e, cg, frag));
    if(b.second.get())
      cond.push_back(b.second);
    return b.first;
  }
}
CG_Cond::T CG::compile(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> cond;
  if(b->type().ispar()) {
    int r_lhs = force_or_bind(b->lhs(), cond, cg, frag);
    int r_rhs = force_or_bind(b->rhs(), cond, cg, frag);
    std::vector<int> r_cond;
    for(CG_Cond::T c : cond)
      r_cond.push_back(CG::force(c, cg, frag));
    int r_ret = bind_binop_par(cg, frag, b->op(), r_lhs, r_rhs);
    if(r_cond.size() > 0) {
      // If any conditions don't hold, evaluate to false.
      int r(GET_REG(cg));
      int l_tl(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
      for(int r_c : r_cond) {
        PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_c), CG::l(l_tl)); 
      }
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_ret), CG::r(r));
      PUSH_LABEL(frag, l_tl);
      return CG_Cond::reg(r);
    } else {
      return CG_Cond::reg(r_ret);
    }
  }
  switch(b->op()) {
    case BOT_LE:
    case BOT_LQ:
    case BOT_GR:
    case BOT_GQ:
    case BOT_EQ:
    case BOT_NQ: {
      // Potentially partial.
      // Converted into canonical form by binop_cond.
      CG::Binding b_lhs(CG::bind(b->lhs(), cg, frag));
      CG::Binding b_rhs(CG::bind(b->rhs(), cg, frag));
      cond.push_back(b_lhs.second);
      cond.push_back(b_rhs.second);
      // cond.push_back(binop_cond(cg, b->op(), ctx, b_lhs.first, b_rhs.first));
      cond.push_back(linear_cond(cg, frag, b->op(), ctx, b_lhs.first, b_rhs.first));
      return CG_Cond::forall(ctx, cond);
    }
    break;
    case BOT_AND: {
      cond.push_back(CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::forall(ctx, cond);
    }
    break;
    case BOT_OR: {
      cond.push_back(CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::exists(ctx, cond);
    }
    break;
    case BOT_IMPL: {
      cond.push_back(~CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::exists(ctx, cond);
    }
    break;
    case BOT_RIMPL: {
      cond.push_back(CG::compile(b->lhs(), cg, frag));
      cond.push_back(~CG::compile(b->rhs(), cg, frag));
      return CG_Cond::exists(ctx, cond);
    }
    case BOT_IN: {
      CG::Binding b_lhs(CG::bind(b->lhs(), cg, frag));
      CG::Binding b_rhs(CG::bind(b->rhs(), cg, frag));
      assert(b->rhs()->type().ispar());
      cond.push_back(b_lhs.second);
      cond.push_back(b_rhs.second);
      cond.push_back(binop_cond(cg, BOT_IN, ctx, b_lhs.first, b_rhs.first));
      return CG_Cond::forall(ctx, cond);
    }
    default: {
      // Standard case.
      int r_lhs(CG::force(CG::compile(b->lhs(), cg, frag), cg, frag));
      int r_rhs(CG::force(CG::compile(b->rhs(), cg, frag), cg, frag));
      return binop_cond(cg, b->op(), ctx, r_lhs, r_rhs);
      /*
      if(b->type().ispar()) {
        return CG_Cond::reg(bind_binop_par(cg, frag, b->op(), r_lhs, r_rhs));
      } else {
        return binop_cond(cg, b->op(), ctx, r_lhs, r_rhs);
      }
      */
    }
  }
}

CG_Cond::T CG::compile(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: Fix CG_Cond to handle negation.
  assert(u->op() == UOT_NOT);
  if(u->type().ispar()) {
    int r_e = CG::force(CG::compile(u->e(), cg, frag), cg, frag);
    int r_neg = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_e), CG::r(r_neg));
    return CG_Cond::reg(r_neg);
  }
  return ~CG::compile(u->e(), cg, frag);
}


CG_Cond::T compile_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  std::vector<CG_Cond::T> p_arg;
  
  // Evaluate the args, collecting the conditionality.
  for(int ii = 0; ii < sz; ++ii) {
    Expression* e(call->arg(ii));
    // CG::Binding b_arg(CG::bind(call->arg(ii), cg, frag));
    if(e->type().isbool()) {
      r_arg[ii] = CG::r(CG::force(CG::compile(e, cg, frag), cg, frag));
    } else {
      CG::Binding b(CG::bind(call->arg(ii), cg, frag));
      r_arg[ii] = CG::r(b.first);
      p_arg.push_back(b.second);
    }
  }
  // And finally, add the call itself
  auto fun = find_call_fun(cg, call, ctx);
  if (BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second)) {
    p_arg.push_back(CG_Cond::call(fun.first, fun.second, r_arg));
  } else {
    assert(call->type().isbool());
    CG_Value ret = CG::r(CG::force(CG_Cond::call(fun.first, fun.second, r_arg), cg, frag));
    fun = find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    assert(fun.second == BytecodeProc::FUN);
    p_arg.push_back(CG_Cond::call(fun.first, BytecodeProc::FUN, ret));
  }
  return CG_Cond::forall(ctx, p_arg);
}

CG_Cond::T CG::compile(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If we have a builtin for this, dispatch to that instead.
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      return it->second.boolean(call, ctx, cg, frag);
    }
  }

  return compile_call(call, ctx, cg, frag);
}

CG_Cond::T CG::compile(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> conj; 
  ASTExprVec<Expression> bindings(let->let());
  std::vector<CG_Cond::T> partial;
  cg.env_push();
  for(Expression* e : bindings) {
    if (VarDecl* vd = e->dyn_cast<VarDecl>()) {
      // Bind the new definitions in context
      // FIXME: Deal with Boolean declarations.
      int r_v;
      if(vd->e()) {
        CG::Binding b_v(CG::bind(vd->e(), cg, frag));
        r_v = b_v.first;
        conj.push_back(b_v.second);
      } else {
        // Variable declaration. Assumes is total and nonempty.
        CG::Binding b_d(bind_domain(vd, cg, frag));
        if(!vd->ti()->isarray()) {
          conj.push_back(b_d.second);
          OPEN_OTHER(cg, frag);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(b_d.first));
          CLOSE_AGG(cg, frag);
        } else {
          std::vector<int> r_regs;
          for(Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            CG::Binding b_reg(CG::bind(dim, cg, frag));
            // post_cond(cg, frag, b_reg.second);
            r_regs.push_back(b_reg.first);
          }
          std::vector<Forset> nesting;
          OPEN_VEC(cg, frag);
          for(int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(frag);
          }
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(b_d.first));
          for(int r_i = r_regs.size()-1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(frag);
          }
          CLOSE_AGG(cg, frag);
        }
        r_v = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));
      }
      // cg.env().bind(vd->id()->v(), Loc::reg(r_v));
      cg.env().bind(vd->id()->v(), CodeGen::Binding(r_v, CG_Cond::ttt()));
    } else {
      conj.push_back(CG::compile(e, cg, frag));
    }
  }
  conj.push_back(CG::compile(let->in(), cg, frag));
  return CG_Cond::forall(ctx, conj);
}
CG_Cond::T CG::compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> conj;

  CG::Binding b_A(CG::bind(a, cg, frag));
  int r_A(b_A.first);
  conj.push_back(b_A.second);

  std::vector<int> r_idx;
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b_x(CG::bind(idx[ii], cg, frag)); 
    r_idx.push_back(b_x.first);
    conj.push_back(b_x.second);
  }
  assert(sz == 1);
  if(idx[0]->type().ispar()) {
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_idx[0]), CG::r(r));
    conj.push_back(CG_Cond::reg(r));
  } else {
    GCLock lock;
    auto fun = find_call_fun(cg, {"element"}, Type::varbool(), {Type::varbool(1), Type::varint()}, ctx);
    assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(fun.second));
    conj.push_back(CG_Cond::call(fun.first, fun.second, CG::r(r_idx[0]), CG::r(r_A)));
  }
  return CG_Cond::forall(ctx, conj);
}

};
