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

#include <minizinc/ast.hh>
#include <minizinc/interpreter/constraint.hh>
#include <minizinc/interpreter/cse.hh>
#include <minizinc/interpreter/values.hh>
#include <minizinc/support/dtrace.h>

#include <deque>
#include <iostream>
#include <map>
#include <unordered_set>
#include <vector>

// #define DBG_INTERPRETER(msg) std::cerr << msg
#define DBG_INTERPRETER(msg) \
  do {                       \
  } while (0)
#define DBG_TRIM_OUTPUT true

namespace MiniZinc {

class Interpreter;

class RegisterFile {
protected:
  std::vector<Val> _r;
  size_t _size;
  // Note: It seems that the standard library sometimes tries to be smart when
  // using resize(n) when n < capacity. We therefore use _r.size() as capacity
  // and _size as the actual size.

  // FIXME: The RegisterFile is said to always have a given size, but it is
  // currently unknown how many globals are used. This can trigger an assertion.
public:
  RegisterFile(int n = 0) : _r(n), _size(n) {}
  const Val& operator[](int r) {
    assert(r < _r.size());
    return _r[r];
  }
  size_t size() { return _size; }
  void resize(Interpreter* interpreter, size_t n) {
    if (n >= _r.size()) {
      _r.resize(_r.size() * 2);
    }
    if (n < _size) {
      for (int i = n; i < _size; ++i) {
        _r[i].rmRef(interpreter);
        _r[i] = 0;
      }
    }
    _size = n;
  }
  std::vector<Val>::const_iterator cbegin() { return _r.cbegin(); }
  std::vector<Val>::const_iterator cend() { return _r.cend(); }
  std::vector<Val>::const_iterator iter_n(size_t n) { return this->cbegin() + n; }
  void assign(Interpreter* interpreter, int r, const Val& v) {
    assert(r < size());
    _r[r].assign(interpreter, v);
  }

  void cp(Interpreter* interpreter, int r1, int r2) {
    assert(r1 < size());
    _r[r2].assign(interpreter, _r[r1]);
  }
  void cp(Interpreter* interpreter, int r1, RegisterFile& rf, int r2) {
    assert(r1 < size());
    rf._r[r2].assign(interpreter, _r[r1]);
  }
  /// Destroy this register file
  void destroy(Interpreter* interpreter) {
    for (auto& v : _r) {
      v.rmRef(interpreter);
    }
  }
  void clear(Interpreter* interpreter) {
    destroy(interpreter);
    _r.clear();
  }
  void dump(std::ostream& os) {
    for (unsigned int i = 0; i < size(); i++) {
      os << "  R" << i << " = " << _r[i].toString() << "\n";
    }
  }
};

class AggregationCtx {
protected:
  /// Stack of values that need to be aggregated
  std::vector<Val> stack;

public:
  /// Earliest time stamp for definitions in the current aggregation
  int def_ident_start;
  /// Constraints attached to the computed value
  std::vector<Constraint*> constraints;
  /// Type of function represented by this context
  enum Symbol { VCTX_AND, VCTX_OR, VCTX_VEC, VCTX_OTHER, MAX_SYMBOL = VCTX_OTHER } symbol;
  static const std::string symbol_to_string[MAX_SYMBOL + 1];
  /// Nesting depth for this symbol (how many of these are open)
  int n_symbols;
  /// Constructor
  AggregationCtx(Interpreter* interpreter, int s);
  /// Destructor
  ~AggregationCtx(void);
  /// Push value onto aggregation stack
  void push(Interpreter* interpreter, const Val& v) {
    assert(stack.empty() || symbol != VCTX_OTHER);
    stack.push_back(v);
    stack.back().addRef(interpreter);
  }
  void pop(Interpreter* interpreter) {
    stack.back().rmRef(interpreter);
    stack.pop_back();
  }
  const Val& back(void) const { return stack.back(); }
  const Val& operator[](int i) const { return stack[i]; }
  int size(void) const { return stack.size(); }
  bool empty(void) const { return stack.empty(); }
  Val createVec(Interpreter* interpreter, int timestamp) const;
  /// Destroy stack values
  void destroyStack(Interpreter* interpreter) {
    for (auto& v : stack) {
      v.rmRef(interpreter);
    }
  }
};

// Frame information for Common Subexpression Elimination
class CSEFrame {
public:
  int proc;
  BytecodeProc::Mode mode;
  int nargs;
  union KeyUnion {
    VariadicKey vk;
    FixedKey<1> f1;
    FixedKey<2> f2;
    FixedKey<3> f3;
    FixedKey<4> f4;

    KeyUnion() { new (&vk) VariadicKey(); }
    ~KeyUnion() {}
  } key;
  size_t stack_size;

  CSEFrame(Interpreter& interpreter, int _proc, BytecodeProc::Mode _mode, arg_iter arg_start,
           arg_iter arg_end, size_t _nargs, size_t _stack_size)
      : proc(_proc), mode(_mode), stack_size(_stack_size), nargs(_nargs) {
    switch (nargs) {
      case 1: {
        key.f1 = FixedKey<1>(interpreter, arg_start, arg_end);
        break;
      }
      case 2: {
        key.f2 = FixedKey<2>(interpreter, arg_start, arg_end);
        break;
      }
      case 3: {
        key.f3 = FixedKey<3>(interpreter, arg_start, arg_end);
        break;
      }
      case 4: {
        key.f4 = FixedKey<4>(interpreter, arg_start, arg_end);
        break;
      }
      default: {
        key.vk = VariadicKey(interpreter, arg_start, arg_end, nargs);
        break;
      }
    }
  }

  CSEFrame(const CSEFrame& other)
      : proc(other.proc), mode(other.mode), stack_size(other.stack_size), nargs(other.nargs) {
    switch (nargs) {
      case 1: {
        key.f1 = other.key.f1;
        break;
      }
      case 2: {
        key.f2 = other.key.f2;
        break;
      }
      case 3: {
        key.f3 = other.key.f3;
        break;
      }
      case 4: {
        key.f4 = other.key.f4;
        break;
      }
      default: {
        key.vk = other.key.vk;
        break;
      }
    }
  };

  inline void destroy(Interpreter& interpreter) {
    switch (nargs) {
      case 1: {
        key.f1.destroy(interpreter);
        break;
      }
      case 2: {
        key.f2.destroy(interpreter);
        break;
      }
      case 3: {
        key.f3.destroy(interpreter);
        break;
      }
      case 4: {
        key.f4.destroy(interpreter);
        break;
      }
      default: {
        key.vk.destroy(interpreter);
        break;
      }
    }
  }

  inline CSEKey& getKey() {
    switch (nargs) {
      case 1: {
        return key.f1;
      }
      case 2: {
        return key.f2;
      }
      case 3: {
        return key.f3;
      }
      case 4: {
        return key.f4;
      }
      default: {
        return key.vk;
      }
    }
  }
};

class BytecodeFrame {
public:
  size_t reg_offset;
  const BytecodeStream* bs;
  int pc;
  int _pred;
  char _mode;
  size_t cse_frame_depth = 0;

  BytecodeFrame(const BytecodeStream& bs0, int pred, char mode)
      : reg_offset(0), bs(&bs0), pc(0), _pred(pred), _mode(mode) {}
};

class Trail {
  friend class MznSolver;

protected:
  std::vector<std::pair<Variable**, Variable*>> var_list_trail;
  std::vector<RefCountedObject*> obj_trail;
  // <Definition, procedure, size, arg(0)>
  std::vector<std::tuple<Variable*, Val>> alias_trail;
  std::vector<std::pair<Variable*, Vec*>> domain_trail;
  std::vector<std::tuple<Variable*, Constraint*, bool>> def_trail;
  // <Var list trail size, Obj trail size, Alias trail size, Domain trail size, Def trail size>
  std::vector<std::tuple<size_t, size_t, size_t, size_t, size_t>> trail_size;
  std::vector<int> timestamp_trail;
  bool last_operation_pop = false;

public:
  Trail() = default;
  virtual ~Trail() {
    for (auto& i : obj_trail) {
      if (i->rcoType() == RefCountedObject::VAR) {
        Variable::free(static_cast<Variable*>(i));
      } else {
        free(i);
      }
    }
    obj_trail.clear();
  };

  size_t len() { return trail_size.size(); }
  inline bool is_trailed(RefCountedObject* rco) {
    return (!trail_size.empty() && timestamp_trail.back() > rco->timestamp());
  }

  // Trail hedge pointer change
  inline bool trail_ptr(Variable* obj, Variable** member) {
    if (!is_trailed(obj)) {
      return false;
    }
    var_list_trail.emplace_back(member, *member);
    return true;
  }
  // Trail Reference Counted Object removal
  inline bool trail_removal(RefCountedObject* obj) {
    if (!is_trailed(obj)) {
      return false;
    }
    obj_trail.push_back(obj);
    return true;
  }
  // Trail variable aliasing
  inline bool trail_alias(Interpreter* interpreter, Variable* v) {
    if (!is_trailed(v)) {
      return false;
    }
    v->alias().addMemRef(interpreter);
    alias_trail.emplace_back(v, v->alias());
    return true;
  }
  // Trail definition domain change
  inline bool trail_domain(Interpreter* interpreter, Variable* v, Vec* dom) {
    if (!is_trailed(v)) {
      return false;
    }
    assert(dom);
    dom->addMemRef(interpreter);
    domain_trail.emplace_back(v, dom);
    return true;
  }
  bool trail_add_def(Variable* var, Constraint* c) {
    if (!is_trailed(var)) {
      return false;
    }
    def_trail.emplace_back(var, c, true);
    return true;
  }
  bool trail_rm_def(Variable* var, Constraint* c) {
    if (!is_trailed(var)) {
      return false;
    }
    def_trail.emplace_back(var, c, false);
    return true;
  }

  size_t save_state(Interpreter* interpreter);
  void untrail(Interpreter* interpreter);
};

// Structure for active loops
struct LoopState {
  LoopState(Vec* vec, int _exit_pc)
      : pos(reinterpret_cast<intptr_t>(vec->begin())),
        end(reinterpret_cast<intptr_t>(vec->end())),
        exit_pc(_exit_pc),
        is_range(false) {}

  LoopState(int l, int u, int _exit_pc) : pos(l), end(u + 1), exit_pc(_exit_pc), is_range(true) {}

  intptr_t pos;
  intptr_t end;

  int exit_pc : 31;
  int is_range : 1;
};

class Interpreter {
  friend class Trail;
  friend class MznSolver;

public:
  enum Status { ROGER, ABORTED, INCONSISTENT, ERROR, MAX_STATUS = ERROR };
  static const std::string status_to_string[MAX_STATUS + 1];

protected:
  RegisterFile _registers;
  std::vector<BytecodeFrame> _stack;
  std::vector<CSEFrame> _cse_stack;
  std::vector<AggregationCtx> _agg;
  std::vector<LoopState> _loops;
  std::vector<BytecodeProc>& _procs;
  int _identCount;
  std::vector<void*> cse;  // Different instantiations of CSETable
  std::unordered_map<Constraint*, Variable*> delayed_constraints;
  std::deque<Constraint*> _propQueue;
  RegisterFile globals;
  Status _status = ROGER;

  // The root variable. It's fixed to true, all toplevel constraints
  // are attached to it, and it's the head of the linked list of
  // all variables
  Variable* _root_var;

  Vec* infinite_dom;
  Vec* boolean_dom;
  Vec* true_dom;

public:
  Trail trail;
  std::unordered_map<int, Val> solutions;

  Interpreter(std::vector<BytecodeProc>& procs, const BytecodeFrame& f, int max_globals = -1)
      : _registers(4096),
        _procs(procs),
        _identCount(0),
        cse(procs.size()),
        globals(max_globals + 1) {
    _stack.reserve(32);
    _cse_stack.reserve(32);
    _stack.emplace_back(f);
    _registers.resize(this, f.bs->maxRegister() + 1);

    infinite_dom = Vec::a(this, newIdent(), {-Val::infinity(), Val::infinity()});
    infinite_dom->addRef(this);
    boolean_dom = Vec::a(this, newIdent(), {0, 1});
    boolean_dom->addRef(this);
    true_dom = Vec::a(this, newIdent(), {1, 1});
    true_dom->addRef(this);
    _root_var = Variable::createRoot(this, Val(true_dom), newIdent());
    _root_var->addRef(this);
    for (int i = 0; i < cse.size(); ++i) {
      switch (_procs[i].nargs) {
        case 1: {
          cse[i] = new CSETable<FixedKey<1>>();
          break;
        }
        case 2: {
          cse[i] = new CSETable<FixedKey<2>>();
          break;
        }
        case 3: {
          cse[i] = new CSETable<FixedKey<3>>();
          break;
        }
        case 4: {
          cse[i] = new CSETable<FixedKey<4>>();
          break;
        }
        default: {
          cse[i] = new CSETable<VariadicKey>();
          break;
        }
      }
    }
  }
  ~Interpreter(void);
  Status status() { return _status; }
  void run(void);
  bool runDelayed();
  void pushAgg(const Val& v, int stackOffset);
  void pushConstraint(Constraint* d);
  std::pair<Val, bool> cse_find(int proc, const CSEKey& key, BytecodeProc::Mode& mode);
  void cse_insert(int proc, CSEKey& key, BytecodeProc::Mode& mode, Val& val);
  void set_global(int i, const Val& val) { globals.assign(this, i, val); }
  void clear_globals() { globals.clear(this); }
  const Val get_global(int i) { return globals[i]; }
  PropStatus subscribe(Constraint* c);
  void unsubscribe(Constraint* d);
  int newIdent(void) { return _identCount++; }
  int currentIdent(void) const { return _identCount; }
  void dumpState(std::ostream& os);
  void dumpState(const BytecodeFrame& bf, std::ostream& os);
  void dumpState();
  void schedule(Constraint* d, const Variable::SubscriptionEvent& ev);
  void deschedule(Constraint* d);
  void propagate(void);
  void call(int code, std::vector<Val>&& args);

  const Val& reg(const BytecodeFrame& bf, int r) { return _registers[bf.reg_offset + r]; }
  void assign(const BytecodeFrame& bf, int r, const Val& v) {
    _registers.assign(this, bf.reg_offset + r, v);
  }
  BytecodeFrame& frame() { return _stack.back(); }

  Val infinite_domain() { return Val(infinite_dom); }
  Val boolean_domain() { return Val(boolean_dom); }
  void register_delayed(Constraint* c, Variable* v) { delayed_constraints[c] = v; }
  void remove_delayed(Constraint* c) { delayed_constraints.erase(c); }

  Variable* root() { return _root_var; }

  /// Perform optimizatin by basic propagation on generated FlatZinc
  void optimize(void);
};

inline void RefCountedObject::rmRef(Interpreter* interpreter, RefCountedObject* rco) {
  assert(rco->_model_ref_count > 0);
  if (--rco->_model_ref_count == 0) {
    switch (rco->rcoType()) {
      case VAR:
        static_cast<Variable*>(rco)->destroy(interpreter);
        break;
      case VEC:
        static_cast<Vec*>(rco)->destroyModel(interpreter);
        break;
      default:
        assert(false);
    }
    if (interpreter->trail.is_trailed(rco)) {
      interpreter->trail.trail_removal(rco);
    } else if (rco->_memory_ref_count == 0) {
      // INVARIANT: All children of a definition are already promoted, cut, or freed.
      //        assert(rco->rcoType() != VAR ||
      //        !static_cast<Variable*>(rco)->constraints().empty());
      if (rco->rcoType() == VAR) {
        Variable::free(static_cast<Variable*>(rco));
      } else {
        ::free(rco);
      }
    }
  }
}

inline void RefCountedObject::rmMemRef(Interpreter* interpreter, RefCountedObject* rco) {
  if (--rco->_memory_ref_count == 0u && !interpreter->trail.is_trailed(rco) &&
      rco->_model_ref_count == 0u) {
    // INVARIANT: All children of a definition are already promoted, cut, or freed.
    //      assert(rco->rcoType() != DEF || !static_cast<Definition*>(rco)->defs());
    if (rco->rcoType() == VEC) {
      static_cast<Vec*>(rco)->destroyMemory(interpreter);
    }
    free(rco);
  }
}

inline AggregationCtx::AggregationCtx(Interpreter* interpreter, int s)
    : def_ident_start(interpreter->currentIdent()), symbol(static_cast<Symbol>(s)), n_symbols(1) {
  assert(s >= 0 && s <= VCTX_OTHER);
}
}  // namespace MiniZinc

#include <minizinc/interpreter.hpp>
#include <minizinc/interpreter/cse.hpp>
