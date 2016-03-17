/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip@dekker.li>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//

#ifndef __MINIZINC_PRESOLVE_UTILS_HH__
#define __MINIZINC_PRESOLVE_UTILS_HH__

#include <minizinc/presolve.hh>
#include <minizinc/model.hh>
#include <minizinc/solns2out.hh>

namespace MiniZinc{

  void recursiveRegisterFns(Model*, EnvI&, FunctionI*);

  Expression* computeDomainExpr(EnvI& env, Expression* exp);

  void computeRanges(EnvI& env, Expression* exp, std::vector<TypeInst*>& ranges);

  void generateFlatZinc(Env& env, bool rangeDomains, bool optimizeFZN, bool newFZN);

  class Presolver::Solns2Vector : public Solns2Out {
  protected:
    EnvI& copyEnv;

//    TODO: using ASTStringMap don't work, ASTStrings don't compare correctly
    std::vector< std::unordered_map<std::string, Expression*>* > solutions;

    std::vector<KeepAlive> GCProhibitors;
  public:
    Solns2Vector(Env* e, EnvI& forEnv) : copyEnv(forEnv) { this->initFromEnv(e); }

    virtual ~Solns2Vector() { for (int i = 0; i < solutions.size(); ++i) delete solutions[i]; }

    const std::vector< std::unordered_map<std::string, Expression*>* >& getSolutions() const { return solutions; }

  protected:
    virtual bool evalOutput();
    virtual bool evalStatus(SolverInstance::Status status) {return true;}
  };

  class Presolver::TableBuilder {
  protected:
    bool boolTable = false;
    long long int rows;

    Expression* variables = nullptr;

    std::vector<Expression*> vVariables;
    std::vector<Expression*> data;

    EnvI& env;
    Model* m;
    Options& options;
  public:
    TableBuilder(EnvI& env, Model* m, Options& options, bool boolTable)
            : env(env), m(m), options(options), boolTable(boolTable) { };

    void buildFromSolver(FunctionI* f, Solns2Vector* solns, ASTExprVec<Expression> variables = ASTExprVec<Expression>());

    void addVariable(Expression* var);

    void addData(Expression* dat);

    Call* getExpression();

    void setRows(long long int rows) { rows = rows; }

  protected:
    void storeVars();

    void registerTableConstraint();
  };

}

#endif //__MINIZINC_PRESOLVE_UTILS_HH__
