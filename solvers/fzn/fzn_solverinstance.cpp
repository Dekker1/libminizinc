/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Guido Tack <guido.tack@monash.edu>
 */

 /* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define NOMINMAX     // Need this before all (implicit) include's of Windows.h
#endif

#include <minizinc/solvers/fzn_solverinstance.hh>
#include <cstdio>
#include <fstream>

#include <minizinc/timer.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/pathfileprinter.hh>
#include <minizinc/parser.hh>
#include <minizinc/typecheck.hh>
#include <minizinc/builtins.hh>
#include <minizinc/eval_par.hh>
#include <minizinc/process.hh>

#ifdef _WIN32
#undef ERROR
#endif

using namespace std;

namespace MiniZinc {
  
  FZN_SolverFactory::FZN_SolverFactory(void) {
    SolverConfig sc("org.minizinc.mzn-fzn",MZN_VERSION_MAJOR "." MZN_VERSION_MINOR "." MZN_VERSION_PATCH);
    sc.name("Generic FlatZinc driver");
    sc.mznlibVersion(1);
    sc.description("MiniZinc generic FlatZinc solver plugin");
    sc.requiredFlags({"--fzn-cmd"});
    sc.stdFlags({"-a","-n","-f","-p","-s","-r","-v"});
    sc.tags({"__internal__"});
    SolverConfigs::registerBuiltinSolver(sc);
  }
  
  string FZN_SolverFactory::getDescription(SolverInstanceBase::Options*)  {
    string v = "FZN solver plugin, compiled  " __DATE__ "  " __TIME__;
    return v;
  }


  string FZN_SolverFactory::getVersion(SolverInstanceBase::Options*) {
    return MZN_VERSION_MAJOR;
  }

  string FZN_SolverFactory::getId()
  {
    return "org.minizinc.mzn-fzn";
  }
  
  void FZN_SolverFactory::printHelp(ostream& os)
  {
    os
    << "MZN-FZN plugin options:" << std::endl
    << "  --fzn-cmd , --flatzinc-cmd <exe>\n     the backend solver filename.\n"
    << "  -b, --backend, --solver-backend <be>\n     the backend codename. Currently passed to the solver.\n"
    << "  --fzn-flags <options>, --flatzinc-flags <options>\n     Specify option to be passed to the FlatZinc interpreter.\n"
    << "  --fzn-flag <option>, --flatzinc-flag <option>\n     As above, but for a single option string that need to be quoted in a shell.\n"
    << "  -n <n>, --num-solutions <n>\n     An upper bound on the number of solutions to output. The default should be 1.\n"
    << "  -t <ms>, --solver-time-limit <ms>, --fzn-time-limit <ms>\n     Set time limit (in milliseconds) for solving.\n"
    << "  --fzn-sigint\n     Send SIGINT instead of SIGTERM.\n"
    << "  -a, --all, --all-solns, --all-solutions\n     Print all solutions.\n"
    << "  -p <n>, --parallel <n>\n     Use <n> threads during search. The default is solver-dependent.\n"
    << "  -k, --keep-files\n     For compatibility only: to produce .ozn and .fzn, use mzn2fzn\n"
                           "     or <this_exe> --fzn ..., --ozn ...\n"
    << "  -r <n>, --seed <n>, --random-seed <n>\n     For compatibility only: use solver flags instead.\n"
    ;
  }

  SolverInstanceBase::Options* FZN_SolverFactory::createOptions(void) {
    return new FZNSolverOptions;
  }

  SolverInstanceBase* FZN_SolverFactory::doCreateSI(std::ostream& log, SolverInstanceBase::Options* opt) {
    return new FZNSolverInstance(log, opt);
  }

  bool FZN_SolverFactory::processOption(SolverInstanceBase::Options* opt, int& i, std::vector<std::string>& argv)
  {
    FZNSolverOptions& _opt = static_cast<FZNSolverOptions&>(*opt);
    CLOParser cop( i, argv );
    string buffer;
    int nn=-1;
    
    if ( cop.getOption( "--fzn-cmd --flatzinc-cmd", &buffer) ) {
      _opt.fzn_solver = buffer;
    } else if ( cop.getOption( "-b --backend --solver-backend", &buffer) ) {
      _opt.backend = buffer;
    } else if ( cop.getOption( "--fzn-flags --flatzinc-flags", &buffer) ) {
      std::vector<std::string> cmdLine = FileUtils::parseCmdLine(buffer);
      for (auto& s : cmdLine) {
        _opt.fzn_flags.push_back(s);
      }
    } else if ( cop.getOption( "-t --solver-time-limit --fzn-time-limit", &nn) ) {
      _opt.fzn_time_limit_ms = nn;
      if (_opt.supports_t) {
        _opt.solver_time_limit_ms = nn;
        _opt.fzn_time_limit_ms += 1000; // kill 1 second after solver should have stopped
      }
    } else if ( cop.getOption( "--fzn-sigint") ) {
      _opt.fzn_sigint = true;
    } else if ( cop.getOption( "--fzn-needs-paths") ) {
      _opt.fzn_needs_paths = true;
    } else if ( cop.getOption( "--fzn-output-passthrough") ) {
      _opt.fzn_output_passthrough = true;
    } else if ( cop.getOption( "--fzn-flag --flatzinc-flag", &buffer) ) {
      _opt.fzn_flags.push_back(buffer);
    } else if ( _opt.supports_n && cop.getOption( "-n --num-solutions", &nn) ) {
      _opt.numSols = nn;
    } else if ( _opt.supports_a && cop.getOption( "-a --all --all-solns --all-solutions") ) {
      _opt.allSols = true;
    } else if ( cop.getOption( "-p --parallel", &nn) ) {
      if (_opt.supports_p)
        _opt.parallel = to_string(nn);
    } else if ( cop.getOption( "-k --keep-files" ) ) {
    } else if ( cop.getOption( "-r --seed --random-seed", &buffer) ) {
      if (_opt.supports_r) {
        _opt.fzn_flags.push_back("-r");
        _opt.fzn_flags.push_back(buffer);
      }
    } else if ( cop.getOption( "-s --solver-statistics") ) {
      if (_opt.supports_s) {
        _opt.printStatistics = true;
      }
    } else if ( cop.getOption( "-v --verbose-solving") ) {
      _opt.verbose = true;
    } else if ( cop.getOption( "-f --free-search") ) {
      if (_opt.supports_f)
        _opt.fzn_flags.push_back("-f");
    } else {
      for (auto& fznf : _opt.fzn_solver_flags) {
        if (fznf.t==MZNFZNSolverFlag::FT_ARG && cop.getOption(fznf.n.c_str(), &buffer)) {
          _opt.fzn_flags.push_back(fznf.n);
          _opt.fzn_flags.push_back(buffer);
          return true;
        } else if (fznf.t==MZNFZNSolverFlag::FT_NOARG && cop.getOption(fznf.n.c_str())) {
          _opt.fzn_flags.push_back(fznf.n);
          return true;
        }
      }

      return false;
    }
    return true;
  }
  
  void FZN_SolverFactory::setAcceptedFlags(SolverInstanceBase::Options* opt, const std::vector<MZNFZNSolverFlag>& flags) {
    FZNSolverOptions& _opt = static_cast<FZNSolverOptions&>(*opt);
    _opt.fzn_solver_flags.clear();
    for (auto& f : flags) {
      if (f.n=="-a") {
        _opt.supports_a = true;
      } else if (f.n=="-n") {
        _opt.supports_n = true;
      } else if (f.n=="-f") {
        _opt.supports_f = true;
      } else if (f.n=="-p") {
        _opt.supports_p = true;
      } else if (f.n=="-s") {
        _opt.supports_s = true;
      } else if (f.n=="-r") {
        _opt.supports_r = true;
      } else if (f.n=="-v") {
        _opt.supports_v = true;
      } else if (f.n=="-t") {
        _opt.supports_t = true;
      } else {
        _opt.fzn_solver_flags.push_back(f);
      }
    }
  }


  FZNSolverInstance::FZNSolverInstance(std::ostream& log, SolverInstanceBase::Options* options)
    : SolverInstanceBase(log, options), _model(new Model()), env(_model) {
    auto ofs = new std::ofstream("/dev/null");
    pS2Out = new Solns2Out(*ofs, log, "");
  }

  FZNSolverInstance::~FZNSolverInstance(void) {}

  void FZNSolverInstance::addFunction(FunctionI* fi) {
    _model->addItem(fi);
    _model->registerFn(env.envi(), fi);
  }

  Expression* FZNSolverInstance::val_to_expr(Type ty, Val v) {
    v = Val::follow_alias(v);
    if (v.isInt()) {
      if (ty.isbool()) {
        return v() == 0 ? constants().lit_true : constants().lit_false;
      }
      return IntLit::a(v());
    } else if (v.isVar()) {
      auto it = vdmap.find(v.timestamp());
      assert(it != vdmap.end());
      VarStore& vs = it->second;
      if (ty.isbool()) {
        if (vs.used_bool) {
          return vs.bool_var()->cast<VarDecl>()->id();
        }
        vs.used_bool = true;

        auto ti = new TypeInst(Location().introduce(), Type::varbool());
        if (vs.used_int) {
          vs.bool_var = new VarDecl(Location().introduce(), ti, "view_" + std::to_string(v.timestamp()));
        } else {
          vs.bool_var = new VarDecl(Location().introduce(), ti, v.timestamp());
        }
        auto vdi = new VarDeclI(Location().introduce(), vs.bool_var()->cast<VarDecl>());
        _model->addItem(vdi);

        if (vs.used_int) {
          _model->addItem(new ConstraintI(Location().introduce(), new Call(Location().introduce(), constants().ids.bool2int, {vs.bool_var()->cast<VarDecl>()->id(), vs.int_var()->cast<VarDecl>()->id()})));
        } else {
          auto output_ti = new TypeInst(Location().introduce(), Type::parbool());
          auto output_vd = new VarDecl(Location().introduce(), output_ti, v.timestamp());
          env.output()->addItem(new VarDeclI(Location().introduce(), output_vd));
        }

        return vs.bool_var()->cast<VarDecl>()->id();
      }
      if (vs.used_int) {
        return vs.int_var()->cast<VarDecl>()->id();
      }
      vs.used_int = true;

      SetLit* dom_set = new SetLit(Location().introduce(), IntSetVal::a(0,1));
      auto ti = new TypeInst(Location().introduce(), Type::varint(), dom_set);
      if (vs.used_bool) {
        vs.int_var = new VarDecl(Location().introduce(), ti, "view_" + std::to_string(v.timestamp()));
      } else {
        vs.int_var = new VarDecl(Location().introduce(), ti, v.timestamp());
      }
      auto vdi = new VarDeclI(Location().introduce(), vs.int_var()->cast<VarDecl>());
      _model->addItem(vdi);

      if (vs.used_bool) {
        _model->addItem(new ConstraintI(Location().introduce(), new Call(Location().introduce(), constants().ids.bool2int, {vs.bool_var()->cast<VarDecl>()->id(), vs.int_var()->cast<VarDecl>()->id()})));
      } else {
        auto output_ti = new TypeInst(Location().introduce(), Type::parint());
        auto output_vd = new VarDecl(Location().introduce(), output_ti, v.timestamp());
        env.output()->addItem(new VarDeclI(Location().introduce(), output_vd));
      }
      return vs.int_var()->cast<VarDecl>()->id();
    } else {
      // Expected [[actual array], [indexes]]
      assert(v.isVec());
      if (v.size() == 2 && v[0].isVec() && v[1].isVec()) {
        // this is an array
        Val vec = v[0];
        std::vector<Expression*> evec(vec.size());
        bool par = true;
        for (int i = 0; i < vec.size(); ++i) {
          assert(!vec[i].isVec());
          Type nty = ty;
          nty.dim(0);
          evec[i] = val_to_expr(nty, vec[i]);
          par = par && evec[i]->type().ispar();
        }
        auto al = new ArrayLit(Location().introduce(), evec);
        al->type(par ? Type::parint(1) : Type::varint(1));
        assert(v[1].size() == 2 && v[1][0]() == 1 && v[1][1]() == vec.size());
        return al;
      } else {
        // this is a set
        std::vector<IntSetVal::Range> ranges;
        Vec* vec = v.toVec();
        for (int i=0; i<vec->size(); i+=2) {
          ranges.push_back(IntSetVal::Range((*vec)[i](),(*vec)[i+1]()));
        }
        auto sl = new SetLit(Location().introduce(), IntSetVal::a(ranges));
        return sl;
      }
    }
  }

  void FZNSolverInstance::addConstraint(const std::vector<BytecodeProc>& bs, Constraint* c) {
    GCLock lock;
    const BytecodeProc& proc = bs[c->pred()];
    std::string name = proc.name;
    auto fnit = _model->fnmap.find(ASTString(name));
    assert(fnit != _model->fnmap.end());
    assert(fnit->second.size() == 1);
    std::vector<Type>& tys = fnit->second[0].t;
    assert(tys.size() == c->size());
    std::vector<Expression*> args(proc.nargs);
    for (int i = 0; i < proc.nargs; ++i) {
      args[i] = val_to_expr(tys[i], c->arg(i));
    }
    auto call = new Call(Location().introduce(), name, args);
    auto ci = new ConstraintI(Location().introduce(), call);
    _model->addItem(ci);
  };

  void FZNSolverInstance::addVariable(Variable* var) {
    GCLock lock;
    Vec* dom = var->domain();
    assert(dom->size() >= 2 && dom->size() % 2 == 0);

    if (dom->size() == 2 && (*dom)[0]() == 0 && (*dom)[1]() == 1) {
      vdmap.emplace(std::piecewise_construct, std::forward_as_tuple(var->timestamp()), std::forward_as_tuple(nullptr, nullptr, false, false));
    }

    std::vector<IntSetVal::Range> ranges;
    for (int i = 0; i < dom->size(); i += 2) {
      ranges.emplace_back((*dom)[i](), (*dom)[i+1]());
    }
    SetLit* dom_set = new SetLit(Location().introduce(), IntSetVal::a(ranges));
    auto ti = new TypeInst(Location().introduce(), Type::varint(), dom_set);
    auto vd = new VarDecl(Location().introduce(), ti, var->timestamp());
    vd->addAnnotation(constants().ann.output_var);

    auto vdi = new VarDeclI(Location().introduce(), vd);
    _model->addItem(vdi);
    vdmap.emplace(std::piecewise_construct, std::forward_as_tuple(var->timestamp()), std::forward_as_tuple(vd, nullptr, true, false));

    auto output_ti = new TypeInst(Location().introduce(), Type::parint(), nullptr);
    auto output_vd = new VarDecl(Location().introduce(), output_ti, var->timestamp());
    env.output()->addItem(new VarDeclI(Location().introduce(), output_vd));
  }

  Val FZNSolverInstance::getSolutionValue(Variable* var) {
    GCLock lock;
    Id id(Location().introduce(), var->timestamp(), nullptr);
    auto de = getSolns2Out()->findOutputVar(id.str());
    assert(de.first->e()); // A solution must have been assigned
    return Val(eval_int(env.envi(), de.first->e()));
  };

  void FZNSolverInstance::pushState() {
    stack.emplace_back(_model->size());
  }


  void FZNSolverInstance::popState() {
    for(auto i = stack.back(); i < _model->size(); ++i) {
      Item* it = (*_model)[i];
      if (!it->isa<FunctionI>()) {
        it->remove();
      }
    }
    _model->compact();
    stack.pop_back();
  }

  void FZNSolverInstance::createFunctionItems() {
    GCLock lock;
    std::vector<FunctionI*> toAdd;
    for (auto ci = _model->begin_constraints(); ci != _model->end_constraints(); ++ci) {
      auto call = ci->e()->cast<Call>();
      FunctionI* fi = _model->matchFn(env.envi(), call, false);
      if (!fi) {
        std::vector<VarDecl*> args;
        for (int i = 0; i < call->n_args(); ++i) {
          TypeInst* ti;
          if (call->arg(i)->type().dim() > 0) {
            auto al = eval_array_lit(env.envi(), call->arg(i));
            std::vector<TypeInst*> ranges(al->dims());
            for (auto& range : ranges) {
              range = new TypeInst(Location().introduce(), Type::parint(), nullptr);
            }
            ti = new TypeInst(Location().introduce(), call->arg(i)->type(), ranges, nullptr);
          } else {
            ti = new TypeInst(Location().introduce(), call->arg(i)->type(), nullptr);
          }
          args.push_back(new VarDecl(Location().introduce(), ti, i));
        }
        TypeInst* ti = new TypeInst(Location().introduce(), Type::varbool());
        fi = new FunctionI(Location().introduce(), call->id().str(), ti, args, nullptr);
        _model->registerFn(env.envi(), fi);
        toAdd.push_back(fi);
      }
      call->decl(fi);
    }
    env.model(nullptr);
    for (const auto& j : toAdd) {
      _model->addItem(j);
    }
  }

  SolverInstance::Status
  FZNSolverInstance::solve(void) {
    FZNSolverOptions& opt = static_cast<FZNSolverOptions&>(*_options);
    if (opt.fzn_solver.empty()) {
      throw InternalError("No FlatZinc solver specified");
    }
    /// Passing options to solver
    vector<string> cmd_line;
    cmd_line.push_back( opt.fzn_solver );
    string sBE = opt.backend;
    if ( sBE.size() ) {
      cmd_line.push_back( "-b" );
      cmd_line.push_back( sBE );
    }
    for (auto& f : opt.fzn_flags) {
      cmd_line.push_back( f );
    }
    if ( opt.numSols != 1 ) {
      cmd_line.push_back( "-n" );
      ostringstream oss;
      oss << opt.numSols;
      cmd_line.push_back( oss.str() );
    }
    if ( opt.allSols ) {
      cmd_line.push_back( "-a" );
    }
    if ( opt.parallel.size() ) {
      cmd_line.push_back( "-p" );
      ostringstream oss;
      oss << opt.parallel;
      cmd_line.push_back( oss.str() );
    }
    if (opt.printStatistics) {
      cmd_line.push_back( "-s" );
    }
    if (opt.solver_time_limit_ms != 0) {
      cmd_line.push_back( "-t" );
      std::ostringstream oss;
      oss << opt.solver_time_limit_ms;
      cmd_line.push_back( oss.str() );
    }
    if (opt.verbose) {
      if (opt.supports_v)
        cmd_line.push_back( "-v" );
      std::cerr << "Using FZN solver " << cmd_line[0]
        << " for solving, parameters: ";
      for ( int i=1; i<cmd_line.size(); ++i )
        cerr << "" << cmd_line[i] << " ";
      cerr << std::endl;
    }
    int timelimit = opt.fzn_time_limit_ms;
    bool sigint = opt.fzn_sigint;

    FileUtils::TmpFile fznFile(".fzn");
    std::ofstream os(fznFile.name());
    Printer p(os, 0, true);
    createFunctionItems();
    for (FunctionIterator it = _model->begin_functions(); it != _model->end_functions(); ++it) {
      if(!it->removed()) {
        Item& item = *it;
        p.print(&item);
      }
    }
    for (VarDeclIterator it = _model->begin_vardecls(); it != _model->end_vardecls(); ++it) {
      if(!it->removed()) {
        Item& item = *it;
        p.print(&item);
      }
    }
    for (ConstraintIterator it = _model->begin_constraints(); it != _model->end_constraints(); ++it) {
      if(!it->removed()) {
        Item& item = *it;
        p.print(&item);
      }
    }
    {
      GCLock lock;
      auto si = SolveI::sat(Location().introduce());
      p.print(si);
    }
    cmd_line.push_back(fznFile.name());

    FileUtils::TmpFile* pathsFile = NULL;
    // if(opt.fzn_needs_paths) {
    //   pathsFile = new FileUtils::TmpFile(".paths");
    //   std::ofstream ofs(pathsFile->name());
    //   PathFilePrinter pfp(ofs, _env.envi());
    //   pfp.print(_fzn);

    //   cmd_line.push_back("--paths");
    //   cmd_line.push_back(pathsFile->name());
    // }
    getSolns2Out()->initFromEnv(&env);

    if(!opt.fzn_output_passthrough) {
      Process<Solns2Out> proc(cmd_line, getSolns2Out(), timelimit, sigint);
      int exitStatus = proc.run();
      delete pathsFile;
      return exitStatus == 0 ? getSolns2Out()->status : SolverInstance::ERROR;
    } else {
      Solns2Log s2l(getSolns2Out()->getOutput(), _log);
      Process<Solns2Log> proc(cmd_line, &s2l, timelimit, sigint);
      int exitStatus = proc.run();
      delete pathsFile;
      return exitStatus==0 ? SolverInstance::NONE : SolverInstance::ERROR;
    }
  }

  void FZNSolverInstance::processFlatZinc(void) {}

  void FZNSolverInstance::resetSolver(void) {}

  Expression*
  FZNSolverInstance::getSolutionValue(Id* id) {
    assert(false);
    return NULL;
  }
}
