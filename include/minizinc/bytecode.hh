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
  class WeakVal;

  class Ref {
  protected:
    int _r;
  public:
    explicit Ref(int r) : _r(r) {}
    int operator ()(void) const { return _r; }
  };
  
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
    bool isRef(void) const {
      return (reinterpret_cast<ptrdiff_t>(_v) & static_cast<ptrdiff_t>(3)) == static_cast<ptrdiff_t>(1);
    }
    bool operator==(const Val& rhs) const;

    /// Access value as Ref
    Ref r(void) const {
      assert(isRef());
      return Ref(reinterpret_cast<ptrdiff_t>(_v) >> 2);
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

    /// Access value as vector, return element \a i
    const Val& operator [](int i) const;
    /// Access value as vector, return size
    size_t size(void) const;
  protected:
    Vec* toVec(void) const {
      assert(isVec());
      return reinterpret_cast<Vec*>(reinterpret_cast<ptrdiff_t>(_v) & ~static_cast<ptrdiff_t>(3));
    }
    void destroy(void);
    void construct(void);
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
    Val(const Ref& r) {
      ptrdiff_t ubi_p = (static_cast<ptrdiff_t>(r()) << 2) | static_cast<ptrdiff_t>(1);
      _v = reinterpret_cast<void*>(ubi_p);
    }
    ~Val(void);
    Val(const Val& v);
    Val(Val&& v);
    Val& operator =(const Val& v);
    Val& operator =(Val&& v);
    std::string toString(void) const;
  };
  
  class Vec {
  protected:
    size_t _size;
    int _ref_count;
    Val _data[1];
    Vec(const std::vector<Val>& v) : _size(v.size()), _ref_count(0) {
      for (unsigned int i=0; i<v.size(); i++) {
        new (&_data[i]) Val(v[i]);
      }
    }
  public:
    size_t size(void) const { return _size; }
    const Val& operator [](int i) const { assert(i >= 0 && i<_size); return _data[i]; }
    static Vec* a(const std::vector<Val>& v) {
      Vec* nv = static_cast<Vec*>(::malloc(sizeof(Vec)+sizeof(Val)*(v.size()-1)));
      new (nv) Vec(v);
      return nv;
    }
    void inc(void) { _ref_count++; }
    static void dec(Vec* v) {
      if ( v && (--v->_ref_count)==0 ) {
        for (unsigned int i=0; i<v->size(); i++) {
          (*v)[i].~Val();
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
    v->inc();
  }
  inline
  void Val::construct() {
    if (isVec()) {
      toVec()->inc();
    }
  }
  inline
  void Val::destroy() {
    if (isVec()) {
      Vec::dec(toVec());
    }
  }
  inline
  Val::~Val(void) { destroy(); }
  inline
  Val::Val(const Val& v) : _v(v._v) { construct(); }
  inline
  Val::Val(Val&& v) : _v(v._v) { v._v = nullptr; }
  inline
  Val& Val::operator =(const Val& v) {
    if (this != &v) {
      destroy();
      _v = v._v;
      construct();
    }
    return *this;
  }
  inline
  Val& Val::operator =(Val&& v) {
    if (this != &v) {
      destroy();
      _v = v._v;
      v._v = nullptr;
    }
    return *this;
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
    void assign(int r, const Val& v) {
      if (r >= _r.size()) {
        _r.resize(r+1);
        assert(_r.size()==r+1);
      }
      _r[r] = v;
    }
    
    void cp(int r1, int r2) {
      assert(r1 < _r.size());
      if (r2 >= _r.size()) _r.resize(r2+1);
      _r[r2] = _r[r1];
    }
    void cp(int r1, RegisterFile& rf, int r2) {
      assert(r1 < _r.size());
      if (r2 >= rf._r.size()) rf._r.resize(r2+1);
      rf._r[r2] = _r[r1];
    }
    void mov(int r1, RegisterFile& rf, int r2) {
      assert(r1 < _r.size());
      if (r2 >= rf._r.size()) rf._r.resize(r2+1);
      rf._r[r2] = std::move(_r[r1]);
    }
    void mov(std::vector<Val>& args) {
      _r = std::move(args);
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
  public:
    Val domain;
    Val ann;
    CallVal call;
    Definition(void) : domain(IntVal(0)), ann(IntVal(0)), call(CallVal(0,0,IntVal(0))) {}
    Definition(Val domain0,int pred0,char mode0,Val args0,Val ann0=IntVal(0))
    : domain(domain0), ann(ann0), call(CallVal(pred0,mode0,args0)) {}
  };
  
  class AggregationCtx {
  public:
    /// Type of function represented by this context
    enum Symbol { VCTX_AND, VCTX_OR, VCTX_LIN, VCTX_VEC, VCTX_OTHER } symbol;
    /// Nesting depth for this symbol (how many of these are open)
    int n_symbols;
    /// Stack of values that need to be aggregated
    std::vector<Val> stack;
    /// Depth of definition stack when this frame was created
    int def_stack_depth;
    AggregationCtx(int s, int d) : symbol(static_cast<Symbol>(s)), n_symbols(1), def_stack_depth(d) {
      assert(s >= 0 && s <= VCTX_OTHER);
    }
  };

  struct CSEHasher {
    size_t operator()(const std::vector<WeakVal>& v) const;
  };
  
  class BytecodeProc {
  public:
    /// The name of this procedure
    std::string name;
    /// Modes
    enum Mode { RAW, ROOT, ROOT_NEG, FUN, FUN_NEG, IMP, IMP_NEG, MAX_MODE=IMP_NEG };
    static const std::string mode_to_string[MAX_MODE+1];
    /// The code for different modes
    BytecodeStream mode[MAX_MODE+1];

    /// CSE table: Saved results of historical executions
    class CSETable {
    public:
      typedef std::unordered_map<std::vector<WeakVal>, std::pair<Mode, WeakVal>, CSEHasher> hashtable;
      typedef hashtable::iterator iterator;
      std::pair<Val, bool> lookup(const std::vector<WeakVal>& key, const BytecodeProc::Mode& mode);
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
    // Procedure information
    int proc_code;
    BytecodeProc::Mode proc_mode;
    // CSE information for RET statement
    std::vector<WeakVal> cse_key;
    size_t stack_size;
    BytecodeFrame(const BytecodeStream& bs0, int procedure, BytecodeProc::Mode mode)
    : reg(bs0.maxRegister()), bs(&bs0), pc(0), proc_code(procedure), proc_mode(mode) {}
  };


  class PrimitiveMap {
  public:
    enum Primitive {
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
  public:
    typedef void (*builtin) (Interpreter& i, std::vector<Val> args);
  protected:
    std::vector<BytecodeFrame> _stack;
    std::vector<Definition> _defstack;
    std::vector<AggregationCtx> _agg;
    std::vector<BytecodeProc>& _procs;
    const std::vector<builtin>& _builtins;
  public:
    Interpreter(std::vector<BytecodeProc>& procs,
                const std::vector<builtin>& builtins,
                const BytecodeFrame& f) : _procs(procs), _builtins(builtins) {
      _stack.push_back(f);
    }
    void run(void);
    const std::vector<Definition>& defStack(void) const { return _defstack; }
    void push(const Val& v, int stackOffset);
  };
  
  std::vector<BytecodeProc> parse(const std::string& s);

}

#endif
