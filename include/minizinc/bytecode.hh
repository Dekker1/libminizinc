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

#include <vector>
#include <iostream>

#include <minizinc/values.hh>
#include <minizinc/ast.hh>

namespace MiniZinc {

  class BytecodeProc;
  class Interpreter;

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
      
      ISPAR,   // R: whether value in R is not a variable
      ISEMPTY, // R: whether vector in R is empty
      LENGTH,  // R1 -> R2: put length of vector in R1 into R2
      GET_VEC, // R1, R2 -> R3: put element R2 of vector in R1 into R3
      
      OPEN_AGGREGATION, // i: Create a new aggregation context with symbol i
      CLOSE_AGGREGATION,  // Close current aggregation context, put result onto context above
      
      PUSH,  // R: push R onto value stack
      POP,   // R: pop from value stack into R
      
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

  /// Value tagged union
  class Val {
    friend class WeakVal;
  protected:
    /// The value
    // Bit 0,1: 0,X=int, 1,0=Ref, 1,1=Vec
    void* _v;
  public:
    bool isVec(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(3)) == static_cast<ptrdiff_t>(3);
    }
    bool isInt(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(1)) == 0;
    }
    bool isDef(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(3)) == static_cast<ptrdiff_t>(1);
    }
    bool operator==(const Val& rhs) const;

    /// Access value as Definition
    Definition* toDef(void) const {
      assert(isDef());
      return reinterpret_cast<Definition*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(1));
    }
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

    void destroy(Interpreter* interpreter);
    void construct(Interpreter* interpreter);

    /// Access value as vector, return element \a i
    const Val& operator [](int i) const;
    /// Access value as vector, return size
    size_t size(void) const;
  protected:
    Vec* toVec(void) const {
      assert(isVec());
      return reinterpret_cast<Vec*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3));
    }
  public:
    explicit Val(Vec* v);
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
    explicit Val(Definition* d) {
      assert(d != NULL);
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
  };
  
  class Vec {
  protected:
    int _size;
    unsigned int _ref_count : 31;
    unsigned int _in_cse : 1;
    Val _data[1];
    Vec(Interpreter* interpreter, const std::vector<Val>& v) : _size(v.size()), _ref_count(0), _in_cse(0) {
      for (unsigned int i=0; i<v.size(); i++) {
        new (&_data[i]) Val(v[i]);
        _data[i].construct(interpreter);
      }
    }
    ~Vec(void) = delete;
  public:
    int size(void) const { return _size; }
    bool isInCSE(void) const { return _in_cse; }
    void addToCSE(void) { _in_cse = 1; }
    const Val& operator [](int i) const { assert(i >= 0 && i<_size); return _data[i]; }
    static Vec* a(Interpreter* interpreter, const std::vector<Val>& v) {
      Vec* nv = static_cast<Vec*>(::malloc(sizeof(Vec)+sizeof(Val)*(v.size()-1)));
      new (nv) Vec(interpreter,v);
      return nv;
    }
    void inc(void) { _ref_count++; }
    static void dec(Interpreter* interpreter, Vec* v) {
      if ( v && (--v->_ref_count)==0 ) {
        for (unsigned int i=0; i<v->size(); i++) {
          v->_data[i].destroy(interpreter);
        }
        free(v);
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

  class WeakVal {
  protected:
    // Value of the Val
    void* _v;
  public:
    WeakVal(const Val& val) : _v(val._v) {}
    Val to_val() { Val v; v._v = _v; return v; }

    size_t hash() const {std::hash<void*> h; return h(_v);}
    inline bool operator==(const WeakVal& rhs) const { return reinterpret_cast<ptrdiff_t>(_v) == reinterpret_cast<ptrdiff_t>(rhs._v); }

    static std::vector<WeakVal> flat_vector(const std::vector<Val>& vec);
  };
  
  
  inline
  Val::Val(Vec* v) {
    assert(v != NULL);
    _v = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(v) | static_cast<ptrdiff_t>(3));
  }
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
    return (*reinterpret_cast<Vec*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3)))[i];
  }
  /// Access value as vector, return size
  inline
  size_t Val::size(void) const {
    assert(isVec());
    return reinterpret_cast<Vec*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3))->size();
  }

  
  class RegisterFile {
  protected:
    std::vector<Val> _r;
  public:
    RegisterFile(int n=0) : _r(n) {}
    const Val& operator [](int r) { assert(r < _r.size()); return _r[r]; }
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
  };

  class CallVal {
  public:
    int pred;
    char mode;
    Val args;
    CallVal(int pred0, char mode0, Val args0) : pred(pred0), mode(mode0), args(args0) {}
  };
  
  class Definition {
  protected:
    Definition* _prev;
    Definition* _next;
    unsigned int _ref_count : 31;
    unsigned int _in_cse : 1;
    const int _ident;
  public:
    Val domain;
    Val ann;
    CallVal call;
    Definition* defs;
    Definition(Interpreter* interpreter, Val domain0,int pred0,char mode0,Val args0,int ident,Val ann0=IntVal(0))
    : _prev(this), _next(this), _ref_count(0), _in_cse(0), _ident(ident),
      domain(domain0), ann(ann0), call(CallVal(pred0,mode0,args0)), defs(nullptr) {
      domain.construct(interpreter);
      ann.construct(interpreter);
      call.args.construct(interpreter);
    }
    /// Destroy and unlink this definition
    void destroy(Interpreter* interpreter) {
      domain.destroy(interpreter);
      ann.destroy(interpreter);
      call.args.destroy(interpreter);
      _prev->_next = _next;
      _next->_prev = _prev;
    }
    /// Insert singleton element into list before \a d
    void insertBefore(Definition* d) {
      assert(_prev==_next);
      _prev = d->_prev;
      _next = d;
      d->_prev->_next = this;
      d->_prev = this;
    }
    /// Append list to other list before \a d
    void appendBefore(Definition* d) {
      Definition* e = d->_prev;      
      d->_prev = this;
      _next = d;
      e->_next = this;
      _prev = e;
    }
    void unlink(void) {
      _prev->_next = _next;
      _next->_prev = _prev;
      _next = this;
      _prev = this;
    }
    void inc(Interpreter* interpreter) { _ref_count++; }
    static void dec(Interpreter* interpreter, Definition* d) {
      if (--d->_ref_count==0) {
        d->destroy(interpreter);
        delete d;
      }
    }
    Definition* prev(void) const { return _prev; }
    Definition* next(void) const { return _next; }
    bool isInCSE(void) const { return _in_cse; }
    void addToCSE(void) { _in_cse = 1; }
    int ident(void) const { return _ident; }
    int listSize(void) const {
      int i=1;
      if (_next != this) {
        for (Definition* d = _next; d != this; d = d->next()) {
          i++;
        }
      }
      return i;
    }
    static void dump(Definition* d, const std::vector<BytecodeProc>& bs, std::ostream& os, bool ignoreHead=true);
  };
  
  class AggregationCtx {
  protected:
    /// Stack of values that need to be aggregated
    std::vector<Val> stack;
  public:
    /// Type of function represented by this context
    enum Symbol { VCTX_AND, VCTX_OR, VCTX_LIN, VCTX_VEC, VCTX_OTHER } symbol;
    /// Nesting depth for this symbol (how many of these are open)
    int n_symbols;
    /// Depth of definition stack when this frame was created
    Definition* def_stack_top;
    /// Constructor
    AggregationCtx(Interpreter* interpreter, int s, Definition* t) : symbol(static_cast<Symbol>(s)), n_symbols(1), def_stack_top(t) {
      assert(s >= 0 && s <= VCTX_OTHER);
      def_stack_top->inc(interpreter);
    }
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
    Val toVec(Interpreter* interpreter) const {
      return Val(Vec::a(interpreter,stack));
    }
    /// Close this context
    void destroy(Interpreter* interpreter) {
      for (auto& v : stack) {
        v.destroy(interpreter);
      }
      Definition::dec(interpreter,def_stack_top);
    }
  };

  struct CSEHasher {
    size_t operator()(const std::vector<WeakVal>& v) const;
  };
  
  class BytecodeProc {
  public:
    /// The name of this procedure
    std::string name;
    /// Number of arguments
    int nargs;
    /// Modes
    enum Mode { RAW, ROOT, ROOT_NEG, FUN, FUN_NEG, IMP, IMP_NEG, MAX_MODE=IMP_NEG };
    static const std::string mode_to_string[MAX_MODE+1];
    static const bool is_neg(const Mode& mode) { return mode == ROOT_NEG || mode == FUN_NEG || mode == IMP_NEG; }
    /// The code for different modes
    BytecodeStream mode[MAX_MODE+1];

    /// CSE table: Saved results of historical executions
    class CSETable {
    public:
      typedef std::unordered_map<std::vector<WeakVal>, std::pair<Mode, WeakVal>, CSEHasher> hashtable;
      typedef hashtable::iterator iterator;
      std::pair<Val, bool> lookup(Interpreter& interpreter, const std::vector<WeakVal>& key, const BytecodeProc::Mode& mode);
      void insert(std::vector<WeakVal>& key, const BytecodeProc::Mode& mode, const Val& val);
    protected:
      hashtable _table;
    } cse;
  };

  class BytecodeFrame {
  public:
    RegisterFile reg;
    const BytecodeStream* bs;
    int pc;
    Definition* def_stack;
    
    // CSE information for RET statement
    // <proc, mode, cse_key, stack size>
    typedef std::tuple<int,BytecodeProc::Mode, std::vector<WeakVal>, size_t> CSEInfo;
    std::vector<CSEInfo> cse_info;

    BytecodeFrame(const BytecodeStream& bs0) : reg(bs0.maxRegister()), bs(&bs0), pc(0), def_stack(new Definition(nullptr,IntVal(0),0,0,IntVal(0),-1)) {
      def_stack->inc(nullptr);
    }
    void destroy(Interpreter* interpreter) {
      reg.destroy(interpreter);
      Definition::dec(interpreter, def_stack);
    }
  };


  class PrimitiveMap {
  public:
    enum Primitive {
      BOOLNOT,
      CLAUSE,
      FORALL,
      LINEXP
    };
    static const Primitive ALL[];
  protected:
    std::unordered_map<std::string,Primitive> _s;
    std::vector<std::string> _n;
  public:
    PrimitiveMap(void);
    Primitive operator [](const std::string& s) { return _s[s]; }
    std::string operator [](Primitive p) { return _n[p]; }
  };
  
  class Interpreter {
    friend class BytecodeProc::CSETable;
  public:
    typedef void (*builtin) (Interpreter& i, std::vector<Val> args);
  protected:
    std::vector<BytecodeFrame> _stack;
    std::vector<AggregationCtx> _agg;
    std::vector<BytecodeProc>& _procs;
    const std::vector<builtin>& _builtins;
    int _identCount;
  public:
    Interpreter(std::vector<BytecodeProc>& procs,
                const std::vector<builtin>& builtins,
                const BytecodeFrame& f) : _procs(procs), _builtins(builtins), _identCount(0)
    {
      _stack.push_back(f);
    }
    ~Interpreter(void);
    void run(void);
    void pushAgg(const Val& v, int stackOffset);
    void pushDef(BytecodeFrame* frame, Definition* d);
    int newIdent(void) { return _identCount++; }
    int currentIdent(void) const { return _identCount; }
  };

  inline
  void Val::construct(Interpreter* interpreter) {
    if (isVec()) {
      toVec()->inc();
    } else if (isDef()) {
      toDef()->inc(interpreter);
    }
  }
  inline
  void Val::destroy(Interpreter* interpreter) {
    if (isVec()) {
      Vec::dec(interpreter,toVec());
    } else if (isDef()) {
      Definition::dec(interpreter,toDef());
    }
  }

  std::vector<BytecodeProc> parse(const std::string& s);

}

#endif
