#include <minizinc/astexception.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/codegen.hh>
#include <minizinc/copy.hh>
#include <minizinc/eval_par.hh>
#include <minizinc/flatten.hh>
#include <minizinc/hash.hh>
#include <minizinc/interpreter.hh>
#include <minizinc/optimize.hh>
#include <minizinc/output.hh>
#include <minizinc/prettyprinter.hh>

#include <../lib/codegen/analysis.hpp>
#include <../lib/codegen/codegen_internal.hpp>
#include <functional>
#include <set>

namespace MiniZinc {

// Known limitations:
// - Comprehensions and generator expressions are assumed to be total, so are evaluated in root
// context. Expression evaluation currently runs in two modes:
// - eval, which places the result onto the value stack on the enclosing context, or
// - locate, which places the result into a register, and returns the register.
// this is done to avoid a unnecessary push/pop sequences, when a result is already bound to
// a register, and is needed for e.g. a call.

#define ENABLE_PLUS 1
// MZNC_COLLECT_LEAVES should generate fewer aggregation contexts for models with par-resolved
// partiality (i.e. lots of array accesses)
#define MZNC_COLLECT_LEAVES

using Mode = CG::Mode;

struct builtin_t {
  std::function<CG_Cond::T(Call*, Mode, CodeGen&, CG_Builder&)> boolean;
  std::function<CG::Binding(Call*, Mode, CodeGen&, CG_Builder&)> general;
};

typedef std::unordered_map<std::string, builtin_t> builtin_table;

const char* instr_names[] = {
    "ADDI",
    "SUBI",
    "MULI",
    "DIVI",
    "MODI",
    "INCI",
    "DECI",

    "IMMI",
    "CLEAR",
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
    "GET_ARRAY",

    "LB",
    "UB",
    "DOM",

    "MAKE_SET",
    "INTERSECTION",
    "UNION",
    "DIFF",

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

    "ITER_ARRAY",
    "ITER_VEC",
    "ITER_RANGE",
    "ITER_NEXT",
    "ITER_BREAK",

    "TRACE",
    "ABORT",
};

const char* mode_names[] = {
    "ROOT", "ROOT_NEG", "FUN", "FUN_NEG", "IMP", "IMP_NEG", "MAX_MODE",
};

const char* agg_names[] = {"AND", "OR", "VEC", "OTHER"};
const char* instr_name(BytecodeStream::Instr i) { return instr_names[i]; }

const char* agg_name(AggregationCtx::Symbol s) { return agg_names[s]; }
const char* mode_name(BytecodeProc::Mode m) { return mode_names[m]; }

inline void TODO(void) { throw InternalError("Not yet implemented!"); }

void CodeGen::register_builtins(void) {
  // Solver Built-ins
  register_builtin("mk_intvar", 1);

  // Constants
  register_builtin("absent", 1);
  register_builtin("infinity", 1);
  register_builtin("boolean_domain", 0);
  register_builtin("infinite_domain", 0);

  // Interpreter Built-ins
  register_builtin("uniform", 2);
  register_builtin("sol", 1);
  register_builtin("sort_by", 2);
  register_builtin("floor", 1);
  register_builtin("ceil", 1);
  register_builtin("slice_Xd", 3);
  register_builtin("array_Xd", 2);
  register_builtin("index_set", 2);
  register_builtin("internal_sort", 1);
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
  if (cg.current_reg_count < old_reg_count) {
    PUSH_INSTR(frag, BytecodeStream::CLEAR, CG::r(cg.current_reg_count), CG::r(old_reg_count - 1));
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  cg.env_pop();
}

void OPEN_AND(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_AND); }
void OPEN_OR(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OR); }
void OPEN_OTHER(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OTHER); }
void OPEN_VEC(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_VEC); }

CG_ProcID CodeGen::register_builtin(std::string s, unsigned int arity) {
  auto it(_proc_map.find(s));
  if (it != _proc_map.end()) {
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

void bind_binop_var(CodeGen& cg, CG_Builder& frag, Mode ctx, BinOpType op, int r_lhs, int r_rhs) {
  GCLock lock;
  switch (op) {
    // Actual builtins
    case BOT_PLUS: {
      auto fun =
          find_call_fun(cg, {"op_plus"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_MINUS: {
      auto fun =
          find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_MULT: {
      auto fun =
          find_call_fun(cg, {"op_times"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_IDIV: {
      auto fun = find_call_fun(cg, {"op_int_division"}, Type::varint(),
                               {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_DIV: {
      auto fun = find_call_fun(cg, {"op_float_division"}, Type::varint(),
                               {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_MOD: {
      auto fun =
          find_call_fun(cg, {"op_modulus"}, Type::varint(), {Type::varint(), Type::varint()}, ctx);
      assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_lhs), CG::r(r_rhs));
      return;
    }
    case BOT_DOTDOT: {
      // The values in r_lhs and r_rhs had better be IMMIs.
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      return;
    }
    case BOT_PLUSPLUS: {
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
      return;
    }
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in bind_binop_var.");
}

int bind_binop_par_int(CodeGen& cg, CG_Builder& frag, BinOpType op, int r_lhs, int r_rhs) {
  int r;
  switch (op) {
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
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    case BOT_IN: {
      int l_head(GET_LABEL(cg));
      int l_stop(GET_LABEL(cg));
      int l_exit(GET_LABEL(cg));
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::ITER_VEC, CG::r(r_rhs), CG::l(l_exit));
      PUSH_LABEL(frag, l_head);
      PUSH_INSTR(frag, BytecodeStream::ITER_NEXT, CG::r(r));
      // Start of interval.
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r), CG::r(r_lhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_stop));
      // End of interval
      PUSH_INSTR(frag, BytecodeStream::ITER_NEXT, CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_lhs), CG::r(r), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r), CG::l(l_stop));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_head));
      PUSH_LABEL(frag, l_stop);
      PUSH_INSTR(frag, BytecodeStream::ITER_BREAK, CG::i(1));
      PUSH_LABEL(frag, l_exit);
      return r;
    }
    case BOT_PLUSPLUS: {
      r = GET_REG(cg);
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
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    }
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in bind_binop_par.");
}

int bind_binop_par_set(CodeGen& cg, CG_Builder& frag, BinOpType op, int r_lhs, int r_rhs) {
  int r;
  switch (op) {
    // Actual builtins
    case BOT_EQ:
    case BOT_EQUIV: {
      r = GET_REG(cg);
      int r_tmp = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::ISEMPTY, CG::r(r), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r_rhs), CG::r(r_lhs), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::ISEMPTY, CG::r(r_tmp), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::AND, CG::r(r), CG::r(r_tmp), CG::r(r));
      return r;
    }
    case BOT_NQ: {
      r = bind_binop_par_set(cg, frag, BOT_EQ, r_lhs, r_rhs);
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
      return r;
    }
    case BOT_LE:
      r = GET_REG(cg);
      TODO();
      return r;
    case BOT_LQ:
      r = GET_REG(cg);
      TODO();
      return r;
    case BOT_GR:
      r = GET_REG(cg);
      TODO();
      return r;
    case BOT_GQ:
      r = GET_REG(cg);
      TODO();
      return r;
    case BOT_UNION:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_INTERSECT:
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::INTERSECTION, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    case BOT_SUBSET: {
      // (L subset R) <-> ((L intersect R) == L)
      r = GET_REG(cg);
      int l_fin(GET_LABEL(cg));
      int r_inter(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::INTERSECTION, CG::r(r_lhs), CG::r(r_rhs), CG::r(r_inter));

      int r_sz(GET_REG(cg));
      int r_tmp(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_lhs), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(r_inter), CG::r(r_sz));
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_tmp), CG::r(r_sz), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_fin));

      int r_idx(GET_REG(cg));
      int l_loop(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_idx));
      PUSH_LABEL(frag, l_loop);
      PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_idx), CG::r(r_sz), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_tmp), CG::l(l_fin));

      int r_tmp2(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_lhs), CG::r(r_idx), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_inter), CG::r(r_idx), CG::r(r_tmp2));
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_tmp), CG::r(r_tmp2), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_fin));

      PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_idx));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_loop));

      PUSH_LABEL(frag, l_fin);
      return r;
    }
    case BOT_SUPERSET: {
      return bind_binop_par_set(cg, frag, BOT_SUBSET, r_rhs, r_lhs);
    }
    case BOT_DIFF: {
      int r(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      return r;
    }
    case BOT_SYMDIFF: {
      int r(GET_REG(cg));
      int r_tmp(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r_rhs), CG::r(r_lhs), CG::r(r_tmp));
      PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r), CG::r(r_tmp), CG::r(r));
      return r;
    }
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in bind_binop_par_set.");
}

CG_Cond::T linear_cond(CodeGen& cg, CG_Builder& frag, BinOpType op, Mode ctx, int r_lhs,
                       int r_rhs) {
  GCLock lock;
  switch (op) {
    // Actual builtins
    case BOT_LQ:
    case BOT_LE: {
      // Check bounds
      int lb_lhs = GET_REG(cg);
      int ub_rhs = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_lhs), CG::r(lb_lhs));
      PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_rhs), CG::r(ub_rhs));
      PUSH_INSTR(frag, (op == BOT_LE ? BytecodeStream::LTI : BytecodeStream::LEI), CG::r(lb_lhs),
                 CG::r(ub_rhs), CG::r(lb_lhs));
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      int k = GET_REG(cg);
      int z = (op == BOT_LE ? +1 : 0);
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(r_lhs), CG::r(r_rhs), CG::i(z), CG::r(c),
                 CG::r(x), CG::r(k));
      auto linear =
          CG_Cond::call({"pre_int_lin_le"}, ctx, true,
                        {Type::varbool(), Type::parint(1), Type::varint(1), Type::parint()},
                        {CG::r(c), CG::r(x), CG::r(k)});
      return CG_Cond::forall(ctx, CG_Cond::reg(lb_lhs, true), linear);
    }
    case BOT_EQUIV:
    case BOT_EQ: {
      // Check domain
      int dom_lhs = GET_REG(cg);
      int dom_rhs = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::DOM, CG::r(r_lhs), CG::r(dom_lhs));
      PUSH_INSTR(frag, BytecodeStream::DOM, CG::r(r_rhs), CG::r(dom_rhs));
      PUSH_INSTR(frag, BytecodeStream::INTERSECTION, CG::r(dom_lhs), CG::r(dom_rhs),
                 CG::r(dom_lhs));
      PUSH_INSTR(frag, BytecodeStream::ISEMPTY, CG::r(dom_lhs), CG::r(dom_rhs));
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(dom_rhs), CG::r(dom_rhs));
      // Create linear equation
      int k = GET_REG(cg);
      int c = GET_REG(cg);
      int x = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::SIMPLIFY_LIN, CG::r(r_lhs), CG::r(r_rhs), CG::i(0), CG::r(c),
                 CG::r(x), CG::r(k));
      auto linear =
          CG_Cond::call({"pre_int_lin_eq"}, ctx, true,
                        {Type::varbool(), Type::parint(1), Type::varint(1), Type::parint()},
                        {CG::r(c), CG::r(x), CG::r(k)});
      return CG_Cond::forall(ctx, CG_Cond::reg(dom_rhs, true), linear);
    }
    case BOT_NQ:
      return ~linear_cond(cg, frag, BOT_EQ, -ctx, r_lhs, r_rhs);
    case BOT_GR:
      return linear_cond(cg, frag, BOT_LE, ctx, r_rhs, r_lhs);
    case BOT_GQ:
      return linear_cond(cg, frag, BOT_LQ, ctx, r_rhs, r_lhs);
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in linear_cond.");
}

CG_ProcID CodeGen::resolve_fun(FunctionI* fun, bool reserved_name) {
  auto it(fun_bodies.find(fun));
  if (it != fun_bodies.end()) return it->second;

  GCLock lock;

  int p_idx = bytecode.size();
  CG_ProcID p_id(CG_ProcID::proc(p_idx));
  ASTExprVec<VarDecl> params(fun->params());

  std::stringstream ss;
  if (!reserved_name && fun->e()) {
    ss << "f_" << fun->id().str();
    for (auto& param : params) {
      ss << "_";
      if (param->type().dim() > 0) {
        ss << "d" << param->type().dim();
      } else if (param->type().dim() < 0) {
        ss << "dT";
      }
      if (param->type().isvar()) {
        ss << "v";
      }
      if (param->type().is_set()) {
        ss << "s";
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

std::tuple<CG_ProcID, BytecodeProc::Mode, bool> find_call_fun(CodeGen& cg, const ASTString& ident,
                                                              const Type& ret_type,
                                                              std::vector<Type> arg_types,
                                                              BytecodeProc::Mode m,
                                                              bool reserved_name) {
  for (auto& arg_type : arg_types) {
    arg_type.ti(Type::TI_PAR);
  }
  int sz = arg_types.size();
  CallSig sig(ident, arg_types);
  auto it(cg.dispatch.find(sig));

  BytecodeProc::Mode call_mode(ret_type.isbool() ? m : BytecodeProc::FUN);
  BytecodeProc::Mode def_mode(ret_type.isbool() ? m : BytecodeProc::ROOT);

  if (it != cg.dispatch.end()) {
    auto d_proc(it->second);
    if (d_proc.first.is_builtin() || cg.bytecode[d_proc.first.id()].is_available(call_mode)) {
      return {d_proc.first, call_mode, d_proc.second};
    }
  }

  GCLock lock;
  auto bodies = std::move(cg.fun_map.get_bodies(ident, arg_types));
  assert(!bodies.empty());
  auto neg_bodies = std::move(cg.fun_map.get_bodies(ASTString(ident.str() + "_neg"), arg_types));

  if (ret_type.isbool() && call_mode != BytecodeProc::ROOT) {
    bool valid = false;
    if (cg.fun_map.defines_mode(ident, arg_types, call_mode).first) {
      valid = true;
    } else if (BytecodeProc::is_neg(m) && std::any_of(neg_bodies.begin(), neg_bodies.end(),
                                                      [](FunctionI* fi) { return fi->e(); })) {
      valid = true;
    } else {
      valid = cg.fun_map.defines_mode(ident, arg_types, BytecodeProc::FUN).first;
      if (valid) {
        call_mode = BytecodeProc::FUN;
        def_mode = BytecodeProc::FUN;
      }
    }
    if (!valid &&
        std::none_of(bodies.begin(), bodies.end(), [](FunctionI* fi) { return fi->e(); })) {
      throw InternalError(ident.str() +
                          " is used in a reified context, but no reification is available.");
    }
  }

  std::vector<CG_ProcID> procs;
  for (FunctionI* b : bodies) {
    CG_ProcID body(cg.resolve_fun(b, reserved_name && bodies.size() == 1));
    // Force the body to be created
    procs.push_back(body);
    if (!cg.bytecode[body.id()].is_available(call_mode)) {
      cg.bytecode[body.id()].body(call_mode);
      cg.pending_bodies.emplace_back(b, std::make_pair(call_mode, def_mode));
    }
  }

  // If there's a unique candidate, go for it.
  if (procs.size() == 1) {
    if (it == cg.dispatch.end()) {
      cg.dispatch.insert(std::make_pair(sig, std::make_pair(procs[0], false)));
    }
    return {procs[0], call_mode, false};
  }

  CG_ProcID d_proc(CG_ProcID::builtin(0));
  if (it != cg.dispatch.end()) {
    d_proc = it->second.first;
  } else {
    // Otherwise, generate the dispatch function.
    int p_idx = cg.bytecode.size();

    std::stringstream ss;
    if (reserved_name) {
      ss << ident.str();
    } else {
      ss << "d_" << ident.str();
      for (auto& type : arg_types) {
        ss << "_";
        if (type.dim() > 0) {
          ss << "d" << type.dim();
        } else if (type.dim() < 0) {
          ss << "dT";
        }
        if (type.is_set()) {
          ss << "s";
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
    }

    d_proc = CG_ProcID::proc(p_idx);
    cg.bytecode.emplace_back(ss.str(), arg_types.size());
    cg.dispatch.insert(std::make_pair(sig, std::make_pair(d_proc, true)));
  }

  // Now generate the dispatch body.
  CG_Builder frag;
  std::vector<uint64_t> var_sig(sz);
  std::vector<uint64_t> def_sig(bodies.size());
  for (int bi = 0; bi < bodies.size(); ++bi) {
    ASTExprVec<VarDecl> params(bodies[bi]->params());
    for (int ii = 0; ii < params.size(); ++ii) {
      if (params[ii]->type().isvar()) {
        var_sig[ii] |= 1ull << bi;
        def_sig[bi] |= 1ull << ii;
      }
    }
  }

  std::vector<std::unordered_map<uint64_t, int>> sig_table(sz + 1);
  std::vector<dispatch_node> nodes;

  nodes.push_back(dispatch_node{-1, 0, (1ull << bodies.size()) - 1});

  for (int ii = 0; ii < nodes.size(); ii++) {
    dispatch_node d(nodes[ii]);
    if (d.label != -1) PUSH_LABEL(frag, d.label);
    if (!d.sig) {
      PUSH_INSTR(frag, BytecodeStream::ABORT);
    } else if (d.level == sz) {
      // Find the best candidate, and emit a call.
      uint64_t candidates(d.sig);
      unsigned int best(find_lsb(candidates));
      uint64_t best_sig(def_sig[best]);
      candidates ^= 1ull << best;
      while (candidates) {
        unsigned int curr(find_lsb(candidates));
        candidates ^= 1ull << curr;
        uint64_t sig(def_sig[curr]);
        if (!(sig & ~best_sig)) {
          // At least as good as the incumbent
          best = curr;
          best_sig = sig;
        }
      }
      CG_ProcID p_id(procs[best]);
      PUSH_INSTR(frag, BytecodeStream::TCALL, call_mode, p_id,
                 CG::i(bodies[best]->ti()->type().isvar()));
    } else {
      int par_label;
      auto p_it(sig_table[d.level + 1].find(d.sig));
      if (p_it != sig_table[d.level + 1].end()) {
        par_label = p_it->second;
      } else {
        par_label = GET_LABEL(cg);
        // int idx = nodes.size();
        sig_table[d.level + 1].insert(std::make_pair(d.sig, par_label));
        nodes.push_back(dispatch_node{par_label, d.level + 1, d.sig});
      }
      uint64_t v_sig(d.sig & var_sig[d.level]);
      if (v_sig == d.sig) {
        PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(par_label));
      } else {
        int var_label;
        auto v_it(sig_table[d.level + 1].find(v_sig));
        if (v_it != sig_table[d.level + 1].end()) {
          var_label = v_it->second;
        } else {
          var_label = GET_LABEL(cg);
          // int idx = nodes.size();
          sig_table[d.level + 1].insert(std::make_pair(v_sig, var_label));
          nodes.push_back(dispatch_node{var_label, d.level + 1, v_sig});
        }

        PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(d.level), CG::r(sz));
        PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(sz), CG::l(par_label));
        PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(var_label));
      }
    }
  }

  cg.append(d_proc.id(), call_mode, frag);

  return {d_proc, call_mode, false};
}

std::tuple<CG_ProcID, BytecodeProc::Mode, bool> find_call_fun(CodeGen& cg, Call* call,
                                                              BytecodeProc::Mode m,
                                                              bool reserved_name) {
  std::vector<Type> arg_types;
  int sz = call->n_args();
  for (int ii = 0; ii < sz; ++ii) {
    Type t(call->arg(ii)->type());
    arg_types.push_back(t);
  }
  return find_call_fun(cg, call->id(), call->type(), arg_types, m, reserved_name);
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
  if (it != _exp_scope.end()) return (*it).second;

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
      for (int ii = 0; ii < sz; ++ii) {
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
      for (int ii = 0; ii < sz; ++ii) {
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
      for (int g = sz - 1; g >= 0; --g) {
        ASTStSet r_in(scope(c->in(g)));

        if (c->where(g)) {
          ASTStSet r_where(scope(c->where(g)));
          r.insert(r_where.begin(), r_where.end());
          r.insert(r_in.begin(), r_in.end());
        }

        // Now remove the variables that were bound.
        for (int d = 0; d < c->n_decls(g); ++d) {
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
      for (int ii = 0; ii < sz; ++ii) {
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
      for (int ii = 0; ii < sz; ++ii) {
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
      for (int ii = sz - 1; ii >= 0; --ii) {
        if (VarDecl* vd = bindings[ii]->dyn_cast<VarDecl>()) {
          r.erase(vd->id()->str());
          if (vd->e()) {
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

CodeGen::Binding CodeGen::cache_lookup(Expression* e) { return env().cache_lookup(e, scope(e)); }
void CodeGen::cache_store(Expression* e, CodeGen::Binding l) { env().cache_store(e, scope(e), l); }

void _debugcond(CG_Cond::T c) {
  CG_Cond::_T* p(c.get());
  if (c.sign()) std::cerr << "~";
  if (!p) {
    std::cerr << "T";
  } else {
    switch (p->kind()) {
      case CG_Cond::CC_Reg:
        std::cerr << "R" << p->reg[0].reg;
        break;
      case CG_Cond::CC_Call:
        std::cerr << "<Call>";
        break;
      case CG_Cond::CC_And:
        std::cerr << "(and";
        for (CG_Cond::T child : static_cast<CG_Cond::C_And*>(p)->children) {
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
  if (cg.env().cache_lookup_cst(x, b)) return b.first;
  int r = GET_REG(cg);
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
  cg.env().cache_store_cst(x, std::make_pair(r, CG_Cond::ttt()));
  return r;
}

// WARNING ON THE USE OF CG_Conds: register caching assumes that CG_Conds dont escape an
// aggregation context, and will be available on all paths. If the cond is forced conditionally
// (e.g. shortcutting), cg.push_env() should be called before the force() call, and cg.pop_env()
// called where control flow rejoins.

// Given CG_Cond cond, collect the disjuncts having positive or negative values.
// Returns false if the conjunction is a contradiction.
bool collect_prod(std::vector<int>& pos, std::vector<int>& neg,
                  std::vector<CG_Cond::C_And*>& delayed, CG_Cond::T cond, CodeGen& cg,
                  CG_Builder& frag) {
  assert(cond.get());
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());

  if (p->reg[1 - sign].is_seen || p->reg[1 - sign].is_root) return false;
  if (p->reg[sign].is_seen || p->reg[sign].is_root) return true;
  p->reg[sign].is_seen = true;

  if (p->reg[sign].has_reg()) {
    pos.push_back(p->reg[sign].reg);
  } else if (p->reg[1 - sign].has_reg()) {
    neg.push_back(p->reg[1 - sign].reg);
  } else if (p->kind() == CG_Cond::CC_And && !sign) {
    // Recursively collect the children.
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      if (!collect_prod(pos, neg, delayed, c, cg, frag)) return false;
    }
  } else if (p->kind() == CG_Cond::CC_And) {
    // How do we decide which way to compile the remaining conditions?
    delayed.push_back(static_cast<CG_Cond::C_And*>(p));
  } else {
    assert(p->kind() == CG_Cond::CC_Call);
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    std::vector<Type> ty(call->ty.begin() + 1, call->ty.end());
    auto fun = find_call_fun(cg, call->ident, call->ty[0], ty, call->m);
    PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
               CG::i((!std::get<2>(fun)) && call->cse), call->params);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    if (BytecodeProc::is_neg(call->m) != BytecodeProc::is_neg(std::get<1>(fun))) {
      if (call->ty[0].ispar()) {
        PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
      } else {
        auto fun =
            find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
        assert(std::get<1>(fun) == BytecodeProc::FUN);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                   CG::i(!std::get<2>(fun)), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      }
    }
    if (!sign)
      pos.push_back(r);
    else
      neg.push_back(r);
  }
  return true;
}

// Given CG_Cond cond, collect the disjuncts having positive or negative values.
// Returns true if the disjunction is a tautology.
bool collect_disj(std::vector<int>& pos, std::vector<int>& neg,
                  std::vector<CG_Cond::C_And*>& delayed, CG_Cond::T cond, CodeGen& cg,
                  CG_Builder& frag) {
  assert(cond.get());
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());

  if (p->reg[1 - sign].is_seen || p->reg[sign].is_root) return true;
  if (p->reg[sign].is_seen || p->reg[1 - sign].is_root) return false;
  p->reg[sign].is_seen = true;

  if (p->reg[sign].has_reg()) {
    pos.push_back(p->reg[sign].reg);
  } else if (p->reg[1 - sign].has_reg()) {
    neg.push_back(p->reg[1 - sign].reg);
  } else if (p->kind() == CG_Cond::CC_And && sign) {
    // Recursively collect the children.
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      if (collect_disj(pos, neg, delayed, ~c, cg, frag)) return true;
    }
  } else if (p->kind() == CG_Cond::CC_And) {
    delayed.push_back(static_cast<CG_Cond::C_And*>(p));
  } else {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    std::vector<Type> ty(call->ty.begin() + 1, call->ty.end());
    auto fun = find_call_fun(cg, call->ident, call->ty[0], ty, call->m);
    PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
               CG::i((!std::get<2>(fun)) && call->cse), call->params);
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    if (BytecodeProc::is_neg(call->m) != BytecodeProc::is_neg(std::get<1>(fun))) {
      if (call->ty[0].ispar()) {
        PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
      } else {
        auto fun =
            find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
        assert(std::get<1>(fun) == BytecodeProc::FUN);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                   CG::i(!std::get<2>(fun)), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      }
    }
    if (!sign)
      pos.push_back(r);
    else
      neg.push_back(r);
  }
  return false;
}

std::pair<int, bool> _force_cond(CG_Cond::T cond, CodeGen& cg, CG_Builder& frag);

void collect_and_leaves(std::vector<CG_Cond::T>& par_leaves, std::vector<CG_Cond::T>& var_leaves,
                        CG_Cond::T child, CodeGen& cg, CG_Builder& frag) {
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  auto push = [&](CG_Cond::T r, bool is_par) {
    if (is_par) {
      par_leaves.push_back(r);
    } else {
      var_leaves.push_back(r);
    }
  };

  if (p->reg[sign].has_reg()) {
    push(child, p->reg[sign].is_par);
    return;
  }
  if (p->reg[1 - sign].has_reg()) {
    push(child, p->reg[1 - sign].is_par);
  } else if (p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      collect_and_leaves(par_leaves, var_leaves, c, cg, frag);
    }
  } else {
    push(child, /* Check if this is par. */ false);
  }
}
void collect_or_leaves(std::vector<CG_Cond::T>& par_leaves, std::vector<CG_Cond::T>& var_leaves,
                       CG_Cond::T child, CodeGen& cg, CG_Builder& frag) {
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  auto push = [&](CG_Cond::T r, bool is_par) {
    if (is_par) {
      par_leaves.push_back(r);
    } else {
      var_leaves.push_back(r);
    }
  };

  if (p->reg[1 - sign].has_reg()) {
    push(~child, p->reg[1 - sign].is_par);
    return;
  }
  if (p->reg[sign].has_reg()) {
    push(~child, p->reg[sign].is_par);
  } else if (p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      collect_or_leaves(par_leaves, var_leaves, c, cg, frag);
    }
  } else {
    push(~child, /* Check if this is par. */ false);
  }
}

void force_and_leaves(std::vector<int>& var_leaves, std::vector<int>& par_leaves, CG_Cond::T child,
                      CodeGen& cg, CG_Builder& frag) {
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  auto push = [&](int r, bool is_par) {
    if (is_par) {
      par_leaves.push_back(r);
    } else {
      var_leaves.push_back(r);
    }
  };
  if (p->reg[sign].has_reg()) {
    push(p->reg[sign].reg, p->reg[sign].is_par);
    return;
  }
  cg.env().record_cached_cond(child);
  if (p->reg[1 - sign].has_reg()) {
    // Create the negation
    int r(GET_REG(cg));
    if (p->reg[1 - sign].is_par) {
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(p->reg[1 - sign].reg), CG::r(r));
    } else {
      GCLock lock;
      auto fun =
          find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(p->reg[1 - sign].reg));
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    p->reg[sign] = CG_Cond::cond_reg(r, p->reg[1 - sign].is_par);
    push(p->reg[sign].reg, p->reg[sign].is_par);
  } else if (p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      force_and_leaves(var_leaves, par_leaves, c, cg, frag);
    }
  } else {
    auto forced = _force_cond(child, cg, frag);
    push(forced.first, forced.second);
  }
}

// Pushing the _negation_ of child.
void force_or_leaves(std::vector<int>& var_leaves, std::vector<int>& par_leaves, CG_Cond::T child,
                     CodeGen& cg, CG_Builder& frag) {
  assert(child.get());
  CG_Cond::_T* p(child.get());
  bool sign(child.sign());
  auto push = [&](int r, bool is_par) {
    if (is_par) {
      par_leaves.push_back(r);
    } else {
      var_leaves.push_back(r);
    }
  };
  if (p->reg[1 - sign].has_reg()) {
    push(p->reg[1 - sign].reg, p->reg[1 - sign].is_par);
    return;
  }
  cg.env().record_cached_cond(~child);
  if (p->reg[sign].has_reg()) {
    // Create the negation
    int r(GET_REG(cg));
    if (p->reg[sign].is_par) {
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(p->reg[sign].reg), CG::r(r));
    } else {
      GCLock lock;
      auto fun =
          find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(p->reg[sign].reg));
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    p->reg[1 - sign] = CG_Cond::cond_reg(r, p->reg[sign].is_par);
    push(p->reg[1 - sign].reg, p->reg[1 - sign].is_par);
  } else if (p->kind() == CG_Cond::CC_And && !sign) {
    std::vector<CG_Cond::T>& children(static_cast<CG_Cond::C_And*>(p)->children);
    for (CG_Cond::T c : children) {
      force_or_leaves(var_leaves, par_leaves, c, cg, frag);
    }
  } else {
    auto forced = _force_cond(~child, cg, frag);
    push(forced.first, forced.second);
  }
}

std::pair<int, bool> _force_cond(CG_Cond::T cond, CodeGen& cg, CG_Builder& frag) {
  CG_Cond::_T* p(cond.get());
  bool negated(cond.sign());
  if (!p) {
    // Either true or false.
    // return CG::locate_immi(1 - cond.sign(), cg, frag);
    return {bind_cst(1 - cond.sign(), cg, frag), true};
  }
  cg.env().record_cached_cond(cond);
  if (p->kind() == CG_Cond::CC_Reg) {
    throw InternalError("_force_cond called on value in register.");
  } else if (p->kind() == CG_Cond::CC_Call) {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    std::vector<Type> ty(call->ty.begin() + 1, call->ty.end());
    int r;
    Mode m(call->m);
    if (m.strength() != CG::Mode::Root) {
      Mode call_m(m.strength(), negated);
      auto fun = find_call_fun(cg, call->ident, call->ty[0], ty, call->m);
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i((!std::get<2>(fun)) && call->cse), call->params);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      if (BytecodeProc::is_neg(call->m) != BytecodeProc::is_neg(std::get<1>(fun))) {
        if (call->ty[0].ispar()) {
          PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
        } else {
          auto fun =
              find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
          assert(std::get<1>(fun) == BytecodeProc::FUN);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                     CG::i(!std::get<2>(fun)), CG::r(r));
          PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        }
      }
      return {r, call->ty[0].ispar()};
    }
    Mode call_m(m.strength(), negated);
    assert(m == call_m);
    auto fun = find_call_fun(cg, call->ident, call->ty[0], ty, call->m);
    if (call_m == std::get<1>(fun)) {
      PUSH_INSTR(frag, BytecodeStream::CALL, call_m, std::get<0>(fun),
                 CG::i((!std::get<2>(fun)) && call->cse), call->params);
    } else {
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      std::cerr << "Warning: emitting reification for " << call->ident
                << " because no ROOT_NEG version is available\n";
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), call->params);
      r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      if (BytecodeProc::is_neg(call->m) != BytecodeProc::is_neg(std::get<1>(fun))) {
        if (call->ty[0].ispar()) {
          PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
        } else {
          auto fun =
              find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
          assert(std::get<1>(fun) == BytecodeProc::FUN);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                     CG::i(!std::get<2>(fun)), CG::r(r));
          PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        }
      }
      PUSH_INSTR(frag, BytecodeStream::POST, CG::r(r));
    }
    return {bind_cst(1, cg, frag), true};
  } else if (p->kind() == CG_Cond::CC_And && !cond.sign()) {
#ifndef MZNC_COLLECT_LEAVES
    std::vector<int> var_leaves;
    std::vector<int> par_leaves;
    int r = GET_REG(cg);
    // I don't think these are necessary
    force_and_leaves(var_leaves, par_leaves, cond, cg, frag);
    if (var_leaves.empty()) {
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(par_leaves[0]), CG::r(r));
      for (int i = 1; i < par_leaves.size(); ++i) {
        PUSH_INSTR(frag, BytecodeStream::AND, CG::r(r), CG::r(par_leaves[i]), CG::r(r));
      }
    } else {
      OPEN_AND(cg, frag);
      for (int r_c : par_leaves) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      for (int r_c : var_leaves) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    return {r, var_leaves.empty()};
#else
    std::vector<CG_Cond::T> par_leaves;
    std::vector<CG_Cond::T> var_leaves;
    collect_and_leaves(par_leaves, var_leaves, cond, cg, frag);

    // If we had a conjunction, there should be at least two leaves.
    assert(var_leaves.size() + par_leaves.size() > 1);

    // If there is at least one par child, we're going to do shortcutting
    // -- so we need a jump, and to push a fresh env.
    int r = GET_REG(cg);
    int lblE = 0xdeadbeef;
    if (par_leaves.size() > 0) {
      lblE = GET_LABEL(cg);
      cg.env_push();

      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
      for (int ii = 0; ii < par_leaves.size() - 1; ii++) {
        CG_Cond::T c(par_leaves[ii]);
        int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
        PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_c), CG::l(lblE));
      }
      CG_Cond::T c(par_leaves.back());
      int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
      if (var_leaves.size() > 0) {
        PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_c), CG::l(lblE));
      } else {
        PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_c), CG::r(r));
      }
    }
    // Excludes the 0 case, because it's just been handled.
    if (var_leaves.size() == 1) {
      // Exactly one var. Don't open a context;
      // we return either the result register
      // or the forced cond.
      int r_c = CG::force(var_leaves[0], BytecodeProc::FUN, cg, frag);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_c), CG::r(r));
    } else if (var_leaves.size() > 1) {
      // In this case, we need to open a context.
      OPEN_AND(cg, frag);
      for (CG_Cond::T c : var_leaves) {
        int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    if (par_leaves.size() > 0) {  // We did some shortcutting.
      PUSH_LABEL(frag, lblE);
      cg.env_pop();
    }
    return {r, var_leaves.empty()};
#endif
  } else {
    assert(p->kind() == CG_Cond::CC_And && cond.sign());
#ifndef MZNC_COLLECT_LEAVES
    std::vector<int> var_leaves;
    std::vector<int> par_leaves;
    int r = GET_REG(cg);
    // I don't think these are necessary
    force_or_leaves(var_leaves, par_leaves, ~cond, cg, frag);
    if (var_leaves.empty()) {
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(par_leaves[0]), CG::r(r));
      for (int i = 1; i < par_leaves.size(); ++i) {
        PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r), CG::r(par_leaves[i]), CG::r(r));
      }
    } else {
      OPEN_OR(cg, frag);
      for (int r_c : par_leaves) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      for (int r_c : var_leaves) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    return {r, var_leaves.empty()};
#else
    std::vector<CG_Cond::T> par_leaves;
    std::vector<CG_Cond::T> var_leaves;
    collect_or_leaves(par_leaves, var_leaves, ~cond, cg, frag);

    // If we had a conjunction, there should be at least two leaves.
    assert(var_leaves.size() + par_leaves.size() > 1);

    // If there is at least one par child, we're going to do shortcutting
    // -- so we need a jump, and to push a fresh env.
    int r = GET_REG(cg);
    int lblE = 0xdeadbeef;
    if (par_leaves.size() > 0) {
      lblE = GET_LABEL(cg);
      cg.env_push();
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r));

      for (int ii = 0; ii < par_leaves.size() - 1; ii++) {
        CG_Cond::T c(par_leaves[ii]);
        int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
        PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_c), CG::l(lblE));
      }
      CG_Cond::T c(par_leaves.back());
      int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
      if (var_leaves.size() > 0) {
        PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_c), CG::l(lblE));
      } else {
        PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_c), CG::r(r));
      }
    }
    // Excludes the 0 case, because it's just been handled.
    if (var_leaves.size() == 1) {
      // Exactly one var. Don't open a context;
      // we return either the result register
      // or the forced cond.
      int r_c = CG::force(var_leaves[0], BytecodeProc::FUN, cg, frag);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_c), CG::r(r));
    } else if (var_leaves.size() > 1) {
      // In this case, we need to open a context.
      OPEN_OR(cg, frag);
      for (CG_Cond::T c : var_leaves) {
        int r_c = CG::force(c, BytecodeProc::FUN, cg, frag);
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    if (par_leaves.size() > 0) {  // We did some shortcutting.
      PUSH_LABEL(frag, lblE);
      cg.env_pop();
    }
    return {r, var_leaves.empty()};
#endif
  }
}
int CG::force(CG_Cond::T _cond, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  // Negate condition if forced in negated context
  CG_Cond::T cond = ctx.is_neg() ? ~_cond : _cond;
  // Check if the condition is already forced.
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  if (!p) {
    return bind_cst(!sign, cg, frag);
  }
  if (p->reg[sign].has_reg()) {
    return p->reg[sign].reg;
  }
  if (p->reg[1 - sign].has_reg()) {
    // Emit the negation
    int r(GET_REG(cg));
    if (p->reg[1 - sign].is_par) {
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(p->reg[1 - sign].reg), CG::r(r));
    } else {
      auto fun =
          find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(p->reg[1 - sign].reg));
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    p->reg[sign] = CG_Cond::cond_reg(r, p->reg[1 - sign].is_par);
    return r;
  }
  auto forced = _force_cond(cond, cg, frag);
  p->reg[sign] = CG_Cond::cond_reg(forced.first, forced.second);
  return forced.first;
}

CG::Binding CG::force_or_bind(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if (e->type().isbool()) {
    return {CG::force(CG::compile(e, cg, frag), ctx, cg, frag), CG_Cond::ttt()};
  }
  return CG::bind(e, cg, frag);
}

int CG::force_or_bind(Expression* e, Mode ctx, std::vector<CG_Cond::T>& cond, CodeGen& cg,
                      CG_Builder& frag) {
  if (e->type().isbool()) {
    return CG::force(CG::compile(e, cg, frag), ctx, cg, frag);
  } else {
    CG::Binding b(CG::bind(e, cg, frag));
    if (b.second.get()) {
      cond.push_back(b.second);
    }
    return b.first;
  }
}

class EnvInit : public ItemVisitor {
private:
  friend class ItemIter<EnvInit>;

  EnvInit(CodeGen& _cg, Model& _m) : cg(_cg), m(_m) {}

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if (!vd->type().isann()) {
      if (vd->type().ispar()) {
        if (!vd->e()) {
          // cg.env().bind(vd->id()->v(), Loc::global(cg.num_globals));
          int g = cg.add_global(vd, true);
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
  void vConstraintI(ConstraintI* c) { modes.use(c->e(), BytecodeProc::ROOT); }
  /// Visit assign item
  void vAssignI(AssignI* ass) {}

  void vFunctionI(FunctionI* f) { cg.register_function(f); }

  void vSolveI(SolveI* si) {
    GCLock lock;
    if (si->st() == SolveI::ST_SAT) {
      return;
    }
    ASTString ident("solve_this");
    int mode;
    Expression* objective;
    switch (si->st()) {
      case SolveI::ST_SAT:
        mode = 0;
        objective = IntLit::a(0);
        break;
      case SolveI::ST_MIN:
        mode = 1;
        objective = si->e();
        break;
      case SolveI::ST_MAX:
        mode = 2;
        objective = si->e();
        break;
    }
    Expression* search_a;
    int search_var = 0;
    int search_val = 0;
    Call* c;
    if (Call* ann = si->ann().getCall(ASTString("int_search"))) {
      search_a = ann->arg(0);
      Id* varsel = ann->arg(1)->cast<Id>();
      if (varsel->idn() == -1 && varsel->v() == ASTString("input_order")) {
        search_var = 1;
      } else if (varsel->idn() == -1 && varsel->v() == ASTString("first_fail")) {
        search_var = 2;
      }
      Id* valsel = ann->arg(2)->cast<Id>();
      if (valsel->idn() == -1 && valsel->v() == ASTString("indomain_min")) {
        search_val = 1;
      } else if (valsel->idn() == -1 && valsel->v() == ASTString("indomain_max")) {
        search_val = 2;
      }
      c = new Call(
          si->loc(), ident,
          {IntLit::a(mode), objective, search_a, IntLit::a(search_var), IntLit::a(search_val)});
    } else {
      c = new Call(si->loc(), ident, {IntLit::a(mode), objective});
    }
    c->type(Type::varbool());
    m.addItem(new ConstraintI(Location().introduce(), c));
  }

  CodeGen& cg;
  Model& m;
  ModeAnalysis modes;

public:
  static void run(CodeGen& cg, Model* m) {
    EnvInit eb(cg, *m);
    iterItems(eb, m);
    cg.mode_map = std::move(eb.modes.extract());
  }
};

struct ShowVal {
  ShowVal(CodeGen& _cg, CG_Value _v) : cg(_cg), v(_v) {}

  CodeGen& cg;
  CG_Value v;
};
std::ostream& operator<<(std::ostream& o, ShowVal s) {
  switch (s.v.kind) {
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

template <class O>
void show_frag(O& out, CodeGen& cg, std::vector<CG_Instr>& frag) {
  auto show = [&cg](CG_Value v) { return ShowVal(cg, v); };

  int num_agg = 0;
  for (CG_Instr& i : frag) {
    auto op(static_cast<BytecodeStream::Instr>(i.tag >> 1));
    if (op == BytecodeStream::CLOSE_AGGREGATION) {
      num_agg--;
    }
    for (int j = 0; j < num_agg; ++j) {
      out << "  ";
    }
    if (i.tag & 1) {
      out << "l" << (i.tag >> 1) << ": ";
      continue;
    }

    out << instr_name(op);
    switch (op) {
      case BytecodeStream::OPEN_AGGREGATION:
        out << " " << agg_name((AggregationCtx::Symbol)i.params[0].value);
        num_agg++;
        break;
      case BytecodeStream::BUILTIN: {
        CG_ProcID p(CG_ProcID::of_val(i.params[0]));
        assert(p.is_builtin());
        out << " " << cg._builtins[p.id()].first;
        for (int ii = 1; ii < i.params.size(); ++ii) {
          out << " " << show(i.params[ii]);
        }
        break;
      }
      case BytecodeStream::CALL: {
        // Mode
        out << " " << mode_name((BytecodeProc::Mode)i.params[0].value);
        // Procedure
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if (p.is_builtin()) {
          out << " " << cg._builtins[p.id()].first;
        } else {
          out << " " << cg.bytecode[p.id()].ident;
        }
        // CSE flag
        out << " " << show(i.params[2]);
        // Arguments
        for (int ii = 3; ii < i.params.size(); ++ii) {
          out << " " << show(i.params[ii]);
        }
        break;
      }
      case BytecodeStream::TCALL: {
        // Mode
        out << " " << mode_name((BytecodeProc::Mode)i.params[0].value);
        // Procedure
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if (p.is_builtin()) {
          out << " " << cg._builtins[p.id()].first;
        } else {
          out << " " << cg.bytecode[p.id()].ident;
        }
        // CSE flag
        out << " " << show(i.params[2]);
        break;
      }
      default:
        for (CG_Value p : i.params) out << " " << show(p);
    }
    out << std::endl;
  }
}

template <class O>
void show(O& out, CodeGen& cg) {
  {
    GCLock lock;
    Model m;
    for (auto g : cg.globals_env) {
      if (g.second.second) {
        TypeInst* ti = g.first->ti();
        // TODO: This removes information from the origin model. Could be reverted after printing
        if (ti->isarray()) {
          std::vector<TypeInst*> ranges(ti->ranges().size());
          for (int i = 0; i < ranges.size(); ++i) {
            ranges[i] = new TypeInst(Location().introduce(), Type::parint());
          }
          auto nti = new TypeInst(Location().introduce(), ti->type(), ranges);
          g.first->ti(nti);
        } else if (ti->domain()) {
          auto nti = new TypeInst(Location().introduce(), ti->type());
          g.first->ti(nti);
        }
        g.first->ann().add(new Call(Location().introduce(), constants().ann.global_register,
                                    {IntLit::a(g.second.first)}));
        m.addItem(new VarDeclI(Location().introduce(), g.first));
      }
    }
    for (auto fun : cg.req_solver_predicates) {
      m.addItem(fun);
    }
    MiniZinc::Printer p(out, 0);
    p.print(&m);
  }
  out << "@@@@@@@@@@" << std::endl;
  for (auto b : cg._builtins) {
    out << ":" << b.first << ": " << b.second << std::endl;
  }
  for (auto& p : cg.bytecode) {
    for (BytecodeProc::Mode m : p) {
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
  if (!cond.get()) {
    if (cond.sign()) PUSH_INSTR(frag, BytecodeStream::POST, CG::r(bind_cst(0, cg, frag)));
    return;
  }
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  if (p->reg[sign].is_root) return;
  if (p->reg[1 - sign].is_root) {
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(bind_cst(0, cg, frag)));
    return;
  }

  if (p->reg[sign].has_reg()) {
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(p->reg[sign].reg));
  } else if (p->reg[1 - sign].has_reg()) {
    int r(GET_REG(cg));
    if (p->reg[1 - sign].is_par) {
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(p->reg[1 - sign].reg), CG::r(r));
    } else {
      auto fun =
          find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(p->reg[1 - sign].reg));
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    }
    PUSH_INSTR(frag, BytecodeStream::POST, CG::r(r));
    p->reg[sign] = CG_Cond::cond_reg(r, p->reg[1 - sign].is_par);
  } else if (p->kind() == CG_Cond::CC_Call) {
    CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(p));
    CG::Mode call_m(CG::Mode::Root, sign);
    std::vector<Type> ty(call->ty.begin() + 1, call->ty.end());
    auto fun = find_call_fun(cg, call->ident, call->ty[0], ty, call_m);
    if (call_m == std::get<1>(fun)) {
      PUSH_INSTR(frag, BytecodeStream::CALL, call_m, std::get<0>(fun),
                 CG::i((!std::get<2>(fun)) && call->cse), call->params);
    } else {
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      std::cerr << "Warning: emitting reification for " << call->ident
                << " because no ROOT_NEG version is available\n";
      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i((!std::get<2>(fun)) && call->cse), call->params);
      int r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      if (BytecodeProc::is_neg(call->m) != BytecodeProc::is_neg(std::get<1>(fun))) {
        if (call->ty[0].ispar()) {
          PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
        } else {
          auto fun =
              find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
          assert(std::get<1>(fun) == BytecodeProc::FUN);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                     CG::i(!std::get<2>(fun)), CG::r(r));
          PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        }
      }
      PUSH_INSTR(frag, BytecodeStream::POST, CG::r(r));
    }
  } else {
    assert(p->kind() == CG_Cond::CC_And);
    CG_Cond::C_And* conj(reinterpret_cast<CG_Cond::C_And*>(p));
    if (!sign) {
      // Recurse.
      for (CG_Cond::T child : conj->children) post_cond(cg, frag, child);
    } else {
      // FIXME: Check force context.
      PUSH_INSTR(frag, BytecodeStream::POST, CG::r(CG::force(cond, BytecodeProc::FUN, cg, frag)));
    }
  }
  p->reg[sign].is_root = true;
}

// FIXME: This always forces calls outside the aggregation.
void aggregate_cond(CodeGen& cg, CG_Builder& frag, CG_Cond::T cond) {
  CG_Cond::_T* p(cond.get());
  bool sign(cond.sign());
  if (!p) {
    // int r_val(CG::locate_immi(1 - sign, cg, frag));
    int r_val(bind_cst(1 - sign, cg, frag));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_val));
    return;
  }
  if (p->reg[sign].has_reg()) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(p->reg[sign].reg));
    return;
  } else if (p->reg[1 - sign].has_reg()) {
    int r_neg(p->reg[1 - sign].reg);
    if (p->reg[1 - sign].is_par) {
      int r(GET_REG(cg));
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_neg), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    } else {
      auto fun =
          find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);
      PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_neg));
    }
    return;
  }
  std::vector<int> var_leaves;
  std::vector<int> par_leaves;
  if (p->kind() == CG_Cond::CC_And) {
    if (!sign) {
      force_and_leaves(var_leaves, par_leaves, cond, cg, frag);
      int r;
      if (var_leaves.empty()) {
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(par_leaves[0]), CG::r(r));
        for (int i = 1; i < par_leaves.size(); ++i) {
          PUSH_INSTR(frag, BytecodeStream::AND, CG::r(r), CG::r(par_leaves[i]), CG::r(r));
        }
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
      } else {
        OPEN_AND(cg, frag);
        for (int r_c : par_leaves) {
          PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
        }
        for (int r_c : var_leaves) {
          PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
        }
        CLOSE_AGG(cg, frag);
      }
    } else {
      force_or_leaves(var_leaves, par_leaves, ~cond, cg, frag);
      int r;
      if (var_leaves.empty()) {
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(par_leaves[0]), CG::r(r));
        for (int i = 1; i < par_leaves.size(); ++i) {
          PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r), CG::r(par_leaves[i]), CG::r(r));
        }
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
      } else {
        OPEN_OR(cg, frag);
        for (int r_c : par_leaves) {
          PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
        }
        for (int r_c : var_leaves) {
          PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
        }
        CLOSE_AGG(cg, frag);
      }
    }
  } else {
    // FIXME: Check force context
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(CG::force(cond, BytecodeProc::FUN, cg, frag)));
  }
}

int locate_range(int l, int u, CodeGen& cg, CG_Builder& frag) {
  CG::Binding b;
  if (cg.env().cache_lookup_range(l, u, b)) {
    return b.first;
  }
  if (l == 0 && u == 1) {
    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("boolean_domain"));
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    cg.env().cache_store_range(l, u, std::make_pair(r, CG_Cond::ttt()));
    return r;
  }

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
  if (vd->type().bt() == Type::BT_BOOL) {
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("boolean_domain"));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    return {r, CG_Cond::ttt()};
  }
  if (Expression* d = vd->ti()->domain()) {
    // Ignoring partiality here.
    return CG::bind(d, cg, frag);
  }
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinite_domain"));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return {r, CG_Cond::ttt()};
}

class Compile : public ItemVisitor {
private:
  friend class ItemIter<Compile>;

  Compile(CodeGen& _cg) : cg(_cg) /*, bool_dom(-1) */ {
    // Force the compilation of predicate definitions that can be generated by the compiler
    find_call_fun(cg, {"op_not"}, Type::varbool(), {Type::varbool()}, BytecodeProc::FUN);
    find_call_fun(cg, {"bool_clause"}, Type::varbool(), {Type::varbool(1), Type::varbool(1)},
                  BytecodeProc::ROOT, true);
    find_call_fun(cg, {"bool_clause_reif"}, Type::varbool(),
                  {Type::varbool(1), Type::varbool(1), Type::varbool()}, BytecodeProc::ROOT, true);
    find_call_fun(cg, {"int_lin_eq"}, Type::varbool(),
                  {Type::parint(1), Type::varint(1), Type::parint()}, BytecodeProc::ROOT, true);
  }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if (vd->type().isann()) return;
    if (vd->type().isopt()) return;
    if (vd->type().isvar()) {
      // In whatever case, we're going to create something,
      // and dump it in a register.
      int r_var;
      if (vd->e()) {
        // Defined
        // Evaluate the definition in root context,
        // add it to a register
        // r_var = CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);
        CG::Binding b_var(CG::bind(vd->e(), cg, root_frag));
        r_var = b_var.first;
        // TODO: Special case for root stuff.
        post_cond(cg, root_frag, b_var.second);

        if (Expression* d = vd->ti()->domain()) {
          if (!vd->ti()->isarray()) {
            CG::Binding b_d = CG::bind(d, cg, root_frag);
            int r_dp = GET_REG(cg);
            PUSH_INSTR(root_frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(r_var), CG::r(b_d.first),
                       CG::r(r_dp));
            post_cond(cg, root_frag, b_d.second);
          } else {
            // Iterate over the elements of the variable we just created, and
            // bind the corresponding domain.
            CG::Binding b_d = CG::bind(d, cg, root_frag);
            int r_dp = GET_REG(cg);
            ITER_ARRAY(cg, root_frag, r_var, [b_d, r_dp](CodeGen& cg, CG_Builder& frag, int r_elt) {
              PUSH_INSTR(frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(r_elt), CG::r(b_d.first),
                         CG::r(r_dp));
            });
            post_cond(cg, root_frag, b_d.second);
          }
        }
      } else {
        CG::Binding b_d(bind_domain(vd, cg, root_frag));
        post_cond(cg, root_frag, b_d.second);
        int r_d = b_d.first;

        r_var = GET_REG(cg);
        if (vd->ti()->isarray()) {
          // Open nested iterators
          int rTMP(GET_REG(cg));
          std::vector<int> r_regs;
          for (Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            CG::Binding b_reg(CG::bind(dim, cg, root_frag));
            post_cond(cg, root_frag, b_reg.second);
            r_regs.push_back(b_reg.first);
          }
          std::vector<Forset> nesting;
          OPEN_VEC(cg, root_frag);
          for (int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::ROOT,
                     cg.find_builtin("mk_intvar"), CG::i(0), CG::r(r_d));
          for (int r_i = r_regs.size() - 1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(root_frag);
          }
          CLOSE_AGG(cg, root_frag);

          PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(r_var));
          OPEN_VEC(cg, root_frag);
          for (int r_r : r_regs) {
            PUSH_INSTR(root_frag, BytecodeStream::GET_VEC, CG::r(r_r),
                       CG::r(bind_cst(1, cg, root_frag)), CG::r(rTMP));
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(rTMP));
            PUSH_INSTR(root_frag, BytecodeStream::GET_VEC, CG::r(r_r),
                       CG::r(bind_cst(2, cg, root_frag)), CG::r(rTMP));
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(rTMP));
          }
          CLOSE_AGG(cg, root_frag);
          PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(rTMP));

          PUSH_INSTR(root_frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(r_var),
                     CG::r(rTMP));
        } else {
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::ROOT,
                     cg.find_builtin("mk_intvar"), CG::i(0), CG::r(r_d));
        }
        PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(r_var));
      }
      // Now copy it into a global, and add it to the env.
      int g = cg.add_global(vd, false);
      PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r_var), CG::g(g));

      // Since it's still in a register, add it to the current env as well.
      cg.env().bind(vd->id()->str(), CG::Binding(r_var, CG_Cond::ttt()));
    } else {
      // For par identifiers with definitions, we evaluate them.
      if (vd->e() && !vd->type().isann()) {
        // Evaluate the definition.
        // int r = vd->type().ispar() ? CG::locate_par(vd->e(), cg, root_frag) : CG::locate(vd->e(),
        // BytecodeProc::ROOT, cg, root_frag);
        int r;
        if (vd->type().isbool()) {
          r = CG::force(CG::compile(vd->e(), cg, root_frag), BytecodeProc::ROOT, cg, root_frag);
        } else {
          // Par expressions may still introduce constraints.
          CG::Binding b_d = CG::bind(vd->e(), cg, root_frag);
          post_cond(cg, root_frag, b_d.second);
          r = b_d.first;
        }
        int g = cg.add_global(vd, false);
        PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(g));

        cg.env().bind(vd->id()->str(), CG::Binding(r, CG_Cond::ttt()));
      }
    }
  }

  /// Visit assign item
  void vAssignI(AssignI* ass) {}

  void vConstraintI(ConstraintI* c) {
    // CG::eval(c->e(), BytecodeProc::ROOT, cg, root_frag);
    post_cond(cg, root_frag, CG::compile(c->e(), cg, root_frag));
  }

  void vFunctionI(FunctionI* f) {
    GCLock l;
    if (f->ann().contains(constants().ann._export)) {
      CG_ProcID body(cg.resolve_fun(f));
      // Force the body to be created
      if (!body.is_builtin()) {
        assert(body.id() < cg.bytecode.size());
        if (!cg.bytecode[body.id()].is_available(BytecodeProc::ROOT)) {
          cg.bytecode[body.id()].body(BytecodeProc::ROOT);
          cg.pending_bodies.push_back(
              std::make_pair(f, std::make_pair(BytecodeProc::ROOT, BytecodeProc::ROOT)));
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
      for (int ii = 0; ii < params.size(); ++ii) {
        cg.env().bind(params[ii]->id()->str(), CG::Binding(ii, CG_Cond::ttt()));
      }
    }

    // Now compile the result.
    CG::Binding b_res = CG::bind(e, cg, frag);
    if (b_res.second.p) {
      // FIXME: What actually happens with this forced result?
      // FIXME: Check force context
      CG::force(b_res.second, BytecodeProc::FUN, cg, frag);
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b_res.first));
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
    {
      GCLock l;
      for (int ii = 0; ii < params.size(); ++ii) {
        cg.env().bind(params[ii]->id()->str(), CG::Binding(ii, CG_Cond::ttt()));
      }
    }

    // Now compile the result.
    CG_Cond::T cond(CG::compile(e, cg, frag));
    if (m.strength() == Mode::Root) {
      post_cond(cg, frag, m.is_neg() ? ~cond : cond);
    } else {
      aggregate_cond(cg, frag, m.is_neg() ? ~cond : cond);
    }
    PUSH_INSTR(frag, BytecodeStream::RET);

    cg.current_reg_count = saved_regs;
    // then restore everything.
    delete cg.current_env;
    cg.current_env = saved_env;
  }
  CodeGen& cg;
  CG_Builder root_frag;

  // int bool_dom;
public:
  static void run(CodeGen& cg, Model* m) {
    Compile c(cg);
    // Compile the main model
    OPEN_OTHER(cg, c.root_frag);
    iterItems(c, m);
    {
      int old_reg_count = cg.current_reg_count;
      cg.current_reg_count = cg.reg_trail.back();
      cg.reg_trail.pop_back();
      PUSH_INSTR(c.root_frag, BytecodeStream::CLEAR, CG::r(cg.current_reg_count),
                 CG::r(old_reg_count - 1));
      cg.env_pop();
    }
    PUSH_INSTR(c.root_frag, BytecodeStream::RET);

    // Now generate procedures for any necessary function/predicate bodies.
    while (!cg.pending_bodies.empty()) {
      GCLock lock;
      auto p(cg.pending_bodies.back());
      cg.pending_bodies.pop_back();

      FunctionI* fun(p.first);
      Mode call_mode(p.second.first);
      Mode def_mode(p.second.second);
      annotate_total(fun);
      // Building structures
      CG_ProcID proc(cg.resolve_fun(fun));
      CG_Builder frag;
      // Find the body.
      std::vector<Type> arg_types(fun->params().size());
      for (int j = 0; j < arg_types.size(); ++j) {
        arg_types[j] = fun->params()[j]->type();
      }

      // Introduce reification if necessary
      if (call_mode == BytecodeProc::IMP || call_mode == BytecodeProc::FUN ||
          call_mode == BytecodeProc::IMP_NEG || call_mode == BytecodeProc::FUN_NEG) {
        auto redef = cg.fun_map.defines_mode(fun->id(), arg_types, call_mode);
        if (redef.first) {
          std::vector<Expression*> args;
          args.reserve(fun->params().size() + 1);
          for (int i = 0; i < fun->params().size(); ++i) {
            VarDecl* vd = fun->params()[i];
            args.emplace_back(vd->id());
          }
          TypeInst var_bool(Location().introduce(), Type::varbool());
          VarDecl new_var(Location().introduce(), &var_bool, "b");
          args.emplace_back(new_var.id());
          Call call(Location().introduce(), redef.second, args);
          call.type(Type::varbool());
          Let let(Location().introduce(), {&new_var, &call}, new_var.id());
          let.type(Type::varbool());
          let.addAnnotation(constants().ann.promise_total);
          c.compile_pred(frag, fun->params(), call_mode, &let);
          cg.append(proc.id(), call_mode, frag);
          continue;
        }
      }
      if (BytecodeProc::is_neg(call_mode)) {
        GCLock lock;
        ASTString ident(fun->id().str() + "_neg");
        auto neg_bodies = std::move(cg.fun_map.get_bodies(ident, arg_types));
        if (std::any_of(neg_bodies.begin(), neg_bodies.end(),
                        [](FunctionI* fi) { return fi->e(); })) {
          std::vector<Expression*> args;
          args.reserve(fun->params().size());
          for (int i = 0; i < fun->params().size(); ++i) {
            VarDecl* vd = fun->params()[i];
            args.emplace_back(vd->id());
          }
          Call call(Location().introduce(), ident, args);
          call.type(Type::varbool());
          // Call negated constraint in positive context (implementation should deal with the
          // negation).
          c.compile_pred(frag, fun->params(), BytecodeProc::negate(call_mode), &call);
          cg.append(proc.id(), call_mode, frag);
          continue;
        }
      }

      if (fun->e()) {
        if (fun->e()->type().isbool()) {
          c.compile_pred(frag, fun->params(), def_mode, fun->e());
        } else {
          assert(def_mode == BytecodeProc::ROOT);
          c.compile_fun(frag, fun->params(), fun->e());
        }
      } else {
        assert(call_mode == BytecodeProc::ROOT);
        cg.req_solver_predicates.push_back(fun);
      }
      cg.append(proc.id(), call_mode, frag);
    }

    // And finally, add the entry function.
    int main_proc = cg.bytecode.size();
    cg.bytecode.emplace_back("main", 0);
    cg.append(main_proc, BytecodeProc::ROOT, c.root_frag);

    show(std::cout, cg);
  }
};

Mode open_conj(Mode ctx, CG_Builder& frag) {
  if (ctx == BytecodeProc::ROOT) return ctx;

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION,
             ctx.is_neg() ? AggregationCtx::VCTX_OR : AggregationCtx::VCTX_AND);
  return +ctx;
}
void close_conj(Mode ctx, CG_Builder& frag) {
  if (ctx != BytecodeProc::ROOT) PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
Mode open_disj(Mode ctx, CG_Builder& frag) {
  if (ctx == BytecodeProc::ROOT_NEG) return ctx;

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION,
             ctx.is_neg() ? AggregationCtx::VCTX_AND : AggregationCtx::VCTX_OR);
  return +ctx;
}
void close_disj(Mode ctx, CG_Builder& frag) {
  if (ctx != BytecodeProc::ROOT_NEG) PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

Mode left_child_ctx(Mode ctx, BinOpType b) {
  switch (b) {
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
  switch (b) {
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
  switch (u) {
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
  // FIXME: Figure out why some expressions don't have a mode attached.
  CG::Mode ctx(BytecodeProc::FUN);
  try {
    ctx = cg.mode_map.at(e);
  } catch (const std::out_of_range& exn) {
  }

  switch (e->eid()) {
    case Expression::E_INTLIT: {
      // return std::make_pair(CG::locate_immi(e->template cast<IntLit>()->v().toInt(), cg, frag),
      // CG_Cond::ttt());
      IntVal i = e->cast<IntLit>()->v();
      if (!i.isFinite()) {
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
                   CG::r(bind_cst(i.isPlusInfinity(), cg, frag)));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        return {r, CG_Cond::ttt()};
      }
      return {bind_cst(i.toInt(), cg, frag), CG_Cond::ttt()};
    }
    case Expression::E_FLOATLIT:
      // return std::make_pair(CG::locate_par(e->template cast<FloatLit>(), cg, frag), nullptr);
      // NOT YET IMPLEMENTED
      return CG::Binding(bind_cst(911911, cg, frag), CG_Cond::ttt());
    case Expression::E_SETLIT:
      return CG::bind(e->template cast<SetLit>(), ctx, cg, frag);
    case Expression::E_BOOLLIT:
      throw InternalError("bind called on Boolean expression.");
    case Expression::E_STRINGLIT:
      // NOT YET IMPLEMENTED
      return CG::Binding(bind_cst(911911, cg, frag), CG_Cond::ttt());
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
  } catch (const CG_Env<CG::Binding>::NotFound& exn) {
    CG::Binding b(_bind(e, cg, frag));
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

// Modified version of execute-comprehension, executing an arbitrary callable on the innter
// expression.
template <class F>
void execute_comprehension_generic(Comprehension* c, Mode ctx, CodeGen& cg, CG_Builder& frag, F f) {
  // Build up the object to build the generator.
  std::vector<EmitPost*> nesting;

  int depth = 0;

  int g = c->n_generators();
  int n_gen = c->n_generators();
  for (int g = 0; g < n_gen; ++g) {
    // Bind the in-expression to a register.
    // assert(c->in(g)->type().ispar());
    Expression* in(c->in(g));
    // std::cout << "Binding R" << r << " to: "; debugprint(in);
    // Open the bindings.
    cg.env_push();
    if (in->type().is_set()) {
      int r(CG::bind(in, cg, frag).first);
      assert(in->type().ispar());
      for (int d = 0; d < c->n_decls(g); ++d) {
        Forset* iter(new Forset(cg, r));
        depth += 2;  // Forset first iterates over the range, then values in the range.
        nesting.push_back(iter);
        iter->emit_pre(frag);
        // Bind vd->id() to iter.val()
        VarDecl* vd(c->decl(g, d));
        ASTString id(vd->id()->str());
        // cg.env().bind(id, Loc::reg(iter.val()));
        cg.env().bind(id, CodeGen::Binding(iter->val(), CG_Cond::ttt()));
      }
    } else {
      assert(in->type().isboolarray() || in->type().isintarray() || in->type().isintsetarray());
      int r_arr(CG::bind(in, cg, frag).first);
      for (int d = 0; d < c->n_decls(g); ++d) {
        Foreach* iter(new Foreach(cg, r_arr));
        nesting.push_back(iter);
        iter->emit_pre(frag);
        depth += 1;

        VarDecl* vd(c->decl(g, d));
        ASTString id(vd->id()->str());
        cg.env().bind(id, CodeGen::Binding(iter->val(), CG_Cond::ttt()));
      }
    }
    // Now emit the where-clause
    Expression* where(c->where(g));
    if (where) {
      int lblCont(nesting.back()->cont());
      assert(where->type().ispar());
      int rC = CG::force(CG::compile(where, cg, frag), BytecodeProc::FUN, cg, frag);
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblCont));
    }
  }
  // We're now in the deepest scope. Generate code for the body.
  f(c->e(), ctx, cg, frag, nesting.back()->cont(), depth);

  // Now close the iterators _in reverse order_, and restore the environment.
  for (int ii = nesting.size() - 1; ii >= 0; --ii) {
    nesting[ii]->emit_post(frag);
    delete nesting[ii];
  }
  for (int ii = 0; ii < n_gen; ++ii) cg.env_pop();
}

// Specialized version when we're binding and pushing the body.
void execute_comprehension_bind(Comprehension* c, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  execute_comprehension_generic(
      c, ctx, cg, frag,
      [](Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
        // Bind and push everything in the comprehension.
        CG::Binding b_e = CG::force_or_bind(e, ctx, cg, frag);
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b_e.first));  // FIXME: Discarding partiality
      });
}

// Special case implementation of folds where body is a generator.
CG_Cond::T eval_forall(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch (param->eid()) {
    case Expression::E_ARRAYLIT: {
      std::vector<CG_Cond::T> conj;
      ArrayLit* a(param->cast<ArrayLit>());
      int sz(a->size());
      for (int ii = 0; ii < sz; ++ii) conj.push_back(CG::compile((*a)[ii], cg, frag));
      return CG_Cond::forall(ctx, conj);
    } break;
    case Expression::E_COMP: {
      Comprehension* c(param->cast<Comprehension>());
      // Special cases: if we're in root, just post everything.
      if (ctx == BytecodeProc::ROOT) {
        execute_comprehension_generic(
            param->cast<Comprehension>(), ctx, cg, frag,
            [](Expression* elt, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
              CG_Cond::T c = CG::compile(elt, cg, frag);
              post_cond(cg, frag, c);
            });
        ;
        return CG_Cond::T::ttt();
      }
      // If it's par, do shortcutting.
      if (c->e()->type().ispar()) {
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r));
        execute_comprehension_generic(
            c, ctx, cg, frag,
            [r](Expression* elt, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
              int r_cond = CG::force(CG::compile(elt, cg, frag), BytecodeProc::FUN, cg, frag);
              PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_cond), CG::l(lbl_cont));
              PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
              PUSH_INSTR(frag, BytecodeStream::ITER_BREAK, CG::i(depth));
            });
        ;
        return CG_Cond::reg(r, true);
      } else {
        // General case, just aggregate the values.
        OPEN_AND(cg, frag);
        execute_comprehension_generic(
            c, ctx, cg, frag,
            [](Expression* elt, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
              int r_cond =
                  CG::force(CG::compile(elt, cg, frag), CG::Mode(ctx.strength(), false), cg, frag);
              PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_cond));
            });
        CLOSE_AGG(cg, frag);
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        return CG_Cond::reg(r, false);
      }
    }
    default: {
      int r(GET_REG(cg));
      CG::Binding b_param(CG::bind(param, cg, frag));
      int r_A(b_param.first);
      std::vector<int> p_A;
      if (!b_param.second.get()) {
        if (b_param.second.sign()) return CG_Cond::T::fff();
      } else {
        // TODO: It's probably not quicker to split par/var since we need the AND Aggregation anyway
        force_and_leaves(p_A, p_A, b_param.second, cg, frag);
      }
      OPEN_AND(cg, frag);
      // First, push the constraints attached to A.
      for (int r_c : p_A) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      }
      ITER_ARRAY(cg, frag, r_A, [](CodeGen& cg, CG_Builder& frag, int r_elt) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_elt));
      });
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return CG_Cond::reg(r, call->type().ispar());
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
  switch (param->eid()) {
    case Expression::E_ARRAYLIT: {
      std::vector<CG_Cond::T> disj;
      ArrayLit* a(param->cast<ArrayLit>());
      int sz(a->size());
      for (int ii = 0; ii < sz; ++ii) {
        CG_Cond::T elt(CG::compile((*a)[ii], cg, frag));
        disj.push_back(elt);
      }
      return CG_Cond::exists(ctx, disj);
    } break;
    /*
  case Expression::E_COMP: {
    Comprehension* c(param->cast<Comprehension>());
    execute_comprehension(c, c_ctx, cg, frag);
    }
    break;
    */
#if 1
    case Expression::E_COMP: {
      Comprehension* c(param->cast<Comprehension>());
      if (c->e()->type().ispar()) {
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
        execute_comprehension_generic(
            c, ctx, cg, frag,
            [r](Expression* elt, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
              int r_cond = CG::force(CG::compile(elt, cg, frag), BytecodeProc::FUN, cg, frag);
              PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_cond), CG::l(lbl_cont));
              PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r));
              PUSH_INSTR(frag, BytecodeStream::ITER_BREAK, CG::i(depth));
            });
        return CG_Cond::reg(r, true);
      } else {
        OPEN_OR(cg, frag);
        execute_comprehension_generic(
            c, ctx, cg, frag,
            [](Expression* elt, Mode ctx, CodeGen& cg, CG_Builder& frag, int lbl_cont, int depth) {
              int r_cond =
                  CG::force(CG::compile(elt, cg, frag), CG::Mode(ctx.strength(), false), cg, frag);
              PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_cond));
            });
        CLOSE_AGG(cg, frag);
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
        return CG_Cond::reg(r, false);
      }
    }  // Fallthrough
#endif
    default: {
      int r(GET_REG(cg));
      // Otherwise, get the result into a register...
      CG::Binding b_A(CG::bind(param, cg, frag));
      int r_A(b_A.first);
      // and push every element.
      OPEN_OR(cg, frag);
      ITER_ARRAY(cg, frag, r_A, [](CodeGen& cg, CG_Builder& frag, int r_elt) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_elt));
      });
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return CG_Cond::forall(ctx, b_A.second, CG_Cond::reg(r, call->type().ispar()));
    }
  }
}

CG_Cond::T eval_isfixed_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* e(call->arg(0));

  if (e->type().ispar()) return CG_Cond::T::ttt();

  int r_e;
  if (e->type().isbool()) {
    CG_Cond::T b_e(CG::compile(e, cg, frag));
    if (!b_e.get()) {
      return CG_Cond::T::ttt();
    }
    r_e = CG::force(b_e, ctx, cg, frag);
  } else {
    CG::Binding b(CG::bind(e, cg, frag));
    r_e = b.first;
  }
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(r_e), CG::r(r));
  return CG_Cond::reg(r, true);
}

CG::Binding bind_fix(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);

  Expression* arg(call->arg(0));
  CG::Binding b_arg(CG::bind(arg, cg, frag));
  if (arg->type().ispar()) {
    return b_arg;
  } else if (arg->type().isintarray() || arg->type().isboolarray()) {
    int r_cond(GET_REG(cg));
    // Init the condition
    PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(b_arg.first), CG::r(r_cond));

    // Gather elements
    OPEN_VEC(cg, frag);
    ITER_ARRAY(cg, frag, b_arg.first, [](CodeGen& cg, CG_Builder& frag, int r_elt) {
      PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_elt), CG::r(r_elt));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_elt));
    });
    CLOSE_AGG(cg, frag);
    int r_elts(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_elts));

    // Ensure resulting array has the same index set
    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(b_arg.first),
               CG::r(bind_cst(0, cg, frag)));
    int r_indxs(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_indxs));

    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(r_elts),
               CG::r(r_indxs));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_elts));

    return {r_elts, CG_Cond::forall(ctx, b_arg.second, CG_Cond::reg(r_cond, true))};
  } else {
    int r(GET_REG(cg));
    int r_cond(GET_REG(cg));

    PUSH_INSTR(frag, BytecodeStream::ISPAR, CG::r(b_arg.first), CG::r(r_cond));
    PUSH_INSTR(frag, BytecodeStream::LB, CG::r(b_arg.first), CG::r(r));

    return {r, CG_Cond::forall(ctx, b_arg.second, CG_Cond::reg(r_cond, true))};
  }
}

CG_Cond::T eval_fix(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);

  // Should we check that the argument is actually fixed??
  int r = CG::force(CG::compile(call->arg(0), cg, frag), ctx, cg, frag);
  return CG_Cond::reg(r, true);
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
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO: Specialise for literals and comprehensions.
  CG::Binding b_elts(CG::bind(e, cg, frag));
  int r_one(bind_cst(1, cg, frag));

  if (e->type().ispar()) {
    int r_sum(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_sum));
    ITER_ARRAY(cg, frag, b_elts.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
      PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_sum), CG::r(r_elt), CG::r(r_sum));
    });
    return CG::Binding(r_sum, b_elts.second);
  }

  int r_sz(GET_REG(cg));
  int r_ret(GET_REG(cg));

  int l_eq0(GET_LABEL(cg));
  int l_eq1(GET_LABEL(cg));
  int l_end(GET_LABEL(cg));

  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_elts.first), CG::r(r_sz));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sz), CG::l(l_eq0));
  PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_one), CG::r(r_sz), CG::r(r_ret));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_ret), CG::l(l_eq1));

  // Sum n arguments
  auto fun = find_call_fun(cg, {"sum_cc"}, Type::varint(), {Type::varint(1)}, BytecodeProc::FUN);
  assert(std::get<1>(fun) == BytecodeProc::FUN);
  PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
             CG::i(!std::get<2>(fun)), CG::r(b_elts.first));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));

  // Sum zero arguments (result must be 0)
  PUSH_LABEL(frag, l_eq0);
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_ret));
  PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));

  // Sum 1 arguments (result == a[1])
  PUSH_LABEL(frag, l_eq1);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_elts.first), CG::r(r_one), CG::r(r_ret));

  // End of sum
  PUSH_LABEL(frag, l_end);
  return CG::Binding(r_ret, b_elts.second);
}

CG::Binding bind_set2array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  // Bind set
  Expression* e = call->arg(0);
  CG::Binding b_elts(CG::bind(e, cg, frag));

  // Create array

  OPEN_VEC(cg, frag);
  ITER_SET(cg, frag, b_elts.first, [](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_elt));
  });
  CLOSE_AGG(cg, frag);
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  return {r_ret, b_elts.second};
}

CG::Binding bind_dom_bounds_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));

  // Open the context here, so we can recover all the registers after.
  OPEN_VEC(cg, frag);

  int r_lb(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_lb));

  int r_ub(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(0, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ub));

  // Do the iteration
  int r_test(GET_REG(cg));
  int r_bound(GET_REG(cg));

  int l_lb(GET_LABEL(cg));
  int l_ub(GET_LABEL(cg));
  // Body
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_elt), CG::r(r_bound));
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_lb), CG::r(r_bound), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_lb));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_bound), CG::r(r_lb));
    PUSH_LABEL(frag, l_lb);
    PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_elt), CG::r(r_bound));
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_bound), CG::r(r_ub), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_test), CG::l(l_ub));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_bound), CG::r(r_ub));
    PUSH_LABEL(frag, l_ub);
  });

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
  if (call->n_args() == 2) {
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

CG::Binding bind_arrayXd(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 2);
  CG::Binding orig(CG::bind(call->arg(0), cg, frag));
  CG::Binding target(CG::bind(call->arg(1), cg, frag));

  // Get index sets from orig
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(orig.first),
             CG::r(bind_cst(0, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  // Create new array
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(target.first),
             CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  return {r, CG_Cond::forall(ctx, orig.second, target.second)};
}

template <int N>
CG::Binding bind_arrayNd(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == N + 1);
  std::vector<CG_Cond::T> cond;

  // Bind all index sets
  std::vector<int> r_index(N);
  for (int ii = 0; ii < N; ++ii) {
    CG::Binding b(CG::bind(call->arg(ii), cg, frag));
    r_index[ii] = b.first;
    cond.push_back(b.second);
  }

  // Bind array expression
  CG::Binding arr = CG::bind(call->arg(N), cg, frag);

  int rI(GET_REG(cg));
  OPEN_VEC(cg, frag);
  for (int ii = 0; ii < N; ++ii) {
    // TODO: Ensure the index set is a range (2-elements)
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_index[ii]), CG::r(bind_cst(1, cg, frag)),
               CG::r(rI));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_index[ii]), CG::r(bind_cst(2, cg, frag)),
               CG::r(rI));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rI));
  }
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(arr.first),
             CG::r(rI));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));

  return {rI, CG_Cond::forall(ctx, cond)};
}

CG::Binding bind_array1d(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if (call->n_args() == 1) {
    int rA(GET_REG(cg));
    CG::Binding arr = CG::bind(call->arg(0), cg, frag);

    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(arr.first),
               CG::r(bind_cst(0, cg, frag)));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rA));

    return {rA, arr.second};
  } else {
    // Index set given by the user
    assert(call->n_args() == 2);
    return bind_arrayNd<1>(call, ctx, cg, frag);
  }
}

CG::Binding bind_array_union(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO: Specialise for literals and comprehensions.
  CG::Binding b_elts(CG::bind(e, cg, frag));

  assert(e->type().ispar());  // TODO: var case
  int r_union(GET_REG(cg));
  OPEN_VEC(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_union));

  ITER_ARRAY(cg, frag, b_elts.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r_union), CG::r(r_elt), CG::r(r_union));
  });

  return {r_union, b_elts.second};
}

CG::Binding bind_length(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b.first), CG::r(r));
  return {r, b.second};
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

  int r_agg(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_agg));

  int r_tmp(GET_REG(cg));
  int r_test(GET_REG(cg));
  int l_end(GET_LABEL(cg));
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::LB, CG::r(r_elt), CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_tmp), CG::r(r_agg), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_tmp), CG::r(r_agg));
    PUSH_LABEL(frag, l_end);
  });

  return CG::Binding(r_agg, b_A.second);
}

CG::Binding bind_ub_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));

  int r_agg(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(0, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_agg));

  int r_tmp(GET_REG(cg));
  int r_test(GET_REG(cg));
  int l_end(GET_LABEL(cg));
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::UB, CG::r(r_elt), CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_agg), CG::r(r_tmp), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_tmp), CG::r(r_agg));
    PUSH_LABEL(frag, l_end);
  });

  return CG::Binding(r_agg, b_A.second);
}

CG::Binding bind_dom_array(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));

  int r_agg(GET_REG(cg));
  OPEN_VEC(cg, frag);
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_agg));

  int r_tmp(GET_REG(cg));
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::DOM, CG::r(r_elt), CG::r(r_tmp));
    PUSH_INSTR(frag, BytecodeStream::UNION, CG::r(r_agg), CG::r(r_tmp), CG::r(r_agg));
  });

  return {r_agg, b_A.second};
}

CG::Binding bind_index_set(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  assert(call->arg(0)->type().dim() == 1);
  CG::Binding b_arg(CG::bind(call->arg(0), cg, frag));

  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(b_arg.first),
             CG::r(bind_cst(1, cg, frag)));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  return {r, b_arg.second};
}

template <int X, int Y>
CG::Binding bind_index_set_XofY(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_arg(CG::bind(call->arg(0), cg, frag));

  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(b_arg.first),
             CG::r(bind_cst(X, cg, frag)));
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  return {r, b_arg.second};
}

CG::Binding bind_bool2int(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  return CG::force_or_bind(call->arg(0), BytecodeProc::FUN, cg, frag);
}
CG::Binding bind_int2float(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  return CG::bind(call->arg(0), cg, frag);
}

CG::Binding bind_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);
CG_Cond::T compile_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag);

CG::Binding bind_set_max(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->type().ispar());
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_sz(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
  int l_fst(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(bind_cst(0, cg, frag)), CG::r(r_sz), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r), CG::l(l_fst));
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  PUSH_LABEL(frag, l_fst);

  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(r_sz), CG::r(r));

  // TODO: Also should not be allowed on empty sets
  return CG::Binding(r, b_A.second);
};

CG::Binding bind_max(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->type().ispar());
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));

  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(0, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  int r_test(GET_REG(cg));
  int l_end(GET_LABEL(cg));
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r), CG::r(r_elt), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r));
    PUSH_LABEL(frag, l_end);
  });

  // TODO: Fail if array size < 1
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding bind_arg_max(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_elt(GET_REG(cg));
  int r_v(GET_REG(cg));
  int r_test(GET_REG(cg));
  int r_index(GET_REG(cg));

  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(b_A.first),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_index));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(0, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));

  int l_end(GET_LABEL(cg));
  ITER_SET(cg, frag, r_index, [&](CodeGen& cg, CG_Builder& frag, int r_i) {
    // Array access should be valid since we are using the actual index set.
    PUSH_INSTR(frag, BytecodeStream::GET_ARRAY, CG::i(1), CG::r(b_A.first), CG::r(r_i),
               CG::r(r_elt), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_v), CG::r(r_elt), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r_v));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_i), CG::r(r));
    PUSH_LABEL(frag, l_end);
  });

  // TODO: Fail if array size < 1
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding bind_arg_min(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_elt(GET_REG(cg));
  int r_v(GET_REG(cg));
  int r_test(GET_REG(cg));
  int r_index(GET_REG(cg));

  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("index_set"), CG::r(b_A.first),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_index));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));

  int l_end(GET_LABEL(cg));
  ITER_SET(cg, frag, r_index, [&](CodeGen& cg, CG_Builder& frag, int r_i) {
    // Array access should be valid since we are using the actual index set.
    PUSH_INSTR(frag, BytecodeStream::GET_ARRAY, CG::i(1), CG::r(b_A.first), CG::r(r_i),
               CG::r(r_elt), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_elt), CG::r(r_v), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r_v));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_i), CG::r(r));
    PUSH_LABEL(frag, l_end);
  });

  // TODO: Fail if array size < 1
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding bind_set_min(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->type().ispar());
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));
  int r(GET_REG(cg));
  int r_sz(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(b_A.first), CG::r(r_sz));
  int l_fst(GET_LABEL(cg));
  PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(bind_cst(1, cg, frag)), CG::r(r_sz), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r), CG::l(l_fst));
  PUSH_INSTR(frag, BytecodeStream::ABORT);

  PUSH_LABEL(frag, l_fst);
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(b_A.first), CG::r(bind_cst(1, cg, frag)),
             CG::r(r));

  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding bind_min(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->type().ispar());
  assert(call->n_args() == 1);
  CG::Binding b_A(CG::bind(call->arg(0), cg, frag));

  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
             CG::r(bind_cst(1, cg, frag)));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  int r_test(GET_REG(cg));
  int l_end(GET_LABEL(cg));
  ITER_ARRAY(cg, frag, b_A.first, [&](CodeGen& cg, CG_Builder& frag, int r_elt) {
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_elt), CG::r(r), CG::r(r_test));
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_test), CG::l(l_end));
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_elt), CG::r(r));
    PUSH_LABEL(frag, l_end);
  });

  return CG::Binding(r, CG_Cond::ttt());
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
  PUSH_INSTR(frag, BytecodeStream::INCI, CG::r(r_i));
  PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r), CG::r(r_e), CG::r(r));

  PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_i), CG::r(r_sz), CG::r(r_e));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r_e), CG::l(l_hd));
  PUSH_LABEL(frag, l_tl);
  return CG::Binding(r, b_A.second);
}

CG::Binding bind_internal(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::string name = call->decl()->id().str();
  CG_ProcID proc = cg.find_builtin(name);
  int r_res(GET_REG(cg));

  std::vector<CG_Value> r_args(call->n_args());
  std::vector<CG_Cond::T> p_args(call->n_args());
  for (int i = 0; i < call->n_args(); ++i) {
    CG::Binding b_arg(CG::bind(call->arg(i), cg, frag));
    r_args[i] = CG::r(b_arg.first);
    p_args[i] = b_arg.second;
  }

  // Push BUILTIN instruction with the correct id
  PUSH_INSTR(frag, BytecodeStream::BUILTIN, proc);
  // Append instruction with register arguments
  CG_Instr& i = frag.instrs.back();
  PUSH_INSTR_OPERAND(i, r_args);

  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_res));

  return {r_res, CG_Cond::forall(ctx, p_args)};
}

CG_Cond::T eval_context_is_root(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: I'm not sure this is actually correct....
  assert(call->n_args() == 1);
  CG::Mode arg_ctx(BytecodeProc::FUN);
  try {
    arg_ctx = cg.mode_map.at(call->arg(0));
  } catch (const std::out_of_range& exn) {
  }

  if (arg_ctx == BytecodeProc::ROOT) {
    return CG_Cond::ttt();
  } else {
    return CG_Cond::fff();
  }
}

CG_Cond::T eval_has_bounds(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: Actual implementation!

  return CG_Cond::ttt();
}

CG::Binding bind_TODO(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  return {0, CG_Cond::fff()};
}
CG_Cond::T eval_TODO(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  PUSH_INSTR(frag, BytecodeStream::ABORT);
  return CG_Cond::fff();
}

template <int X>
CG_Cond::T eval_argX_only(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  return CG::compile(call->arg(X - 1), cg, frag);
}

builtin_table init_builtins(void) {
  builtin_table tbl;
  Constants& c(constants());
  tbl.insert(std::make_pair(c.ids.sum.str(), builtin_t{eval_error_b, bind_sum}));
  tbl.insert(std::make_pair(c.ids.exists.str(), builtin_t{eval_exists, bind_error_g}));
  tbl.insert(std::make_pair(c.ids.forall.str(), builtin_t{eval_forall, bind_error_g}));
  tbl.insert(std::make_pair(c.ids.assert.str(), builtin_t{eval_assert_b, bind_assert_g}));
  tbl.insert(std::make_pair("arrayXd", builtin_t{eval_error_b, bind_arrayXd}));
  tbl.insert(std::make_pair("array1d", builtin_t{eval_error_b, bind_array1d}));
  tbl.insert(std::make_pair("array2d", builtin_t{eval_error_b, bind_arrayNd<2>}));
  tbl.insert(std::make_pair("array3d", builtin_t{eval_error_b, bind_arrayNd<3>}));
  tbl.insert(std::make_pair("array4d", builtin_t{eval_error_b, bind_arrayNd<4>}));
  tbl.insert(std::make_pair("array5d", builtin_t{eval_error_b, bind_arrayNd<5>}));
  tbl.insert(std::make_pair("array6d", builtin_t{eval_error_b, bind_arrayNd<6>}));
  tbl.insert(std::make_pair("array7d", builtin_t{eval_error_b, bind_arrayNd<7>}));
  tbl.insert(std::make_pair("array8d", builtin_t{eval_error_b, bind_arrayNd<8>}));
  tbl.insert(std::make_pair("array9d", builtin_t{eval_error_b, bind_arrayNd<9>}));
  tbl.insert(std::make_pair("array_union", builtin_t{eval_error_b, bind_array_union}));
  tbl.insert(std::make_pair("index_set", builtin_t{eval_error_b, bind_index_set}));
  tbl.insert(std::make_pair("index_set_1of2", builtin_t{eval_error_b, bind_index_set_XofY<1, 2>}));
  tbl.insert(std::make_pair("index_set_2of2", builtin_t{eval_error_b, bind_index_set_XofY<2, 2>}));
  tbl.insert(std::make_pair("index_set_1of3", builtin_t{eval_error_b, bind_index_set_XofY<1, 3>}));
  tbl.insert(std::make_pair("index_set_2of3", builtin_t{eval_error_b, bind_index_set_XofY<2, 3>}));
  tbl.insert(std::make_pair("index_set_3of3", builtin_t{eval_error_b, bind_index_set_XofY<3, 3>}));
  tbl.insert(std::make_pair("index_set_1of4", builtin_t{eval_error_b, bind_index_set_XofY<1, 4>}));
  tbl.insert(std::make_pair("index_set_2of4", builtin_t{eval_error_b, bind_index_set_XofY<2, 4>}));
  tbl.insert(std::make_pair("index_set_3of4", builtin_t{eval_error_b, bind_index_set_XofY<3, 4>}));
  tbl.insert(std::make_pair("index_set_4of4", builtin_t{eval_error_b, bind_index_set_XofY<4, 4>}));
  tbl.insert(std::make_pair("length", builtin_t{eval_error_b, bind_length}));
  tbl.insert(std::make_pair("lb", builtin_t{eval_error_b, bind_lb}));
  tbl.insert(std::make_pair("ub", builtin_t{eval_error_b, bind_ub}));
  tbl.insert(std::make_pair("dom", builtin_t{eval_error_b, bind_dom}));
  tbl.insert(std::make_pair("lb_array", builtin_t{eval_error_b, bind_lb_array}));
  tbl.insert(std::make_pair("ub_array", builtin_t{eval_error_b, bind_ub_array}));
  tbl.insert(std::make_pair("dom_array", builtin_t{eval_error_b, bind_dom_array}));
  tbl.insert(std::make_pair("set2array", builtin_t{eval_error_b, bind_set2array}));
  tbl.insert(std::make_pair("dom_bounds_array", builtin_t{eval_error_b, bind_dom_bounds_array}));
  tbl.insert(std::make_pair("arg_max", builtin_t{eval_error_b, bind_arg_max}));
  tbl.insert(std::make_pair("arg_min", builtin_t{eval_error_b, bind_arg_min}));
  tbl.insert(std::make_pair("card", builtin_t{eval_error_b, bind_card}));
  tbl.insert(std::make_pair(c.ids.bool2int.str(), builtin_t{eval_error_b, bind_bool2int}));
  tbl.insert(std::make_pair(c.ids.int2float.str(), builtin_t{eval_error_b, bind_bool2int}));
  tbl.insert(std::make_pair("uniform", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("sol", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("sort_by", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("floor", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("ceil", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("mzn_in_root_context", builtin_t{eval_context_is_root, bind_error_g}));
  tbl.insert(std::make_pair("has_bounds", builtin_t{eval_has_bounds, bind_error_g}));
  tbl.insert(std::make_pair("is_fixed", builtin_t{eval_isfixed_b, bind_error_g}));
  tbl.insert(std::make_pair("fix", builtin_t{eval_fix, bind_fix}));
  tbl.insert(std::make_pair("array_Xd", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("slice_Xd", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("internal_sort", builtin_t{eval_error_b, bind_internal}));
  tbl.insert(std::make_pair("internal_max", builtin_t{eval_error_b, bind_max}));
  tbl.insert(std::make_pair("internal_set_max", builtin_t{eval_error_b, bind_set_max}));
  tbl.insert(std::make_pair("internal_min", builtin_t{eval_error_b, bind_min}));
  tbl.insert(std::make_pair("internal_set_min", builtin_t{eval_error_b, bind_set_min}));
  tbl.insert(
      std::make_pair("symmetry_breaking_constraint", builtin_t{eval_argX_only<1>, bind_error_g}));
  tbl.insert(std::make_pair("redundant_constraint", builtin_t{eval_argX_only<1>, bind_error_g}));
  // OPTIONAL TYPE INTERNALS
  tbl.insert(std::make_pair("reverse_map", builtin_t{eval_error_b, bind_TODO}));
  tbl.insert(std::make_pair("occurs", builtin_t{eval_TODO, bind_error_g}));
  tbl.insert(std::make_pair("absent", builtin_t{eval_TODO, bind_error_g}));
  tbl.insert(std::make_pair("deopt", builtin_t{eval_error_b, bind_TODO}));
  return tbl;
}
builtin_table& builtins(void) {
  static builtin_table tbl(init_builtins());
  return tbl;
}

CG::Binding CG::bind(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    return CG::Binding(cg.env().lookup(x->v()).first, CG_Cond::ttt());
  } catch (const CG_Env<CodeGen::Binding>::NotFound& exn) {
    VarDecl* vd = follow_id_to_decl(x)->cast<VarDecl>();
    int g = cg.find_global(vd);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG::Binding(r, CG_Cond::ttt());
  }
}

CG::Binding CG::bind(SetLit* sl, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  bool is_vec(false);
  OPEN_VEC(cg, frag);
  if (IntSetVal* s = sl->isv()) {
    int r_t(GET_REG(cg));
    for (int ii = 0; ii < s->size(); ++ii) {
      IntVal l(s->min(ii));
      IntVal u(s->max(ii));
      if (l.isFinite()) {
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(l.toInt()), CG::r(r_t));
      } else {
        assert(l.isMinusInfinity());
        PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
                   CG::r(bind_cst(0, cg, frag)));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_t));
      }
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      if (u.isFinite()) {
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u.toInt()), CG::r(r_t));
      } else {
        assert(u.isPlusInfinity());
        PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("infinity"),
                   CG::r(bind_cst(1, cg, frag)));
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_t));
      }
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
    }
  } else {
    is_vec = true;
    int sz = sl->v().size();
    int r_t(GET_REG(cg));
    for (int ii = 0; ii < sz; ii++) {
      // Assumes the set is sorted.
      /*
      IntLit* k(l->v()[ii]->template cast<IntLit>());
      assert(k);
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(k->v().toInt()), CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
      */
      CG::Binding b(CG::bind(sl->v()[ii], cg, frag));  // Ignores any partiality.
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b.first));
    }
  }
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  if (is_vec) PUSH_INSTR(frag, BytecodeStream::MAKE_SET, CG::r(r), CG::r(r));
  return CG::Binding(r, CG_Cond::ttt());
}

CG::Binding CG::bind(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Build up the array.
  std::vector<std::pair<IntLit*, int>> r_vec;
  std::vector<CG_Cond::T> p_vec;

  int sz(a->size());
  for (int ii = 0; ii < sz; ++ii) {
    if ((*a)[ii]->type().isbool()) {
      CG_Cond::T c(CG::compile((*a)[ii], cg, frag));
      if (!c.get()) {
        if (!c.sign()) {
          r_vec.emplace_back(nullptr, bind_cst(1, cg, frag));
        } else {
          r_vec.emplace_back(nullptr, bind_cst(0, cg, frag));
        }
      } else {
        r_vec.emplace_back(nullptr, CG::force(c, ctx, cg, frag));
      }
    } else if (auto il = (*a)[ii]->dyn_cast<IntLit>()) {
      // Avoid lookup cost when constructing large array literals
      r_vec.emplace_back(il, 0xdeadbeef);
    } else {
      Binding b_ii(CG::bind((*a)[ii], cg, frag));
      r_vec.emplace_back(nullptr, b_ii.first);
      p_vec.push_back(b_ii.second);
    }
  }

  //  Build Array
  int r(GET_REG(cg));
  OPEN_VEC(cg, frag);
  for (auto r_c : r_vec) {
    if (r_c.first) {
      assert(r_c.first->v().isFinite());
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(r_c.first->v().toInt()), CG::r(r));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    } else {
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c.second));
    }
  }
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  // Add index set if required
  if (!(a->dims() == 1 && a->min(0))) {
    int rI;
    if (a->dims() == 1) {
      rI = (locate_range(a->min(0), a->max(0), cg, frag));
    } else {
      rI = GET_REG(cg);
      OPEN_VEC(cg, frag);
      for (int ii = 0; ii < a->dims(); ++ii) {
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(a->min(ii), cg, frag)));
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(a->max(ii), cg, frag)));
      }
      CLOSE_AGG(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rI));
    }

    // Combine array and index sets
    PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"), CG::r(r), CG::r(rI));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  }

  return {r, CG_Cond::forall(ctx, p_vec)};
}

std::pair<int, CG_Cond::T> execute_array_access(int r_A, std::vector<int> r_idxs, CodeGen& cg,
                                                CG_Builder& frag) {
  int sz = r_idxs.size();
  int r(GET_REG(cg));
  int r_cond(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::GET_ARRAY, CG::i(sz), CG::r(r_A));
  std::vector<CG_Value> r_args(sz + 2);
  for (int ii = 0; ii < sz; ++ii) {
    r_args[ii] = CG::r(r_idxs[ii]);
  }
  r_args[sz] = CG::r(r);
  r_args[sz + 1] = CG::r(r_cond);
  CG_Instr& i = frag.instrs.back();
  PUSH_INSTR_OPERAND(i, r_args);
  return {r, CG_Cond::reg(r_cond, true)};
}

CG::Binding CG::bind(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  GCLock lock;
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());

  bool is_var = false;
  for (int ii = 0; ii < sz; ++ii) {
    is_var = is_var || idx[ii]->type().isvar();
  }

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T> cond;

  // Now evaluate the array body, and emit the indices.
  std::vector<int> r_idxs(sz);
  for (int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    cond.push_back(b.second);
  }

  Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  cond.push_back(b_A.second);

  if (is_var) {
    int r = GET_REG(cg);

    std::vector<Type> types(sz + 1, Type::varint());
    types[sz] = Type::varint(sz);
    auto fun = find_call_fun(cg, {"element"}, Type::varint(), types, BytecodeProc::FUN);
    assert(std::get<1>(fun) == BytecodeProc::FUN);

    std::vector<CG_Value> r_args(sz + 1);
    for (int ii = 0; ii < sz; ++ii) {
      r_args[ii] = CG::r(r_idxs[ii]);
    }
    r_args[sz] = CG::r(r_A);

    // Push CALL instruction with the correct id
    PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
               CG::i(!std::get<2>(fun)));
    // Append instruction with register arguments
    CG_Instr& i = frag.instrs.back();
    PUSH_INSTR_OPERAND(i, r_args);

    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    return {r, CG_Cond::forall(ctx, cond)};
  } else {
    // Just read the vector, and get the appropriate element.
    int r;
    CG_Cond::T ncond;
    std::tie(r, ncond) = execute_array_access(r_A, r_idxs, cg, frag);
    cond.push_back(ncond);
    return {r, CG_Cond::forall(ctx, cond)};
  }
}

int make_vec(CodeGen& cg, CG_Builder& frag, const std::vector<int>& regs) {
  OPEN_VEC(cg, frag);
  for (int r : regs) PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  CLOSE_AGG(cg, frag);
  int r_vec(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_vec));
  return r_vec;
}

void call_clause(CodeGen& cg, CG_Builder& frag, Mode ctx, const std::vector<int>& pos,
                 const std::vector<int>& neg) {
  GCLock lock;
  // Build the arguments vectors.
  int r_pos(make_vec(cg, frag, pos));
  int r_neg(make_vec(cg, frag, neg));
  auto fun =
      find_call_fun(cg, {"clause"}, Type::varbool(), {Type::varbool(1), Type::varbool(1)}, ctx);
  assert(BytecodeProc::is_neg(ctx) == BytecodeProc::is_neg(std::get<1>(fun)));
  PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
             CG::i(!std::get<2>(fun)), CG::r(r_pos), CG::r(r_neg));
}

int deinterlace(CodeGen& cg, CG_Builder& frag, int vec, int width, int offset) {
  int r_sz(GET_REG(cg));
  int r_slice(GET_REG(cg));

  OPEN_VEC(cg, frag);
  // int r_i(CG::locate_immi(1 + offset, cg, frag));
  // int r_step(CG::locate_immi(width, cg, frag));
  int r_i(bind_cst(1 + offset, cg, frag));
  int r_step(bind_cst(width, cg, frag));
  int r_elt(GET_REG(cg));

  PUSH_INSTR(frag, BytecodeStream::LENGTH, CG::r(vec), CG::r(r_sz));
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
  int sz(ite->size());
  int r_one(bind_cst(1, cg, frag));

  bool all_par = true;
  for (int ii = 0; ii < sz; ++ii) {
    if (!ite->e_if(ii)->type().ispar()) {
      all_par = false;
      break;
    }
  }

  if (all_par) {
    int l_end(GET_LABEL(cg));
    int r_ret(GET_REG(cg));
    int r_cond(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(1), CG::r(r_cond));

    bool is_total = true;
    // We need to interfere with the env, here, because stuff may not be available.
    // Except, ite->e_if(0) will always be available.
    for (int ii = 0; ii < sz; ++ii) {
      int r_sel = CG::force(CG::compile(ite->e_if(ii), cg, frag), BytecodeProc::FUN, cg, frag);
      int l_cont(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sel), CG::l(l_cont));
      cg.env_push();
      if (!ite->e_then(ii)->type().ispar()) all_par = false;
      CG::Binding b_res(CG::bind(ite->e_then(ii), cg, frag));
      if (b_res.second.get()) {
        is_total = false;
        int r_condii(CG::force(b_res.second, ctx, cg, frag));
        PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_condii), CG::r(r_cond));
      } else if (!b_res.second.sign()) {
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_cond));
      }
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_res.first), CG::r(r_ret));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));
      cg.env_pop();
      cg.env_push();
      PUSH_LABEL(frag, l_cont);
    }
    // Else case.
    if (!ite->e_else()->type().ispar()) all_par = false;

    CG::Binding b_res(CG::bind(ite->e_else(), cg, frag));
    if (b_res.second.get()) {
      is_total = false;
      int r_condii(CG::force(b_res.second, ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_condii), CG::r(r_cond));
    } else if (!b_res.second.sign()) {
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_cond));
    }
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(b_res.first), CG::r(r_ret));
    PUSH_LABEL(frag, l_end);
    // Now kill the availability of all the expressions.
    for (int ii = 0; ii < sz; ++ii) {
      cg.env_pop();
    }
    return {r_ret, is_total ? CG_Cond::ttt() : CG_Cond::reg(r_cond, all_par)};
  } else {
    GCLock lock;
    std::vector<int> r_cond;
    std::vector<int> p_res;
    std::vector<int> r_res;

    bool is_total = true;
    for (int ii = 0; ii < sz; ++ii) {
      r_cond.push_back(
          CG::force(CG::compile(ite->e_if(ii), cg, frag), BytecodeProc::FUN, cg, frag));
      CG::Binding b_res(CG::bind(ite->e_then(ii), cg, frag));
      r_res.push_back(b_res.first);
      // Make sure b_res.second is evaluated _outside_ the aggregation.
      if (b_res.second.get()) {
        is_total = false;
        p_res.push_back(CG::force(b_res.second, ctx, cg, frag));
      } else {
        if (b_res.second.sign()) {
          p_res.push_back(bind_cst(0, cg, frag));
        } else {
          p_res.push_back(r_one);
        }
      }
    }
    Binding b_final = CG::bind(ite->e_else(), cg, frag);
    r_cond.push_back(r_one);
    r_res.push_back(b_final.first);
    if (b_final.second.get()) {
      is_total = false;
      p_res.push_back(CG::force(b_final.second, ctx, cg, frag));
    } else {
      if (b_final.second.sign()) {
        p_res.push_back(bind_cst(0, cg, frag));
      } else {
        p_res.push_back(r_one);
      }
    }

    int r_vec_cond(vec2array(r_cond, cg, frag));

    int r_part;
    if (!is_total) {
      int r_vec_part(vec2array(p_res, cg, frag));

      auto fun = find_call_fun(cg, {"if_then_else_partiality"}, Type::varbool(),
                               {Type::varbool(1), Type::varbool(1)}, BytecodeProc::FUN);
      assert(std::get<1>(fun) == BytecodeProc::FUN);

      PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
                 CG::i(!std::get<2>(fun)), CG::r(r_vec_cond), CG::r(r_vec_part));
      r_part = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_part));
    }

    int r_vec_res(vec2array(r_res, cg, frag));

    auto fun = find_call_fun(cg, {"if_then_else"}, Type::varint(),
                             {Type::varbool(1), Type::varint(1)}, BytecodeProc::FUN);
    assert(std::get<1>(fun) == BytecodeProc::FUN);

    int r_ret(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::CALL, std::get<1>(fun), std::get<0>(fun),
               CG::i(!std::get<2>(fun)), CG::r(r_vec_cond), CG::r(r_vec_res));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

    return {r_ret, is_total ? CG_Cond::ttt() : CG_Cond::reg(r_part, false)};
  }
}

CG::Binding CG::bind(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if (b->type().ispar()) {
    CG::Binding b_lhs(CG::force_or_bind(b->lhs(), ctx, cg, frag));
    CG::Binding b_rhs(CG::force_or_bind(b->rhs(), ctx, cg, frag));
    if (b->lhs()->type().isintset()) {
      int r = bind_binop_par_set(cg, frag, b->op(), b_lhs.first, b_rhs.first);
      return {r, CG_Cond::ttt()};
    }
    std::vector<CG_Cond::T> cond = {b_lhs.second, b_rhs.second};

    int l_skip;
    int r_ret;
    if (b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
      l_skip = GET_LABEL(cg);
      int r_cond = GET_REG(cg);
      r_ret = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r_ret));
      cond.push_back(CG_Cond::reg(r_cond, true));
      PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(b_rhs.first), CG::r(r_ret), CG::r(r_cond));
      PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_cond), CG::r(r_cond));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_cond), CG::l(l_skip));
    }
    int r = bind_binop_par_int(cg, frag, b->op(), b_lhs.first, b_rhs.first);
    if (b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r), CG::r(r_ret));
      r = r_ret;
      PUSH_LABEL(frag, l_skip);
    }

    return {r, CG_Cond::forall(ctx, cond)};
  } else {
    std::vector<CG_Cond::T> partial;
    int r_lhs = CG::force_or_bind(b->lhs(), ctx, partial, cg, frag);
    int r_rhs = CG::force_or_bind(b->rhs(), ctx, partial, cg, frag);

    int r = GET_REG(cg);
    bind_binop_var(cg, frag, BytecodeProc::FUN, b->op(), r_lhs, r_rhs);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

    if (b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
      // These all require rhs() != 0. Which means we need to evaluate the rhs,
      // and put it in a register.
      int r_zero(bind_cst(0, cg, frag));
      partial.push_back(~linear_cond(cg, frag, BOT_EQ, -ctx, r_rhs, r_zero));
    }
    return {r, CG_Cond::forall(ctx, partial)};
  }
}

CG::Binding CG::bind(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Unop is easy.
  switch (u->op()) {
    case UOT_NOT:
      throw InternalError("Non-Boolean bind called on boolean operator.");
    case UOT_PLUS:
      return CG::bind(u->e(), cg, frag);
    case UOT_MINUS: {
      Binding b_e(CG::bind(u->e(), cg, frag));
      int r;
      if (u->type().isvar()) {
        GCLock lock;
        auto fun =
            find_call_fun(cg, {"op_minus"}, Type::varint(), {Type::varint()}, BytecodeProc::FUN);
        assert(std::get<1>(fun) == BytecodeProc::FUN);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
                   CG::i(!std::get<2>(fun)), CG::r(b_e.first));
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      } else {
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r), CG::r(b_e.first), CG::r(r));
      }
      return {r, b_e.second};
    }
  }
}

CG::Binding bind_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Collects the partiality of the arguments
  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  std::vector<CG_Cond::T> p_arg(sz);
  for (int ii = 0; ii < sz; ++ii) {
    CG::Binding r_bind(CG::force_or_bind(call->arg(ii), BytecodeProc::FUN, cg, frag));
    r_arg[ii] = CG::r(r_bind.first);
    p_arg[ii] = r_bind.second;
  }

  // Bind the value part.
  auto fun = find_call_fun(cg, call, BytecodeProc::ROOT);
  assert(std::get<1>(fun) == BytecodeProc::FUN);
  PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, std::get<0>(fun),
             CG::i((!std::get<2>(fun)) && call->type().isvar()), r_arg);

  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  return std::make_pair(r_ret, CG_Cond::forall(ctx, p_arg));
}

CG::Binding CG::bind(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if (it != builtins().end()) {
      return (*it).second.general(call, ctx, cg, frag);
    }
  }

  return bind_call(call, ctx, cg, frag);
}

CG::Binding CG::bind(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> partial;

  cg.env_push();

  if (let->ann().contains(constants().ann.promise_total)) {
    OPEN_OTHER(cg, frag);
    eval_let_body(let, BytecodeProc::ROOT, cg, frag, partial);

    CG::Binding b_in(CG::bind(let->in(), cg, frag));
    partial.push_back(b_in.second);
    post_cond(cg, frag, CG_Cond::forall(BytecodeProc::ROOT, partial));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(b_in.first));
    CLOSE_AGG(cg, frag);
    cg.env_pop();

    int ret = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(ret));

    return {ret, CG_Cond::forall(ctx, partial)};
  }

  eval_let_body(let, BytecodeProc::ROOT, cg, frag, partial);
  CG::Binding b_in(CG::bind(let->in(), cg, frag));
  partial.push_back(b_in.second);

  cg.env_pop();
  return {b_in.first, CG_Cond::forall(ctx, partial)};
}

CG::Binding CG::bind(Comprehension* comp, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Lift out the outer-most comprehension, since it will always
  // be executed.
  CG::bind(comp->in(0), cg, frag);

  cg.env_push();
  OPEN_VEC(cg, frag);
  execute_comprehension_bind(comp, ctx, cg, frag);
  CLOSE_AGG(cg, frag);
  cg.env_pop();
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  if (comp->type().is_set()) {
    PUSH_INSTR(frag, BytecodeStream::MAKE_SET, CG::r(r), CG::r(r));
  }
  return CG::Binding(r, CG_Cond::ttt());
}

CG_Cond::T _compile(Expression* e, CodeGen& cg, CG_Builder& frag) {
  // Look up the mode we need to compile e in.
  CG::Mode ctx(BytecodeProc::FUN);
  try {
    ctx = cg.mode_map.at(e);
  } catch (const std::out_of_range& exn) {
    std::cerr << "%% Missing mode for: ";
    debugprint(e);
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
  } catch (const CG_Env<CG::Binding>::NotFound& exn) {
    CG_Cond::T cond(_compile(e, cg, frag));
    CG::Binding b(0xdeadbeef, cond);
    cg.cache_store(e, b);
    return cond;
  }
  // return _compile(e, cg, frag); // FIXME: Update env representation.
}

CG_Cond::T CG::compile(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    CG::Binding b(cg.env().lookup(x->v()));
    // Check if there is a complex condition
    if (b.second.get()) {
      // If so then the register is a placeholder and the condition holds the value
      assert(b.first == 0xdeadbeef);
      return b.second;
    } else {
      // Otherwise the value should be in the register
      return CG_Cond::reg(b.first, x->type().ispar());
    }
  } catch (const CG_Env<Binding>::NotFound& exn) {
    VarDecl* vd = follow_id_to_decl(x)->cast<VarDecl>();
    int g = cg.find_global(vd);
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG_Cond::reg(r, x->type().ispar());
  }
}

CG_Cond::T CG::compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());

  bool is_var = false;
  for (int ii = 0; ii < sz; ++ii) {
    is_var = is_var || idx[ii]->type().isvar();
  }

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T> cond;

  // Now evaluate the array body, and emit the indices.
  std::vector<int> r_idxs(sz);
  for (int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    cond.push_back(b.second);
  }

  CG::Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  cond.push_back(b_A.second);

  if (is_var) {
    GCLock lock;
    std::vector<Type> types(sz + 2, Type::varint());
    types[0] = Type::varbool();
    types[sz + 1] = Type::varbool(sz);

    std::vector<CG_Value> r_args(sz + 1);
    for (int ii = 0; ii < sz; ++ii) {
      r_args[ii] = CG::r(r_idxs[ii]);
    }
    r_args[sz] = CG::r(r_A);

    cond.push_back(CG_Cond::call({"element"}, -ctx, true, types, r_args));
    return CG_Cond::forall(ctx, cond);
  } else {
    // Just read the vector, and get the appropriate element.
    int r;
    CG_Cond::T ncond;
    std::tie(r, ncond) = execute_array_access(r_A, r_idxs, cg, frag);
    cond.push_back(ncond);
    cond.push_back(CG_Cond::reg(r, a->type().ispar()));
    return CG_Cond::forall(ctx, cond);
  }
}

CG_Cond::T CG::compile(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int sz(ite->size());

  // Check whether all the conditions are par.
  bool all_par = true;
  for (int ii = 0; ii < sz; ++ii) {
    if (!ite->e_if(ii)->type().ispar()) {
      all_par = false;
      break;
    }
  }

  if (all_par) {
    // We need to interfere with the env, here, because stuff may not be available.
    // Except, ite->e_if(0) will always be available.
    int l_end(GET_LABEL(cg));
    int r_ret(GET_REG(cg));
    for (int ii = 0; ii < sz; ++ii) {
      int r_sel = CG::force(CG::compile(ite->e_if(ii), cg, frag), BytecodeProc::FUN, cg, frag);
      int l_cont(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_sel), CG::l(l_cont));
      cg.env_push();
      if (!ite->e_then(ii)->type().ispar()) all_par = false;
      int r_val = CG::force(CG::compile(ite->e_then(ii), cg, frag), ctx, cg, frag);
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_val), CG::r(r_ret));
      PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));
      cg.env_pop();
      cg.env_push();
      PUSH_LABEL(frag, l_cont);
    }
    // Else case.
    if (!ite->e_else()->type().ispar()) all_par = false;
    int r_val = CG::force(CG::compile(ite->e_else(), cg, frag), ctx, cg, frag);
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_val), CG::r(r_ret));
    PUSH_LABEL(frag, l_end);
    // Now kill the availability of all the expressions.
    for (int ii = 0; ii < sz; ++ii) cg.env_pop();
    return CG_Cond::reg(r_ret, all_par);
  } else {
    GCLock lock;

    // Collect conditions
    OPEN_VEC(cg, frag);
    for (int ii = 0; ii < sz; ++ii) {
      int r_if(CG::force(CG::compile(ite->e_if(ii), cg, frag), BytecodeProc::FUN, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_if));
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(1, cg, frag)));
    CLOSE_AGG(cg, frag);
    int r_if(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_if));

    // Collect results
    OPEN_VEC(cg, frag);
    for (int ii = 0; ii < sz; ++ii) {
      int r_then(CG::force(CG::compile(ite->e_then(ii), cg, frag), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_then));
    }
    {
      int r_else(CG::force(CG::compile(ite->e_else(), cg, frag), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_else));
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(bind_cst(1, cg, frag)));
    CLOSE_AGG(cg, frag);
    int r_then(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_then));

    return CG_Cond::call({"if_then_else"}, BytecodeProc::FUN, true,
                         {Type::varbool(), Type::varbool(1), Type::varbool(1)},
                         {CG::r(r_if), CG::r(r_then)});
  }
}

CG_Cond::T shortcut_par_or(Mode ctx, CodeGen& cg, CG_Builder& frag, Expression* lhs,
                           Expression* rhs) {
  Mode f_mode(ctx.strength(), false);  // Check
  assert(lhs->type().ispar());
  int r(GET_REG(cg));
  int l_exit(GET_LABEL(cg));
  int r_lhs(CG::force(CG::compile(lhs, cg, frag), f_mode, cg, frag));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_lhs), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::JMPIF, CG::r(r), CG::l(l_exit));
  cg.env_push();
  int r_rhs(CG::force(CG::compile(rhs, cg, frag), f_mode, cg, frag));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_rhs), CG::r(r));
  cg.env_pop();
  PUSH_LABEL(frag, l_exit);
  return CG_Cond::reg(r, rhs->type().ispar());
}

CG_Cond::T shortcut_par_and(Mode ctx, CodeGen& cg, CG_Builder& frag, Expression* lhs,
                            Expression* rhs) {
  Mode f_mode(ctx.strength(), false);  // Check
  assert(lhs->type().ispar());
  int r(GET_REG(cg));
  int l_exit(GET_LABEL(cg));
  int r_lhs(CG::force(CG::compile(lhs, cg, frag), f_mode, cg, frag));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_lhs), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r), CG::l(l_exit));
  cg.env_push();
  int r_rhs(CG::force(CG::compile(rhs, cg, frag), f_mode, cg, frag));
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_rhs), CG::r(r));
  cg.env_pop();
  PUSH_LABEL(frag, l_exit);
  return CG_Cond::reg(r, rhs->type().ispar());
}

CG_Cond::T CG::compile(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> cond;
  // For anything we force here, its calling mode is considered positive (because
  // it needs to mean what we said.)
  CG::Mode f_mode(ctx.strength(), false);
  if (b->lhs()->type().dim() > 0) {
    GCLock lock;
    assert(b->rhs()->type().dim() > 0);
    assert(b->op() == BOT_EQ || b->op() == BOT_EQUIV);
    int r_lhs = CG::force_or_bind(b->lhs(), f_mode, cond, cg, frag);
    int r_rhs = CG::force_or_bind(b->rhs(), f_mode, cond, cg, frag);
    cond.push_back(CG_Cond::call(
        {"op_equals"}, BytecodeProc::FUN, true,
        {b->type().isvar() ? Type::varbool() : Type::parbool(), b->lhs()->type(), b->rhs()->type()},
        {CG::r(r_lhs), CG::r(r_rhs)}));
    return CG_Cond::forall(ctx, cond);
  }
  if (b->op() == BOT_AND) {
    if (b->lhs()->type().ispar()) return shortcut_par_and(ctx, cg, frag, b->lhs(), b->rhs());
    if (b->rhs()->type().ispar()) return shortcut_par_and(ctx, cg, frag, b->rhs(), b->lhs());
  } else if (b->op() == BOT_OR) {
    if (b->lhs()->type().ispar()) return shortcut_par_or(ctx, cg, frag, b->lhs(), b->rhs());
    if (b->rhs()->type().ispar()) return shortcut_par_or(ctx, cg, frag, b->rhs(), b->lhs());
  }
  if (b->type().ispar()) {
    int r_lhs = CG::force_or_bind(b->lhs(), f_mode, cond, cg, frag);
    int r_rhs = CG::force_or_bind(b->rhs(), f_mode, cond, cg, frag);
    std::vector<int> r_cond;
    for (CG_Cond::T c : cond) {
      // r_cond.push_back(CG::force(c, ctx, cg, frag));
      // FIXME: This is probably safe, assuming all the conditions
      // are also par. We need these to be in their literal,
      // non-context-adjusted modes for the disentailment check to work.
      // If we want to handle non-par conditions, we probably need to
      // force in ctx, then flip back.
      r_cond.push_back(CG::force(c, f_mode, cg, frag));
    }
    int r_ret;
    if (b->lhs()->type().isintset()) {
      r_ret = bind_binop_par_set(cg, frag, b->op(), r_lhs, r_rhs);
    } else {
      r_ret = bind_binop_par_int(cg, frag, b->op(), r_lhs, r_rhs);
    }
    if (r_cond.size() > 0) {
      // If any conditions don't hold, evaluate to false.
      int r(GET_REG(cg));
      int l_tl(GET_LABEL(cg));
      PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
      for (int r_c : r_cond) {
        PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_c), CG::l(l_tl));
      }
      PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_ret), CG::r(r));
      PUSH_LABEL(frag, l_tl);
      return CG_Cond::reg(r, true);
    } else {
      return CG_Cond::reg(r_ret, true);
    }
  }
  switch (b->op()) {
    case BOT_LE:
    case BOT_LQ:
    case BOT_GR:
    case BOT_GQ:
    case BOT_EQ:
    case BOT_EQUIV:
    case BOT_NQ: {
      // Potentially partial.
      int r_lhs = CG::force_or_bind(b->lhs(), f_mode, cond, cg, frag);
      int r_rhs = CG::force_or_bind(b->rhs(), f_mode, cond, cg, frag);
      cond.push_back(linear_cond(cg, frag, b->op(), ctx, r_lhs, r_rhs));
      return CG_Cond::forall(ctx, cond);
    } break;
    case BOT_XOR: {
      CG_Cond::T c_lhs = CG::compile(b->lhs(), cg, frag);
      CG_Cond::T c_rhs = CG::compile(b->rhs(), cg, frag);
      return CG_Cond::forall(ctx, CG_Cond::exists(ctx, c_lhs, c_rhs),
                             CG_Cond::exists(ctx, ~c_lhs, ~c_rhs));
    } break;
    case BOT_AND: {
      cond.push_back(CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::forall(ctx, cond);
    } break;
    case BOT_OR: {
      cond.push_back(CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::exists(ctx, cond);
    } break;
    case BOT_IMPL: {
      cond.push_back(~CG::compile(b->lhs(), cg, frag));
      cond.push_back(CG::compile(b->rhs(), cg, frag));
      return CG_Cond::exists(ctx, cond);
    } break;
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
      if (ctx == BytecodeProc::ROOT) {
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(b_lhs.first), CG::r(b_rhs.first),
                   CG::r(r));
        // If INTERSECT_DOMAIN fails, then the interpreter will mark inconsistent and ABORT
        cond.push_back(CG_Cond::ttt());
      } else if (ctx == BytecodeProc::ROOT_NEG) {
        int r(GET_REG(cg));
        PUSH_INSTR(frag, BytecodeStream::DOM, CG::r(b_lhs.first), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::DIFF, CG::r(r), CG::r(b_rhs.first), CG::r(r));
        PUSH_INSTR(frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(b_lhs.first), CG::r(r), CG::r(r));
        // If INTERSECT_DOMAIN fails, then the interpreter will mark inconsistent and ABORT
        cond.push_back(CG_Cond::ttt());
      } else {
        GCLock lock;
        cond.push_back(CG_Cond::call({"set_in"}, ctx, true,
                                     {Type::varbool(), Type::varint(), Type::varsetint()},
                                     {CG::r(b_lhs.first), CG::r(b_rhs.first)}));
      }
      return CG_Cond::forall(ctx, cond);
    }
    default:
      break;
  }
  throw InternalError("Unexpected fall-through in compilation of BinOp.");
}

CG_Cond::T CG::compile(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(u->op() == UOT_NOT);
  return ~CG::compile(u->e(), cg, frag);
}

CG_Cond::T compile_call(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  std::vector<CG_Cond::T> p_arg;

  // Evaluate the args, collecting the conditionality.
  std::vector<Type> arg_types(sz + 1);
  arg_types[0] = call->type();
  for (int ii = 0; ii < sz; ++ii) {
    arg_types[ii + 1] = call->arg(ii)->type();
    r_arg[ii] = CG::r(CG::force_or_bind(call->arg(ii), BytecodeProc::FUN, p_arg, cg, frag));
  }

  // And finally, add the call itself
  p_arg.push_back(CG_Cond::call(call->id(), ctx, call->type().isvar(), arg_types, r_arg));
  return CG_Cond::forall(ctx, p_arg);
}

CG_Cond::T CG::compile(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If we have a builtin for this, dispatch to that instead.
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if (it != builtins().end()) {
      return it->second.boolean(call, ctx, cg, frag);
    }
  }

  return compile_call(call, ctx, cg, frag);
}

void eval_let_body(Let* let, BytecodeProc::Mode ctx, CodeGen& cg, CG_Builder& frag,
                   std::vector<CG_Cond::T>& conj) {
  ASTExprVec<Expression> bindings(let->let());
  for (Expression* e : bindings) {
    if (auto vd = e->dyn_cast<VarDecl>()) {
      // Bind the new definitions in context
      CodeGen::Binding to_bind(0xdeadbeef, CG_Cond::ttt());
      if (vd->e() && vd->type().isbool()) {
        CG_Cond::T cond = CG::compile(vd->e(), cg, frag);
        if (!cond.get()) {
          to_bind.first = bind_cst(cond.sign(), cg, frag);
        } else {
          to_bind.second = cond;
        }
      } else if (vd->e()) {
        // FIXME: Assuming domain isn't constraining.
        CG::Binding b_v(CG::bind(vd->e(), cg, frag));
        to_bind.first = b_v.first;
        conj.push_back(b_v.second);
        if (Expression* d = vd->ti()->domain()) {
          if (!vd->ti()->isarray()) {
            CG::Binding b_d = CG::bind(d, cg, frag);  // Discarding any constraints on the set.
            conj.push_back(b_d.second);
            int r_dp = GET_REG(cg);
            PUSH_INSTR(frag, BytecodeStream::INTERSECT_DOMAIN, CG::r(to_bind.first),
                       CG::r(b_d.first), CG::r(r_dp));
          }
        }
      } else {
        // Variable declaration. Assumes is total and nonempty.
        CG::Binding b_d(bind_domain(vd, cg, frag));
        to_bind.first = GET_REG(cg);
        if (!vd->ti()->isarray()) {
          conj.push_back(b_d.second);
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::ROOT, cg.find_builtin("mk_intvar"),
                     CG::i(0), CG::r(b_d.first));
        } else {
          int rTMP(GET_REG(cg));
          std::vector<int> r_regs;
          for (Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            CG::Binding b_reg(CG::bind(dim, cg, frag));
            // post_cond(cg, frag, b_reg.second);
            r_regs.push_back(b_reg.first);
          }
          std::vector<Forset> nesting;
          OPEN_VEC(cg, frag);
          for (int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(frag);
          }
          PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::ROOT, cg.find_builtin("mk_intvar"),
                     CG::i(0), CG::r(b_d.first));
          for (int r_i = r_regs.size() - 1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(frag);
          }
          CLOSE_AGG(cg, frag);
          PUSH_INSTR(frag, BytecodeStream::POP, CG::r(to_bind.first));
          OPEN_VEC(cg, frag);
          for (int r_r : r_regs) {
            PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_r), CG::r(bind_cst(1, cg, frag)),
                       CG::r(rTMP));
            PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rTMP));
            PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_r), CG::r(bind_cst(2, cg, frag)),
                       CG::r(rTMP));
            PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(rTMP));
          }
          CLOSE_AGG(cg, frag);
          PUSH_INSTR(frag, BytecodeStream::POP, CG::r(rTMP));

          PUSH_INSTR(frag, BytecodeStream::BUILTIN, cg.find_builtin("array_Xd"),
                     CG::r(to_bind.first), CG::r(rTMP));
        }
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(to_bind.first));
      }
      cg.env().bind(vd->id()->v(), to_bind);
    } else {
      conj.push_back(CG::compile(e, cg, frag));
    }
  }
}

CG_Cond::T CG::compile(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T> conj;

  cg.env_push();

  if (let->ann().contains(constants().ann.promise_total)) {
    OPEN_OTHER(cg, frag);
    eval_let_body(let, BytecodeProc::ROOT, cg, frag, conj);
    // int r = CG::force(CG_Cond::forall(ctx, conj), BytecodeProc::ROOT, cg, frag);
    post_cond(cg, frag, CG_Cond::forall(BytecodeProc::ROOT, conj));
    int r = CG::force(CG::compile(let->in(), cg, frag), ctx, cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    CLOSE_AGG(cg, frag);
    r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    conj.clear();
    conj.push_back(CG_Cond::reg(r, let->in()->type().ispar()));
  } else {
    eval_let_body(let, BytecodeProc::ROOT, cg, frag, conj);
    conj.push_back(CG::compile(let->in(), cg, frag));
  }
  cg.env_pop();
  return CG_Cond::forall(ctx, conj);
}

int CG::vec2array(std::vector<int> vec, CodeGen& cg, CG_Builder& frag) {
  int sz(vec.size());
  OPEN_VEC(cg, frag);
  for (int ii = 0; ii < sz; ++ii) {
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(vec[ii]));
  }
  CLOSE_AGG(cg, frag);
  int r_vec(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_vec));

  return r_vec;
}
};  // namespace MiniZinc
