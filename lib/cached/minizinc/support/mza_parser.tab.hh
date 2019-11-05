/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_MZA_YY_HOME_JDEKKER_BUILD_PKG_MINIZINC_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED
# define YY_MZA_YY_HOME_JDEKKER_BUILD_PKG_MINIZINC_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int mza_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    MZA_INT = 258,
    MZA_REG = 259,
    MZA_MODE = 260,
    MZA_CTX = 261,
    MZA_ID = 262,
    MZA_COLON = 263,
    MZA_DELAY = 264,
    MZA_ADDI = 265,
    MZA_SUBI = 266,
    MZA_MULI = 267,
    MZA_DIVI = 268,
    MZA_MODI = 269,
    MZA_INCI = 270,
    MZA_DECI = 271,
    MZA_IMMI = 272,
    MZA_LOAD_GLOBAL = 273,
    MZA_STORE_GLOBAL = 274,
    MZA_MOV = 275,
    MZA_JMP = 276,
    MZA_JMPIF = 277,
    MZA_JMPIFNOT = 278,
    MZA_EQI = 279,
    MZA_LTI = 280,
    MZA_LEI = 281,
    MZA_AND = 282,
    MZA_OR = 283,
    MZA_NOT = 284,
    MZA_XOR = 285,
    MZA_ISPAR = 286,
    MZA_ISEMPTY = 287,
    MZA_LENGTH = 288,
    MZA_GET_VEC = 289,
    MZA_LB = 290,
    MZA_UB = 291,
    MZA_DOM = 292,
    MZA_MAKE_SET = 293,
    MZA_INTERSECTION = 294,
    MZA_UNION = 295,
    MZA_INTERSECT_DOMAIN = 296,
    MZA_OPEN_AGGREGATION = 297,
    MZA_CLOSE_AGGREGATION = 298,
    MZA_SIMPLIFY_LIN = 299,
    MZA_PUSH = 300,
    MZA_POP = 301,
    MZA_POST = 302,
    MZA_RET = 303,
    MZA_CALL = 304,
    MZA_BUILTIN = 305,
    MZA_TCALL = 306,
    MZA_TRACE = 307,
    MZA_ABORT = 308
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{


  int iValue;
  char* sValue;
  MiniZinc::BytecodeStream::Instr bValue;
  std::list<int>* liValue;


};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE mza_yylval;

int mza_yyparse (MZAContext& ctx);

#endif /* !YY_MZA_YY_HOME_JDEKKER_BUILD_PKG_MINIZINC_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED  */
