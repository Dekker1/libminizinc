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

struct builtin_t {
  std::function<void(Call*, BCtx, CodeGen&, CG_Builder&)> boolean;
  std::function<void(Call*, BCtx, CodeGen&, CG_Builder&, CG_Builder&)> general;
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
  "VCTX_LIN",
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

CG_ProcID CodeGen::builtin_proc(std::string s) {
  auto it(_builtins.find(s));
  if(it != _builtins.end())
    return (*it).second;
  CG_ProcID id(_proc_info.size());
  _proc_info.push_back(s);
  _builtins.insert(std::make_pair(s, id));
  return id;
}


CG_ProcID find_op(CodeGen& cg, BinOpType op) {
  return CG_ProcID(0xbeef);
}
CG_ProcID find_op(CodeGen& cg, UnOpType op) {
  return CG_ProcID(0xfeed);
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
// The code generation is done with a combinator style.
// Combinators for an expression take a CodeGen object

// BIND(id, e) : reg_id -> codegen -> code
// Evaluates e with id bound to the received register.
#if 0
template<class E>
struct _BIND {
  ASTString id;
  int r;
  E e;
  _BIND(ASTString _id, int _r, E _e) 
    : id(_id), r(_r), e(_e) { }
  
  auto operator()(CodeGen& cg) -> decltype(e(cg)) {
    auto it = cg.env().find(id);
    if(it != cg.env().end()) {
      // A binding already existed.
      Loc old = (*it).second;
      cg.env().erase(id);
      cg.env().insert(std::make_pair(id, Loc::reg(r)));
      /*
      auto ret(e(cg));
      cg.env().erase(id);
      cg.env().insert(std::make_pair(id, old));
      return ret;
      */
      return e(cg);
    } else {
      // Not yet bound.
      cg.env().insert(std::make_pair(id, Loc::reg(r)));
      /*
      auto ret(e(cg));
      cg.env().erase(id);
      return ret;
      */
      return e(cg);
    }
  }
};
template<class E>
auto BIND(ASTString& id, E e) -> std::function<_BIND<E>(int)> { return [&](int r) { return _BIND<E>(id, r, e); }; }
#endif

// FIXME
unsigned int BINOP_TAG(BinOpType b) { return 1; }
unsigned int UNOP_TAG(UnOpType b) { return 1; }

template<class ...Args>
struct _SEQ { };

template<>
struct _SEQ<> {
  void operator()(CodeGen& cg) { }
};
template<class Car, class ...Args>
struct _SEQ<Car, Args...> {
  _SEQ(Car _car, Args... _args) : car(_car), cdr(_args...) { }

  void operator()(CodeGen& cg) {
    car(cg);
    cdr(cg);
  }

  Car car;
  _SEQ<Args...> cdr;
};
template<class ...Args>
_SEQ<Args...> SEQ(Args... args) { return _SEQ<Args...>(args...); }

// Apply an n-ary Boolean operator
template<class ...Args>
struct _OP {
  _OP(char _op, Args... _args)
    : op(_op), args(_args...) { }
  void operator()(CodeGen& cg, CG_Builder& frag) {
    PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER); // FIXME
    args(cg, frag);
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
  }
  char op;
  _SEQ<Args...> args;
};
template<class ...Args>
_OP<Args...> OP(int op, Args... args) { return _OP<Args...>(op, args...); }

struct EVAL {
  EVAL(Expression* _e, BCtx _ctx) : e(_e), ctx(_ctx) { }

  void operator()(CodeGen& cg, CG_Builder& frag) { CG::eval(e, ctx, cg, frag); }

  Expression* e;
  BCtx ctx;
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

  EnvInit(CG_Env<Loc>& _env)
    : slot(0), env(_env) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(!vd->type().isvar() && !vd->type().isann()) {
      std::cerr << "%%%% Binding " << vd->id()->str() << " at g" << slot << std::endl;
      std::cerr << "%%%% "; debugprint(vd);
      if(!vd->e()) {
        // debugprint(vd);
        env.bind(vd->id()->v(), Loc::global(slot));
        ++slot;
      } else {
        // Evaluate the definition
        env.bind(vd->id()->v(), Loc::global(slot));
        ++slot;
      }
    }
  }
  /// Visit assign item
  void vAssignI(AssignI* ass) {
    // debugprint(ass); 
  }

  void vFunctionI(FunctionI* f) {
    std::cout << "%% F: "; debugprint(f);
  }

  int slot;
  CG_Env<Loc>& env;
public:
  static void run(CG_Env<Loc>& env, Model* m) {
    EnvInit eb(env);
    iterItems(eb, m);
  }
};

// Placeholders.
class Compile : public ItemVisitor {
private:
  friend class ItemIter<Compile>;

  Compile(CodeGen& _cg) : cg(_cg) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(vd->type().isann())
      return;
    std::cerr << "## Binding " << vd->id()->str() << std::endl;
    if(vd->type().isvar()) {
      // In whatever case, we're going to create something,
      // and dump it in a register.
      int r;
      if(vd->e()) {
        // Defined
        // Evaluate the definition in root context,
        // add it to a register
        r = CG::locate(vd->e(), C_ROOT, cg, root_frag);
      } else {
        Expression* d(vd->ti()->domain());
        assert(d);
        int r_d = CG::locate(d, C_ROOT, cg, root_frag);
        if(vd->ti()->isarray()) {
          // Open nested iterators
          std::vector<int> r_regs;
          for(Expression* r : vd->ti()->ranges()) {
            Expression* dim(r->template cast<TypeInst>()->domain());
            r_regs.push_back(CG::locate(dim, C_ROOT, cg, root_frag));
          }
          std::vector<Forset> nesting;
          PUSH_INSTR(root_frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_VEC);
          for(int r_r : r_regs) {
            Forset iter(cg, r_r);
            nesting.push_back(iter);
            iter.emit_pre(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CALL, BytecodeProc::FUN, cg.builtin_proc("make-var"), CG::r(r_d));
          for(int r_i = r_regs.size()-1; r_i >= 0; --r_i) {
            nesting[r_i].emit_post(root_frag);
          }
          PUSH_INSTR(root_frag, BytecodeStream::CLOSE_AGGREGATION);
        } else {
          // FIXME: Compute the domain of the variable.
          PUSH_INSTR(root_frag, BytecodeStream::CALL, cg.builtin_proc("make-var"), CG::r(r_d));
        }
        r = TEMP_REG(cg);
        PUSH_INSTR(root_frag, BytecodeStream::POP, CG::r(r));
      }
      // Now copy it into a global, and add it to the env.
      PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(cg.env().size()));
      cg.env().bind(vd->id()->v(), Loc::global(cg.env().size()));
    } else {
      // FIXME: Handle the par case.
      if(vd->e() && !vd->type().isann()) {
        // Evaluate the definition.
        int r = CG::locate(vd->e(), C_ROOT, cg, root_frag);
        PUSH_INSTR(root_frag, BytecodeStream::STORE_GLOBAL, CG::r(r), CG::g(cg.env().size()));
        cg.env().bind(vd->id()->v(), Loc::global(cg.env().size()));
      }
    }
  }

  /// Visit assign item
  void vAssignI(AssignI* ass) {
    std::cerr << "%%%% Assign: "; debugprint(ass); 
  }

  void vConstraintI(ConstraintI* c) {
    std::cerr << "%%%% "; debugprint(c->e());
    CG::eval(c->e(), C_ROOT, cg, root_frag);
  }
  CodeGen& cg;
  CG_Builder root_frag;
public:
  static void run(CodeGen& cg, Model* m) {
    Compile c(cg);
    iterItems(c, m);

    // Now generate procedures for any necessary function/predicate bodies.
  }
};

void CG::run(CodeGen& cg, Model* m) {
  // First, collect the model parameters, and assign them
  // global slots.
  EnvInit::run(cg.env(), m);
  Compile::run(cg, m); 
}

// Evaluate an expression, place it on the value stack.
void CG::eval(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  // If the value is already available in a register somewhere, just push it.
  Loc l(Loc::reg(0));
  if(cg.cache_lookup(e, l)) {
    int r;
    if(l.is_reg()) {
      r = l.index();
    } else {
      // If a global, put it in a register, and update the cached value.
      r = GET_REG(cg); 
      PUSH_INSTR(frag, BytecodeStream::LOAD_GLOBAL, CG::g(l.index()), CG::r(r));
      cg.cache_store(e, Loc::reg(r));
    }
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
    return;
  }
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
    CG::eval(e->template cast<BoolLit>(), cg, frag);
    break;
  case Expression::E_STRINGLIT:
    CG::eval(e->template cast<StringLit>(), cg, frag);
    break;
  case Expression::E_ID:
    CG::eval(e->template cast<Id>(), cg, frag);
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
void CG::eval(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
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
    CG::eval(e->template cast<BoolLit>(), cg, value);
    break;
  case Expression::E_STRINGLIT:
    CG::eval(e->template cast<StringLit>(), cg, value);
    break;
  case Expression::E_ID:
    CG::eval(e->template cast<Id>(), cg, value);
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

int CG::locate(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
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
    r = CG::locate(e->template cast<BoolLit>(), cg, frag);
    break;
  case Expression::E_STRINGLIT:
    r = CG::locate(e->template cast<StringLit>(), cg, frag);
    break;
  case Expression::E_ID:
    r = CG::locate(e->template cast<Id>(), cg, frag);
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
    r = GET_REG(cg);
    PUSH_INSTR(frag, BytecodeStream::POP, CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
    break;
  }
  }
  // Now save it in the cache.
  cg.cache_store(e, Loc::reg(r));
  return r;
}

int CG::locate(Expression* e, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  switch (e->eid()) {
  case Expression::E_INTLIT:
    return CG::locate(e->template cast<IntLit>(), cg, value);
    break;
  case Expression::E_FLOATLIT:
    return CG::locate(e->template cast<FloatLit>(), cg, value);
    break;
  case Expression::E_BOOLLIT:
    return CG::locate(e->template cast<BoolLit>(), cg, value);
    break;
  case Expression::E_STRINGLIT:
    return CG::locate(e->template cast<StringLit>(), cg, value);
    break;
  case Expression::E_ID:
    return CG::locate(e->template cast<Id>(), cg, value);
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
    int r = GET_REG(cg);
    PUSH_INSTR(pred, BytecodeStream::POP, CG::r(r));
    PUSH_INSTR(pred, BytecodeStream::CLOSE_AGGREGATION);
    cg.cache_store(e, Loc::reg(r));
    return r;
    break;
  }
  }
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

int CG::locate(BoolLit* z, CodeGen& cg, CG_Builder& frag) {
  int x(z->v());
  int r(GET_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(x), CG::r(r));
  return r;
}

void CG::eval(FloatLit* f, CodeGen& cg, CG_Builder& frag) { throw InternalError("FloatLit not yet supported by bytecode generator."); }
int CG::locate(FloatLit* f, CodeGen& cg, CG_Builder& frag) { throw InternalError("FloatLit not yet supported by bytecode generator."); }

BCtx CHILD_CTX(BCtx ctx, unsigned int op) { return C_MIX; }
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
    PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(u+1), CG::r(r));
    PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

void CG::eval(BoolLit* b, CodeGen& cg, CG_Builder& frag) {
  int r(TEMP_REG(cg));
  PUSH_INSTR(frag, BytecodeStream::IMMI, CG::i(b->v()), CG::r(r));
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
}

void CG::eval(StringLit* s, CodeGen& cg, CG_Builder& frag) { throw InternalError("StringLit not yet handled by bytecode generator."); }
int CG::locate(StringLit* s, CodeGen& cg, CG_Builder& frag) { throw InternalError("StringLit not yet handled by bytecode generator."); }

void CG::eval(Id* id, CodeGen& cg, CG_Builder& frag) {
  // Look up the identifier in the environment.
  LOC(cg.env().lookup(id->v()))(cg, frag);
}

int CG::locate(Id * id, CodeGen& cg, CG_Builder& frag) {
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

void CG::eval(ArrayLit* a, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
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

void CG::eval(ArrayAccess* a, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  // If the array elements are Boolean, we need to check for partiality
  // in the indices, and that the accesses are within-range.
  ASTExprVec<Expression> idx(a->idx());
  Expression* A(a->v());

  // Evaluate the indices, put them in registers.
  // Indices might be partial, so we need to add the index constraints.
  int sz(idx.size());
  std::vector<int> r_idxs(sz);;
  CG_Builder frag_tl;

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  for(int ii = 0; ii < sz; ++ii)
    r_idxs[ii] = locate(idx[ii], ctx, cg, frag, frag_tl);
  frag.append(frag_tl);

  // Now evaluate the array body, and emit the indices.
  int r_A = CG::locate(A, ctx, cg, frag);
  // Finally, process the element lookup.
  eval_element_nd(r_A, r_idxs, cg, frag, frag_tl);
  PUSH_INSTR(frag_tl, BytecodeStream::CLOSE_AGGREGATION);
  frag.append(frag_tl);
}
// General (non-Boolean) version. Roughly the same shape as the Boolean case, except we separate
// the Boolean and integer context.
void CG::eval(ArrayAccess* a, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
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
  for(int ii = 0; ii < sz; ++ii)
    r_idxs[ii] = CG::locate(idx[ii], ctx, cg, pred, pred_val);

  // Now evaluate the array body, and emit the indices.
  int r_A = CG::locate(A, ctx, cg, pred, pred_val);
  // At the end of pred_val, everything is available, so we can
  // evaluate the partiality of A[i1, ...].
  eval_element_nd(r_A, r_idxs, cg, pred_val, value);
  PUSH_INSTR(pred_val, BytecodeStream::CLOSE_AGGREGATION);
  pred.append(pred_val);
}

// Evaluate a comprehension, pushing all the generated values onto the
// value stack.
// We're assuming comprehensions are always total.
void execute_comprehension(Comprehension* c, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
   // Build up the object to build the generator.
  std::vector<Forset> nesting;
  std::vector< std::pair<ASTString, Loc> > trail;

  int g = c->n_generators();
  for(int g = 0; g < c->n_generators(); ++g) {
    // Bind the in-expression to a register.
    int r = CG::locate(c->in(g), C_ROOT, cg, frag);
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
      int rC = CG::locate(where, C_MIX, cg, frag);
      // ASSUMING PAR HERE.
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

void CG::eval(Comprehension* c, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OTHER);
  execute_comprehension(c, ctx, cg, frag);
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}

void CG::eval(ITE* ite, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  // Successively evaluate the <condition, result> pairs.
  std::vector<int> r_cond;
  std::vector<int> r_res;

  int sz(ite->size());
  for(int ii = 0; ii < sz; ++ii) {
    r_cond.push_back(CG::locate(ite->e_if(ii), C_MIX, cg, frag));
    r_res.push_back(CG::locate(ite->e_then(ii), +ctx, cg, frag));
  }
  int r_final = CG::locate(ite->e_else(), +ctx, cg, frag);

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  // Post c_1 || ... || c_{k-1} || ~c_k || v_k, for k in 1..n.
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
void CG::eval(ITE* ite, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
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
void eval_forall(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          CG::eval((*a)[ii], ctx, cg, frag);
      }
      break;
    case Expression::E_COMP: {
      execute_comprehension(param->cast<Comprehension>(), ctx, cg, frag);
      }
      break;
    default:
      {
        int r = CG::locate(param, ctx, cg, frag);
        FOREACH(RETN()(r), PUSH())(cg, frag);
      }
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
// Same as forall, but producing an or-context.
void eval_exists(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 1);
  Expression* param = call->arg(0);

  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OR);
  // Now check the expression's type. If it's an array literal or
  // comprehension, we generate code directly, rather than generating
  // a concrete vector.
  switch(param->eid()) {
    case Expression::E_ARRAYLIT: {
        ArrayLit* a(param->cast<ArrayLit>());
        int sz(a->size());
        for(int ii = 0; ii < sz; ++ii)
          CG::eval((*a)[ii], ctx, cg, frag);
      }
      break;
    case Expression::E_COMP: {
      Comprehension* c(param->cast<Comprehension>());
      execute_comprehension(c, ctx, cg, frag);
      }
      break;
    default:
      {
        // Otherwise, get the result into a register...
        int r = CG::locate(param, ctx, cg, frag);
        // and push every element.
        FOREACH(RETN()(r), PUSH())(cg, frag);
      }
  }
  PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
}
void eval_error_b(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  throw InternalError("Call should only appear in general context.");
}
void eval_error_g(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& pred, CG_Builder& value) {
  throw InternalError("Call should only appear in Boolean context.");
}
void eval_sum(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  std::cerr << "## Evaluating sum" << std::endl;
  assert(call->n_args() == 1);
  Expression* e = call->arg(0);
  // Components of the sum may be partial.
  // TODO
}

void eval_assert_b(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  assert(call->n_args() == 2);
  // return CG::eval(call->arg(2), ctx, cg, frag);
  // FIXME: Ignoring for now.
  int r = CG::locate_immi(1, cg, frag);
  PUSH_INSTR(frag, BytecodeStream::PUSH, CG::r(r));
}

builtin_table init_builtins(void) {
  builtin_table tbl;
  Constants& c(constants());
  tbl.insert(std::make_pair(c.ids.sum, builtin_t { eval_error_b, eval_sum } ));
  tbl.insert(std::make_pair(c.ids.exists, builtin_t { eval_exists, eval_error_g } ));
  tbl.insert(std::make_pair(c.ids.forall, builtin_t { eval_forall, eval_error_g } ));
  tbl.insert(std::make_pair(c.ids.assert, builtin_t { eval_assert_b, eval_error_g } ));
  return tbl;
}
builtin_table& builtins(void) {
  static builtin_table tbl(init_builtins());
  return tbl;
}

// FIXME: Correct contexts.
void CG::eval(BinOp* b, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  // Check whether this is a transition Bool -> Value.
  switch(b->op()) {
    case BOT_LE:
    case BOT_LQ:
    case BOT_GR:
    case BOT_GQ:
    case BOT_EQ:
    case BOT_NQ: {
      // Potentially partial.
      CG_Builder frag_tl;
      PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
      int r_lhs(CG::locate(b->lhs(), ctx, cg, frag, frag_tl));
      int r_rhs(CG::locate(b->rhs(), ctx, cg, frag, frag_tl));
      PUSH_INSTR(frag_tl, BytecodeStream::CALL, CG::r(r_lhs), CG::r(r_rhs));
      PUSH_INSTR(frag_tl, BytecodeStream::CLOSE_AGGREGATION);
    }
    break;
    case BOT_AND: {
      PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
      int r_lhs(CG::locate(b->lhs(), ctx, cg, frag));
      int r_rhs(CG::locate(b->rhs(), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
    }
    break;
    case BOT_OR: {
      PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_OR);
      int r_lhs(CG::locate(b->lhs(), ctx, cg, frag));
      int r_rhs(CG::locate(b->rhs(), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::CLOSE_AGGREGATION);
    }
    break;
    default: {
      // Standard case.
      int r_lhs(CG::locate(b->lhs(), ctx, cg, frag));
      int r_rhs(CG::locate(b->rhs(), ctx, cg, frag));
      PUSH_INSTR(frag, BytecodeStream::CALL, /* FIND THE BUILTIN */ CG::r(r_lhs), CG::r(r_rhs));
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

void CG::eval(BinOp* b, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // Check if the operator is itself partial.
  PUSH_INSTR(cond, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  if(b->op() == BOT_DIV || b->op() == BOT_IDIV || b->op() == BOT_MOD) {
    // These all require rhs() != 0. Which means we need to evaluate the rhs,
    // and put it in a register.
    int r_lhs(CG::locate(b->lhs(), ctx, cg, cond, value));
    CG_Builder cond_tl;
    int r_rhs(CG::locate(b->rhs(), ctx, cg, cond_tl, value));
    int r_zero(CG::locate_immi(0, cg, cond_tl));
    PUSH_INSTR(cond_tl, BytecodeStream::CALL, find_op(cg, BOT_NQ), CG::r(r_rhs), CG::r(r_zero));
    PUSH_INSTR(value, BytecodeStream::CALL , find_op(cg, b->op()), CG::r(r_lhs), CG::r(r_rhs)); 
    cond.append(cond_tl);
  } else {
    // TODO: Check whether the expression is aggregatable.
    int r_lhs = CG::locate(b->lhs(), ctx, cg, cond, value);
    int r_rhs = CG::locate(b->rhs(), ctx, cg, cond, value);
    PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN /*, find_builtin(cg, b->op())*/, CG::r(r_lhs), CG::r(r_rhs));
  }
  PUSH_INSTR(cond, BytecodeStream::CLOSE_AGGREGATION);
}

void CG::eval(UnOp* u, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  // If we're in a Boolean context, we _must_ be evaluating not.
  // So just push the stuff inwards.
  assert(u->op() == UOT_NOT);
  CG::eval(u->e(), -ctx, cg, frag);
}

void CG::eval(UnOp* u, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  // All the unary operators are total, so just evaluate the result, and call the appropriate
  // builtin.
  int r_e = CG::locate(u->e(), ctx, cg, cond, value);
  PUSH_INSTR(value, BytecodeStream::CALL, BytecodeProc::FUN /*,  FIND THE BUILTIN */, CG::r(r_e));
}

void CG::eval(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
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
  
  PUSH_INSTR(frag, BytecodeStream::OPEN_AGGREGATION, AggregationCtx::VCTX_AND);
  for(int ii = 0; ii < sz; ++ii) {
    r_arg[ii] = CG::r(CG::locate(call->arg(ii), ctx, cg, frag, frag_tl));
  }
  // Emit the call to the Boolean part.
  // And finally, add the arithmetic part.
  PUSH_INSTR(frag_tl, BytecodeStream::CALL /*, find_call_fun(cg, call) */, r_arg);
  PUSH_INSTR(frag_tl, BytecodeStream::CLOSE_AGGREGATION);
  frag.append(frag_tl);
}

void CG::eval(Call* call, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
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
  
  for(int ii = 0; ii < sz; ++ii) {
    r_arg[ii] = CG::r(CG::locate(call->arg(ii), ctx, cg, cond, cond_tl));
  }
  // Emit the call to the Boolean part.
  PUSH_INSTR(cond_tl, BytecodeStream::CALL /*, find_call_pred(cg, call) */, r_arg);
  
  // And finally, add the arithmetic part.
  PUSH_INSTR(value, BytecodeStream::CALL /*, find_call_fun(cg, call) */, r_arg);
  cond.append(cond_tl);
}

void CG::eval(Let* let, BCtx ctx, CodeGen& cg, CG_Builder& frag) {
  TODO();
}

void CG::eval(Let* let, BCtx ctx, CodeGen& cg, CG_Builder& cond, CG_Builder& value) {
  TODO();
}

};
