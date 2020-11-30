#include <minizinc/codegen.hh>

#include <../lib/codegen/analysis.hpp>

namespace MiniZinc {

/// Visit identifier
void ModeAnalysis::vId(Id& x, Occurrence o, CG::Mode m) {
  // Use sites follow
  VarDecl* decl(x.decl());
  if (o == Use && decl) update(decl, o, m);
}
/// Visit array literal
void ModeAnalysis::vArrayLit(ArrayLit& a, Occurrence o, CG::Mode m) {
  // If the array contents are Boolean, this doesn't count as an occurrence;
  // we care about the context of the access.
  // But for anything else, the partiality emerges at the occurrence site.
  int sz(a.size());
  for (int ii = 0; ii < sz; ++ii) {
    Expression* elt(a[ii]);
    if (o == Use || elt->type().bt() != Type::BT_BOOL) {
      update(elt, o, m);
    }
  }
}
/// Visit array access
void ModeAnalysis::vArrayAccess(ArrayAccess& a, Occurrence o, CG::Mode m) {
  ASTExprVec<Expression> idx(a.idx());
  for (int ii = 0; ii < idx.size(); ++ii) update(idx[ii], o, m);
  if (a.type().bt() == Type::BT_BOOL)
    update(a.v(), o, +m);
  else
    update(a.v(), o, m);
}
/// Visit array comprehension
void ModeAnalysis::vComprehension(Comprehension& comp, Occurrence o, CG::Mode m) {
  // Comprehension behaves like an array access:
  // The current mode propagates to the generators, but only to non-Boolean bodies.
  if (o == Def) {
    int g_n(comp.n_generators());
    for (int gg = 0; gg < g_n; ++gg) {
      int d_n(comp.n_decls(gg));
      for (int d = 0; d < d_n; ++d) update(comp.decl(gg, d), o, m);
      update(comp.in(gg), o, m);
      if (comp.where(gg)) {
        update(comp.where(gg), Use, BytecodeProc::FUN);
      }
    }
  }
  if (o == Use || comp.e()->type().bt() != Type::BT_BOOL) {
    int g_n(comp.n_generators());
    for (int gg = 0; gg < comp.n_generators(); ++gg) {
      if (comp.where(gg)) {
        update(comp.where(gg), Use, BytecodeProc::FUN);
      }
    }
    update(comp.e(), o, m);
  }
}
/// Visit if-then-else
void ModeAnalysis::vITE(ITE& ite, Occurrence o, CG::Mode m) {
  int sz(ite.size());
  for (int ii = 0; ii < sz; ++ii) {
    // We need the condition to compute the value.
    update(ite.e_if(ii), Use, BytecodeProc::FUN);
    update(ite.e_then(ii), o, m);
  }
  update(ite.e_else(), o, m);
}

/// Visit binary operator
void ModeAnalysis::vBinOp(BinOp& b, Occurrence o, CG::Mode m) {
  switch (b.op()) {
    case BOT_IMPL: {
      CG::Mode m_c = m.is_neg() ? m : +m;
      update(b.lhs(), o, -m_c);
      update(b.rhs(), o, m_c);
      break;
    }
    case BOT_RIMPL: {
      CG::Mode m_c = m.is_neg() ? m : +m;
      update(b.lhs(), o, m_c);
      update(b.rhs(), o, -m_c);
      break;
    }
    case BOT_OR: {
      CG::Mode m_c = m.is_neg() ? m : +m;
      update(b.lhs(), o, m_c);
      update(b.rhs(), o, m_c);
      break;
    }
    case BOT_AND: {
      CG::Mode m_c = m.is_neg() ? +m : m;
      update(b.lhs(), o, m_c);
      update(b.rhs(), o, m_c);
      break;
    }
    case BOT_EQ:
    case BOT_NQ: {
      CG::Mode m_l = b.lhs()->type().bt() == Type::BT_BOOL ? CG::Mode(BytecodeProc::FUN) : +m;
      CG::Mode m_r = b.rhs()->type().bt() == Type::BT_BOOL ? CG::Mode(BytecodeProc::FUN) : +m;
      update(b.lhs(), o, m_l);
      update(b.rhs(), o, m_r);
    }
    case BOT_EQUIV:
    case BOT_XOR:
      update(b.lhs(), o, BytecodeProc::FUN);
      update(b.rhs(), o, BytecodeProc::FUN);
      break;
    default:
      update(b.lhs(), o, m);
      update(b.rhs(), o, m);
  }
}
/// Visit unary operator
void ModeAnalysis::vUnOp(UnOp& u, Occurrence o, CG::Mode m) {
  switch (u.op()) {
    case UOT_NOT:
      update(u.e(), o, -m);
      break;
    default:
      update(u.e(), o, m);
  }
}

/// Visit call
// TODO: Add special cases for builtin calls, and interprocedural analysis
// so we can get more precise mode analysis.
void ModeAnalysis::vCall(Call& call, Occurrence o, CG::Mode m) {
  int sz(call.n_args());
  if (call.id() == constants().ids.forall) {
    assert(sz == 1);
    CG::Mode m_r(m.is_neg() ? +m : m);
    update(call.arg(0), o, m_r);
  } else if (call.id() == constants().ids.exists) {
    assert(sz == 1);
    CG::Mode m_r(m.is_neg() ? m : +m);
    update(call.arg(0), o, m_r);
  } else if (call.id() == constants().ids.clause) {
    assert(sz == 2);
    CG::Mode m_r(m.is_neg() ? m : +m);
    update(call.arg(0), o, m_r);
    update(call.arg(1), o, -m_r);
  } else if (call.id() == constants().ids.assert) {
    update(call.arg(0), Use, BytecodeProc::ROOT);
    if (sz == 3) update(call.arg(2), o, m);
  } else if (call.id() == "array1d") {
    // No-op
    update(call.arg(sz - 1), o, m);
  } else if (call.id() == "index_set" || call.id() == "length") {
    if (o == Def) update(call.arg(0), Def, m);
  } else if (call.id() == "symmetry_breaking_constraint" || call.id() == "redundant_constraint") {
    // No-op
    update(call.arg(0), o, m);
  } else {
    // Propagate to the other functions.
    for (int ii = 0; ii < sz; ++ii) {
      Expression* arg(call.arg(ii));
      // If arg is non-Boolean, this the partiality gets embedded here.
      // But if arg is Boolean, we have to assume the callee can do
      // anything with it, so the occurrence is considered FUN.
      if (arg->type().bt() == Type::BT_BOOL) {
        update(arg, Use, BytecodeProc::FUN);
      } else {
        update(arg, o, m);
      }
    }
  }
}
/// Visit let
void ModeAnalysis::vLet(Let& let, Occurrence o, CG::Mode m) {
  ASTExprVec<Expression> bindings(let.let());
  if (o == Def || let.type().bt() == Type::BT_BOOL) {
    // If the let has Boolean type, the _use_ of the let defines
    // the def-mode of the bound variables.
    for (Expression* e : bindings) {
      // Check whether this is a decl with a def.
      if (auto vd = e->dyn_cast<VarDecl>()) {
        if (vd->type().bt() != Type::BT_BOOL) update(vd, Def, m);
        /*
      if(Expression* v_e = vd->e()) {
        if(!v_e->type().isbool()) {
          update(v_e, Def, m);
        }
      }
      */
      } else {
        // Must be a constraint, so it's a use.
        assert(e->type().isbool());
        update(e, Use, m);
      }
    }
    update(let.in(), o, m);
  } else {
    // Non-Boolean use; the partiality is in an enclosing
    // context.
    update(let.in(), o, m);
  }
}
/// Visit variable declaration
void ModeAnalysis::vVarDecl(VarDecl& decl, Occurrence o, CG::Mode m) {
  if (Id* x = decl.id()) update(x, o, m);
  if (Expression* d = decl.ti()->domain()) {
    if (o == Def) {
      update(d, Def, m);
      update(d, Use, m);
    }
  }
  if (Expression* e = decl.e()) update(e, o, m);
}

void ModeAnalysis::vFunctionI(FunctionI& fun, Occurrence o, CG::Mode m) {
  // DO STUFF
}
};  // namespace MiniZinc
