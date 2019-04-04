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

using Mode = CG::Mode;

struct builtin_t {
  std::function<CG_Cond::T*(Call*, Mode, CodeGen&, CG_Builder&)> boolean;
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
      
      "OPEN_AGGREGATION",
      "CLOSE_AGGREGATION",
      "SIMPLIFY_LIN",
      
      "PUSH",
      "POP",
      
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
  register_builtin("mk_intvar", 1);

  register_builtin("bool_not", 1);
  register_builtin("bool_clause", 2);
  
  register_builtin("int_eq", 2);
  register_builtin("int_lt", 2);
  register_builtin("int_le", 2);

  register_builtin("int_plus", 2);
  register_builtin("int_minus", 2);
  register_builtin("int_times", 2);
  register_builtin("int_pow", 2);
  register_builtin("int_div", 2);

  register_builtin("int_element", 2);
  register_builtin("bool_element", 2);

  register_builtin("float_div", 2);

  register_builtin("absent", 1);
}

void OPEN_AGG(CodeGen& cg, CG_Builder& frag, AggregationCtx::Symbol ctx) {
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
}

void OPEN_AND(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_AND); }
void OPEN_OR(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OR); }
void OPEN_OTHER(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_OTHER); }
void OPEN_VEC(CodeGen& cg, CG_Builder& frag) { OPEN_AGG(cg, frag, AggregationCtx::VCTX_VEC); }

void CodeGen::register_builtin(std::string s, unsigned int arity) {
  auto it(_proc_map.find(s));
  assert(it == _proc_map.end());

  CG_ProcID id(CG_ProcID::builtin(_builtins.size()));
  _builtins.push_back(std::make_pair(s, arity));
  _proc_map.insert(std::make_pair(s, id));
}

CG_ProcID CodeGen::find_builtin(std::string s) {
  auto it(_proc_map.find(s));
  assert(it != _proc_map.end());
  return (*it).second;
}

// template<class O>
// struct 
void call_binop(CodeGen& cg, CG_Builder& frag, Mode ctx, BinOpType op, int r_lhs, int r_rhs) {
  switch(op) {
    // Actual builtins
    case BOT_EQ:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_eq"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    case BOT_LE:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_le"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    case BOT_PLUS:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_plus"), CG::r(r_lhs), CG::r(r_rhs));
    case BOT_MINUS:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_plus"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    case BOT_MULT:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_times"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    case BOT_IDIV:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_div"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    case BOT_DIV:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("float_div"), CG::r(r_lhs), CG::r(r_rhs));
      return;
    // Normalisation
    case BOT_NQ:
      call_binop(cg, frag, -ctx, BOT_EQ, r_lhs, r_rhs);
      return;
    case BOT_LQ:
      call_binop(cg, frag, -ctx, BOT_LE,  r_rhs, r_lhs);
      return;
    case BOT_GR:
      call_binop(cg, frag, -ctx, BOT_LE, r_lhs, r_rhs);
      return;
    case BOT_GQ:
      call_binop(cg, frag, ctx, BOT_LE, r_rhs, r_lhs);
      return;

    case BOT_XOR:
      call_binop(cg, frag, -ctx, BOT_EQUIV, r_lhs, r_rhs);
      return;
    case BOT_RIMPL:
      call_binop(cg, frag, ctx, BOT_IMPL, r_rhs, r_lhs);
      return;
    case BOT_DOTDOT:
      // The values in r_lhs and r_rhs had better be IMMIs.
      OPEN_VEC(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_lhs));
      PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_rhs));
      CLOSE_AGG(cg, frag);
      return;
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
    default:
      TODO();
    /*
    // BOT_PLUS, BOT_MINUS, BOT_MULT, BOT_DIV, BOT_IDIV, BOT_MOD, BOT_POW,
    // BOT_LE, BOT_LQ, BOT_GR, BOT_GQ, BOT_EQ, BOT_NQ,
    // BOT_IN, BOT_SUBSET, BOT_SUPERSET, BOT_UNION, BOT_DIFF, BOT_SYMDIFF,
    // BOT_INTERSECT,
    // BOT_PLUSPLUS,
    */
  }
}

CG_Cond::T* binop_cond(CodeGen& cg, BinOpType op, Mode ctx, int r_lhs, int r_rhs) {
  switch(op) {
    // Actual builtins
    case BOT_EQ:
      return CG_Cond::call(cg.find_builtin("int_eq"), ctx, CG::r(r_lhs), CG::r(r_rhs));
    case BOT_LE:
      return CG_Cond::call(cg.find_builtin("int_le"), ctx, CG::r(r_lhs), CG::r(r_rhs));
    // Normalisation
    case BOT_NQ:
      return binop_cond(cg, BOT_EQ, -ctx, r_lhs, r_rhs);
    case BOT_LQ:
      return binop_cond(cg, BOT_LE, -ctx, r_rhs, r_lhs);
    case BOT_GR:
      return binop_cond(cg, BOT_LE, -ctx, r_lhs, r_rhs);
    case BOT_GQ:
      return binop_cond(cg, BOT_LE, ctx, r_rhs, r_lhs);
    case BOT_XOR:
      return binop_cond(cg, BOT_EQUIV, -ctx, r_lhs, r_rhs);
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

CG_ProcID find_call_fun(CodeGen& cg, Call* c) {
  return CG_ProcID::proc(0xdead);
}
CG_ProcID find_call_pred(CodeGen& cg, Call* c) {
  return CG_ProcID::proc(0xbead);
}

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

// Manipulating conditions
void force_and_leaves(std::vector<int>& leaves, CG_Cond::T* child, CodeGen& cg, CG_Builder& frag) {
  if(!child)
    return;
  // assert(child);
  if(child->reg != -1) {
    leaves.push_back(child->reg);
  } else if(child->kind() == CG_Cond::CC_And) {
    std::vector<CG_Cond::T*>& children(static_cast<CG_Cond::C_And*>(child)->children);
    for(CG_Cond::T* c : children)
      force_and_leaves(leaves, c, cg, frag);
  } else {
    leaves.push_back(CG::force(child, cg, frag));
  }
}

void force_or_leaves(std::vector<int>& leaves, CG_Cond::T* child, CodeGen& cg, CG_Builder& frag) {
  assert(child);
  if(child->reg != -1) {
    leaves.push_back(child->reg);
  } else if(child->kind() == CG_Cond::CC_Or) {
    std::vector<CG_Cond::T*>& children(static_cast<CG_Cond::C_And*>(child)->children);
    for(CG_Cond::T* c : children)
      force_and_leaves(leaves, c, cg, frag);
  } else {
    leaves.push_back(CG::force(child, cg, frag));
  }
}

int _force_cond(CG_Cond::T* cond, CodeGen& cg, CG_Builder& frag) {
  assert(cond);
  switch(cond->kind()) {
    case CG_Cond::CC_Reg:
      return cond->reg;
    case CG_Cond::CC_Call: { // Evaluate the call, put the result in a register.
      CG_Cond::C_Call* call(static_cast<CG_Cond::C_Call*>(cond));
      OPEN_OTHER(cg, frag);
      PUSH_INSTR(frag, BytecodeStream::CALL, call->m, call->p, call->params);
      CLOSE_AGG(cg, frag);
      int r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    }
    case CG_Cond::CC_And: {
      std::vector<int> leaves;
      force_and_leaves(leaves, cond, cg, frag);
      OPEN_OTHER(cg, frag);
      OPEN_AND(cg, frag);
      for(int r_c : leaves)
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      int r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    }
    case CG_Cond::CC_Or: {
      std::vector<int> leaves;
      force_or_leaves(leaves, cond, cg, frag);
      OPEN_OTHER(cg, frag);
      OPEN_AND(cg, frag);
      for(int r_c : leaves)
        PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
      CLOSE_AGG(cg, frag);
      CLOSE_AGG(cg, frag);
      int r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      return r;
    }
  }
}
int CG::force(CG_Cond::T* cond, CodeGen& cg, CG_Builder& frag) {
  // Check if the condition is already forced.
  assert(cond);
  if(cond->reg != -1)
    return cond->reg;
  return cond->reg = _force_cond(cond, cg, frag);
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
      if(!vd->type().isvar()) {
        // std::cerr << "%%%% Binding " << vd->id()->str() << " at g" << slot << std::endl;
        // std::cerr << "%%%% "; debugprint(vd);
        if(!vd->e()) {
          // debugprint(vd);
          // cg.env().bind(vd->id()->v(), Loc::global(cg.num_globals));
          cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
          std::cout << "%% " << vd->id()->v() << " ~> " << cg.num_globals << std::endl;
          // FIXME
          ++cg.num_globals;
        } else {
          // Evaluate the definition
          // cg.env().bind(vd->id()->v(), Loc::global(cg.num_globals));
          cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
          // FIXME
          ++cg.num_globals;
        }
      } else {
        // If it's a var with a body, feed it into the mode analyser.
        modes.def(vd, BytecodeProc::ROOT);
      }
    }
  }
  void vConstraintI(ConstraintI* c) {
    modes.use(c->e(), BytecodeProc::ROOT);
  }
  /// Visit assign item
  void vAssignI(AssignI* ass) {
    // debugprint(ass); 
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
    /*
    for(auto p : cg.mode_map) {
      std::cerr << mode_name(p.second) << "[" << p.first << "] "; debugprint(p.first);
    }
    */
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

  for(CG_Instr& i : frag) {
    if(i.tag&1) {
      out << "l" << (i.tag>>1) << ":" << std::endl;
      continue;
    }

    BytecodeStream::Instr op(static_cast<BytecodeStream::Instr>(i.tag>>1));
    out << instr_name(op);
    switch(op) {
      case BytecodeStream::OPEN_AGGREGATION:
        out << " " << agg_name((AggregationCtx::Symbol) i.params[0].value);
        break;
      case BytecodeStream::CALL: {
        out << " " << mode_name((BytecodeProc::Mode) i.params[0].value);
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if(p.is_builtin())
          out << " " << cg._builtins[p.id()].first;
        else
          out << " #P" << p.id(); 
        for(int ii = 2; ii < i.params.size(); ++ii) {
          out << " " << show(i.params[ii]);
        }
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

void post_cond(CodeGen& cg, CG_Builder& frag, CG_Cond::T* cond) {
  std::vector<int> leaves;
  force_and_leaves(leaves, cond, cg, frag);
  for(int r_c : leaves)
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
}
// Placeholders.
class Compile : public ItemVisitor {
private:
  friend class ItemIter<Compile>;

  Compile(CodeGen& _cg) : cg(_cg), bool_dom(-1) { }

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
      } else {
        int r_d;
        if(vd->type().isbool()) {
          if(bool_dom < 0) {
            int l = CG::locate_immi(0, cg, root_frag);
            int u = CG::locate_immi(1, cg, root_frag);
            bool_dom = GET_REG(cg);
            OPEN_VEC(cg, root_frag);
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(l));
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(u));
            CLOSE_AGG(cg, root_frag);
            PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(bool_dom));
          }
          r_d = bool_dom;
        } else {
          Expression* d(vd->ti()->domain());
          assert(d);
          assert(d->type().ispar());
          debugprint(d);
          CG::Binding b_d(CG::bind(d, cg, root_frag));
          post_cond(cg, root_frag, b_d.second);
          r_d = b_d.first;
        }
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
      // cg.env().bind(vd->id()->v(), Loc::global(cg.env().size()));
      cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
      ++cg.num_globals;
      // FIXME
    } else {
      // FIXME: Handle the par case.
      if(vd->e() && !vd->type().isann()) {
        // Evaluate the definition.
        // int r = vd->type().ispar() ? CG::locate_par(vd->e(), cg, root_frag) : CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);
        int r;
        if(vd->type().isbool()) {
          r = CG::force(CG::compile(vd->e(), cg, root_frag), cg, root_frag);
        } else {
          CG::Binding b_d = CG::bind(vd->e(), cg, root_frag);
          post_cond(cg, root_frag, b_d.second);
          r = b_d.first;
        }
        PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(cg.num_globals));
        // cg.env().bind(vd->id()->v(), Loc::global(cg.num_globals));
        cg.globals_env.insert(std::make_pair(vd->id()->v(), cg.num_globals));
        // std::cout << "%% " << vd->id()->v() << cg.num_globals << std::endl;
        // FIXME
        ++cg.num_globals;
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

  CodeGen& cg;
  CG_Builder root_frag;

  int globals_count;
  int bool_dom;
public:
  static void run(CodeGen& cg, Model* m) {
    Compile c(cg);
    OPEN_OTHER(cg, c.root_frag);
    iterItems(c, m);

    // Now generate procedures for any necessary function/predicate bodies.
    PUSH_INSTR(c.root_frag, BytecodeStream::RET);
    cg.append(0, BytecodeProc::ROOT, c.root_frag);
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
std::pair<int, CG_Cond::T*> _bind(Expression* e, CodeGen& cg, CG_Builder& frag) {
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
    return std::make_pair(CG::locate_immi(e->template cast<IntLit>()->v().toInt(), cg, frag), nullptr);
  case Expression::E_FLOATLIT:
    // return std::make_pair(CG::locate_par(e->template cast<FloatLit>(), cg, frag), nullptr);
    TODO();
    return CG::Binding(0, nullptr);
  case Expression::E_SETLIT:
    return CG::bind(e->template cast<SetLit>(), ctx, cg, frag);
  case Expression::E_BOOLLIT:
    throw InternalError("bind called on Boolean expression.");
  case Expression::E_STRINGLIT:
    TODO();
    return CG::Binding(0, nullptr);
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
    break;
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
  for(int g = 0; g < c->n_generators(); ++g) {
    // Bind the in-expression to a register.
    // assert(c->in(g)->type().ispar());
    Expression* in(c->in(g));
    int r(CG::bind(in, cg, frag).first);
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
        cg.env().bind(id, CodeGen::Binding(iter->val(), nullptr));
      }
    } else {
      assert(in->type().isboolarray() || in->type().isintarray());
      for(int d = 0; d < c->n_decls(g); ++d) {
        Foreach* iter(new Foreach(cg, r));
        nesting.push_back(iter);
        iter->emit_pre(frag);

        VarDecl* vd(c->decl(g, d));
        ASTString id(vd->id()->str());
        cg.env().bind(id, CodeGen::Binding(iter->val(), nullptr));
      }
    }
    // Now emit the where-clause
    Expression* where(c->where(g));
    if(where) {
      int lblCont(nesting.back()->cont());
      assert(where->type().ispar());
      int rC = CG::bind(where, cg, frag).first;
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblCont));
    }
  }
  // We're now in the deepest scope. Generate code for the body.
  int r_e = CG::bind(c->e(), cg, frag).first; // FIXME: Discarding partiality
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_e));
  // Now close the iterators _in reverse order_, and restore the environment.
  for(int ii = nesting.size()-1; ii >= 0; --ii) {
    nesting[ii]->emit_post(frag);
    delete nesting[ii];
    cg.env_pop();
  }
}

// Special case implementation of folds where body is a generator.
CG_Cond::T* eval_forall(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        std::vector<CG_Cond::T*> conj;
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          conj.push_back(CG::compile((*a)[ii], cg, frag));
        return CG_Cond::forall(ctx, conj);
      }
      break;
      /*
    case Expression::E_COMP: {
      execute_comprehension(param->cast<Comprehension>(), c_ctx, cg, frag);
      }
      break;
      */
    default:
      {
        CG::Binding b_param(CG::bind(param, cg, frag));
        int r_A(b_param.first);
        std::vector<int> p_A;
        force_and_leaves(p_A, b_param.second, cg, frag);
        OPEN_OTHER(cg, frag);
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
CG_Cond::T* eval_exists(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        std::vector<CG_Cond::T*> disj;
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii) {
          CG_Cond::T* elt(CG::compile((*a)[ii], cg, frag));
          if(!elt)
            return nullptr;
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
        if(b_A.second)
          return CG_Cond::forall(ctx, b_A.second, CG_Cond::reg(r));
        else
          return CG_Cond::reg(r);
      }
  }
}
CG_Cond::T* eval_error_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in general context.");
  return nullptr;
}
CG::Binding bind_error_g(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in Boolean context.");
}
CG::Binding bind_sum(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::cerr << "%%%% Evaluating sum" << std::endl;
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO
  TODO();
  return CG::Binding(0, nullptr);
}

CG_Cond::T* eval_assert_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
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
    return nullptr;
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

builtin_table init_builtins(void) {
  builtin_table tbl;
  Constants& c(constants());
  tbl.insert(std::make_pair(c.ids.sum, builtin_t { eval_error_b, bind_sum } ));
  tbl.insert(std::make_pair(c.ids.exists, builtin_t { eval_exists, bind_error_g } ));
  tbl.insert(std::make_pair(c.ids.forall, builtin_t { eval_forall, bind_error_g } ));
  tbl.insert(std::make_pair(c.ids.assert, builtin_t { eval_assert_b, bind_assert_g } ));
  return tbl;
}
builtin_table& builtins(void) {
  static builtin_table tbl(init_builtins());
  return tbl;
}

CG::Binding CG::bind(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    return CG::Binding(cg.env().lookup(x->v()).first, nullptr);
  } catch(const CG_Env<CodeGen::Binding>::NotFound& exn) {
    debugprint(x);
    int g = cg.globals_env.at(x->v());
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG::Binding(r, nullptr);
  }
}

CG::Binding CG::bind(SetLit* l, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  IntSetVal* s(l->isv());
  assert(s);
  OPEN_VEC(cg, frag);
  int r_t(GET_REG(cg));
  for(int ii = 0; ii < s->size(); ++ii) {
    int l(s->min(ii).toInt());
    int u(s->max(ii).toInt());
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(l), CG::r(r_t));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u), CG::r(r_t));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_t));
  }
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return CG::Binding(r, nullptr);
}

CG::Binding CG::bind(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Build up the array.
  std::vector<int> r_vec;
  std::vector<CG_Cond::T*> p_vec;

  int sz(a->size());
  for(int ii = 0; ii < sz; ++ii) {
    Binding b_ii(CG::bind((*a)[ii], cg, frag));
    r_vec.push_back(b_ii.first);
    p_vec.push_back(b_ii.second);
  }
  OPEN_VEC(cg, frag);
  for(int r_c : r_vec)
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_c));
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));

  return CG::Binding(r, CG_Cond::forall(ctx, p_vec));
}

CG::Binding CG::bind(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T*> cond;

  // Now evaluate the array body, and emit the indices.
  int sz(idx.size());
  std::vector<int> r_idxs(sz);
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    if(b.second)
      cond.push_back(b.second);
  }

  Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  if(b_A.second)
    cond.push_back(b_A.second);

  assert(sz == 1);
  int r(GET_REG(cg));
  OPEN_OTHER(cg, frag);
  if(idx[0]->type().ispar()) {
    // Just read the vector, and get the appropriate element.
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_idxs[0]), CG::r(r));
  } else {
    PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_element"), CG::r(r_A), CG::r(r_idxs[0]));
  }
  CLOSE_AGG(cg, frag);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return CG::Binding(r, CG_Cond::forall(ctx, cond));
}

CG::Binding CG::bind(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<int> r_cond;
  std::vector<int> p_res;
  std::vector<int> r_res;

  int sz(ite->size());
  for(int ii = 0; ii < sz; ++ii) {
    r_cond.push_back(CG::force(CG::compile(ite->e_if(ii), cg, frag), cg, frag));
    CG::Binding b_res(CG::bind(ite->e_then(ii), cg, frag));
    r_res.push_back(b_res.first);
    p_res.push_back(CG::force(b_res.second, cg, frag));
  }
  Binding b_final = CG::bind(ite->e_else(), cg, frag);

  // Build the correct selector.
  /*
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  // Post c_1 || ... || c_{k-1} || ~c_k || v_k, for k in 1..n.
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  */
  // Build up a vector [ b_0, b_1, ..., b_n ]
  // s.t. b_0 = ~if(0). b_i = b_{i-1} && ~if(ii).
  // Then i = 
  // For now, only Boolean ITEs.
  TODO();
  return CG::Binding(0, nullptr);
}

CG::Binding CG::bind(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  Binding b_lhs(CG::bind(b->lhs(), cg, frag));
  Binding b_rhs(CG::bind(b->rhs(), cg, frag));

  std::vector<CG_Cond::T*> partial;
  if(b_lhs.second)
    partial.push_back(b_lhs.second);
  int r;
  if(b_rhs.second)
    partial.push_back(b_rhs.second);
    r = bind_binop_par(cg, frag, b->op(), b_lhs.first, b_rhs.first);
  if(b->type().ispar()) {
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
    int r_zero(CG::locate_immi(0, cg, frag));
    partial.push_back(CG_Cond::call(cg.find_builtin("int_eq"), -ctx, CG::r(b_rhs.first), CG::r(r_zero)));
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
        OPEN_OTHER(cg, frag);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::FUN, cg.find_builtin("int_neg"), CG::r(b_e.first));
        CLOSE_AGG(cg, frag);
        r = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
      }
      return CG::Binding(r, b_e.second);
    }
  }
}
CG::Binding CG::bind(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      return (*it).second.general(call, ctx, cg, frag);
    }
  }

  // Collects the partiality of the arguments
  std::vector<CG_Cond::T*> p_arg;

  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  for(int ii = 0; ii < sz; ++ii) {
    Binding r_bind(CG::bind(call->arg(ii), cg, frag));
    r_arg[ii] = CG::r(r_bind.first);
    if(r_bind.second)
      p_arg.push_back(r_bind.second);
  }
  p_arg.push_back(CG_Cond::call(find_call_pred(cg, call), ctx, r_arg));

  // Bind the value part.
  int r_ret(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, find_call_fun(cg, call), r_arg);
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_ret));

  return std::make_pair(r_ret, CG_Cond::forall(ctx, p_arg));
}

CG::Binding CG::bind(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  ASTExprVec<Expression> bindings(let->let());
  std::vector<CG_Cond::T*> partial;
  cg.env_push();
  for(Expression* e : bindings) {
    if (VarDecl* vd = e->dyn_cast<VarDecl>()) {
      // Create the new name
      int r_v;
      if(vd->e()) {
        // FIXME: Assuming domain isn't constraining.
        CG::Binding b_v(CG::bind(vd->e(), cg, frag));
        r_v = b_v.first;
        if(b_v.second)
          partial.push_back(b_v.second);
      } else {
        // Variable declaration. Assumes is total and nonempty.
        Expression* d(vd->ti()->domain());
        if(vd->ti()->isarray())
          TODO();
        int r_d = CG::bind(d, cg, frag).first; // Discarding any constraints
        OPEN_OTHER(cg, frag);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
        CLOSE_AGG(cg, frag);
        r_v = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));
      }
      // cg.env().bind(vd->id()->v(), Loc::reg(r_v));
      cg.env().bind(vd->id()->v(), CodeGen::Binding(r_v, nullptr));
    } else {
      // Must be a constraint
      partial.push_back(CG::compile(e, cg, frag));
    }
  }
  CG::Binding b_in(CG::bind(let->in(), cg, frag));
  if(b_in.second)
    partial.push_back(b_in.second);
  cg.env_pop();
  return CG::Binding(b_in.first, CG_Cond::forall(ctx, partial));
}

CG::Binding CG::bind(Comprehension* comp, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  OPEN_VEC(cg, frag);
  execute_comprehension_bind(comp, ctx, cg, frag);
  CLOSE_AGG(cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return CG::Binding(r, nullptr);
}

CG_Cond::T* _compile(Expression* e, CodeGen& cg, CG_Builder& frag) {
  // Look up the mode we need to compile e in.
  debugprint(e);
  CG::Mode ctx(cg.mode_map.at(e));

  switch (e->eid()) {
  case Expression::E_INTLIT:
  case Expression::E_FLOATLIT:
  case Expression::E_SETLIT:
  case Expression::E_STRINGLIT:
  case Expression::E_ARRAYLIT:
  case Expression::E_COMP:
    throw InternalError("compile called on non-Boolean expression.");
  case Expression::E_BOOLLIT:
    TODO();
    return nullptr; 
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

CG_Cond::T* CG::compile(Expression* e, CodeGen& cg, CG_Builder& frag) {
  assert(e->type().isbool());
  try {
    return cg.cache_lookup(e).second;
  } catch(const CG_Env<CG::Binding>::NotFound& exn) {
    CG_Cond::T* cond(_compile(e, cg, frag));
    CG::Binding b(0xdeadbeef, cond);
    cg.cache_store(e, b);
    return cond;
  }
  // return _compile(e, cg, frag); // FIXME: Update env representation.
}

CG_Cond::T* CG::compile(Id* x, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  try {
    return cg.env().lookup(x->v()).second;
  } catch(const CG_Env<Binding>::NotFound& exn) {
    debugprint(x);
    int g = cg.globals_env.at(x->v());
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(g), CG::r(r));
    return CG_Cond::reg(r);
  }
}

CG_Cond::T* compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());

  // Evaluate the indices, put them in registers.
  // Collect partiality of the expression.
  std::vector<CG_Cond::T*> cond;

  // Now evaluate the array body, and emit the indices.
  int sz(idx.size());
  std::vector<int> r_idxs(sz);
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b(CG::bind(idx[ii], cg, frag));
    r_idxs[ii] = b.first;
    if(b.second)
      cond.push_back(b.second);
  }

  CG::Binding b_A(CG::bind(A, cg, frag));
  int r_A(b_A.first);
  if(b_A.second)
    cond.push_back(b_A.second);

  assert(sz == 1);
  if(idx[0]->type().ispar()) {
    // Just read the vector, and get the appropriate element.
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_idxs[0]), CG::r(r));
    cond.push_back(CG_Cond::reg(r));
  } else {
    cond.push_back(CG_Cond::call(cg.find_builtin("bool_element"), ctx, CG::r(r_A), CG::r(r_idxs[0])));
  }
  return CG_Cond::forall(ctx, cond);
}

CG_Cond::T* CG::compile(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int sz(ite->size());
  std::vector<int> r_if;
  std::vector<CG_Cond::T*> c_then;

  // Put the conditions in registers, and compile the results.
  for(int ii = 0; ii < sz; ++ii) {
    r_if.push_back(CG::force(CG::compile(ite->e_if(ii), cg, frag), cg, frag)); 
    c_then.push_back(CG::compile(ite->e_then(ii), cg, frag));
  }

  TODO();
  return nullptr;
}

CG_Cond::T* CG::compile(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T*> cond;
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
      cond.push_back(binop_cond(cg, b->op(), ctx, b_lhs.first, b_rhs.first));
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
      TODO();
      return nullptr;
      /*
      Mode c_ctx(open_disj(ctx, frag));
      CG::eval(b->lhs(), -c_ctx, cg, frag);
      CG::eval(b->rhs(), c_ctx, cg, frag);
      close_disj(ctx, frag);
      */
    }
    break;
    case BOT_RIMPL: {
      TODO();
      return nullptr;
      /*
      Mode c_ctx(open_disj(ctx, frag));
      CG::eval(b->lhs(), c_ctx, cg, frag);
      CG::eval(b->rhs(), -c_ctx, cg, frag);
      close_disj(ctx, frag);
      */
    }
    default: {
      // Standard case.
      int r_lhs(CG::force(CG::compile(b->lhs(), cg, frag), cg, frag));
      int r_rhs(CG::force(CG::compile(b->rhs(), cg, frag), cg, frag));
      if(b->type().ispar()) {
        return CG_Cond::reg(bind_binop_par(cg, frag, b->op(), r_lhs, r_rhs));
      } else {
        return binop_cond(cg, b->op(), ctx, r_lhs, r_rhs);
      }
    }
  }
}

CG_Cond::T* CG::compile(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // TODO: Fix CG_Cond to handle negation.
  assert(u->op() == UOT_NOT);
  int r_e(CG::force(CG::compile(u->e(), cg, frag), cg, frag));
  return CG_Cond::call(cg.find_builtin("bool_not"), ctx, CG::r(r_e));
}

CG_Cond::T* CG::compile(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If we have a builtin for this, dispatch to that instead.
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      return it->second.boolean(call, ctx, cg, frag);
    }
  }

  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  std::vector<CG_Cond::T*> p_arg;
  
  // Evaluate the args, collecting the conditionality.
  for(int ii = 0; ii < sz; ++ii) {
    Expression* e(call->arg(ii));
    // CG::Binding b_arg(CG::bind(call->arg(ii), cg, frag));
    if(e->type().isbool()) {
      r_arg[ii] = CG::r(CG::force(CG::compile(e, cg, frag), cg, frag));
    } else {
      CG::Binding b(CG::bind(call->arg(ii), cg, frag));
      r_arg[ii] = CG::r(b.first);
      if(b.second)
        p_arg.push_back(b.second);
    }
  }
  // And finally, add the call itself
  auto bodies(cg.fun_map.get_bodies(call));
  std::cerr << "%% Found " << bodies.size() << " definitions matching ";
  debugprint(call);
//  for(auto b : bodies)
//    debugprint(b);
  p_arg.push_back(CG_Cond::call(find_call_fun(cg, call), ctx, r_arg));
  return CG_Cond::forall(ctx, p_arg);
}

CG_Cond::T* CG::compile(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T*> conj; 
  ASTExprVec<Expression> bindings(let->let());
  std::vector<CG_Cond::T*> partial;
  cg.env_push();
  for(Expression* e : bindings) {
    if (VarDecl* vd = e->dyn_cast<VarDecl>()) {
      // Bind the new definitions in context
      // FIXME: Deal with Boolean declarations.
      int r_v;
      if(vd->e()) {
        CG::Binding b_v(CG::bind(vd->e(), cg, frag));
        r_v = b_v.first;
        if(b_v.second)
          conj.push_back(b_v.second);
      } else {
        // Variable declaration. Assumes is total and nonempty.
        Expression* d(vd->ti()->domain());
        if(vd->ti()->isarray())
          TODO();
        int r_d = CG::bind(d, cg, frag).first; // Discarding any constraints
        OPEN_OTHER(cg, frag);
        PUSH_INSTR(frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
        CLOSE_AGG(cg, frag);
        r_v = GET_REG(cg);
        PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r_v));
      }
      // cg.env().bind(vd->id()->v(), Loc::reg(r_v));
      cg.env().bind(vd->id()->v(), CodeGen::Binding(r_v, nullptr));
    } else {
      conj.push_back(CG::compile(e, cg, frag));
    }
  }
  conj.push_back(CG::compile(let->in(), cg, frag));
  return CG_Cond::forall(ctx, conj);
}
CG_Cond::T* CG::compile(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  std::vector<CG_Cond::T*> conj;

  CG::Binding b_A(CG::bind(a, cg, frag));
  int r_A(b_A.first);
  if(b_A.second)
    conj.push_back(b_A.second);

  std::vector<int> r_idx;
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());
  int sz(idx.size());
  for(int ii = 0; ii < sz; ++ii) {
    CG::Binding b_x(CG::bind(idx[ii], cg, frag)); 
    r_idx.push_back(b_x.first);
    if(b_x.second)
      conj.push_back(b_x.second);
  }
  assert(sz == 1);
  if(idx[0]->type().ispar()) {
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r_idx[0]), CG::r(r));
    conj.push_back(CG_Cond::reg(r));
  } else {
    conj.push_back(CG_Cond::call(cg.find_builtin("bool_element"), ctx, CG::r(r_A), CG::r(r_idx[0])));
  }
  return CG_Cond::forall(ctx, conj);
}

};
