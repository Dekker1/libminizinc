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
  std::function<void(Call*, Mode, CodeGen&, CG_Builder&)> boolean;
  std::function<void(Call*, Mode, CodeGen&, CG_Builder&, CG_Builder&)> general;
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
  "VCTX_AND",
  "VCTX_OR",
  "VCTX_VEC",
  "VCTX_OTHER"
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

  register_builtin("bool_clause", 2);
  
  register_builtin("int_eq", 2);
  register_builtin("int_lt", 2);
  register_builtin("int_le", 2);

  register_builtin("int_plus", 2);
  register_builtin("int_times", 2);
  register_builtin("int_pow", 2);
}

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
      return;
    case BOT_MULT:
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, cg.find_builtin("int_times"), CG::r(r_lhs), CG::r(r_rhs));
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

CG_ProcID find_op(CodeGen& cg, BinOpType op) {
  return CG_ProcID::proc(0xbeef);
}

CG_ProcID find_op(CodeGen& cg, UnOpType op) {
  return CG_ProcID::proc(0xfeed);
}
CG_ProcID find_call_fun(CodeGen& cg, Call* c) {
  return CG_ProcID::proc(0xdead);
}
CG_ProcID find_call_pred(CodeGen& cg, Call* c) {
  return CG_ProcID::proc(0xbead);
}

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
      ASTStSet r_where(scope(c->where(g)));
      ASTStSet r_in(scope(c->in(g)));

      r.insert(r_where.begin(), r_where.end());
      r.insert(r_in.begin(), r_in.end());

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
    TODO();
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

bool CodeGen::cache_lookup(Expression* e, Loc& out) {
  return env().cache_lookup(e, scope(e), out);
}
void CodeGen::cache_store(Expression* e, Loc l) {
  env().cache_store(e, scope(e), l); 
}

struct EVAL {
  EVAL(Expression* _e, Mode _ctx) : e(_e), ctx(_ctx) { }

  void operator()(CodeGen& cg, CG_Builder& frag) { CG::eval(e, ctx, cg, frag); }

  Expression* e;
  Mode ctx;
};

/*
struct EVAL_COND {
  EVAL_COND(Expression* _e, BCtx _ctx) : e(_e), ctx(_ctx) { }

  void operator()(CodeGen& cg) { CG::run_condition(cg, e, ctx); }

  Expression* e;
  BCtx ctx;
};
*/

class EnvInit : public ItemVisitor {
private:
  friend class ItemIter<EnvInit>;

  EnvInit(CodeGen& _cg)
    : slot(0), cg(_cg) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(!vd->type().isvar() && !vd->type().isann()) {
      // std::cerr << "%%%% Binding " << vd->id()->str() << " at g" << slot << std::endl;
      // std::cerr << "%%%% "; debugprint(vd);
      if(!vd->e()) {
        // debugprint(vd);
        cg.env().bind(vd->id()->v(), Loc::global(slot));
        ++slot;
      } else {
        // Evaluate the definition
        cg.env().bind(vd->id()->v(), Loc::global(slot));
        ++slot;
      }
    }
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
  }

  int slot;
  CodeGen& cg;
public:
  static void run(CodeGen& cg, Model* m) {
    EnvInit eb(cg);
    iterItems(eb, m);
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

void show_frag(CodeGen& cg, std::vector<CG_Instr>& frag) {
  auto show = [&cg](CG_Value v) { return ShowVal(cg, v); };

  for(CG_Instr& i : frag) {
    if(i.tag&1) {
      std::cerr << "l" << (i.tag>>1) << ":" << std::endl;
      continue;
    }

    BytecodeStream::Instr op(static_cast<BytecodeStream::Instr>(i.tag>>1));
    std::cerr << instr_name(op);
    switch(op) {
      case BytecodeStream::OPEN_AGGREGATION:
        std::cerr << " " << agg_name((AggregationCtx::Symbol) i.params[0].value);
        break;
      case BytecodeStream::CALL: {
        std::cerr << " " << mode_name((BytecodeProc::Mode) i.params[0].value);
        CG_ProcID p(CG_ProcID::of_val(i.params[1]));
        if(p.is_builtin())
          std::cerr << " " << cg._builtins[p.id()].first;
        else
          std::cerr << " #P" << p.id(); 
        for(int ii = 2; ii < i.params.size(); ++ii) {
          std::cerr << " " << show(i.params[ii]);
        }
        break;
      }
      default:
        for(CG_Value p : i.params)
          std::cerr << " " << show(p);
    }
    std::cout << std::endl;
  }
}

void show(CodeGen& cg) {
  for(auto b : cg._builtins) {
    std::cerr << ":" << b.first << ": " << b.second << std::endl;
  }
  for(auto p : cg.bytecode) {
    std::cerr << ":" << p.ident << ":" << mode_name(p.m) << " " << p.arity << std::endl;
    show_frag(cg, p.body);
  }
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
    std::cerr << "%%%% Binding " << vd->id()->str() << std::endl;
    if(vd->type().isvar()) {
      // In whatever case, we're going to create something,
      // and dump it in a register.
      int r_var;
      if(vd->e()) {
        // Defined
        // Evaluate the definition in root context,
        // add it to a register
        r_var = CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);
      } else {
        int r_d;
        if(vd->type().isbool()) {
          if(bool_dom < 0) {
            int l = CG::locate_immi(0, cg, root_frag);
            int u = CG::locate_immi(1, cg, root_frag);
            bool_dom = GET_REG(cg);
            PUSH_INSTR(root_frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC); 
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(l));
            PUSH_INSTR(root_frag, BytecodeStream::PUSH, CG::r(u));
            PUSH_INSTR(root_frag, BytecodeStream::CLOSE_AGGREGATION);
            PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(bool_dom));
          }
          r_d = bool_dom;
        } else {
          Expression* d(vd->ti()->domain());
          assert(d);
          r_d = CG::locate(d, BytecodeProc::ROOT, cg, root_frag);
        }
        if(vd->ti()->isarray()) {
          // Open nested iterators
          std::vector<int> r_regs;
          for(Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            r_regs.push_back(CG::locate(dim, BytecodeProc::ROOT, cg, root_frag));
          }
          std::vector<Forset> nesting;
          PUSH_INSTR(root_frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC);
          for(int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
          for(int r_i = r_regs.size()-1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CLOSE_AGGREGATION);
        } else {
          // FIXME: Compute the domain of the variable.
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::RAW, cg.find_builtin("mk_intvar"), CG::r(r_d));
        }
        r_var = GET_REG(cg);
        PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(r_var));
      }
      // Now copy it into a global, and add it to the env.
      PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r_var), CG::g(cg.env().size()));
      cg.env().bind(vd->id()->v(), Loc::global(cg.env().size()));
    } else {
      // FIXME: Handle the par case.
      if(vd->e() && !vd->type().isann()) {
        // Evaluate the definition.
        int r = vd->type().ispar() ? CG::locate_par(vd->e(), cg, root_frag) : CG::locate(vd->e(), BytecodeProc::ROOT, cg, root_frag);;
        PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(cg.env().size()));
        cg.env().bind(vd->id()->v(), Loc::global(cg.env().size()));
      }
    }
  }

  /// Visit assign item
  void vAssignI(AssignI* ass) {
    // std::cerr << "%%%% Assign: "; debugprint(ass); 
  }

  void vConstraintI(ConstraintI* c) {
    // std::cerr << "%%%% "; debugprint(c->e());
    CG::eval(c->e(), BytecodeProc::ROOT, cg, root_frag);
  }


  CodeGen& cg;
  CG_Builder root_frag;

  int bool_dom;
public:
  static void run(CodeGen& cg, Model* m) {
    Compile c(cg);
    iterItems(c, m);

    // Now generate procedures for any necessary function/predicate bodies.
    PUSH_INSTR(c.root_frag, BytecodeStream::RET);
    cg.append(0, c.root_frag);
    show(cg);
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

int relocate_expression(Expression* e, CodeGen& cg, CG_Builder& frag) {
  Loc l(Loc::reg(0));
  if(!cg.cache_lookup(e, l))
    return -1;

  if(l.is_reg()) {
    return l.index();
  } else {
    // If a global, put it in a register, and update the cached value.
    int r = GET_REG(cg); 
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(l.index()), CG::r(r));
    cg.cache_store(e, Loc::reg(r));
    return r;
  }
  return true;
}

bool retrieve_expression(Expression* e, CodeGen& cg, CG_Builder& frag) {
  int r(relocate_expression(e, cg, frag));
  if(r < 0)
    return false;
  
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  return true;
}

// Evaluate an expression, place it on the value stack.
void CG::eval(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the value is already available in a register somewhere, just push it.
  if(retrieve_expression(e, cg, frag))
    return;

  switch (e->eid()) {
  case Expression::E_INTLIT:
    CG::eval(e->template cast<IntLit>(), cg, frag);
    break;
  case Expression::E_FLOATLIT:
    CG::eval(e->template cast<FloatLit>(), cg, frag);
    break;
  case Expression::E_SETLIT:
    CG::eval(e->template cast<SetLit>(), cg, frag);
    break;
  case Expression::E_BOOLLIT:
    CG::eval(e->template cast<BoolLit>(), ctx, cg, frag);
    break;
  case Expression::E_STRINGLIT:
    CG::eval(e->template cast<StringLit>(), cg, frag);
    break;
  case Expression::E_ID:
    CG::eval(e->template cast<Id>(), ctx, cg, frag);
    break;
  case Expression::E_ANON:
    CG::eval(e->template cast<AnonVar>(), cg, frag);
    break;
  case Expression::E_ARRAYLIT:
    CG::eval(e->template cast<ArrayLit>(), ctx, cg, frag);
    break;
  case Expression::E_ARRAYACCESS:
    CG::eval(e->template cast<ArrayAccess>(), ctx, cg, frag);
    break;
  case Expression::E_COMP:
    CG::eval(e->template cast<Comprehension>(), ctx, cg, frag);
    break;
  case Expression::E_ITE:
    CG::eval(e->template cast<ITE>(), ctx, cg, frag);
    break;
  case Expression::E_BINOP:
    CG::eval(e->template cast<BinOp>(), ctx, cg, frag);
    break;
  case Expression::E_UNOP:
    CG::eval(e->template cast<UnOp>(), ctx, cg, frag);
    break;
  case Expression::E_CALL:
    CG::eval(e->template cast<Call>(), ctx, cg, frag);
    break;
  case Expression::E_LET:
    CG::eval(e->template cast<Let>(), ctx, cg, frag);
    break;
    /*
  case Expression::E_VARDECL:
    CG::eval(cg, e->template cast<VarDecl>());
    break;
  case Expression::E_TI:
    CG::eval(cg, e->template cast<TypeInst>());
    break;
  case Expression::E_TIID:
    CG::eval(cg, e->template cast<TIId>());
    break;
    */
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
  }
}
// Evaluate an expression, place it on the value stack.
int CG::locate_par(Expression* e, CodeGen& cg, CG_Builder& frag) {
  // If the value is already available in a register somewhere, just push it.
  int r = relocate_expression(e, cg, frag);
  if(r >= 0)
    return r;
  switch (e->eid()) {
  case Expression::E_INTLIT:
    r = CG::locate(e->template cast<IntLit>(), cg, frag);
    break;
  case Expression::E_FLOATLIT:
    r = CG::locate(e->template cast<FloatLit>(), cg, frag);
    break;
  case Expression::E_SETLIT:
    r = CG::locate(e->template cast<SetLit>(), cg, frag);
    break;
  case Expression::E_BOOLLIT:
    r = CG::locate(e->template cast<BoolLit>(), BytecodeProc::ROOT, cg, frag);
    break;
  case Expression::E_STRINGLIT:
    r = CG::locate(e->template cast<StringLit>(), cg, frag);
    break;
  case Expression::E_ID:
    r = CG::locate(e->template cast<Id>(), BytecodeProc::ROOT, cg, frag);
    break;
  case Expression::E_ANON:
    // FIXME.
    TODO();
    break;
  case Expression::E_ARRAYLIT:
    // r = CG::locate(e->template cast<ArrayLit>(), C_ROOT, cg, frag);
    TODO();
    break;
  case Expression::E_ARRAYACCESS:
    r = CG::locate_par(e->template cast<ArrayAccess>(), cg, frag);
    break;
  case Expression::E_COMP:
    r = CG::locate_par(e->template cast<Comprehension>(), cg, frag);
    break;
  case Expression::E_ITE:
    r = CG::locate_par(e->template cast<ITE>(), cg, frag);
    break;
  case Expression::E_BINOP:
    r = CG::locate_par(e->template cast<BinOp>(), cg, frag);
    break;
  case Expression::E_UNOP:
    r = CG::locate_par(e->template cast<UnOp>(), cg, frag);
    break;
  case Expression::E_CALL:
    r = CG::locate_par(e->template cast<Call>(), cg, frag);
    break;
  case Expression::E_LET:
    r = CG::locate_par(e->template cast<Let>(), cg, frag);
    break;
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
  }
  cg.cache_store(e, Loc::reg(r));
  return r;
}

// Evaluate an expression, place it on the value stack.
void CG::eval(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  switch (e->eid()) {
  case Expression::E_INTLIT:
    CG::eval(e->template cast<IntLit>(), cg, value);
    break;
  case Expression::E_FLOATLIT:
    CG::eval(e->template cast<FloatLit>(), cg, value);
    break;
  case Expression::E_SETLIT:
    CG::eval(e->template cast<SetLit>(), cg, value);
    break;
  case Expression::E_BOOLLIT:
    CG::eval(e->template cast<BoolLit>(), ctx, cg, value);
    break;
  case Expression::E_STRINGLIT:
    CG::eval(e->template cast<StringLit>(), cg, value);
    break;
  case Expression::E_ID:
    CG::eval(e->template cast<Id>(), ctx, cg, value);
    break;
  case Expression::E_ANON:
    CG::eval(e->template cast<AnonVar>(), cg, value);
    break;
  case Expression::E_ARRAYLIT:
    CG::eval(e->template cast<ArrayLit>(), ctx, cg, value);
    break;
  case Expression::E_ARRAYACCESS:
    CG::eval(e->template cast<ArrayAccess>(), ctx, cg, pred, value);
    break;
  case Expression::E_COMP:
    // Assuming comprehensions are total
    CG::eval(e->template cast<Comprehension>(), ctx, cg, value);
    break;
  case Expression::E_ITE:
    CG::eval(e->template cast<ITE>(), ctx, cg, pred, value);
    break;
  case Expression::E_BINOP:
    CG::eval(e->template cast<BinOp>(), ctx, cg, pred, value);
    break;
  case Expression::E_UNOP:
    CG::eval(e->template cast<UnOp>(), ctx, cg, pred, value);
    break;
  case Expression::E_CALL:
    CG::eval(e->template cast<Call>(), ctx, cg, pred, value);
    break;
  case Expression::E_LET:
    CG::eval(e->template cast<Let>(), ctx, cg, pred, value);
    break;
    /*
  case Expression::E_VARDECL:
    CG::eval(cg, e->template cast<VarDecl>());
    break;
  case Expression::E_TI:
    CG::eval(cg, e->template cast<TypeInst>());
    break;
  case Expression::E_TIID:
    CG::eval(cg, e->template cast<TIId>());
    break;
    */
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
  }
}

int CG::locate(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  Loc l(Loc::reg(0));
  if(cg.cache_lookup(e, l)) {
    if(l.is_reg()) {
      return l.index();
    } else {
      // Otherwise, move the result into a register, and update the cache entry.
      int r = GET_REG(cg);
      PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(l.index()), CG::r(r));
      cg.cache_store(e, Loc::reg(r));
      return r;
    }
  }
  int r;
  switch (e->eid()) {
  case Expression::E_INTLIT:
    r = CG::locate(e->template cast<IntLit>(), cg, frag);
    break;
  case Expression::E_FLOATLIT:
    r = CG::locate(e->template cast<FloatLit>(), cg, frag);
    break;
  case Expression::E_BOOLLIT:
    r = CG::locate(e->template cast<BoolLit>(), ctx, cg, frag);
    break;
  case Expression::E_STRINGLIT:
    r = CG::locate(e->template cast<StringLit>(), cg, frag);
    break;
  case Expression::E_ID:
    r = CG::locate(e->template cast<Id>(), ctx, cg, frag);
    break;
  case Expression::E_ARRAYACCESS:
    r = CG::locate(e->template cast<ArrayAccess>(), ctx, cg, frag);
    break;
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
    break;
  default: {
    PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER);
    CG::eval(e, ctx, cg, frag);
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
    r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    break;
  }
  }
  // Now save it in the cache.
  cg.cache_store(e, Loc::reg(r));
  return r;
}

int CG::locate(Expression* e, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  switch (e->eid()) {
  case Expression::E_INTLIT:
    return CG::locate(e->template cast<IntLit>(), cg, value);
    break;
  case Expression::E_FLOATLIT:
    return CG::locate(e->template cast<FloatLit>(), cg, value);
    break;
  case Expression::E_BOOLLIT:
    return CG::locate(e->template cast<BoolLit>(), ctx, cg, value);
    break;
  case Expression::E_STRINGLIT:
    return CG::locate(e->template cast<StringLit>(), cg, value);
    break;
  case Expression::E_ID:
    return CG::locate(e->template cast<Id>(), ctx, cg, value);
    break;
  case Expression::E_ARRAYACCESS:
    return CG::locate(e->template cast<ArrayAccess>(), ctx, cg, pred, value);
    break;
  case Expression::E_VARDECL:
  case Expression::E_TI:
  case Expression::E_TIID:
    throw InternalError("Bytecode generator encountered unexpected expression type.");
    break;
  default: {
    // We bind e at the end of the _pred_ fragment, so that compiler-level CSE is well-behaved.
    // Otherwise, we end up in situations where e is bound in [value], but not in [pred].
    CG_Builder pred_tl;
    PUSH_INSTR(pred_tl, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER);
    CG::eval(e, ctx, cg, pred, pred_tl);
    pred.append(pred_tl);
    PUSH_INSTR(pred, BytecodeStream::CLOSE_AGGREGATION);
    int r = GET_REG(cg);
    PUSH_INSTR(pred, BytecodeStream::POP, CG::r(r));
    cg.cache_store(e, Loc::reg(r));
    return r;
    break;
  }
  }
}

int CG::locate_par(ArrayAccess* a, CodeGen& cg, CG_Builder& frag) {
  // int A = locate_par(a->); 
  TODO();
  return 0;
}

int CG::locate_par(Comprehension* c, CodeGen& cg, CG_Builder& frag) {
  // We're building up a vector.
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC);
  std::vector<Forset> nesting;
  std::vector< std::pair<ASTString, Loc> > trail;

  int g = c->n_generators();
  for(int g = 0; g < c->n_generators(); ++g) {
    // Bind the in-expression to a register.
    int r = CG::locate_par(c->in(g), cg, frag);
    // Open the bindings.
    cg.env_push();
    for(int d = 0; d < c->n_decls(g); ++d) {
      Forset iter(cg, r);
      nesting.push_back(iter);
      iter.emit_pre(frag);
      // Bind vd->id() to iter.val()
      VarDecl* vd(c->decl(g, d));
      ASTString id(vd->id()->str());
      cg.env().bind(id, Loc::reg(iter.val()));
    }
    // Now emit the where-clause
    Expression* where(c->where(g));
    if(where) {
      int lblCont(nesting.back().cont());
      int rC = CG::locate_par(where, cg, frag);
      // ASSUMING PAR HERE.
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblCont));
    }
  }
  // We're now in the deepest scope. Generate code for the body.
  int r_e = CG::locate_par(c->e(), cg, frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r_e));
  // Now close the iterators _in reverse order_, and restore the environment.
  for(int ii = nesting.size()-1; ii >= 0; --ii) {
    nesting[ii].emit_post(frag);
    cg.env_pop();
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return r;
}

int CG::locate_par(ITE* ite, CodeGen& cg, CG_Builder& frag) {
  int l_end(GET_LABEL(cg));
  int r(GET_REG(cg));
  // Expression availability cascades; anything in the first condition
  // available everywhere, anything in the second is available to [2,...], etc.
  cg.env_push();
  for(int ii = 0; ii < ite->size(); ++ii) {
    int l_next(GET_LABEL(cg));
    int r_cond = CG::locate_par(ite->e_if(ii), cg, frag);
    PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(r_cond), CG::l(l_next));
    cg.env_push();
    int r_then = CG::locate_par(ite->e_then(ii), cg, frag);
    PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_then), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::JMP, CG::l(l_end));
    cg.env_pop();
    PUSH_LABEL(frag, l_next);
  }
  int r_else = CG::locate_par(ite->e_else(), cg, frag);
  PUSH_INSTR(frag, BytecodeStream::MOV, CG::r(r_else), CG::r(r));
  cg.env_pop();
  PUSH_LABEL(frag, l_end);
  return r;
}

int CG::locate_par(BinOp* b, CodeGen& cg, CG_Builder& frag) {
  int r_lhs = CG::locate_par(b->lhs(), cg, frag);
  int r_rhs = CG::locate_par(b->rhs(), cg, frag);
  int r(GET_REG(cg));
  switch(b->op()) {
  case BOT_PLUS:
    PUSH_INSTR(frag, BytecodeStream::ADDI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_MINUS:
    PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_MULT:
    PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_DIV:
    TODO(); // No floats yet.
    PUSH_INSTR(frag, BytecodeStream::MULI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_IDIV:
    PUSH_INSTR(frag, BytecodeStream::DIVI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_MOD:
  case BOT_POW:
  case BOT_LE:
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_LQ:
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_lhs), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_GR:
    PUSH_INSTR(frag, BytecodeStream::LTI, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_GQ:
    PUSH_INSTR(frag, BytecodeStream::LEI, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_EQ:
  case BOT_EQUIV:
    PUSH_INSTR(frag, BytecodeStream::EQI, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_NQ:
    PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r), CG::r(r));
    break;
  case BOT_OR:
    PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_AND:
    PUSH_INSTR(frag, BytecodeStream::AND, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_XOR:
    PUSH_INSTR(frag, BytecodeStream::XOR, CG::r(r_rhs), CG::r(r_lhs), CG::r(r));
    break;
  case BOT_IMPL:
    PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_lhs), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r), CG::r(r_rhs), CG::r(r));
    break;
  case BOT_RIMPL:
    PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_rhs), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::OR, CG::r(r_lhs), CG::r(r), CG::r(r));
    break;
  case BOT_DOTDOT:
  case BOT_IN:
  case BOT_SUBSET:
  case BOT_SUPERSET:
  case BOT_UNION:
  case BOT_DIFF:
  case BOT_SYMDIFF:
  case BOT_INTERSECT:
  case BOT_PLUSPLUS:
    TODO();
  }
  return r;
}
int CG::locate_par(UnOp* u, CodeGen& cg, CG_Builder& frag) {
  int r_e = CG::locate_par(u->e(), cg, frag);
  switch(u->op()) {
  case UOT_NOT: {
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::NOT, CG::r(r_e), CG::r(r));
    return r;
  }
  case UOT_PLUS:
    return r_e;
  case UOT_MINUS: {
    int r(GET_REG(cg));
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(0), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::SUBI, CG::r(r), CG::r(r_e), CG::r(r));
    return r;
  }
  }
}

int CG::locate_par(Call* call, CodeGen& cg, CG_Builder& frag) {
  // Call 
  TODO();
  return 0;
}
int CG::locate_par(Let* let, CodeGen& cg, CG_Builder& frag) {
  TODO(); 
  return 0;
}

void CG::eval(IntLit* z, CodeGen& cg, CG_Builder& frag) {
  int x(z->v().toInt());
  int r(TEMP_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
}

int CG::locate_immi(int x, CodeGen& cg, CG_Builder& frag) {
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
  return r;
}
int CG::locate(IntLit* z, CodeGen& cg, CG_Builder& frag) {
  return locate_immi(z->v().toInt(), cg, frag);
}

int CG::locate(BoolLit* z, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int v(ctx.is_neg() ? !z->v() : z->v());
  return CG::locate_immi(v, cg, frag);
}

void CG::eval(FloatLit* f, CodeGen& cg, CG_Builder& frag) { throw InternalError("FloatLit not yet supported by bytecode generator."); }
int CG::locate(FloatLit* f, CodeGen& cg, CG_Builder& frag) { throw InternalError("FloatLit not yet supported by bytecode generator."); }

void CG::eval(SetLit* l, CodeGen& cg, CG_Builder& frag) {
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC); 
  IntSetVal* s(l->isv());
  assert(s);
  int r(TEMP_REG(cg));
  for(int ii = 0; ii < s->size(); ++ii) {
    int l(s->min(ii).toInt());
    int u(s->max(ii).toInt());
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(l), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

int CG::locate(SetLit* l, CodeGen& cg, CG_Builder& frag) {
  eval(l, cg, frag);
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
  return r;
}

void CG::eval(BoolLit* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  int r(TEMP_REG(cg));
  bool v = ctx.is_neg() ? !b->v() : b->v();
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(v), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
}

void CG::eval(StringLit* s, CodeGen& cg, CG_Builder& frag) { throw InternalError("StringLit not yet handled by bytecode generator."); }
int CG::locate(StringLit* s, CodeGen& cg, CG_Builder& frag) { throw InternalError("StringLit not yet handled by bytecode generator."); }

void CG::eval(Id* id, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Look up the identifier in the environment.
  LOC(cg.env().lookup(id->v()))(cg, frag);
}

int CG::locate(Id * id, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Look up the identifier in the environment.
  Loc l(cg.env().lookup(id->v()));
  if(l.is_global()) {
    int r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(l.index()), CG::r(r));
    return r;
  } else {
    return l.index();
  }
}

void CG::eval(AnonVar* v, CodeGen& cg, CG_Builder& frag) { throw InternalError("AnonVar not yet handled by bytecode generator."); }

void CG::eval(ArrayLit* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Build up the array.
  // FIXME: Also need to deal with the indexing structure.
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC);
  int sz(a->size());
  for(int ii = 0; ii < sz; ++ii)
    CG::eval((*a)[ii], ctx, cg, frag);
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

void vec_get_ith(int r_A, int pos, CodeGen& cg, CG_Builder& frag, int r) {
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(pos), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::GET_VEC, CG::r(r_A), CG::r(r), CG::r(r));
}
int vec_get_ith(int r_A, int pos, CodeGen& cg, CG_Builder& frag) {
  int r = GET_REG(cg);
  vec_get_ith(r_A, pos, cg, frag, r);
  return r;
}

void eval_element_1d(int r_A, int r_idx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  // Get the array indices.
  int r_temp = GET_REG(cg);
  vec_get_ith(r_A, 0, cg, pred, r_temp);
  // PUSH_INSTR(pred, BytecodeStream::CALL, CG::builtins.set_in(), r_idx, r_iset);

  // Get the array contents.
  vec_get_ith(r_A, 1, cg, pred, r_temp);
  // PUSH_INSTR(value, BytecodeStream::CALL, CG::builtins.element_1d(), r_A, r_temp);
}
void eval_element_1d(int r_A, int r_idx, CodeGen& cg, CG_Builder& frag) {
  CG_Builder frag_val;
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  eval_element_1d(r_A, r_idx, cg, frag, frag_val);
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  frag.append(frag_val);
}

void eval_element_nd(int r_A, std::vector<int>& r_idxs, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  // Two parts: first, add constraints on the indices, then compute the indexing information,
  // then emit the lookup.
  int r_value = GET_REG(cg);
  for(int ii = 0; ii < r_idxs.size(); ++ii) {
    vec_get_ith(r_A, ii, cg, pred, r_value);
    // PUSH_INSTR(value, BytecodeStream::CALL, CG::builtins.set_in(), r_idx, r_value);
  }
  // FIXME
}

void CG::eval(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());

  // Evaluate the indices, put them in registers.
  // Indices might be partial, so we need to add the index constraints.
  int sz(idx.size());
  std::vector<int> r_idxs(sz);;
  CG_Builder frag_tl;

  // FIXME
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  Mode c_ctx = open_conj(ctx, frag);
  for(int ii = 0; ii < sz; ++ii)
    r_idxs[ii] = locate(idx[ii], BytecodeProc::FUN, cg, frag, frag_tl);
  frag.append(frag_tl);

  // Now evaluate the array body, and emit the indices.
  int r_A = CG::locate(A, +c_ctx, cg, frag);
  // Finally, process the element lookup.
  eval_element_nd(r_A, r_idxs, cg, frag, frag_tl);
  close_conj(ctx, frag_tl);
  frag.append(frag_tl);
}
// General (non-Boolean) version. Roughly the same shape as the Boolean case, except we separate
// the Boolean and integer context.
void CG::eval(ArrayAccess* a, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());

  // Evaluate the indices, put them in registers.
  // Indices might be partial, so they get evaluated in the predicate fragment.
  int sz(idx.size());
  std::vector<int> r_idxs(sz);
  CG_Builder pred_val;
  PUSH_INSTR(pred, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  Mode c_ctx(open_conj(ctx, pred));
  for(int ii = 0; ii < sz; ++ii)
    r_idxs[ii] = CG::locate(idx[ii], BytecodeProc::FUN, cg, pred, pred_val);

  // Now evaluate the array body, and emit the indices.
  int r_A = CG::locate(A, +c_ctx, cg, pred, pred_val);
  // At the end of pred_val, everything is available, so we can
  // evaluate the partiality of A[i1, ...].
  eval_element_nd(r_A, r_idxs, cg, pred_val, value);
  close_conj(ctx, pred_val);
  pred.append(pred_val);
}

// Evaluate a comprehension, pushing all the generated values onto the
// value stack.
// Currently assumes in- and where- expressions are par, and therefore total.
void execute_comprehension(Comprehension* c, Mode ctx, CodeGen& cg, CG_Builder& frag) {
   // Build up the object to build the generator.
  std::vector<Forset> nesting;
  std::vector< std::pair<ASTString, Loc> > trail;

  int g = c->n_generators();
  for(int g = 0; g < c->n_generators(); ++g) {
    // Bind the in-expression to a register.
    assert(c->in(g)->type().ispar());
    int r = CG::locate_par(c->in(g), cg, frag);
    // Open the bindings.
    cg.env_push();
    for(int d = 0; d < c->n_decls(g); ++d) {
      Forset iter(cg, r);
      nesting.push_back(iter);
      iter.emit_pre(frag);
      // Bind vd->id() to iter.val()
      VarDecl* vd(c->decl(g, d));
      ASTString id(vd->id()->str());
      cg.env().bind(id, Loc::reg(iter.val()));
    }
    // Now emit the where-clause
    Expression* where(c->where(g));
    if(where) {
      int lblCont(nesting.back().cont());
      assert(where->type().ispar());
      int rC = CG::locate_par(where, cg, frag);
      PUSH_INSTR(frag, BytecodeStream::JMPIFNOT, CG::r(rC), CG::l(lblCont));
    }
  }
  // We're now in the deepest scope. Generate code for the body.
  CG::eval(c->e(), ctx, cg, frag);
  // Now close the iterators _in reverse order_, and restore the environment.
  for(int ii = nesting.size()-1; ii >= 0; --ii) {
    nesting[ii].emit_post(frag);
    cg.env_pop();
  }
}

void CG::eval(Comprehension* c, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER);
  execute_comprehension(c, ctx, cg, frag);
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

void CG::eval(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Successively evaluate the <condition, result> pairs.
  std::vector<int> r_cond;
  std::vector<int> r_res;

  int sz(ite->size());
  for(int ii = 0; ii < sz; ++ii) {
    r_cond.push_back(CG::locate(ite->e_if(ii), BytecodeProc::FUN, cg, frag));
    r_res.push_back(CG::locate(ite->e_then(ii), +ctx, cg, frag));
  }
  int r_final = CG::locate(ite->e_else(), +ctx, cg, frag);

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  // Post c_1 || ... || c_{k-1} || ~c_k || v_k, for k in 1..n.
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
void CG::eval(ITE* ite, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  // Successively evaluate the <condition, result> pairs.
#if 0
  std::vector<int> r_cond;
  std::vector<int> r_res;

  int sz(ite->size());
  for(int ii = 0; ii < sz; ++ii) {
    r_cond.push_back(CG::locate(ite->e_if(ii), C_MIX, cg, frag));
    r_res.push_back(CG::locate(ite->e_then(ii), +ctx, cg, frag));
  }
  int r_final = CG::locate(ite->e_else(), +ctx, cg, frag);

  PUSH_INSTR(cg, BytecodeStream::OPEN_AGGREGATION, VCTX_AND);
  // Post c_1 || ... || c_{k-1} || ~c_k || v_k, for k in 1..n.

  PUSH_INSTR(cg, BytecodeStream::CLOSE_AGGREGATION);
#endif
  TODO();
}

// Special case implementation of folds where body is a generator.
void eval_forall(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  Mode c_ctx(open_conj(ctx, frag));
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          CG::eval((*a)[ii], c_ctx, cg, frag);
      }
      break;
    case Expression::E_COMP: {
      execute_comprehension(param->cast<Comprehension>(), c_ctx, cg, frag);
      }
      break;
    default:
      {
        int r = CG::locate(param, c_ctx, cg, frag);
        FOREACH(RETN()(r), PUSH())(cg, frag);
      }
  }
  close_conj(ctx, frag);
}
// Same as forall, but producing an or-context.
void eval_exists(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  Mode c_ctx(open_disj(ctx, frag));
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          CG::eval((*a)[ii], c_ctx, cg, frag);
      }
      break;
    case Expression::E_COMP: {
      Comprehension* c(param->cast<Comprehension>());
      execute_comprehension(c, c_ctx, cg, frag);
      }
      break;
    default:
      {
        // Otherwise, get the result into a register...
        int r = CG::locate(param, c_ctx, cg, frag);
        // and push every element.
        FOREACH(RETN()(r), PUSH())(cg, frag);
      }
  }
  close_disj(ctx, frag);
}
void eval_error_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in general context.");
}
void eval_error_g(Call* call, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  throw InternalError("Call should only appear in Boolean context.");
}
void eval_sum(Call* call, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  std::cerr << "%%%% Evaluating sum" << std::endl;
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO
}

void eval_assert_b(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  if(call->n_args() == 2) {
    int r = CG::locate_immi(1, cg, frag);
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  } else {
    assert(call->n_args() == 3);
    CG::eval(call->arg(2), ctx, cg, frag);
  }
}

void eval_assert_g(Call* call, Mode ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  assert(call->n_args() == 3);
  CG::eval(call->arg(2), ctx, cg, pred, value);
}

builtin_table init_builtins(void) {
  builtin_table tbl;
  Constants& c(constants());
  tbl.insert(std::make_pair(c.ids.sum, builtin_t { eval_error_b, eval_sum } ));
  tbl.insert(std::make_pair(c.ids.exists, builtin_t { eval_exists, eval_error_g } ));
  tbl.insert(std::make_pair(c.ids.forall, builtin_t { eval_forall, eval_error_g } ));
  tbl.insert(std::make_pair(c.ids.assert, builtin_t { eval_assert_b, eval_assert_g } ));
  return tbl;
}
builtin_table& builtins(void) {
  static builtin_table tbl(init_builtins());
  return tbl;
}

void CG::eval(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // Check whether this is a transition Bool -> Value.
  switch(b->op()) {
    case BOT_LE:
    case BOT_LQ:
    case BOT_GR:
    case BOT_GQ:
    case BOT_EQ:
    case BOT_NQ: {
      // Potentially partial.
      // TODO: Convert into canonical form.
      CG_Builder frag_tl;
      Mode c_ctx(open_conj(ctx, frag));
      int r_lhs(CG::locate(b->lhs(), c_ctx, cg, frag, frag_tl));
      int r_rhs(CG::locate(b->rhs(), c_ctx, cg, frag, frag_tl));
      // PUSH_INSTR(frag_tl, BytecodeStream::CALL, c_ctx, find_op(cg, b->op()), CG::r(r_lhs), CG::r(r_rhs));
      call_binop(cg, frag_tl, ctx, b->op(), r_lhs, r_rhs);
      close_conj(ctx, frag_tl);
      frag.append(frag_tl);
    }
    break;
    case BOT_AND: {
      Mode c_ctx(open_conj(ctx, frag));
      CG::eval(b->lhs(), c_ctx, cg, frag);
      CG::eval(b->rhs(), c_ctx, cg, frag);
      close_conj(ctx, frag);
    }
    break;
    case BOT_OR: {
      Mode c_ctx(open_disj(ctx, frag));
      CG::eval(b->lhs(), c_ctx, cg, frag);
      CG::eval(b->rhs(), c_ctx, cg, frag);
      close_disj(ctx, frag);
    }
    break;
    case BOT_IMPL: {
      Mode c_ctx(open_disj(ctx, frag));
      CG::eval(b->lhs(), -c_ctx, cg, frag);
      CG::eval(b->rhs(), c_ctx, cg, frag);
      close_disj(ctx, frag);
    }
    break;
    case BOT_RIMPL: {
      Mode c_ctx(open_disj(ctx, frag));
      CG::eval(b->lhs(), c_ctx, cg, frag);
      CG::eval(b->rhs(), -c_ctx, cg, frag);
      close_disj(ctx, frag);
    }
    break;
    default: {
      // Standard case.
      int r_lhs(CG::locate(b->lhs(), ctx, cg, frag));
      int r_rhs(CG::locate(b->rhs(), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::CALL, ctx, find_op(cg, b->op()), CG::r(r_lhs), CG::r(r_rhs));
      break;
    }
  }
#if 0
  PUSH_INSTR(cond, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  if(b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
    // These all require rhs() != 0. Which means we need to evaluate the rhs,
    // and put it in a register.
    int r_lhs(CG::locate(b->lhs(), ctx, cg, cond, value));
    CG_Builder cond_tl;
    int r_rhs(CG::locate(b->rhs(), ctx, cg, cond_tl, value));
    int r_zero(CG::locate_immi(0, cg, cond_tl));
    PUSH_INSTR(cond_tl, BytecodeStream::CALL, /* find_builtin(cg, BOT_NQ), */ CG::r(r_rhs), CG::r(r_zero));
    PUSH_INSTR(value, BytecodeStream::CALL /*, find_builtin(cg, b->op(), r_lhs, r_rhs) */); 
    cond.append(cond_tl);
  } else {
    // TODO: Check whether the expression is aggregatable.
    int r_lhs = CG::locate(b->lhs(), ctx, cg, cond, value);
    int r_rhs = CG::locate(b->rhs(), ctx, cg, cond, value);
    PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN /*, find_builtin(cg, b->op())*/, CG::r(r_lhs), CG::r(r_rhs));
  }
  PUSH_INSTR(cond, BytecodeStream::CLOSE_AGGREGATION);
#endif
}

void CG::eval(BinOp* b, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // Check if the operator is itself partial.
  // Appropriate aggregation context should have been opened in the enclosing Boolean
  // expression, so we don't need to here.
  if(b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
    // These all require rhs() != 0. Which means we need to evaluate the rhs,
    // and put it in a register.
    int r_lhs(CG::locate(b->lhs(), ctx, cg, cond, value));
    CG_Builder cond_tl;
    int r_rhs(CG::locate(b->rhs(), ctx, cg, cond_tl, value));
    int r_zero(CG::locate_immi(0, cg, cond_tl));
    // PUSH_INSTR(cond_tl, BytecodeStream::CALL, ctx, find_op(cg, BOT_NQ), CG::r(r_rhs), CG::r(r_zero));
    // PUSH_INSTR(value, BytecodeStream::CALL, ctx, find_op(cg, b->op()), CG::r(r_lhs), CG::r(r_rhs)); 
    call_binop(cg, cond_tl, ctx, BOT_NQ, r_lhs, r_rhs);
    call_binop(cg, value, BytecodeProc::FUN, b->op(), r_lhs, r_rhs);
    cond.append(cond_tl);
  } else {
    // TODO: Check whether the expression is aggregatable.
    int r_lhs = CG::locate(b->lhs(), ctx, cg, cond, value);
    int r_rhs = CG::locate(b->rhs(), ctx, cg, cond, value);
    // PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN, find_op(cg, b->op()), CG::r(r_lhs), CG::r(r_rhs));
    call_binop(cg, value, BytecodeProc::FUN, b->op(), r_lhs, r_rhs);
  }
}

void CG::eval(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If we're in a Boolean context, we _must_ be evaluating not.
  // So just push the stuff inwards.
  assert(u->op() == UOT_NOT);
  CG::eval(u->e(), -ctx, cg, frag);
}

void CG::eval(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // All the unary operators are total, so just evaluate the result, and call the appropriate
  // builtin.
  switch(u->op()) {
    case UOT_NOT:
      TODO();
    case UOT_PLUS:
      CG::eval(u->e(), ctx, cg, cond, value);
      return;
    case UOT_MINUS:
      int r_e = CG::locate(u->e(), ctx, cg, cond, value);
      PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN, cg.find_builtin("int_neg"), CG::r(r_e));
      return;
  }
}
int CG::locate(UnOp* u, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // All the unary operators are total, so just evaluate the result, and call the appropriate
  // builtin.
  switch(u->op()) {
    case UOT_NOT:
      TODO();
      return 0xdeadbeef;
    case UOT_PLUS:
      return CG::locate(u->e(), ctx, cg, cond, value);
    case UOT_MINUS:
      int r_e = CG::locate(u->e(), ctx, cg, cond, value);
      PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN, cg.find_builtin("int_neg"), CG::r(r_e));
      int r(GET_REG(cg));
      PUSH_INSTR(value, BytecodeStream::POP, CG::r(r));
      return r;
  }
}

void CG::eval(Call* call, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  // If we have a builtin for this, dispatch to that instead.
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      (*it).second.boolean(call, ctx, cg, frag);
      return;
    }
  }

  // Might have some partiality from the arguments.
  CG_Builder frag_tl;

  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  
  // FIXME: Do some mode analysis on the arguments.
  Mode c_ctx(open_conj(ctx, frag));
  for(int ii = 0; ii < sz; ++ii) {
    r_arg[ii] = CG::r(CG::locate(call->arg(ii), BytecodeProc::FUN, cg, frag, frag_tl));
  }
  // And finally, add the call itself
  PUSH_INSTR(frag_tl, BytecodeStream::CALL, c_ctx, find_call_fun(cg, call), r_arg);
  close_conj(ctx, frag_tl);
  frag.append(frag_tl);
}

void CG::eval(Call* call, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // For a call, we need to bind each of the arguments into registers.
  // They need to be available for the conditional call, so they get evaluated in the
  // conditional bit.
  {
    GCLock gc;
    auto it(builtins().find(call->id().str()));
    if(it != builtins().end()) {
      (*it).second.general(call, ctx, cg, cond, value);
      return;
    }
  }
  CG_Builder cond_tl;

  int sz = call->n_args();
  std::vector<CG_Value> r_arg(sz);
  
  Mode c_ctx(open_conj(ctx, cond));
  for(int ii = 0; ii < sz; ++ii) {
    r_arg[ii] = CG::r(CG::locate(call->arg(ii), BytecodeProc::FUN, cg, cond, cond_tl));
  }
  // Emit the call to the Boolean part.
  PUSH_INSTR(cond_tl, BytecodeStream::CALL, c_ctx, find_call_pred(cg, call), r_arg);
  close_conj(ctx, cond_tl);
  cond.append(cond_tl);
  
  // And finally, add the arithmetic part.
  PUSH_INSTR(value, BytecodeStream::CALL, c_ctx, find_call_fun(cg, call), r_arg);
}

void CG::eval(Let* let, Mode ctx, CodeGen& cg, CG_Builder& frag) {
  TODO();
}

void CG::eval(Let* let, Mode ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  TODO();
}

};
