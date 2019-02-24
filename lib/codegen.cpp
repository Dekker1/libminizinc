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

namespace MiniZinc {

/*
struct CallSig {
  ASTString id;
};
*/

// Location is either a register or global.
class Loc {
private:
  Loc(int _x) : x(_x) { }
public:
  static Loc reg(int r) { return r<<1; }
  static Loc global(int g) { return (g<<1)|1; }

  inline bool is_reg(void) const { return !(x&1); }
  inline bool is_global(void) const { return x&1; }
  inline int index(void) const { return x>>1; }

  int x;
};

// During code generation, we track the mapping between
// identifiers and their locations.
typedef std::unordered_map<ASTString, Loc> CG_Env;

// The code generation is done with a combinator style.
// Combinators for an expression take a CodeGen object
void PUSH_INSTR(CodeGen& cg) { return; }

template<typename... Args>
void PUSH_INSTR(CodeGen& cg, BytecodeStream::Instr i, Args... args) {
  cg.bytecode[cg.current_proc].addInstr(i);
  PUSH_INSTR(cg, args...);
}
template<typename T, typename... Args>
void PUSH_INSTR(CodeGen& cg, T x, Args... args) {
  /*
  char mem[sizeof(T)];
  memcpy(mem, &x, sizeof(T));
  for(int ii = 0; ii < sizeof(T); ++ii)
    cg.bytecode.push_back(mem[ii]);
    */
  PUSH_INSTR(cg, args...);
}


struct REGb {
  void operator()(CodeGen& cg) {
//    PUSH_INSTR(cg, BytecodeStream::PUSH_COND, (char) r);
  }
  int r;
};

inline int GET_REG(CodeGen& cg) { return cg.current_reg_count++; }
inline int TEMP_REG(CodeGen& cg) { return cg.current_reg_count; }

template<class V, class E>
struct _LETb {
  V v;
  E e; 
  auto operator()(CodeGen& cg) -> decltype(e(0)(cg)) {
    // 
    int reg(GET_REG(cg));
//    PUSH_INSTR(cg, BytecodeStream::OPEN_BOOL_CTX, 0);
    v(cg);
//    PUSH_INSTR(cg, BytecodeStream::CLOSE_BOOL_CTX);
//    PUSH_INSTR(cg, BytecodeStream::POP_COND, reg);
    return e(reg)(cg);
  }
};
template<class V, class E>
_LETb<V, E> LETb(V&& v, E&& e) { return _LETb<V, E> { std::move(v), std::move(e) }; }

template<class V, class E>
struct _LETx {
  V v;
  E e; 
  auto operator()(CodeGen& cg) -> decltype(e(0)(cg)) {
    int reg(GET_REG(cg));
//    PUSH_INSTR(cg, BytecodeStream::OPEN_VAL_CTX, 0);
    v(cg);
//    PUSH_INSTR(cg, BytecodeStream::CLOSE_VAL_CTX);
//    PUSH_INSTR(cg, BytecodeStream::POP_VAL, reg);
    return e(reg)(cg);
  }
};
template<class V, class E>
_LETx<V, E> LETx(V&& v, E&& e) { return _LETx<V, E> { std::move(v), std::move(e) }; }

template<class T>
struct Retn {
  Retn(T _x) : x(_x) { }

  T operator()(CodeGen& cg) const { return x; }
  T x;
};
struct RETN {
  RETN() { }

  template<class T>
  Retn<T> operator()(T x) { return Retn<T>(x); }
};

#if 0
// When called, c returns a register which
// will contain the result.
template<class C, class T, class E>
struct ITE {
  void operator()(CodeGen& cg) {
    int c_reg(c(cg));
    int else_label(cg.get_label());
    PUSH_INSTR(cg, BytecodeStream::JMPIFNOT, c_reg, else_label);     
    t(cg); 
    int done_label(cg.get_label());
    PUSH_INSTR(cg, BytecodeStream::JMP, done_label);
    PUSH_LABEL(cg, else_label);
    e(cg);
    PUSH_LABEL(cg, done_label);
  }
  C c;
  T t;
  E e;
};
#endif

template<class ...Args>
struct SEQ { };

template<>
struct SEQ<> {
  void operator()(CodeGen& cg) { }
};
template<class Car, class ...Args>
struct SEQ<Car, Args...> {
  SEQ(Car _car, Args... _args) : car(_car), cdr(_args...) { }

  void operator()(CodeGen& cg) {
    car(cg);
    cdr(cg);
  }

  Car car;
  SEQ<Args...> cdr;
};

// Apply an n-ary Boolean operator
template<class ...Args>
struct OPb {
  OPb(char _op, Args... _args)
    : op(_op), args(_args...) { }
  void operator()(CodeGen& cg) {
//    PUSH_INSTR(cg, BytecodeStream::OPEN_BOOL_CTX, op);
    args(cg);
//    PUSH_INSTR(cg, BytecodeStream::CLOSE_BOOL_CTX);
  }
  char op;
  SEQ<Args...> args;
};

template<class ...Args>
struct OPx {
  OPx(char _op, Args... _args)
    : op(_op), args(_args...) { }
  void operator()(CodeGen& cg) {
//    PUSH_INSTR(cg, BytecodeStream::OPEN_VAL_CTX, op);
    args(cg);
//    PUSH_INSTR(cg, BytecodeStream::CLOSE_VAL_CTX);
  }
  char op;
  SEQ<Args...> args;
};

#if 0
void Bool::emit(CodeGen& cg, ITE* ite) {
  // Evaluate the condition, and load it in a register
  return LET(ite->e_cond(), [](int r_cond) {
      LET(ite->e_then(), [](int r_then) {
        LET(ite->e_false(), [](int r_false) {
          OR(
            AND(REG(r_cond), REG(r_then)),
            AND(NOT(REG(r_cond)), REG(r_else))
          )
        })
      })
    })(env);
}

void Bool::emit(CodeGen& cg, BinOp* e) {
  PUSH_INSTR(cg, BytecodeStream::OPEN_BOOL_CTX, e->op());
  Bool::emit(cg, e->lhs());
  Bool::emit(cg, e->rhs());
  PUSH_INSTR(cg, BytecodeStream::CLOSE_BOOL_CTX);
}
/*
  class IntLit;
  class FloatLit;
  class SetLit;
  class BoolLit;
  class StringLit;
  class Id;
  class AnonVar;
  class ArrayLit;
  class ArrayAccess;
  class Comprehension;
  class ITE;
  class BinOp;
  class UnOp;
  class Call;
  class VarDecl;
  class Let;
  class TypeInst;
  */
#endif
struct EVAL {
  EVAL(Expression* _e) : e(_e) { }

  void operator()(CodeGen& cg) { CG::run(cg, e); }

  Expression* e;
};

#if 0
void CG::run(CodeGen& cg, IntLit* z) {
  // First push to a register, then the stack.
  int r(TEMP_REG(cg));
  PUSH_INSTR(cg, IMMI, z->v().toInt(), r);
  PUSH_INSTR(cg, PUSH_VAL, r);
}
void CG::run(CodeGen& cg, FloatLit* f) {
  throw InternalError("floats not yet supported");
}
void CG::run(CodeGen& cg, SetLit* s) {
  throw InternalError("sets not yet supported");
}
void CG::run(CodeGen& cg, BoolLit* b) {
  int r(TEMP_REG(cg));
  PUSH_INSTR(cg, IMMI, b->v());
  PUSH_INSTR(cg, PUSH_COND, r);
}
void CG::run(CodeGen& cg, StringLit* s) {
  throw InternalError("strings not yet supported");
}
void CG::run(CodeGen& cg, Id* id) {
  //
}
void CG::run(CodeGen& cg, AnonVar* v) {
  throw InternalError("anonymous variable not handled.");
}
void CG::run(CodeGen& cg, ArrayLit* a) {
  // Build up the array contents, and aggregate
  // it into a value.
  PUSH_INSTR(cg, OPEN_VAL_CTX, VEC);
  for(int ii = 0; ii < a->size(); ++ii)
    CG::run(cg, (*a)[ii]);
  PUSH_INSTR(cg, CLOSE_VAL_CTX);
}
void CG::run(CodeGen& cg, ArrayAccess* a) {
  throw InternalError("array accesses not yet handled.");
}
void CG::run(CodeGen& cg, Comprehension* c) {
    
}
void CG::run(CodeGen& cg, ITE* ite) {
  // Simple version, nothing fancy.
  Expression* c(ite->e_cond());
  Expression* t(ite->e_then());
  Expression* e(ite->e_else());
  if(ite.type().isbool()) {
    // Ends up on the Bool stack.
    LET(EVAL(c), [](int r_cond) {
      LET(EVAL(t), [](int r_then) {
        LET(EVAL(e), [](int r_false) {
          OPb(OR,
            OPb(AND, REG(r_cond), REG(r_then)),
            OPb(AND, UOPb(NOT(REG(r_cond))), REG(r_else))
          )
        })
      })
    })(cg);
  } else {
    // FIXME
  }
}

void CG::run(CodeGen& cg, BinOp* e) {
  if(e.type().isbool()) {
    OPb(e->op(),
      EVAL(e->lhs()),
      EVAL(e->rhs())); 
  } else {
    OPx(e->op(),
      EVAL(e->lhs()),
      EVAL(e->rhs()));
  }
}
void CG::run(CodeGen& cg, UnOp* op) {
  if(e.type().isbool()) {
    UNOPb(e->op(),
      EVAL(e->e()));
  } else {
    UNOPx(e->op(),
      EVAL(e->e()));
  }
}

void CG::run(CodeGen& cg, Call* call) {

}

void CG::run(CodeGen& cg, VarDecl* decl) {
  // Should only be part of a let, or a top-level item.
  ASTString x(decl->id()->v());
  Expression* e(decl->e());
  if(e) {
    // If there is a declaration, evaluate it, and bind
    // it to a register.
    if(e->type()->isbool()) {
      LETb(EVAL(decl->e()),
        [&x](int r_ex) {
          cg.current_env.insert(std::make_pair(x, r_ex));
        })(cg);
    } else {
      // CHECK: If not Boolean, should open a conjunctive context,
      // in case decl->e() is partial.
      LETx(EVAL(decl->e()),
        [&x](int r_ex) {
          cg.current_env.insert(std::make_pair(x, r_ex));
        })(cg);
    }
  } else {
    // Otherwise, we just bind a fresh variable.
    // FIXME  
  }
}

void CG::run(CodeGen& cg, Let* let) {
  // Update the environment with whatever
  // appears in the let expression, then
  // push the result onto the appropriate stack.

}

void CG::run(CodeGen& cg, TypeInst* ti) {

}
#endif

class EnvInit : public ItemVisitor {
private:
  friend class ItemIter<EnvInit>;

  EnvInit(void)
    : slot(0) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(!vd->type().isvar() && !vd->type().isann() && !vd->e()) {
      debugprint(vd);
      env.insert(std::make_pair(vd->id()->v(), Loc::global(slot)));
      ++slot;
    }
  }
  /// Visit assign item
  void vAssignI(AssignI* ass) {
    // debugprint(ass); 
  }

  int slot;
  CG_Env env;
public:
  static CG_Env init(Model* m) {
    EnvInit eb;
    iterItems(eb, m);
    return eb.env;
  }
};

class Compile : public ItemVisitor {
private:
  friend class ItemIter<Compile>;

  Compile(CodeGen& _cg, CG_Env& _env0) : cg(_cg), env0(_env0) { }

  /// Enter model
  bool enterModel(Model* m) { return true; }
  /// Enter item
  bool enter(Item* m) { return true; }
  /// Visit variable declaration
  void vVarDeclI(VarDeclI* vdi) {
    VarDecl* vd(vdi->e());
    if(vd->type().isvar()) {
      // In whatever case, we're going to create something,
      // and dump it in a register.
      int r;
      if(vd->e()) {
        // Defined
        // Evaluate the definition in root context,
        // add it to a register
        if(vd->type().isbool()) {
          r = LETb(EVAL(vd->e()), RETN())(cg);
        } else {
          r = LETx(EVAL(vd->e()), RETN())(cg);
        }
      } else {
        // Just introduce a new variable, and bind it.
        int r = 0;
      }
      // Now copy it into a global, and add it to the env.
      PUSH_INSTR(cg, BytecodeStream::STORE_GLOBAL, r, env0.size());
      env0.insert(std::make_pair(vd->id()->v(), Loc::global(env0.size())));
    } else {
      // FIXME: Handle the par case.
          
    }
  }
  /// Visit assign item
  void vAssignI(AssignI* ass) {
    std::cerr << "Assign: ";
    debugprint(ass); 
  }

  void vConstraintI(ConstraintI*) {
    // 
  }
  CodeGen& cg;
  CG_Env env0;
public:
  static void run(CodeGen& cg, CG_Env& env0, Model* m) {
    Compile c(cg, env0);
    iterItems(c, m);
  }
};


void CG::run(CodeGen& cg, Model* m) {
  // First, collect the model parameters, and assign them
  // global slots.
  CG_Env env(EnvInit::init(m));
  Compile::run(cg, env, m); 
}

void CG::run(CodeGen& cg, Expression* e) {
  assert(0); 
}

};
