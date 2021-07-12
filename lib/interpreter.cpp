/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/eval_par.hh>
#include <minizinc/interpreter.hh>
#include <minizinc/interpreter/primitives.hh>
#include <minizinc/iter.hh>
#include <minizinc/model.hh>
#include <minizinc/solver_instance_base.hh>

#include <fstream>
#include <iostream>
#include <streambuf>
#include <unordered_map>

// #define DBG_CALLTRACE(msg) std::cerr << msg
#define DBG_CALLTRACE(msg) \
  do {                     \
  } while (0)

namespace MiniZinc {

Val AggregationCtx::createVec(Interpreter* interpreter, int timestamp) const {
  return Val(Vec::a(interpreter, timestamp, stack));
}

AggregationCtx::~AggregationCtx(void) {
  /// TODO
}

const std::string AggregationCtx::symbol_to_string[] = {"AND", "OR", "VEC", "OTHER"};

const std::string Interpreter::status_to_string[] = {"Roger", "Aborted", "Inconsistent", "Error"};

void Interpreter::pushAgg(const Val& v, int stackOffset) {
  assert(stackOffset < 0);
  assert(_agg.size() + stackOffset >= 0);
  // push value onto surrounding context
  _agg[_agg.size() + stackOffset].push(this, v);
}

void Interpreter::pushConstraint(Constraint* c) { _agg.back().constraints.push_back(c); }

PropStatus Interpreter::subscribe(Constraint* c) {
  // TODO: This disables propagation after trailing, this shouldn't be necessary if we can correctly
  // communicate with the solvers
  if (trail.is_trailed(_root_var)) {
    return PS_OK;
  }
  if (c->pred() < primitiveMap().size()) {
    return primitiveMap()[c->pred()]->subscribe(*this, c);
  }
  return PS_OK;
}

void Interpreter::unsubscribe(Constraint* c) {
  if (c->pred() < primitiveMap().size()) {
    primitiveMap()[c->pred()]->unsubscribe(*this, c);
  }
}

void Interpreter::schedule(Constraint* c, const Variable::SubscriptionEvent& ev) {
  if (!c->scheduled()) {
    _propQueue.push_back(c);
    c->scheduled(true);
  }
}

void Interpreter::deschedule(Constraint* c) {
  /// TODO: is this required? seems to be unused
  if (c->scheduled()) {
    auto it = std::find(_propQueue.begin(), _propQueue.end(), c);
    if (it != _propQueue.end()) {
      _propQueue.erase(it);
    }
  }
}

void Interpreter::propagate(void) {
  while (!_propQueue.empty()) {
    Constraint* c = _propQueue.front();
    _propQueue.pop_front();
    c->scheduled(false);
    auto ps = primitiveMap()[c->pred()]->propagate(*this, c);
    switch (ps) {
      case PS_OK:
        break;
      case PS_FAILED:
        _status = INCONSISTENT;
        return;
      case PS_ENTAILED:
        // TODO: remove constraint
        break;
    }
  }
}

void Interpreter::run(void) {
  if (_status != ROGER) {
    return;
  }
  for (;;) {
    DBG_INTERPRETER(frame().pc << " ");
    switch (frame().bs->instr(frame().pc)) {
      case BytecodeStream::ADDI: {
        if (frame().pc > 130) {
          DBG_INTERPRETER("NOOOOO!\n");
        }
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ADDI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, reg(frame(), r1) + reg(frame(), r2));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::SUBI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("SUBI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, reg(frame(), r1) - reg(frame(), r2));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::MULI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("MULI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, reg(frame(), r1) * reg(frame(), r2));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::DIVI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        if (reg(frame(), r2) == 0)
          frame().pc = frame().bs->size() - 1;
        else
          assign(frame(), r3, reg(frame(), r1) / reg(frame(), r2));
        assign(frame(), r3, reg(frame(), r1) / reg(frame(), r2));
        DBG_INTERPRETER("DIVI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << " R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                                 << " " << r3 << "(" << reg(frame(), r3).toString() << ")"
                                 << "\n");
      } break;
      case BytecodeStream::MODI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        if (reg(frame(), r2) == 0)
          frame().pc = frame().bs->size() - 1;
        else
          assign(frame(), r3, reg(frame(), r1) % reg(frame(), r2));
        DBG_INTERPRETER("MODI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << " R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                                 << " " << r3 << "(" << reg(frame(), r3).toString() << ")"
                                 << "\n");
      } break;
      case BytecodeStream::INCI: {
        int r1 = frame().bs->reg(frame().pc);
        assign(frame(), r1, reg(frame(), r1) + 1);
        DBG_INTERPRETER("INCI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << "\n");
      } break;
      case BytecodeStream::DECI: {
        int r1 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("DECI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                 << "\n");
        assign(frame(), r1, reg(frame(), r1) - 1);
      } break;
      case BytecodeStream::IMMI: {
        Val i = frame().bs->intval(frame().pc);
        int r1 = frame().bs->reg(frame().pc);
        assign(frame(), r1, i);
        DBG_INTERPRETER("IMMI " << i.toString() << " R" << r1 << "(" << reg(frame(), r1).toString()
                                << ")"
                                << "\n");
      } break;
      case BytecodeStream::CLEAR: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        assert(r1 <= r2);
        for (int i = r1; i <= r2; i++) {
          assign(frame(), i, 0);
        }
        DBG_INTERPRETER("CLEAR "
                        << " R" << r1 << " " << r2 << "\n");
      } break;
      case BytecodeStream::LOAD_GLOBAL: {
        int i = frame().bs->reg(frame().pc);
        int r1 = frame().bs->reg(frame().pc);
        globals.cp(this, i, _registers, frame().reg_offset + r1);
        DBG_INTERPRETER("LOAD_GLOBAL " << i << " R" << r1 << "("
                                       << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")"
                                       << "\n");
      } break;
      case BytecodeStream::STORE_GLOBAL: {
        int r1 = frame().bs->reg(frame().pc);
        int i = frame().bs->reg(frame().pc);
        _registers.cp(this, frame().reg_offset + r1, globals, i);
        DBG_INTERPRETER("STORE_GLOBAL R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                         << ")"
                                         << " " << i << "\n");
      } break;
      case BytecodeStream::MOV: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("MOV R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        _registers.cp(this, frame().reg_offset + r1, frame().reg_offset + r2);
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::JMP: {
        int i = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("JMP " << i << "\n");
        frame().pc = i;
      } break;
      case BytecodeStream::JMPIF: {
        int r0 = frame().bs->reg(frame().pc);
        int i = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("JMPIF R" << r0 << "(" << reg(frame(), r0).toString() << ")"
                                  << " " << i << "\n");
        if (reg(frame(), r0) != 0) {
          frame().pc = i;
        }
      } break;
      case BytecodeStream::JMPIFNOT: {
        int r0 = frame().bs->reg(frame().pc);
        int i = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("JMPIFNOT R" << r0 << "(" << reg(frame(), r0).toString() << ")"
                                     << " " << i << "\n");
        if (reg(frame(), r0) == 0) {
          frame().pc = i;
        }
      } break;
      case BytecodeStream::EQI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("EQI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assert(reg(frame(), r1) == reg(frame(), r2) ||
               reg(frame(), r1).toInt() != reg(frame(), r2).toInt());
        assign(frame(), r3, reg(frame(), r1) == reg(frame(), r2));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::LTI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("LTI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, (reg(frame(), r1) < reg(frame(), r2)));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::LEI: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("LEI R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, (reg(frame(), r1) <= reg(frame(), r2)));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::AND: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("AND R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, (reg(frame(), r1) != 0 && reg(frame(), r2) != 0));
        DBG_INTERPRETER(" " << r3 << "(" << reg(frame(), r3).toString() << ")"
                            << "\n");
      } break;
      case BytecodeStream::OR: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("OR R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                               << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, (reg(frame(), r1) != 0 || reg(frame(), r2) != 0));
        DBG_INTERPRETER(" " << r3 << "(" << reg(frame(), r3).toString() << ")"
                            << "\n");
      } break;
      case BytecodeStream::NOT: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("NOT R" << r1);
        assign(frame(), r2, (reg(frame(), r1) == 0));
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::XOR: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("XOR R" << r1 << "(" << reg(frame(), r1).toString() << ")"
                                << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assign(frame(), r3, ((reg(frame(), r1) != 0) ^ (reg(frame(), r2) != 0)));
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::ISPAR: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ISPAR R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                  << ")");
        Val v = Val::follow_alias(reg(frame(), r1), this);
        if (v.isInt()) {
          assign(frame(), r2, 1);
        } else if (v.isVar()) {
          assign(frame(), r2, 0);
        } else {
          assert(v.isVec());
          Val ret = v.toVec()->isPar();
          assign(frame(), r2, ret);
        }
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::ISEMPTY: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ISEMPTY R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                    << ")");
        assert(reg(frame(), r1).isInt() || reg(frame(), r1).isVec());
        assign(frame(), r2, (reg(frame(), r1).isInt() || reg(frame(), r1).size() == 0));
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::LENGTH: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("LENGTH R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                   << ")");
        assert(reg(frame(), r1).isVec());
        if (!reg(frame(), r1).toVec()->hasIndexSet()) {
          assign(frame(), r2, reg(frame(), r1).size());
        } else {
          assign(frame(), r2, reg(frame(), r1)[0].size());
        }
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString() << ")"
                             << "\n");
      } break;
      case BytecodeStream::GET_VEC: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("GET_VEC R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                    << ")"
                                    << " R" << r2 << "(" << reg(frame(), r2).toString() << ")");
        assert(reg(frame(), r1).isVec());
        assert(reg(frame(), r2).isInt());
        assert(reg(frame(), r2) > 0 && reg(frame(), r2) <= reg(frame(), r1).size());
        Val v = Val::follow_alias(reg(frame(), r1)[reg(frame(), r2).toInt() - 1], this);
        assign(frame(), r3, v);
        DBG_INTERPRETER(" R" << r3 << "(" << v.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::GET_ARRAY: {
        Val n = frame().bs->intval(frame().pc);
        int r1 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("GET_ARRAY " << n.toString() << " R" << r1 << "("
                                     << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        std::vector<Val> idx(n.toInt());
        for (int i = 0; i < n; i++) {
          int rr = frame().bs->reg(frame().pc);
          DBG_INTERPRETER(" R" << rr << "(" << reg(frame(), rr).toString(DBG_TRIM_OUTPUT) << ")");
          idx[i] = reg(frame(), rr);
        }
        int r_res = frame().bs->reg(frame().pc);
        int r_cond = frame().bs->reg(frame().pc);
        assert(reg(frame(), r1).isVec());

        bool success = true;
        Val v = Val(-2000);

        Val content = reg(frame(), r1).toVec()->raw_data();
        if (reg(frame(), r1).toVec()->hasIndexSet()) {
          // N-Dimensional array representation
          Val index_set = reg(frame(), r1).toVec()->index_set();
          std::vector<std::pair<Val, Val>> dimensions;
          Val realdim = 1;
          for (int i = 0; i < index_set.size(); i += 2) {
            Val a = index_set[i];
            Val b = index_set[i + 1];
            dimensions.emplace_back(a, b);
            realdim *= b - a + 1;
          }

          Val realidx = 0;
          for (int i = 0; i < idx.size(); i++) {
            Val ix = idx[i];
            if (ix < dimensions[i].first || ix > dimensions[i].second) {
              success = false;
              break;
            }
            realdim /= dimensions[i].second - dimensions[i].first + 1;
            realidx += (ix - dimensions[i].first) * realdim;
          }
          assert(realidx >= 0 && realidx < content.size());

          if (success) {
            v = Val::follow_alias(content[realidx.toInt()], this);
          }
        } else {
          // Compact array representation (index set 1..n)
          success = 1 <= idx[0] && idx[0] <= reg(frame(), r1).size();
          if (success) {
            v = Val::follow_alias(content[idx[0].toInt() - 1], this);
          }
        }

        assign(frame(), r_res, v);
        assign(frame(), r_cond, Val(success));
        DBG_INTERPRETER(" R" << r_res << "(" << v.toString(DBG_TRIM_OUTPUT) << ") R" << r_cond
                             << "(" << Val(success).toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::LB: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("LB R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        Val v = Val::follow_alias(reg(frame(), r1), this);
        assign(frame(), r2, v.lb());
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::UB: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("UB R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        Val v = Val::follow_alias(reg(frame(), r1), this);
        assign(frame(), r2, v.ub());
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::DOM: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("DOM R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        Val v = Val::follow_alias(reg(frame(), r1), this);
        if (v.isInt()) {
          assign(frame(), r2, Val(Vec::a(this, newIdent(), {v, v})));
        } else if (v.isVar()) {
          Variable* var = v.toVar();
          assign(frame(), r2, Val(var->domain()));
        } else {
          throw Error("Error: dom on invalid type");
        }
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::MAKE_SET: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("MAKE_SET R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                     << ")");
        Val v1 = Val::follow_alias(reg(frame(), r1), this);
        assert(reg(frame(), r1).isVec());
        Vec* a1 = v1.toVec();

        std::vector<Val> result;
        if (a1->size() > 0) {
          std::vector<Val> vals(a1->size());
          for (int i = 0; i < a1->size(); i++) vals[i] = (*a1)[i];

          std::sort(vals.begin(), vals.end());
          Val l(vals[0]);
          Val u(vals[0]);
          for (int i = 1; i < vals.size(); ++i) {
            if (u + 1 < vals[i]) {
              result.emplace_back(l);
              result.emplace_back(u);
              l = vals[i];
            }
            u = vals[i];
          }
          result.emplace_back(l);
          result.emplace_back(u);
        }
        Val result_val(Vec::a(this, newIdent(), result));
        assign(frame(), r2, result_val);
        DBG_INTERPRETER(" R" << r2 << "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::INTERSECTION: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("INTERSECTION R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                         << ") R" << r2 << "("
                                         << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")");
        Val v1 = Val::follow_alias(reg(frame(), r1), this);
        Val v2 = Val::follow_alias(reg(frame(), r2), this);
        Val result_val;
        if (v1.isInt()) {
          result_val = v2;
        } else if (v2.isInt()) {
          result_val = v1;
        } else {
          Vec* s1 = v1.toVec();
          Vec* s2 = v2.toVec();
          VecSetRanges vsr1(s1);
          VecSetRanges vsr2(s2);
          Ranges::Inter<Val, VecSetRanges, VecSetRanges> inter(vsr1, vsr2);
          std::vector<Val> result;
          for (; inter(); ++inter) {
            result.emplace_back(inter.min());
            result.emplace_back(inter.max());
          }
          result_val = Val(Vec::a(this, newIdent(), result));
        }
        assign(frame(), r3, result_val);
        DBG_INTERPRETER(" R" << r3 << "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::UNION: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("UNION R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT)
                                  << ") R" << r2 << "("
                                  << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")");
        Val v1 = Val::follow_alias(reg(frame(), r1), this);
        Val v2 = Val::follow_alias(reg(frame(), r2), this);
        Val result_val;
        if (v1.isInt()) {
          result_val = v1;
        } else if (v2.isInt()) {
          result_val = v2;
        } else {
          Vec* s1 = v1.toVec();
          Vec* s2 = v2.toVec();
          VecSetRanges vsr1(s1);
          VecSetRanges vsr2(s2);
          Ranges::Union<Val, VecSetRanges, VecSetRanges> union_r(vsr1, vsr2);
          std::vector<Val> result;
          for (; union_r(); ++union_r) {
            result.emplace_back(union_r.min());
            result.emplace_back(union_r.max());
          }
          result_val = Val(Vec::a(this, newIdent(), result));
        }
        assign(frame(), r3, result_val);
        DBG_INTERPRETER(" R" << r3 << "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::DIFF: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("DIFF R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ") R"
                                 << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")");
        Val v1 = Val::follow_alias(reg(frame(), r1), this);
        Val v2 = Val::follow_alias(reg(frame(), r2), this);
        Val result_val;
        Vec* s1 = v1.toVec();
        Vec* s2 = v2.toVec();
        VecSetRanges vsr1(s1);
        VecSetRanges vsr2(s2);
        Ranges::Diff<Val, VecSetRanges, VecSetRanges> diff_r(vsr1, vsr2);
        std::vector<Val> result;
        for (; diff_r(); ++diff_r) {
          result.emplace_back(diff_r.min());
          result.emplace_back(diff_r.max());
        }
        result_val = Val(Vec::a(this, newIdent(), result));
        assign(frame(), r3, result_val);
        DBG_INTERPRETER(" R" << r3 << "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::INTERSECT_DOMAIN: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("INTERSECT_DOMAIN R"
                        << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ") R" << r2
                        << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")");
        Val v1 = Val::follow_alias(reg(frame(), r1), this);
        Val v2 = reg(frame(), r2);
        assert(v2.isVec());

        Val result_val;
        Vec* s2 = v2.toVec();
        if (v1.isVar()) {
          Vec* s1 = v1.toVar()->domain();
          VecSetRanges vsr1(s1);
          VecSetRanges vsr2(s2);
          Ranges::Inter<Val, VecSetRanges, VecSetRanges> inter(vsr1, vsr2);
          std::vector<Val> result;
          for (; inter(); ++inter) {
            result.emplace_back(inter.min());
            result.emplace_back(inter.max());
          }
          if (result.empty()) {
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            frame().pc = frame().bs->size() - 1;
            break;
          }
          v1.toVar()->domain(this, result, true);
          Val v1a = Val::follow_alias(v1);
          if (v1a.isVar()) {
            result_val = Val(v1a.toVar()->domain());
          } else {
            result_val = Val(Vec::a(this, newIdent(), {v1a, v1a}));
          }
        } else {
          Ranges::Const<Val> vsr1(v1, v1);
          VecSetRanges vsr2(s2);
          if (Ranges::subset(vsr1, vsr2)) {
            result_val = Val(Vec::a(this, newIdent(), {v1, v1}));
          } else {
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            frame().pc = frame().bs->size() - 1;
            break;
          }
        }
        assign(frame(), r3, result_val);
        DBG_INTERPRETER(" R" << r3 << "(" << result_val.toString(DBG_TRIM_OUTPUT) << ")"
                             << "\n");
      } break;
      case BytecodeStream::RET: {
        DBG_INTERPRETER("RET");
        if (frame().cse_frame_depth < _cse_stack.size()) {
          CSEFrame& info = _cse_stack.back();
          DBG_INTERPRETER(" %-- " << info.proc << " (" << _procs[info.proc].name << ") "
                                  << BytecodeProc::mode_to_string[info.mode]);
        }
        DBG_INTERPRETER("\n");
        assert(!_stack.empty());
      execute_ret:
        if (_stack.size() == 1) {
          // Always leave final frame on the stack
          // Copy remaining constraints into toplevel
          assert(_agg.size() == 1);
          for (Constraint* c : _agg[0].constraints) {
            root()->addDefinition(this, c);
          }
          _agg[0].constraints.clear();
          return;
        }

        DBG_CALLTRACE("<");
        for (size_t i = 0; i < _stack.size() - 1; ++i) {
          DBG_CALLTRACE("--");
        }
        DBG_CALLTRACE(" " << (_stack.back()._mode == BytecodeProc::ROOT ||
                                      _stack.back()._mode == BytecodeProc::ROOT_NEG
                                  ? "POSTED"
                                  : (_cse_stack.back().stack_size == _agg.back().size() - 1
                                         ? _agg[_agg.size() - 1].back().toString(DBG_TRIM_OUTPUT)
                                         : ""))
                          << "\n");

        while (_cse_stack.size() > frame().cse_frame_depth) {
          CSEFrame& entry = _cse_stack.back();
          int nargs = _procs[entry.proc].nargs;
          if (entry.mode == BytecodeProc::ROOT || entry.mode == BytecodeProc::ROOT_NEG) {
            Val v = Val(1);
            cse_insert(entry.proc, entry.getKey(), entry.mode, v);
          } else if (entry.stack_size == _agg.back().size() - 1) {
            Val ret = _agg[_agg.size() - 1].back();
            cse_insert(entry.proc, entry.getKey(), entry.mode, ret);
          } else {
            entry.destroy(*this);
          }
          _cse_stack.pop_back();
        }

        _registers.resize(this, frame().reg_offset);
        _stack.pop_back();
      } break;
      case BytecodeStream::CALL: {
        char mode_c = frame().bs->chr(frame().pc);
        int code = frame().bs->reg(frame().pc);
        bool cse = frame().bs->chr(frame().pc);
        assert(code >= 0);
        assert(code < _procs.size());
        assert(mode_c >= 0);
        assert(mode_c <= BytecodeProc::MAX_MODE);
        auto mode = static_cast<BytecodeProc::Mode>(mode_c);
        int n = _procs[code].nargs;
        DBG_INTERPRETER("CALL " << BytecodeProc::mode_to_string[mode] << " " << code << "("
                                << _procs[code].name << ")" << (cse ? "" : " no_cse"));
        for (size_t i = 0; i < _stack.size(); ++i) {
          DBG_CALLTRACE("--");
        }
        DBG_CALLTRACE("> " << _procs[code].name << "(");

        // Allocate new frame on the stack
        _stack.emplace_back(_procs[code].mode[mode], code, mode);
        frame().reg_offset = _registers.size();
        _registers.resize(this, frame().reg_offset + frame().bs->maxRegister() + 1);
        // Copy arguments
        for (int i = 0; i < n; i++) {
          int r = _stack[_stack.size() - 2].bs->reg(_stack[_stack.size() - 2].pc);
          assign(frame(), i, reg(_stack[_stack.size() - 2], r));
          DBG_INTERPRETER(" R" << r << "(" << reg(frame(), i).toString(DBG_TRIM_OUTPUT) << ")"
                               << ((i + 1 < n) ? "" : "\n"));
          DBG_CALLTRACE(reg(frame(), i).toString(DBG_TRIM_OUTPUT) << ((i + 1 < n) ? ", " : ")\n"));
        }

        auto immediate_return = [&] {
          _registers.resize(this, frame().reg_offset);
          _stack.pop_back();
        };

        // Lookup for Common Subexpression Elimination
        size_t cse_depth = _cse_stack.size();
        if (cse) {
          _cse_stack.emplace_back(*this, code, mode, _registers.iter_n(frame().reg_offset),
                                  _registers.iter_n(frame().reg_offset + n), n, _agg.back().size());
          // Lookup item in CSE
          auto lookup = cse_find(code, _cse_stack.back().getKey(), mode);
          if (lookup.second) {
            _cse_stack.back().destroy(*this);
            _cse_stack.pop_back();
            if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
              assert(lookup.first.isInt());
              if (lookup.first.toInt() != 1) {
                _status = INCONSISTENT;
                // Invariant: Last instruction in the frame is always an ABORT instruction
                frame().pc = frame().bs->size() - 1;
              }
            } else {
              pushAgg(lookup.first, -1);
            }
            immediate_return();
            break;
          }
        }

        if (_procs[code].mode[mode].size() == 0 || _procs[code].delay) {
          DBG_INTERPRETER((_procs[code].delay ? "--- Delayed CALL\n" : "--- FZN Builtin\n"));
          DBG_CALLTRACE("<");
          for (size_t i = 0; i < _stack.size(); ++i) {
            DBG_CALLTRACE("--");
          }
          // this is a FlatZinc builtin
          if (code == PrimitiveMap::MK_INTVAR) {
            assert(mode == BytecodeProc::ROOT);
            Variable* v = Variable::a(this, _registers[frame().reg_offset], true, newIdent());
            pushAgg(Val(v), -1);
            DBG_CALLTRACE(" " << Val(v).toString() << "\n");
          } else {
            assert(mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG);
            auto c = Constraint::a(this, code, mode, _registers.iter_n(frame().reg_offset),
                                   _registers.iter_n(frame().reg_offset + n), n);
            if (!(c.first || c.second)) {
              // Propagation failed
              _status = INCONSISTENT;
              // Invariant: Last instruction in the frame is always an ABORT instruction
              frame().pc = frame().bs->size() - 1;
              break;
            }
            if (c.first) {
              pushConstraint(c.first);
            }
            if (cse) {
              Val ret(c.second);
              cse_insert(code, _cse_stack.back().getKey(), mode, ret);
              _cse_stack.pop_back();
            }
            DBG_CALLTRACE(" POSTED\n");
            /// TODO: delayed calls
            //            if (_procs[code].delay) {
            //              delayed_calls.push_back(c);
            //            }
          }
          immediate_return();
        }
      } break;
      case BytecodeStream::BUILTIN: {
        int code = frame().bs->reg(frame().pc);
        assert(code >= 0);
        DBG_INTERPRETER("BUILTIN " << code << "(" << _procs[code].name << ")");
        assert(code < primitiveMap().size());
        // this is a Interpreter builtin
        int n = _procs[code].nargs;
        std::vector<Val> args(n);
        for (int i = 0; i < n; i++) {
          int r = frame().bs->reg(frame().pc);
          // Do not increase the reference count. Vector is destroyed after
          // BUILTIN execution without cleanup
          args[i] = reg(frame(), r);
          DBG_INTERPRETER(" R" << r << "(" << args[i].toString(DBG_TRIM_OUTPUT) << ")");
        }
        DBG_INTERPRETER("\n");
        primitiveMap()[code]->execute(*this, args);
      } break;
      case BytecodeStream::TCALL: {
        char mode_c = frame().bs->chr(frame().pc);
        int code = frame().bs->reg(frame().pc);
        bool cse = frame().bs->chr(frame().pc);
        assert(code >= 0);
        assert(code < _procs.size());
        assert(mode_c >= 0);
        assert(mode_c <= BytecodeProc::MAX_MODE);
        auto mode = static_cast<BytecodeProc::Mode>(mode_c);
        DBG_INTERPRETER("TCALL " << BytecodeProc::mode_to_string[mode] << " " << code << "("
                                 << _procs[code].name << ")" << (cse ? "" : " no_cse") << "\n");
        for (size_t i = 0; i < _stack.size() - 1; ++i) {
          DBG_CALLTRACE("==");
        }
        DBG_CALLTRACE("> " << _procs[code].name << "(");
        int nargs = _procs[code].nargs;
        for (int i = 0; i < nargs; ++i) {
          DBG_CALLTRACE(reg(frame(), i).toString(DBG_TRIM_OUTPUT)
                        << ((i + 1 < nargs) ? ", " : ")\n"));
        }

        if (cse) {
          _cse_stack.emplace_back(*this, code, mode, _registers.iter_n(frame().reg_offset),
                                  _registers.iter_n(frame().reg_offset + nargs), nargs,
                                  _agg.back().size());
          bool found;
          Val ret;
          std::tie(ret, found) = cse_find(code, _cse_stack.back().getKey(), mode);
          if (found) {
            _cse_stack.back().destroy(*this);
            _cse_stack.pop_back();
            // RET with CSE found value
            if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
              assert(ret.isInt());
              if (ret.toInt() != 1) {
                _status = INCONSISTENT;
                // Invariant: Last instruction in the frame is always an ABORT instruction
                frame().pc = frame().bs->size() - 1;
              }
            } else {
              pushAgg(ret, -1);
            }
            // TODO: Does this actually "return" correctly?? It seems the code would continue;
            while (_cse_stack.size() > frame().cse_frame_depth) {
              CSEFrame& entry = _cse_stack.back();
              int nargs = _procs[entry.proc].nargs;
              cse_insert(entry.proc, entry.getKey(), entry.mode, ret);
              _cse_stack.pop_back();
            }
            _registers.resize(this, frame().reg_offset);
            _stack.pop_back();
            break;
          }
        }
        if (_procs[code].mode[mode].size() == 0 || _procs[code].delay) {
          DBG_INTERPRETER((_procs[code].delay ? "--- Delayed CALL\n" : "--- FZN Builtin\n"));
          // this is a FlatZinc builtin
          assert(mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG);
          auto c = Constraint::a(this, code, mode, _registers.iter_n(frame().reg_offset),
                                 _registers.iter_n(frame().reg_offset + nargs), nargs);
          if (!(c.first || c.second)) {
            // Propagation failed
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            frame().pc = frame().bs->size() - 1;
            break;
          }
          if (c.first) {
            pushConstraint(c.first);
          }
          if (cse) {
            Val ret(c.second);
            // TODO: Does this actually "return" correctly?? It seems this would not set all
            // necessary values in the _cse_stack
            cse_insert(code, _cse_stack.back().getKey(), mode, ret);
            _cse_stack.pop_back();
          }
          /// TODO: delayed calls
          //            if (_procs[code].delay) {
          //              delayed_calls.push_back(def);
          //            }
          goto execute_ret;
        } else {
          // Replace frame with new procedure
          frame()._pred = code;
          frame()._mode = mode;
          frame().bs = &_procs[code].mode[mode];
          _registers.resize(this, frame().reg_offset + frame().bs->maxRegister() + 1);
          frame().pc = 0;
        }
      } break;
      case BytecodeStream::ITER_ARRAY: {
        int r1 = frame().bs->reg(frame().pc);
        int l = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ITER_ARRAY " << r1 << " " << l << "\n");
        assert(reg(frame(), r1).isVec());
        if (reg(frame(), r1).toVec()->hasIndexSet()) {
          assert(reg(frame(), r1).size() == 2);
          assert(reg(frame(), r1)[0].isVec());

          Vec* v(reg(frame(), r1)[0].toVec());
          _loops.push_back(LoopState(v, l));
        } else {
          // Compact array representation (index set 1..n)
          Vec* v(reg(frame(), r1).toVec());
          _loops.push_back(LoopState(v, l));
        }
      } break;
      case BytecodeStream::ITER_VEC: {
        int r1 = frame().bs->reg(frame().pc);
        int l = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ITER_VEC " << r1 << " " << l << "\n");
        assert(reg(frame(), r1).isVec());
        Vec* v(reg(frame(), r1).toVec());
        _loops.push_back(LoopState(v, l));
      } break;
      case BytecodeStream::ITER_RANGE: {
        int r1 = frame().bs->reg(frame().pc);
        int r2 = frame().bs->reg(frame().pc);
        int lbl = frame().bs->reg(frame().pc);

        Val lb = Val::follow_alias(reg(frame(), r1), this);
        Val ub = Val::follow_alias(reg(frame(), r2), this);
        assert(lb.isInt());
        assert(ub.isInt());
        DBG_INTERPRETER("ITER_RANGE " << r1 << " " << r2 << " " << lbl << "\n");
        _loops.push_back(LoopState(lb.toInt(), ub.toInt(), lbl));
      } break;
      case BytecodeStream::ITER_NEXT: {
        int r1 = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("ITER_NEXT");
        assert(_loops.size() > 0);
        LoopState& outer(_loops.back());
        if (outer.pos < outer.end) {
          Val v;
          if (outer.is_range) {
            v = outer.pos;
            outer.pos++;
          } else {
            Val* ptr(reinterpret_cast<Val*>(outer.pos));
            v = Val::follow_alias(*ptr);
            outer.pos += sizeof(Val*);
          }
          assign(frame(), r1, v);
          DBG_INTERPRETER(" R" << r1 << "(" << v.toString(DBG_TRIM_OUTPUT) << ")"
                               << "\n");
        } else {
          frame().pc = outer.exit_pc;
          _loops.pop_back();
          DBG_INTERPRETER(" (EXIT " << outer.exit_pc << ")\n");
        }
      } break;
      case BytecodeStream::ITER_BREAK: {
        int num = frame().bs->intval(frame().pc).toInt();
        DBG_INTERPRETER("ITER_BREAK " << num << "\n");
        assert(_loops.size() >= num);
        auto it(_loops.end() - num);
        frame().pc = it->exit_pc;
        _loops.erase(it, _loops.end());
      } break;
      case BytecodeStream::TRACE: {
        int r = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("TRACE " << r << "\n");
        std::cerr << reg(frame(), r).toString() << "\n";
        //            std::cerr << frame().reg.e(r)->cast<StringLit>();
        //            std::cerr << frame()().reg.e(r) << "\n";
      } break;
      case BytecodeStream::ABORT: {
        DBG_INTERPRETER("ABORT\n");

        _registers.resize(this, 0);
        _stack.clear();
        // TODO: Should the Aggregation stack be emptied?

        if (_status == ROGER) {
          _status = ABORTED;
        }
        return;
      }
      case BytecodeStream::PUSH: {
        int r = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("PUSH R" << r << " (" << reg(frame(), r).toString(DBG_TRIM_OUTPUT)
                                 << ")\n");
        assert(!_agg.empty());
        _agg.back().push(this, reg(frame(), r));
      } break;
      case BytecodeStream::POP: {
        int r = frame().bs->reg(frame().pc);
        assert(!_agg.empty());
        assert(!_agg.back().empty());
        assign(frame(), r, _agg.back().back());
        DBG_INTERPRETER("POP R" << r << " (" << reg(frame(), r).toString(DBG_TRIM_OUTPUT) << ")\n");
        _agg.back().pop(this);
      } break;
      case BytecodeStream::POST: {
        int r = frame().bs->reg(frame().pc);
        DBG_INTERPRETER("POST R" << r << " (" << reg(frame(), r).toString(DBG_TRIM_OUTPUT)
                                 << ")\n");
        Val v1 = Val::follow_alias(reg(frame(), r), this);
        if (v1.isInt()) {
          if (v1 == 0) {
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            frame().pc = frame().bs->size() - 1;
          }
        } else {
          bool success = v1.toVar()->setVal(this, 1);
          if (!success) {
            _status = INCONSISTENT;
            // Invariant: Last instruction in the frame is always an ABORT instruction
            frame().pc = frame().bs->size() - 1;
          }
          /// TODO: check why this is adding a ref
          /// Shouldn't be necessary
          //            v1.toVar()->addRef(this);
        }
      } break;
      case BytecodeStream::OPEN_AGGREGATION: {
        int r = frame().bs->chr(frame().pc);
        DBG_INTERPRETER("OPEN_AGGREGATION " << AggregationCtx::symbol_to_string[r] << ", depth "
                                            << _agg.size() + 1 << "\n");
        assert(r >= 0 && r <= AggregationCtx::VCTX_OTHER);
        if (r == AggregationCtx::VCTX_OTHER || r == AggregationCtx::VCTX_VEC || _agg.empty() ||
            _agg.back().symbol != r) {
          // Push a new aggregation context
          _agg.emplace_back(this, r);
        } else {
          // Increment depth counter for current aggregation context
          _agg.back().n_symbols++;
        }
      } break;
      case BytecodeStream::SIMPLIFY_LIN: {
        DBG_INTERPRETER("SIMPLIFY_LIN");

        int r0 = frame().bs->reg(frame().pc);
        int r1 = frame().bs->reg(frame().pc);
        Val i = frame().bs->intval(frame().pc);
        DBG_INTERPRETER(" R" << r0 << "(" << reg(frame(), r0).toString(DBG_TRIM_OUTPUT) << ")");
        DBG_INTERPRETER(" R" << r1 << "(" << reg(frame(), r1).toString(DBG_TRIM_OUTPUT) << ")");
        DBG_INTERPRETER(" " << i.toString());
        int r2 = frame().bs->reg(frame().pc);
        int r3 = frame().bs->reg(frame().pc);
        int r4 = frame().bs->reg(frame().pc);

        std::vector<Val> coeffs({1, -1});
        std::vector<Val> vars({reg(frame(), r0), reg(frame(), r1)});
        Val d = i;

        simplify_linexp(coeffs, vars, d);

        Val coeffs_v = Val(Vec::a(this, newIdent(), coeffs));
        Val vars_v = Val(Vec::a(this, newIdent(), vars));
        assign(frame(), r2, coeffs_v);
        assign(frame(), r3, vars_v);
        assign(frame(), r4, -d);
        DBG_INTERPRETER(" R" << r2 << "(" << reg(frame(), r2).toString(DBG_TRIM_OUTPUT) << ")");
        DBG_INTERPRETER(" R" << r3 << "(" << reg(frame(), r3).toString(DBG_TRIM_OUTPUT) << ")\n");
        DBG_INTERPRETER(" R" << r4 << "(" << reg(frame(), r4).toString(DBG_TRIM_OUTPUT) << ")\n");
      } break;
      case BytecodeStream::CLOSE_AGGREGATION: {
        DBG_INTERPRETER("CLOSE_AGGREGATION ("
                        << AggregationCtx::symbol_to_string[_agg.back().symbol] << ", depth "
                        << _agg.size() << ")\n");
        assert(!_agg.empty());
        // Decrement depth counter for current aggregation context
        _agg.back().n_symbols--;
        if (_agg.back().n_symbols == 0) {
          // Result produced by this aggregation
          Variable* result = nullptr;

          assert(_agg.size() >= 2);
          switch (_agg.back().symbol) {
            case AggregationCtx::VCTX_AND: {
              // Create a conjunction on the definition stack
              std::vector<Val> args;
              args.reserve(_agg.back().size());
              bool isFalse = false;
              for (int i = 0; i < _agg.back().size(); i++) {
                const Val& v = _agg.back()[i];
                if (v.isInt()) {
                  if (v == 0) {
                    // Disjunction is constant false
                    isFalse = true;
                    break;
                  }
                } else {
                  args.push_back(v);
                }
              }
              if (isFalse || args.empty()) {
                /// TODO: check, what if _agg.size()==2 as below?
                // Conjunction is constant true or false
                pushAgg(!isFalse, -2);
              } else if (_agg.size() == 2) {
                // Push into root context
                for (Val v : args) {
                  auto success = v.toVar()->setVal(this, 1);
                  // FIXME: Deal with unsuccessful setVal
                  assert(success);
                }
                pushAgg(1, -2);

                // Why do we need a definition? If this is in ROOT, then all arguments should be
                // true
                /* Definition* d =
                 * Definition::a(this,boolean_domain(),false,PrimitiveMap::FORALL,BytecodeProc::ROOT,
                 */
                /*                               {Val(Vec::a(this,newIdent(),args))},-1); */
                /* if (defs) { */
                /*   d->appendBefore(this, defs); */
                /* } else { */
                /*   defs = d; */
                /* } */
              } else if (args.size() == 1) {
                pushAgg(args[0], -2);
              } else {
                Vec* arr = Vec::a(this, newIdent(), args);
                result = Variable::a(this, boolean_domain(), false, newIdent());
                auto def_c = Constraint::a(this, PrimitiveMap::FORALL, BytecodeProc::ROOT,
                                           {Val(arr), Val(result)});
                assert(def_c.first);
                result->addRef(this);
                result->addDefinition(this, def_c.first);
                pushAgg(Val(result), -2);
                RefCountedObject::rmRef(this, result);
              }
            } break;
            case AggregationCtx::VCTX_OR: {
              // Create a clause on the definition stack, and push a reference
              // to it onto the aggregation stack
              std::vector<Val> pos;
              std::vector<Val> neg;
              bool isTrue = false;
              for (int i = 0; i < _agg.back().size(); i++) {
                const Val& v = Val::follow_alias(_agg.back()[i]);
                if (v.isInt()) {
                  if (v != 0) {
                    // Disjunction is constant true
                    isTrue = true;
                    break;
                  }
                } else {
                  Variable* cur = v.toVar();
                  if (Constraint* defby = cur->defined_by()) {
                    if (defby->pred() == PrimitiveMap::BOOLNOT) {
                      if (Val::follow_alias(defby->arg(0)) == v) {
                        neg.push_back(Val::follow_alias(defby->arg(1)));
                      } else {
                        neg.push_back(Val::follow_alias(defby->arg(0)));
                      }
                      continue;
                    }
                  }
                  pos.push_back(v);
                }
              }
              if (isTrue || (pos.empty() && neg.empty())) {
                // Disjunction is constant true or false
                pushAgg(isTrue, -2);
              } else if (pos.size() == 1 && neg.empty()) {
                if (_agg.size() == 2) {
                  auto success = pos[0].toVar()->setVal(this, 1);
                  // FIXME: Deal with unsuccessful setVal
                  assert(success);
                  pushAgg(1, -2);
                } else {
                  pushAgg(pos[0], -2);
                }
              } else {
                // Push into root context
                Vec* vpos = Vec::a(this, newIdent(), pos);
                Vec* vneg = Vec::a(this, newIdent(), neg);
                if (_agg.size() == 2) {
                  auto c = Constraint::a(this, PrimitiveMap::CLAUSE, BytecodeProc::ROOT,
                                         {Val(vpos), Val(vneg)}, 0, 1, true);
                  assert(c.first);
                  root()->addDefinition(this, c.first);
                  pushAgg(1, -2);
                } else {
                  result = Variable::a(this, boolean_domain(), false, newIdent());
                  auto def_c = Constraint::a(this, PrimitiveMap::CLAUSE_REIF, BytecodeProc::ROOT,
                                             {Val(vpos), Val(vneg), Val(result)}, 0, 1, true);
                  assert(def_c.first);
                  result->addRef(this);
                  result->addDefinition(this, def_c.first);
                  pushAgg(Val(result), -2);
                  RefCountedObject::rmRef(this, result);
                }
              }
            } break;
            case AggregationCtx::VCTX_VEC: {
              // Create a vector on the aggregation stack
              _agg[_agg.size() - 2].push(this, _agg.back().createVec(this, newIdent()));
            } break;
            case AggregationCtx::VCTX_OTHER:
              // When closing a VCTX_OTHER context, it should contain at most one value
              assert(_agg.back().size() <= 1);
              if (_agg.back().size() == 1) {
                if (_agg.back()[0].isVar()) {
                  result = _agg.back()[0].toVar();
                }
                // push value onto surrounding context
                pushAgg(_agg.back()[0], -2);
              }
              break;
          }

          _agg.back().destroyStack(this);
          if (result && !_agg.back().constraints.empty()) {
            // Aggregation produced constraints and exactly one return value, which is a variable
            if (result->timestamp() >= _agg.back().def_ident_start) {
              // the definition was produced by the current frame, so
              // attach all constraints to it
              for (Constraint* c : _agg.back().constraints) {
                result->addDefinition(this, c);
              }
              _agg.back().constraints.clear();
              // INVARIANT: The result of aggregation is not referenced by any of the registers.
              assert(std::none_of(_registers.cbegin() + frame().reg_offset, _registers.cend(),
                                  [result](Val v) { return v.contains(Val(result)); }));
              result->binding(this, false);
            }
          }
          if (!_agg.back().constraints.empty()) {
            if (_agg.size() == 2) {
              // only one frame left, so move all constraints into root context
              for (Constraint* c : _agg.back().constraints) {
                root()->addDefinition(this, c);
              }
            } else {
              // Move definitions to parent aggregation
              for (Constraint* c : _agg.back().constraints) {
                _agg[_agg.size() - 2].constraints.push_back(c);
              }
            }
            _agg.back().constraints.clear();
          }
          _agg.pop_back();
        }
      } break;
    }
    assert(!frame().bs->eos(frame().pc));
  }
}

void Interpreter::dumpState(std::ostream& os) {
  Variable::dump(root(), _procs, os);
  //    if (!_agg.empty()) {
  //      Definition::dump(_agg.back().def_stack, _procs, os, true);
  //    }
}
void Interpreter::dumpState(const BytecodeFrame& bf, std::ostream& os) {
  for (unsigned int i = bf.reg_offset; i < bf.reg_offset + bf.bs->maxRegister(); i++) {
    os << "  R" << i << " = " << _registers[i].toString() << "\n";
  }
}

void Interpreter::dumpState() { dumpState(std::cerr); }

Interpreter::~Interpreter(void) {
  globals.destroy(this);
  _registers.resize(this, 0);
  _stack.clear();
  for (auto& a : _agg) {
    a.destroyStack(this);
    //      a.destroyDef(this); /// TODO: replace with what? Just delete all constraints?
  }
  assert(cse.size() == _procs.size());
  for (int i = 0; i < _procs.size(); ++i) {
    switch (_procs[i].nargs) {
      case 1: {
        auto table = static_cast<CSETable<FixedKey<1>>*>(cse[i]);
        table->destroy(this);
        break;
      }
      case 2: {
        auto table = static_cast<CSETable<FixedKey<2>>*>(cse[i]);
        table->destroy(this);
        break;
      }
      case 3: {
        auto table = static_cast<CSETable<FixedKey<3>>*>(cse[i]);
        table->destroy(this);
        break;
      }
      case 4: {
        auto table = static_cast<CSETable<FixedKey<4>>*>(cse[i]);
        table->destroy(this);
        break;
      }
      default: {
        auto table = static_cast<CSETable<VariadicKey>*>(cse[i]);
        table->destroy(this);
        break;
      }
    }
  }
  RefCountedObject::rmRef(this, infinite_dom);
  RefCountedObject::rmRef(this, boolean_dom);
  RefCountedObject::rmRef(this, true_dom);
}

void Interpreter::call(int code, std::vector<Val>&& args) {
  if (_status != ROGER) {
    return;
  }
  assert(code >= 0);
  assert(code < _procs.size());
  int n = _procs[code].nargs;
  const bool cse = true;
  BytecodeProc::Mode mode = BytecodeProc::ROOT;
  DBG_INTERPRETER("Interpreter::call " << BytecodeProc::mode_to_string[mode] << " " << code << "("
                                       << _procs[code].name << ")" << (cse ? "" : " no_cse"));
  DBG_CALLTRACE("\n> " << _procs[code].name << "(");
  assert(n == args.size());
  for (int i = 0; i < n; i++) {
    DBG_INTERPRETER(" R" << i << "(" << args[i].toString(DBG_TRIM_OUTPUT) << ")");
    DBG_CALLTRACE(args[i].toString(DBG_TRIM_OUTPUT) << ((i + 1 < n) ? ", " : ")\n"));
  }
  DBG_INTERPRETER("\n");

  // Lookup for Common Subexpression Elimination
  size_t cse_depth = _cse_stack.size();
  if (cse) {
    _cse_stack.emplace_back(*this, code, mode, args.cbegin(), args.cend(), n, _agg.back().size());
    // Lookup item in CSE
    auto lookup = cse_find(code, _cse_stack.back().getKey(), mode);
    if (lookup.second) {
      _cse_stack.back().destroy(*this);
      _cse_stack.pop_back();
      if (mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG) {
        assert(lookup.first.isInt());
        if (lookup.first.toInt() != 1) {
          _status = INCONSISTENT;
          // Invariant: Last instruction in the frame is always an ABORT instruction
          return;
        }
      } else {
        // pushAgg(lookup.first, -1);
        assert(false);  // Delayed calls can only be ROOT mode
      }
      return;
    }
  }

  if (_procs[code].mode[mode].size() == 0) {
    DBG_INTERPRETER("--- FZN Builtin\n");
    // this is a FlatZinc builtin
    assert(code != PrimitiveMap::MK_INTVAR);
    assert(mode == BytecodeProc::ROOT || mode == BytecodeProc::ROOT_NEG);
    auto c = Constraint::a(this, code, mode, args);
    if (!(c.first || c.second)) {
      // Propagation failed
      _status = INCONSISTENT;
      return;
    }
    if (c.first) {
      pushConstraint(c.first);
    }
    if (cse) {
      Val ret(c.second);
      cse_insert(code, _cse_stack.back().getKey(), mode, ret);
      _cse_stack.pop_back();
    }
    return;
  }

  // Make the next instruction RET
  _stack.back().pc--;
  _stack.emplace_back(_procs[code].mode[mode], code, mode);
  BytecodeFrame& newFrame = _stack.back();
  newFrame.reg_offset = _registers.size();
  _registers.resize(this, newFrame.reg_offset + newFrame.bs->maxRegister() + 1);
  for (int i = 0; i < args.size(); i++) {
    assign(newFrame, i, args[i]);
  }
  newFrame.cse_frame_depth = cse_depth;

  return run();
}

bool Interpreter::runDelayed() {
  std::unordered_map<Constraint*, Variable*> wave = std::move(delayed_constraints);
  delayed_constraints.clear();
  for (auto pair : wave) {
    // Only run delayed constraints that have a definition.
    Constraint* c = pair.first;
    assert(c->delayed());
    if (_procs[c->pred()].mode[BytecodeProc::ROOT].size() == 0) {
      continue;
    }

    // Remove placeholder constraint from variable
    Val alias = Val::follow_alias(Val(pair.second));
    Variable* v = alias.isVar() ? alias.toVar() : root();
    // TODO: This might be quite expensive. Should we store _definitions differently?
    auto pos = std::find(std::begin(v->_definitions), std::end(v->_definitions), c);
    assert(pos != v->_definitions.end());
    v->_definitions.erase(pos);

    // Copy arguments into vector
    std::vector<Val> args(c->size());
    for (int i = 0; i < c->size(); ++i) {
      args[i] = c->arg(i);
    }

    // Execute constraint
    std::swap(v, _root_var);
    call(c->pred(), std::move(args));
    std::swap(v, _root_var);

    // Stop if execution caused an error
    if (_status != ROGER) {
      break;
    }
  }
  return !delayed_constraints.empty();
}

size_t Trail::save_state(MiniZinc::Interpreter* interpreter) {
  trail_size.emplace_back(var_list_trail.size(), obj_trail.size(), alias_trail.size(),
                          domain_trail.size(), def_trail.size());
  timestamp_trail.push_back(interpreter->_identCount);
  for (int i = 0; i < interpreter->_procs.size(); ++i) {
    switch (interpreter->_procs[i].nargs) {
      case 1: {
        auto table = static_cast<CSETable<FixedKey<1>>*>(interpreter->cse[i]);
        table->push(interpreter, !last_operation_pop);
        break;
      }
      case 2: {
        auto table = static_cast<CSETable<FixedKey<2>>*>(interpreter->cse[i]);
        table->push(interpreter, !last_operation_pop);
        break;
      }
      case 3: {
        auto table = static_cast<CSETable<FixedKey<3>>*>(interpreter->cse[i]);
        table->push(interpreter, !last_operation_pop);
        break;
      }
      case 4: {
        auto table = static_cast<CSETable<FixedKey<4>>*>(interpreter->cse[i]);
        table->push(interpreter, !last_operation_pop);
        break;
      }
      default: {
        auto table = static_cast<CSETable<VariadicKey>*>(interpreter->cse[i]);
        table->push(interpreter, !last_operation_pop);
        break;
      }
    }
  }
  last_operation_pop = false;
  return len();
}

void Trail::untrail(MiniZinc::Interpreter* interpreter) {
  assert(len() > 0);
  assert(interpreter->_stack.size() == 1);
  size_t vlt_size, ot_size, at_size, dt_size, deft_size;
  std::tie(vlt_size, ot_size, at_size, dt_size, deft_size) = trail_size.back();
  trail_size.pop_back();
  int timestamp = timestamp_trail.back();
  timestamp_trail.pop_back();
  // Reconstruct destroyed items
  while (obj_trail.size() > ot_size) {
    auto obj = obj_trail.back();
    switch (obj->rcoType()) {
      case RefCountedObject::VAR:
        static_cast<Variable*>(obj)->reconstruct(interpreter);
        break;
      case RefCountedObject::VEC:
        static_cast<Vec*>(obj)->reconstruct(interpreter);
        break;
      default:
        assert(false);
    }
    obj_trail.pop_back();
  }
  // Restore hedge pointers back to their previous versions
  while (var_list_trail.size() > vlt_size) {
    auto entry = var_list_trail.back();
    *entry.first = entry.second;
    var_list_trail.pop_back();
  }
  // Restore original definitions for created aliases
  while (alias_trail.size() > at_size) {
    Variable* var;
    Val dom;
    std::tie(var, dom) = alias_trail.back();
    var->unalias(interpreter, dom);
    dom.rmMemRef(interpreter);
    alias_trail.pop_back();
  }
  // Restore original domains
  while (domain_trail.size() > dt_size) {
    Variable* var;
    Vec* dom;
    std::tie(var, dom) = domain_trail.back();
    Val nd(dom);
    nd.addRef(interpreter);
    var->_domain.rmRef(interpreter);
    var->_domain = nd;
    RefCountedObject::rmMemRef(interpreter, dom);
    domain_trail.pop_back();
  }
  // Remove all additions/changes to the CSE table
  for (int i = 0; i < interpreter->_procs.size(); ++i) {
    switch (interpreter->_procs[i].nargs) {
      case 1: {
        auto table = static_cast<CSETable<FixedKey<1>>*>(interpreter->cse[i]);
        table->pop(interpreter);
        break;
      }
      case 2: {
        auto table = static_cast<CSETable<FixedKey<2>>*>(interpreter->cse[i]);
        table->pop(interpreter);
        break;
      }
      case 3: {
        auto table = static_cast<CSETable<FixedKey<3>>*>(interpreter->cse[i]);
        table->pop(interpreter);
        break;
      }
      case 4: {
        auto table = static_cast<CSETable<FixedKey<4>>*>(interpreter->cse[i]);
        table->pop(interpreter);
        break;
      }
      default: {
        auto table = static_cast<CSETable<VariadicKey>*>(interpreter->cse[i]);
        table->pop(interpreter);
        break;
      }
    }
  }
  // Undo all changes to definitions
  while (def_trail.size() > deft_size) {
    Variable* var;
    Constraint* con;
    bool remove;
    std::tie(var, con, remove) = def_trail.back();
    if (remove) {
      assert(var->definitions().back() == con);
      var->_definitions.pop_back();
    } else {
      assert(false);
    }
    def_trail.pop_back();
  }
  // Remove all newly created variables
  Variable* back = interpreter->root()->prev();
  while (back->timestamp() > timestamp) {
    Variable* rem = back;
    back = back->prev();
    rem->destroy(interpreter);
    Variable::free(rem);
  }
  // TODO: Should we remove newly created propagators??
  // Reset the timestamp count to its previous value
  interpreter->_identCount = timestamp;
  last_operation_pop = true;
}

void Interpreter::optimize(void) {}

}  // namespace MiniZinc
