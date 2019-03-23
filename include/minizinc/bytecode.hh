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
      SIMPLIFY_LIN, // R1 -> R2, R3, R4: simplify linear expression in R1, return coefficients (R2), variables (R3), constant (R4)
      
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

  class RefCountedObject {
  public:
    enum RCOType { VEC, DEF };
  protected:
    unsigned int _ref_count;
    unsigned int _cse_ref_count : 31;
    unsigned int _rco_type : 1;
    int _timestamp;
    RefCountedObject(const RCOType& t, int timestamp) : _ref_count(0), _cse_ref_count(0), _rco_type(t==VEC ? 1 : 0), _timestamp(timestamp) {}
  public:
    RCOType rcoType(void) const { return _rco_type==1 ? VEC : DEF; }
    const int timestamp() const { return _timestamp; }

    void addRef(Interpreter* interpreter) { _ref_count++; }
    static void rmRef(Interpreter* interpreter, RefCountedObject* rco);
    bool exists() { return _ref_count > 0; }

    void addCSE(Interpreter* interpreter) { assert(_ref_count > 0); _cse_ref_count++; }
    static void rmCSE(Interpreter* interpreter, RefCountedObject* rco);
    bool inCSE() { return _cse_ref_count > 0; }
  };
  

  /// Value tagged union
  class Val {
    friend class WeakVal;
  protected:
    /// The value
    // Bit 0: 0=int, 1=RefCountedObject
    void* _v;
  public:
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

    void follow_aliases(Interpreter*);

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
    void addToCSE(Interpreter* interpreter) {
      if (isRCO()) {
        toRCO()->addCSE(interpreter);
      }
    };
    void removeFromCSE(Interpreter* interpreter) {
      if (isRCO()) {
        RefCountedObject::rmCSE(interpreter, toRCO());
      }
    };

    /// Access value as vector, return element \a i
    const Val& operator [](int i) const;
    /// Access value as vector, return size
    size_t size(void) const;
  protected:
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
    Expression* const toFZN(const std::map<int, VarDecl*>& vdmap = {}) {
      GCLock lock;
      if (this->isInt()) {
        return IntLit::a((*this)());
      } else if (this->isDef()) {
        auto it = vdmap.find(this->timestamp());
        VarDecl* vd = it != vdmap.end() ? it->second : nullptr;
        return new Id(Location().introduce(), this->timestamp(), vd);
      } else {
        assert(this->isVec());
        std::vector<Expression*> vec(this->size());
        for (int i = 0; i < this->size(); ++i) {
          Val v = (*this)[i];
          vec[i] = v.toFZN(vdmap);
        }
        return new ArrayLit(Location().introduce(), vec);
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

  class CallVal {
  public:
    int pred;
    char mode;
    Val args;
    CallVal(int pred0, char mode0, Val args0) : pred(pred0), mode(mode0), args(args0) {}
  };
  
  class Definition : public RefCountedObject {
  protected:
    int _size;
    Definition* _prev;
    Definition* _next;
    Val _domain;
    Val _ann;
    Definition* _defs;
    int _pred;
    char _mode;
    Val _args[1];
    Definition(Interpreter* interpreter, Val domain,int pred,char mode,const std::vector<Val>& args,int ident,Val ann)
    : RefCountedObject(RefCountedObject::DEF,ident), _size(args.size()), _prev(this), _next(this),
    _domain(domain), _ann(ann), _defs(nullptr), _pred(pred), _mode(mode) {
      _domain.construct(interpreter);
      _ann.construct(interpreter);
      for (unsigned int i=0; i<args.size(); i++) {
        new (&_args[i]) Val(args[i]);
        _args[i].construct(interpreter);
      }
    }
  public:
    Val domain(void) const { return _domain; }
    Val ann(void) const { return _ann; }
    int pred(void) const { return _pred; }
    char mode(void) const { return _mode; }
    int size(void) const { return _size; }
    Val arg(int i) const { assert(i < _size); return _args[i]; }
    Definition* defs(void) const { return _defs; }
    void defs(Definition* defs) { _defs = defs; }
    static Definition* a(Interpreter* interpreter, Val domain,int pred,char mode,const std::vector<Val>& args,int ident,Val ann=IntVal(0)) {
      Definition* d = static_cast<Definition*>(::malloc(sizeof(Definition)+sizeof(Val)*(std::max(0,static_cast<int>(args.size())-1))));
      new (d) Definition(interpreter,domain,pred,mode,args,ident,ann);
      return d;
    }
    static void free(Definition* def) {
      // INVARIANT: def->destroy() should be called before free(def);
      assert(def->_ref_count == 0 && def->_cse_ref_count == 0);
      if (def->_defs) {
        // destroy all linked definitions
        Definition* d = def->_defs;
        bool finished = false;
        while (!finished) {
          Definition* cur = d;
          d = d->next();
          finished = (cur == d);
          // INVARIANT: def->destroy() should ensure that no children with reference counts are still linked
          assert(cur->_ref_count == 0 && cur->_cse_ref_count == 0);
          Definition::free(cur);
        }
      }
      ::free(def);
    }
    ~Definition(void) = delete;
    /// Destroy and unlink this definition
    void destroy(Interpreter* interpreter);
    void reconstruct(Interpreter* interpreter) {
      assert(_ref_count == 0);
      for (int i = 0; i < _size; ++i) {
        _args[i].construct(interpreter);
      }
      _ann.construct(interpreter);
      _domain.construct(interpreter);
      _ref_count = 0;
    }
    /// Insert singleton element into list before \a d
    void insertBefore(Interpreter* interpreter, Definition* d);
    /// Append list to other list before \a d
    void appendBefore(Interpreter* interpreter, Definition* d);
    void unlink(Interpreter* interpreter);
    /// Set the reference count to 1
    void makeUniqueReference(void) { _ref_count = 1; }
    void alias(Interpreter* interpreter, Val v);
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
    static void dump(Definition* d, const std::vector<BytecodeProc>& bs, std::ostream& os, bool ignoreHead=true, int indent=0);
    static Model* toFZN(Definition* d, const std::vector<BytecodeProc>& bs, bool ignoreHead = true, Model* model = nullptr);
  };

  class WeakVal {
  protected:
    // Value of the Val
    void* _v;
  public:
    explicit WeakVal(const Val& val) {
      if(val.isRCO()) {
        _v = reinterpret_cast<void*>(static_cast<ptrdiff_t>(val.toRCO()->timestamp()) | static_cast<ptrdiff_t>(1));
      } else {
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
    Val toVec(Interpreter* interpreter, int timestamp) const {
      return Val(Vec::a(interpreter,timestamp,stack));
    }
    /// Destroy stack values
    void destroyStack(Interpreter* interpreter) {
      int i=0;
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
    /// Modes
    enum Mode { RAW, ROOT, ROOT_NEG, FUN, FUN_NEG, IMP, IMP_NEG, MAX_MODE=IMP_NEG };
    static const std::string mode_to_string[MAX_MODE+1];
    static const bool is_neg(const Mode& mode) { return mode == ROOT_NEG || mode == FUN_NEG || mode == IMP_NEG; }
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
          item.second.second.removeFromCSE(interpreter);
        }
      }
      _table = std::vector<impl>(1);
    }
    void push() { _table.emplace_back(); }
    void pop(Interpreter* interpreter) {
      for (auto& item : _table.back()) {
        item.first.destroy();
        item.second.second.removeFromCSE(interpreter);
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


  class PrimitiveMap {
  public:
    enum Id {
      ALIAS,
      BOOLNOT,
      CLAUSE,
      FORALL,
      EXISTS,
      INT_SUM,
      INT_TIMES,
      LINEXP
    };
    struct Primitive {
      Id ident;
      int n_args;
      Primitive(void) {}
      Primitive(const Id& ident0, int n_args0) : ident(ident0), n_args(n_args0) {}
    };
    static const Primitive ALL[];
  protected:
    std::unordered_map<std::string,Primitive> _s;
    std::vector<std::string> _n;
  public:
    PrimitiveMap(void);
    Primitive operator [](const std::string& s) { return _s[s]; }
    std::string operator [](Primitive p) { return _n[p.ident]; }
    int size(void) const { return _n.size(); }
  };

  class Trail {
  protected:
    std::vector<std::pair<Definition**, Definition*>> hedge_trail;
    std::vector<RefCountedObject*> obj_trail;
    // <Obj trail size, Hedge trail size>
    std::vector<std::pair<size_t, size_t>> trail_size;
    std::vector<int> timestamp_trail;
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
    bool is_trailed(RefCountedObject* rco) { return (!trail_size.empty() && timestamp_trail.back() > rco->timestamp()); }

    // Trail hedge pointer change
    inline bool operator() (Definition* def, Definition** member) {
      if (trail_size.empty() || timestamp_trail.back() <= def->timestamp()) {
        return false;
      }
      hedge_trail.emplace_back(member, *member);
      return true;
    }
    // Trail Reference Counted Object removal
    inline bool operator() (RefCountedObject* obj) {
      if (trail_size.empty() || timestamp_trail.back() <= obj->timestamp()) {
        return false;
      }
      obj_trail.push_back(obj);
      return true;
    }
    size_t save_state(Interpreter* interpreter);
    void untrail(Interpreter* interpreter);
  };
  
  class Interpreter {
    friend class Trail;
  public:
    typedef void (*builtin) (Interpreter& i, std::vector<Val> args);
  protected:
    std::vector<BytecodeFrame> _stack;
    std::vector<AggregationCtx> _agg;
    std::vector<BytecodeProc>& _procs;
    const std::vector<builtin>& _builtins;
    int _identCount;
    std::vector<CSETable> cse;
  public:
    Trail trail;

    Interpreter(std::vector<BytecodeProc>& procs,
                const std::vector<builtin>& builtins,
                const BytecodeFrame& f) : _procs(procs), _builtins(builtins), _identCount(0), cse(procs.size())
    {
      _stack.push_back(f);
    }
    ~Interpreter(void);
    void run(void);
    void pushAgg(const Val& v, int stackOffset);
    void pushDef(Definition* d);
    std::pair<Val, bool> cse_lookup(int proc, const CSETable::Key& key, BytecodeProc::Mode& mode) {
      return cse[proc].lookup(this, key, mode);
    }
    void cse_insert(int proc, CSETable::Key& key, BytecodeProc::Mode& mode, Val& val) {
      return cse[proc].insert(this, key, mode, val);
    }
    int newIdent(void) { return _identCount++; }
    int currentIdent(void) const { return _identCount; }
    void dumpState(std::ostream& os);
    Model* toFZN();
    void call(int code, const BytecodeProc::Mode& mode, const std::vector<Val>& args);
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
        interpreter->trail(rco);
      } else if (rco->_cse_ref_count==0) {
        // INVARIANT: All children of a definition are already promoted, cut, or freed.
        assert(rco->rcoType() != DEF || !static_cast<Definition*>(rco)->defs());
        free(rco);
      }
    }
  }
  inline
  void RefCountedObject::rmCSE(Interpreter* interpreter, RefCountedObject* rco) {
    if(--rco->_cse_ref_count == 0 && !interpreter->trail.is_trailed(rco) && rco->_ref_count == 0) {
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
    def_stack(Definition::a(interpreter,IntVal(0),0,0,{},-1)),
    def_ident_start(interpreter->currentIdent()),
    symbol(static_cast<Symbol>(s)), n_symbols(1) {
    assert(s >= 0 && s <= VCTX_OTHER);
    def_stack->addRef(interpreter);
  }

  std::vector<BytecodeProc> parse(const std::string& s);

}

#endif
