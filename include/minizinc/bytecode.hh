/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MINIZINC_BYTECODE_HH__
#define __MINIZINC_BYTECODE_HH__

#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_set>

#include <minizinc/values.hh>
#include <minizinc/ast.hh>

namespace MiniZinc {

  class BytecodeProc;
  class Interpreter;
  class SolverInstanceBase;

  class BytecodeStream {
  protected:
    /// The bytecode stream
    std::vector<char> _bs;
    /// The largest register used by this code
    int _max_reg;
  public:
    /// Constructor
    BytecodeStream(void) : _max_reg(0) {}
    
    enum Instr {
      ADDI, // R1, R2 -> R3
      SUBI, // R1, R2 -> R3
      MULI, // R1, R2 -> R3
      DIVI, // R1, R2 -> R3
      MODI, // R1, R2 -> R3
      INCI, // R1
      DECI, // R1
      
      IMMI, // I, R : Load immediate integer into register
      LOAD_GLOBAL, // i -> R : Load global i into register R (globals are registers of the bottom stack frame)
      STORE_GLOBAL, // R -> i : Store register R into global i
      MOV,  // R1 -> R2

      JMP,  // i : pc=i
      JMPIF,     // R, i: if R then pc=i
      JMPIFNOT,  // R, i: if not R then pc=i
      
      EQI,
      LTI,
      LEI,
      
      AND,
      OR,
      NOT,
      XOR,
      
      ISPAR,   // R1 -> R2: put whether value in R1 is not a variable into R2
      ISEMPTY, // R1 -> R2: put whether vector in R1 is empty into R2
      LENGTH,  // R1 -> R2: put length of vector in R1 into R2
      GET_VEC, // R1, R2 -> R3: put element R2 of vector in R1 into R3
      
      LB, // R1 -> R2: put lower bound of value in R1 into R2
      UB, // R1 -> R2: put upper bound of value in R1 into R2
      DOM, // R1 -> R2: put domain of value in R1 into R2
      
      INTERSECTION, // R1, R2 -> R3: put intersection of sets in R1 and R2 into R3
      UNION, // R1, R2 -> R3: put union of sets in R1 and R2 into R3
      
      INTERSECT_DOMAIN, // R1, R2 -> R3: Update domain of R1 with set R2, place result in R3
      
      OPEN_AGGREGATION, // i: Create a new aggregation context with symbol i
      CLOSE_AGGREGATION,  // Close current aggregation context, put result onto context above
      SIMPLIFY_LIN, // R1 -> R2, R3, R4: simplify linear expression in R1, return coefficients (R2), variables (R3), constant (R4)
      
      PUSH,  // R: push R onto value stack
      POP,   // R: pop from value stack into R
      POST,  // R: post constraint in R
      
      RET, // return from call
      CALL, // m, i, n, R1, ..., Rn: call code i in mode m with n arguments
      BUILTIN, // i, n, R1, ..., Rn : call builtin function i
      TCALL, // m, i : call code i in mode m (arguments are assumed to be in correct registers already)
      
      TRACE, // R: output string representation of R
      ABORT, // abort execution
      
    };
    
    /// Get instruction at \a pc and increment \a pc
    Instr instr(int& pc) const { assert(pc < _bs.size()); return static_cast<Instr>(_bs[pc++]); }
    IntVal intval(int& pc) const { assert(pc < _bs.size()); const IntVal* iv = reinterpret_cast<const IntVal*>(&_bs[pc]); pc += sizeof(IntVal); return *iv; }
    Expression* expr(int& pc) { assert(pc < _bs.size()); Expression** e = reinterpret_cast<Expression**>(&_bs[pc]); pc += sizeof(Expression*); return *e; }
    int reg(int& pc) const { assert(pc < _bs.size()); const int* iv = reinterpret_cast<const int*>(&_bs[pc]); pc += sizeof(int); return *iv;}
    char chr(int& pc) const { assert(pc < _bs.size()); return _bs[pc++]; }
    const char* str(int& pc) const {
      assert(pc < _bs.size());
      int n = reg(pc);
      const char* ret = &_bs[pc];
      pc += n+1;
      return ret;
    }
    
    int size(void) const { return _bs.size(); }
    bool eos(int pc) const { return pc >= _bs.size(); }
    
    void patchAddress(int pc, int addr) {
      const char* cp = reinterpret_cast<const char*>(&addr);
      for (int i=0; i<sizeof(int); i++) {
        _bs[pc+i] = cp[i];
      }
    }
    
    void addInstr(const Instr& i) { _bs.push_back(i); }
    void addReg(int iv) {
      _max_reg = std::max(_max_reg,iv);
      const char* cp = reinterpret_cast<const char*>(&iv);
      for (int i=0; i<sizeof(int); i++) {
        _bs.push_back(cp[i]);
      }
    }
    void addSmallInt(int iv) {
      const char* cp = reinterpret_cast<const char*>(&iv);
      for (int i=0; i<sizeof(int); i++) {
        _bs.push_back(cp[i]);
      }
    }
    void addIntVal(const IntVal& iv) {
      const char* cp = reinterpret_cast<const char*>(&iv);
      for (int i=0; i<sizeof(IntVal); i++) {
        _bs.push_back(cp[i]);
      }
    }
    void addCharVal(const char c) {
      _bs.push_back(c);
    }
    void addExpr(Expression* e) {
      const char* cp = reinterpret_cast<const char*>(e);
      for (int i=0; i<sizeof(Expression*); i++) {
        _bs.push_back(cp[i]);
      }
    }
    void addStr(const std::string& s) {
      addReg(s.size());
      for (char c: s)
        _bs.push_back(c);
      _bs.push_back(0);
    }
    
    std::string toString(const std::vector<BytecodeProc>& procs = std::vector<BytecodeProc>()) const;
    int maxRegister(void) const { return _max_reg; }
  };

  class Vec;
  class Definition;
  class WeakVal;

  class RefCountedObject {
  public:
    enum RCOType { VEC, DEF };
  protected:
    unsigned int _ref_count;
    unsigned int _weak_ref_count : 31;
    unsigned int _rco_type : 1;
    int _timestamp;
    RefCountedObject(const RCOType& t, int timestamp) : _ref_count(0), _weak_ref_count(0), _rco_type(t==VEC ? 1 : 0), _timestamp(timestamp) {}
  public:
    RCOType rcoType(void) const { return _rco_type==1 ? VEC : DEF; }
    const int timestamp() const { return _timestamp; }

    void addRef(Interpreter* interpreter) { _ref_count++; }
    static void rmRef(Interpreter* interpreter, RefCountedObject* rco);
    bool exists() { return _ref_count > 0; }

    void addWRef(Interpreter* interpreter) { assert(_ref_count > 0); _weak_ref_count++; }
    static void rmWRef(Interpreter* interpreter, RefCountedObject* rco);
  };
  

  /// Value tagged union
  class Val {
    friend class WeakVal;
  protected:
    /// The value
    // Bit 0: 0=int, 1=RefCountedObject
    void* _v;
  public:
    static Val follow_alias(const Val& v, Interpreter* interpreter = nullptr);

    bool isRCO(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(1)) == static_cast<ptrdiff_t>(1);
    }
    RefCountedObject* toRCO(void) const {
      assert(isRCO());
      return reinterpret_cast<RefCountedObject*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(1));
    }
    bool exists() const { return !isRCO() || toRCO()->exists(); }
    bool isVec(void) const {
      return isRCO() && toRCO()->rcoType()==RefCountedObject::VEC;
    }
    bool isInt(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(1)) == 0;
    }
    bool isDef(void) const {
      return isRCO() && toRCO()->rcoType()==RefCountedObject::DEF;
    }
    bool operator==(const Val& rhs) const;
    bool contains(const Val& v) const {
      if (*this == v) {
        return true;
      }
      if (this->isVec()) {
        for (int i = 0; i < this->size(); ++i) {
          if ((*this)[i].contains(v)) {
            return true;
          }
        }
      }
      return false;
    }

    /// Access value as Definition
    Definition* toDef(void) const;
    /// Access value as IntVal
    IntVal operator() (void) const {
      assert(isInt());
      unsigned long long int i = reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3);
      bool pos = ((reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(2)) == 0);
      if (pos) {
        return i >> 2;
      } else {
        return -(static_cast<long long int>(i>>2));
      }
    }
    int timestamp() const {
      assert(isRCO());
      return toRCO()->timestamp();
    }

    void destroy(Interpreter* interpreter) {
      if (isRCO()) {
        RefCountedObject::rmRef(interpreter, toRCO());
      }
    };
    void construct(Interpreter* interpreter) {
      if (isRCO()) {
        toRCO()->addRef(interpreter);
      }
    }
    void addWeakRef(Interpreter* interpreter) {
      if (isRCO()) {
        toRCO()->addWRef(interpreter);
      }
    };
    void removeWeakRef(Interpreter* interpreter) {
      if (isRCO()) {
        RefCountedObject::rmWRef(interpreter, toRCO());
      }
    };

    /// Access value as vector, return element \a i
    const Val& operator [](int i) const;
    /// Access value as vector, return size
    size_t size(void) const;
    /// Access value as vector
    Vec* toVec(void) const;
  public:
    Val(const IntVal& i=IntVal(0)) {
      assert(i.isFinite());
      static const unsigned int pointerBits = sizeof(void*)*8;
      static const long long int maxUnboxedVal = (static_cast<long long int>(1) << (pointerBits - 2)) - static_cast<long long int>(1);
      assert(i >= -maxUnboxedVal && i <= maxUnboxedVal);
      long long int j = i < 0 ? -i.toInt() : i.toInt();
      ptrdiff_t ubi_p = (static_cast<ptrdiff_t>(j) << 2);
      if (i < 0)
        ubi_p = ubi_p | static_cast<ptrdiff_t>(2);
      _v = reinterpret_cast<void*>(ubi_p);
    }
    explicit Val(RefCountedObject* d) {
      assert(d != nullptr);
      _v = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(d) | static_cast<ptrdiff_t>(1));
    }
    ~Val(void);
    Val(const Val& v);
    Val(Val&& v);
    Val& operator =(const Val& v);
    Val& operator =(Val&& v);
    void assign(Interpreter* interpreter, const Val& v);
    void assign(Interpreter* interpreter, Val&& v);
    std::string toString(void) const;
    Expression* const toFZN(const std::unordered_map<int, VarDecl*>& vdmap = {}) {
      GCLock lock;
      if (this->isInt()) {
        return IntLit::a((*this)());
      } else if (this->isDef()) {
        auto it = vdmap.find(this->timestamp());
        assert(it != vdmap.end());
        VarDecl* vd = it->second;
        auto id = new Id(Location().introduce(), this->timestamp(), vd);
        id->type(vd->type());
        return id;
      } else {
        assert(this->isVec());
        std::vector<Expression*> vec(this->size());
        bool par = true;
        for (int i = 0; i < this->size(); ++i) {
          Val v = (*this)[i];
          assert(!v.isVec()); // TODO: Handle multi-dimentional vectors
          vec[i] = v.toFZN(vdmap);
          par = par && vec[i]->type().ispar();
        }
        auto al = new ArrayLit(Location().introduce(), vec);
        al->type(par ? Type::parint(1) : Type::varint(1));
        return al;
      }
    }
  };
  
  class Vec : public RefCountedObject {
  protected:
    int _size;
    Val _data[1];
    Vec(Interpreter* interpreter, int timestamp, const std::vector<Val>& v) : RefCountedObject(RefCountedObject::VEC,timestamp), _size(v.size()) {
      for (unsigned int i=0; i<v.size(); i++) {
        new (&_data[i]) Val(v[i]);
        _data[i].construct(interpreter);
      }
    }
    ~Vec(void) = delete;
  public:
    int size(void) const { return _size; }
    // TODO: Should vectors be indexed from 1 internally?
    const Val& operator [](int i) const { assert(i >= 0 && i<_size); return _data[i]; }
    static Vec* a(Interpreter* interpreter, int timestamp, const std::vector<Val>& v) {
      Vec* nv = static_cast<Vec*>(::malloc(sizeof(Vec)+sizeof(Val)*(v.size()-1)));
      new (nv) Vec(interpreter,timestamp,v);
      return nv;
    }
    void destroy(Interpreter* interpreter) {
      for (unsigned int i=0; i<size(); i++) {
        _data[i].destroy(interpreter);
      }
    }
    void reconstruct(Interpreter* interpreter) {
      for (unsigned int i=0; i<size(); i++) {
        _data[i].construct(interpreter);
      }
    }
    inline bool operator==(const Vec& rhs) const {
      if (_size != rhs._size) {
        return false;
      }
      for (int i = 0; i < _size; ++i) {
        if (!((*this)[i] == rhs[i])) {
          return false;
        }
      }
      return true;
    }
  };
  
  /// Iterator over a Vec interpreted as a range set
  class VecSetRanges {
    /// The vector
    const Vec* rs;
    /// The current range
    int n;
  public:
    /// Constructor
    VecSetRanges(const Vec* r) : rs(r), n(0) {}
    /// Check if iterator is still valid
    bool operator()(void) const { return n+1<rs->size(); }
    /// Move to next range
    void operator++(void) { n+=2; }
    /// Return minimum of current range
    IntVal min(void) const { return (*rs)[n](); }
    /// Return maximum of current range
    IntVal max(void) const { return (*rs)[n+1](); }
    /// Return width of current range
    IntVal width(void) const { return (*rs)[n+1]()-(*rs)[n]()+1; }
  };

  
  inline
  Val::~Val(void) { }
  inline
  Val::Val(const Val& v) : _v(v._v) { }
  inline
  Val& Val::operator =(const Val& v) {
    _v = v._v;
    return *this;
  }
  inline
  Val& Val::operator =(Val&& v) {
    _v = v._v;
    v._v = nullptr;
    return *this;
  }
  inline
  Val::Val(Val&& v) : _v(v._v) { v._v = nullptr; }
  inline
  void Val::assign(Interpreter* interpreter, const Val& v) {
    if (this != &v) {
      destroy(interpreter);
      _v = v._v;
      construct(interpreter);
    }
  }
  inline
  void Val::assign(Interpreter* interpreter, Val&& v) {
    if (this != &v) {
      destroy(interpreter);
      _v = v._v;
      v._v = nullptr;
    }
  }
  
  inline
  const Val& Val::operator [](int i) const {
    assert(isVec());
    return (*toVec())[i];
  }
  /// Access value as vector, return size
  inline
  size_t Val::size(void) const {
    assert(isVec());
    return toVec()->size();
  }

  
  class RegisterFile {
  protected:
    std::vector<Val> _r;
  public:
    RegisterFile(int n=0) : _r(n) {}
    const Val& operator [](int r) { assert(r < _r.size()); return _r[r]; }
    std::vector<Val>::const_iterator cbegin() { return _r.cbegin(); }
    std::vector<Val>::const_iterator cend() { return _r.cend(); }
    void assign(Interpreter* interpreter, int r, const Val& v) {
      if (r >= _r.size()) {
        _r.resize(r+1);
        assert(_r.size()==r+1);
      }
      _r[r].assign(interpreter,v);
    }
    
    void cp(Interpreter* interpreter, int r1, int r2) {
      assert(r1 < _r.size());
      if (r2 >= _r.size()) _r.resize(r2+1);
      _r[r2].assign(interpreter,_r[r1]);
    }
    void cp(Interpreter* interpreter, int r1, RegisterFile& rf, int r2) {
      assert(r1 < _r.size());
      if (r2 >= rf._r.size()) rf._r.resize(r2+1);
      rf._r[r2].assign(interpreter,_r[r1]);
    }
    void mov(Interpreter* interpreter, int r1, RegisterFile& rf, int r2) {
      assert(r1 < _r.size());
      if (r2 >= rf._r.size()) rf._r.resize(r2+1);
      rf._r[r2].assign(interpreter,std::move(_r[r1]));
    }
    void mov(Interpreter* interpreter, std::vector<Val>& args) {
      for (auto& v : args) {
        v.construct(interpreter);
      }
      _r = std::move(args);
    }
    /// Destroy this register file
    void destroy(Interpreter* interpreter) {
      for (auto& v : _r) {
        v.destroy(interpreter);
      }
    }
    void dump(std::ostream& os) {
      for (unsigned int i=0; i<_r.size(); i++) {
        os << "  R" << i << " = " << _r[i].toString() << "\n";
      }
    }
  };

  class Definition : public RefCountedObject {
    friend class Trail;
  public:
    enum SubscriptionEvent { SEV_VAL, SEV_UNIFY, SEV_DOM, SEV };
    /// Event sets propagators can subscribe to: only value events, value+unification, or any change
    enum SubscriptionEventSet { SES_VAL, SES_VALUNIFY, SES_ANY };
    typedef std::unordered_map<Definition*, SubscriptionEventSet> Subscriptions;
  protected:
    Definition* _prev;
    Definition* _next;
    Val _domain;
    Val _ann;
    Definition* _defs;
    int _pred : 32;
    int _size : 31;
    unsigned int _flag : 1;
    /// Whether current domain is binding
    unsigned int _binding : 1;
    char _mode : 8;
    Subscriptions _subscriptions;
    Val _args[1];
    Definition(Interpreter* interpreter,Val domain,bool binding,int pred,char mode,const std::vector<Val>& args,int ident,Val ann);
  public:
    Val domain(void) const { return _domain; }
    /// Set domain to \a newDomain, schedule propagators
    void domain(Interpreter* interpreter, const Val& newDomain, bool binding);
    /// Set domain to \a newDomain, schedule propagators
    void domain(Interpreter* interpreter, const std::vector<Val>& newDomain, bool binding);
    Val ann(void) const { return _ann; }
    int pred(void) const { return _pred; }
    char mode(void) const { return _mode; }
    int size(void) const { return _size; }
    Val arg(int i) const { assert(i < _size); return _args[i]; }
    Definition* defs(void) const { return _defs; }
    void defs(Interpreter* interpreter, Definition* defs) {
      if (!_defs) {
        _defs = Definition::a(interpreter,IntVal(0),false,0,0,{},-1);
      }
      _defs->appendBefore(interpreter, defs);
    }
    static Definition* a(Interpreter* interpreter,Val domain,bool binding,int pred,char mode,const std::vector<Val>& args,int ident,Val ann=IntVal(0)) {
      Definition* d = static_cast<Definition*>(::malloc(sizeof(Definition)+sizeof(Val)*(std::max(0,static_cast<int>(args.size())-1))));
      new (d) Definition(interpreter,domain,binding,pred,mode,args,ident,ann);
      return d;
    }
    static void free(Definition* def) {
      // INVARIANT: def->destroy() should be called before free(def);
      assert(def->_ref_count == 0 && def->_weak_ref_count == 0);
      if (def->_defs) {
        // destroy all linked definitions
        Definition* d = def->_defs;
        bool finished = false;
        while (!finished) {
          Definition* cur = d;
          d = d->next();
          finished = (cur == d);
          // INVARIANT: def->destroy() should ensure that no children with reference counts are still linked
          assert(cur->_ref_count == 0 && cur->_weak_ref_count == 0);
          Definition::free(cur);
        }
      }
      ::free(def);
    }
    ~Definition(void) = delete;
    /// Destroy and unlink this definition
    void destroy(Interpreter* interpreter);
    void reconstruct(Interpreter* interpreter);
    /// Insert singleton element into list before \a d
    void insertBefore(Interpreter* interpreter, Definition* d);
    /// Append list to other list before \a d
    void appendBefore(Interpreter* interpreter, Definition* d);
    void unlink(Interpreter* interpreter);
    /// Set the reference count to 1
    void makeUniqueReference(void) { _ref_count = 1; }
    void alias(Interpreter* interpreter, Val v);
    void unalias(Interpreter* interpreter, int proc, int size, const Val& arg0);
    Definition* prev(void) const { return _prev; }
    Definition* next(void) const { return _next; }
    int listSize(void) const {
      int i=1;
      if (_next != this) {
        for (Definition* d = _next; d != this; d = d->next()) {
          i++;
        }
      }
      return i;
    }
    bool attached() { return !(this == _prev); }

    static void dump(Definition* d, const std::vector<BytecodeProc>& bs, std::ostream& os, int indent=0);
    static void toFZN(Definition* d, const std::vector<BytecodeProc>& bs, Model* model, std::unordered_map<int, VarDecl*>& vdmap, Interpreter* interpreter=nullptr);
    static void toFZNItem(Definition* d, const std::vector<BytecodeProc>& bs, Model* model, std::unordered_map<int, VarDecl*>& vdmap, Interpreter* interpreter=nullptr);
    static void addToSolver(Interpreter* interpreter, Definition* d, const std::vector<BytecodeProc>& bs, SolverInstanceBase* si);

    // Propagation interface
    
    /// Flag whether definition is currently scheduled
    bool flag(void) const { return _flag==1; }
    /// Set flag whether definition is currently scheduled
    void flag(bool f) { _flag = f; }
    /// Flag whether definition's domain is binding
    bool binding(void) const { return _binding==1; }
    /// Set flag whether definition's domain is binding
    void binding(Interpreter* interpreter, bool f);
    /// Add \a d to set of subscribed constraints
    void subscribe(Definition* d, const SubscriptionEventSet& events);
    /// Remove \a d from set of subscribed constraints
    void unsubscribe(Definition* d);
    /// Return subscribed definitions
    const Subscriptions& subscriptions(void) const {
      return _subscriptions;
    }
    /// Return subscribed definitions
    Subscriptions& subscriptions(void) {
      return _subscriptions;
    }
    
  };

  class WeakVal {
  protected:
    // Value of the Val
    void* _v;
  public:
    explicit WeakVal(const Val& val) {
      if(val.isRCO()) {
        auto timestamp = val.timestamp();
        // TODO: assert timestamp <= unboxed int
        assert(timestamp > 0);
        _v = reinterpret_cast<void*>(static_cast<ptrdiff_t>(timestamp) << 1 | static_cast<ptrdiff_t>(1));
      } else {
        assert((reinterpret_cast<ptrdiff_t>(val._v) & static_cast<ptrdiff_t>(1)) == 0);
        _v = val._v;
      }
    }

    size_t hash() const {std::hash<void*> h; return h(_v);}
    inline bool operator==(const WeakVal& rhs) const { return reinterpret_cast<ptrdiff_t>(_v) == reinterpret_cast<ptrdiff_t>(rhs._v); }
    inline bool operator!=(const WeakVal& rhs) const { return reinterpret_cast<ptrdiff_t>(_v) != reinterpret_cast<ptrdiff_t>(rhs._v); }
    inline bool operator<(const WeakVal& rhs) const { return reinterpret_cast<ptrdiff_t>(_v) < reinterpret_cast<ptrdiff_t>(rhs._v); }

  };

  class AggregationCtx {
  protected:
    /// Stack of values that need to be aggregated
    std::vector<Val> stack;
  public:
    /// Definitions attached to the computed value
    Definition* def_stack;
    /// Earliest time stamp for definitions in the current aggregation
    int def_ident_start;
    /// Type of function represented by this context
    enum Symbol { VCTX_AND, VCTX_OR, VCTX_VEC, VCTX_OTHER } symbol;
    /// Nesting depth for this symbol (how many of these are open)
    int n_symbols;
    /// Constructor
    AggregationCtx(Interpreter* interpreter, int s);
    /// Push value onto aggregation stack
    void push(Interpreter* interpreter, const Val& v) {
      stack.push_back(v);
      stack.back().construct(interpreter);
    }
    void pop(Interpreter* interpreter) {
      stack.back().destroy(interpreter);
      stack.pop_back();
    }
    const Val& back(void) const {
      return stack.back();
    }
    const Val& operator [](int i) const { return stack[i]; }
    int size(void) const { return stack.size(); }
    bool empty(void) const { return stack.empty(); }
    Val createVec(Interpreter* interpreter, Definition* defs, int timestamp) const;
    /// Destroy stack values
    void destroyStack(Interpreter* interpreter) {
      for (auto& v : stack) {
        v.destroy(interpreter);
      }
    }
    /// Destroy head of linked definitions
    void destroyDef(Interpreter* interpreter) {
      RefCountedObject::rmRef(interpreter, def_stack);
    }
  };

  class BytecodeProc {
  public:
    /// The name of this procedure
    std::string name;
    /// Number of arguments
    int nargs;
    /// Delayed execution
    bool delay;
    /// Modes
    enum Mode { RAW, ROOT, ROOT_NEG, FUN, FUN_NEG, IMP, IMP_NEG, MAX_MODE=IMP_NEG };
    static const std::string mode_to_string[MAX_MODE+1];
    static const bool is_neg(const Mode& mode) { return mode == ROOT_NEG || mode == FUN_NEG || mode == IMP_NEG; }
    static const Mode negate(const Mode& mode) {
      switch(mode) {
      case ROOT: return ROOT_NEG;
      case IMP: return IMP_NEG;
      case FUN: return FUN_NEG;
      case ROOT_NEG: return ROOT;
      case IMP_NEG: return IMP;
      case FUN_NEG: return FUN;
      default:
        break;
      }
      return RAW;
    }
    /// The code for different modes
    BytecodeStream mode[MAX_MODE+1];
  };

  /// CSE table: Saved results of historical executions
  class CSETable {
  public:
    class Key {
    private:
      size_t _size;
      WeakVal* _vals;
    public:
      Key() : _size(0), _vals(nullptr) {}
      explicit Key(const std::vector<Val>& vec);
      // INVARIANT: Key is not used after destroy is called
      void destroy() const { free(_vals); }

      const size_t& size() const { return _size; }
      const WeakVal& operator [](int i) const { assert(i < _size); return _vals[i]; }
      const bool operator==(const Key& rhs) const {
        if (size() != rhs.size()) {
          return false;
        }
        for (int i = 0; i < size(); ++i) {
          if (operator[](i) != rhs[i]) {
            return false;
          }
        }
        return true;
      }
      const bool operator<(const Key& rhs) const {
        if (size() != rhs.size()) {
          return size() < rhs.size();
        } else {
          for (int i = 0; i < size(); ++i) {
            if (operator[](i) != rhs[i]) {
              return operator[](i) < rhs[i];
            }
          }
          return false;
        }
      }
      const size_t hash() const {
        auto combine = [](size_t& incumbent, size_t h) { incumbent ^= h + 0x9e3779b9 + (incumbent << 6) + (incumbent >> 2); };
        size_t hash = 0;
        for (int i = 0; i < size(); ++i) {
          combine(hash, operator[](i).hash());
        }
        return hash;
      }
    };
    struct Hash { size_t operator()(const Key& key) const {
      return key.hash();
    }};
    struct Equals { bool operator()(const Key& lhs, const Key& rhs) const {
      return lhs == rhs;
    }};
    struct Less { bool operator()(const Key& lhs, const Key& rhs) const {
      return lhs < rhs;
    }};
//    typedef std::unordered_map<Key, std::pair<BytecodeProc::Mode, Val>, Hash, Equals> impl;
    typedef std::map<Key, std::pair<BytecodeProc::Mode, Val>> impl;
    typedef impl::iterator iterator;
    std::pair<Val, bool> lookup(Interpreter* interpreter, const Key& key, BytecodeProc::Mode& mode);
    void insert(Interpreter* interpreter, Key& key, const BytecodeProc::Mode& mode, Val& val);
  protected:
    std::vector<impl> _table = std::vector<impl>(1);
  public:
    ~CSETable() { assert(_table.size() == 1 && _table[0].empty()); }
    void destroy(Interpreter* interpreter) {
      for (auto &table : _table) {
        for(auto &item : table) {
          item.first.destroy();
          item.second.second.removeWeakRef(interpreter);
        }
      }
      _table = std::vector<impl>(1);
    }
    void push(Interpreter* interpreter, bool cleanup) {
      if (cleanup) {
        auto& table = _table.back();
        auto it = table.begin();
        while (it != table.end()) {
          if (!it->second.second.exists()) {
            it->first.destroy();
            it->second.second.removeWeakRef(interpreter);
            it = table.erase(it);
          } else {
            ++it;
          }
        }
      }
      _table.emplace_back();
    }
    void pop(Interpreter* interpreter) {
      for (auto& item : _table.back()) {
        item.first.destroy();
        item.second.second.removeWeakRef(interpreter);
      }
      _table.pop_back();
    }
  };

  class BytecodeFrame {
  public:
    RegisterFile reg;
    const BytecodeStream* bs;
    int pc;
    
    // CSE information for RET statement
    // <proc, mode, cse_key, stack size>
    typedef std::tuple<int,BytecodeProc::Mode, CSETable::Key, size_t> CSEInfo;
    std::vector<CSEInfo> cse_info;

    BytecodeFrame(const BytecodeStream& bs0) :
    reg(bs0.maxRegister()), bs(&bs0),
    pc(0) {}
    void destroyRegisters(Interpreter* interpreter) {
      reg.destroy(interpreter);
    }
    void destroy(Interpreter* interpreter) {
      destroyRegisters(interpreter);
    }
    void dump(std::ostream& os) {
      reg.dump(os);
    }
  };

  class Trail {
    friend class MznSolver;
  protected:
    std::vector<std::pair<Definition**, Definition*>> hedge_trail;
    std::vector<RefCountedObject*> obj_trail;
    // <Definition, procedure, size, arg(0)>
    std::vector<std::tuple<Definition*, int, int, Val>> alias_trail;
    std::vector<std::pair<Definition*, Val>> domain_trail;
    // <Hedge trail size, Obj trail size, Alias trail size, Domain trail size>
    std::vector<std::tuple<size_t, size_t, size_t, size_t>> trail_size;
    std::vector<int> timestamp_trail;
    // Trail for the last item in the hedge
    std::vector<Definition*> end_trail;
    bool last_operation_pop = false;
  public:
    Trail() = default;
    virtual ~Trail() {
      for (auto &i : obj_trail) {
        if (i->rcoType() == RefCountedObject::DEF) {
          Definition::free(static_cast<Definition*>(i));
        } else {
          free(i);
        }
      }
      obj_trail.clear();
    };

    size_t len() { return trail_size.size(); }
    inline
    bool is_trailed(RefCountedObject* rco) { return (!trail_size.empty() && timestamp_trail.back() > rco->timestamp()); }

    // Trail hedge pointer change
    inline bool trail_ptr(Definition* def, Definition** member) {
      if (!is_trailed(def)) {
        return false;
      }
      hedge_trail.emplace_back(member, *member);
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
    // Trail definition aliasing
    inline bool trail_alias(Interpreter* interpreter, Definition* def) {
      if (!is_trailed(def)) {
        return false;
      }
      def->arg(0).addWeakRef(interpreter);
      alias_trail.emplace_back(def, def->pred(), def->size(), def->arg(0));
      return true;
    }
    // Trail definition domain change
    inline bool trail_domain(Interpreter* interpreter, Definition* def, Val dom) {
      if (!is_trailed(def)) {
        return false;
      }
      dom.addWeakRef(interpreter);
      domain_trail.emplace_back(def, dom);
      return true;
    }
    size_t save_state(Interpreter* interpreter);
    void untrail(Interpreter* interpreter);
  };
  
  class Interpreter {
    friend class Trail;
    friend class MznSolver;
  public:
    enum Status { ROGER, ABORTED, INCONSISTENT, ERROR, MAX_STATUS=ERROR };
    static const std::string status_to_string[MAX_STATUS+1];
  protected:
    std::vector<BytecodeFrame> _stack;
    std::vector<AggregationCtx> _agg;
    std::vector<BytecodeProc>& _procs;
    int _identCount;
    std::vector<CSETable> cse;
    std::vector<Definition*> delayed_calls;
    std::deque<Definition*> _propQueue;
    RegisterFile globals;
    Status _status = ROGER;
  public:
    Trail trail;

    Interpreter(std::vector<BytecodeProc>& procs,
                const BytecodeFrame& f) : _procs(procs), _identCount(0), cse(procs.size())
    {
      _stack.push_back(f);
    }
    ~Interpreter(void);
    Status status() { return _status; }
    void run(void);
    bool runDelayed();
    void pushAgg(const Val& v, int stackOffset);
    void pushDef(Definition* d);
    void pushDefs(Definition* defs);
    std::pair<Val, bool> cse_lookup(int proc, const CSETable::Key& key, BytecodeProc::Mode& mode) {
      return cse[proc].lookup(this, key, mode);
    }
    void cse_insert(int proc, CSETable::Key& key, BytecodeProc::Mode& mode, Val& val) {
      return cse[proc].insert(this, key, mode, val);
    }
    void set_global(int i, const Val& val) { globals.assign(this, i, val); }
    const Val get_global(int i) { return globals[i]; }
    void subscribe(Definition* d);
    void unsubscribe(Definition* d);
    int newIdent(void) { return _identCount++; }
    int currentIdent(void) const { return _identCount; }
    void dumpState(std::ostream& os);
    void schedule(Definition* d, const Definition::SubscriptionEvent& ev);
    void deschedule(Definition* d);
    void propagate(void);
    Model* toFZN();
    void call(int code, const BytecodeProc::Mode& mode, const std::vector<Val>& args, bool delayed=false);
    
    /// Perform optimizatin by basic propagation on generated FlatZinc
    void optimize(void);
  };

  inline
  void RefCountedObject::rmRef(Interpreter* interpreter, RefCountedObject* rco) {
    if(--rco->_ref_count==0) {
      switch (rco->rcoType()) {
        case DEF:
          static_cast<Definition*>(rco)->destroy(interpreter);
          break;
        case VEC:
          static_cast<Vec*>(rco)->destroy(interpreter);
          break;
        default:
          assert(false);
      }
      if (interpreter->trail.is_trailed(rco)) {
        interpreter->trail.trail_removal(rco);
      } else if (rco->_weak_ref_count==0) {
        // INVARIANT: All children of a definition are already promoted, cut, or freed.
        assert(rco->rcoType() != DEF || !static_cast<Definition*>(rco)->defs());
        free(rco);
      }
    }
  }
  inline
  void RefCountedObject::rmWRef(Interpreter* interpreter, RefCountedObject* rco) {
    if(--rco->_weak_ref_count == 0 && !interpreter->trail.is_trailed(rco) && rco->_ref_count == 0) {
      // INVARIANT: All children of a definition are already promoted, cut, or freed.
      assert(rco->rcoType() != DEF || !static_cast<Definition*>(rco)->defs());
      free(rco);
    }
  }

  inline
  Definition* Val::toDef(void) const {
    assert(isDef());
    return static_cast<Definition*>(toRCO());
  }
  inline
  Vec* Val::toVec(void) const {
    assert(isVec());
    return static_cast<Vec*>(toRCO());
  }

  inline
  AggregationCtx::AggregationCtx(Interpreter* interpreter, int s) :
    def_stack(Definition::a(interpreter,IntVal(0),false,0,0,{},-1)),
    def_ident_start(interpreter->currentIdent()),
    symbol(static_cast<Symbol>(s)), n_symbols(1) {
    assert(s >= 0 && s <= VCTX_OTHER);
    def_stack->addRef(interpreter);
  }

  std::vector<BytecodeProc> parse(const std::string& s);

}

#endif
