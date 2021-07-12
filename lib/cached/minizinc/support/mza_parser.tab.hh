/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_MZA_YY_USERS_DEKKER1_CODE_GITHUB_COM_MINIZINC_LIBMINIZINC_BYTE_BUILD_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED
# define YY_MZA_YY_USERS_DEKKER1_CODE_GITHUB_COM_MINIZINC_LIBMINIZINC_BYTE_BUILD_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int mza_yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    MZA_INT = 258,                 /* MZA_INT  */
    MZA_REG = 259,                 /* MZA_REG  */
    MZA_MODE = 260,                /* MZA_MODE  */
    MZA_CTX = 261,                 /* MZA_CTX  */
    MZA_ID = 262,                  /* MZA_ID  */
    MZA_COLON = 263,               /* ":"  */
    MZA_DELAY = 264,               /* "D"  */
    MZA_GLOBAL = 265,              /* "GLOBAL"  */
    MZA_ADDI = 266,                /* MZA_ADDI  */
    MZA_SUBI = 267,                /* MZA_SUBI  */
    MZA_MULI = 268,                /* MZA_MULI  */
    MZA_DIVI = 269,                /* MZA_DIVI  */
    MZA_MODI = 270,                /* MZA_MODI  */
    MZA_INCI = 271,                /* MZA_INCI  */
    MZA_DECI = 272,                /* MZA_DECI  */
    MZA_IMMI = 273,                /* MZA_IMMI  */
    MZA_CLEAR = 274,               /* MZA_CLEAR  */
    MZA_LOAD_GLOBAL = 275,         /* MZA_LOAD_GLOBAL  */
    MZA_STORE_GLOBAL = 276,        /* MZA_STORE_GLOBAL  */
    MZA_MOV = 277,                 /* MZA_MOV  */
    MZA_JMP = 278,                 /* MZA_JMP  */
    MZA_JMPIF = 279,               /* MZA_JMPIF  */
    MZA_JMPIFNOT = 280,            /* MZA_JMPIFNOT  */
    MZA_EQI = 281,                 /* MZA_EQI  */
    MZA_LTI = 282,                 /* MZA_LTI  */
    MZA_LEI = 283,                 /* MZA_LEI  */
    MZA_AND = 284,                 /* MZA_AND  */
    MZA_OR = 285,                  /* MZA_OR  */
    MZA_NOT = 286,                 /* MZA_NOT  */
    MZA_XOR = 287,                 /* MZA_XOR  */
    MZA_ISPAR = 288,               /* MZA_ISPAR  */
    MZA_ISEMPTY = 289,             /* MZA_ISEMPTY  */
    MZA_LENGTH = 290,              /* MZA_LENGTH  */
    MZA_GET_VEC = 291,             /* MZA_GET_VEC  */
    MZA_GET_ARRAY = 292,           /* MZA_GET_ARRAY  */
    MZA_LB = 293,                  /* MZA_LB  */
    MZA_UB = 294,                  /* MZA_UB  */
    MZA_DOM = 295,                 /* MZA_DOM  */
    MZA_MAKE_SET = 296,            /* MZA_MAKE_SET  */
    MZA_DIFF = 297,                /* MZA_DIFF  */
    MZA_INTERSECTION = 298,        /* MZA_INTERSECTION  */
    MZA_UNION = 299,               /* MZA_UNION  */
    MZA_INTERSECT_DOMAIN = 300,    /* MZA_INTERSECT_DOMAIN  */
    MZA_OPEN_AGGREGATION = 301,    /* MZA_OPEN_AGGREGATION  */
    MZA_CLOSE_AGGREGATION = 302,   /* MZA_CLOSE_AGGREGATION  */
    MZA_SIMPLIFY_LIN = 303,        /* MZA_SIMPLIFY_LIN  */
    MZA_PUSH = 304,                /* MZA_PUSH  */
    MZA_POP = 305,                 /* MZA_POP  */
    MZA_POST = 306,                /* MZA_POST  */
    MZA_RET = 307,                 /* MZA_RET  */
    MZA_CALL = 308,                /* MZA_CALL  */
    MZA_BUILTIN = 309,             /* MZA_BUILTIN  */
    MZA_TCALL = 310,               /* MZA_TCALL  */
    MZA_ITER_ARRAY = 311,          /* MZA_ITER_ARRAY  */
    MZA_ITER_VEC = 312,            /* MZA_ITER_VEC  */
    MZA_ITER_RANGE = 313,          /* MZA_ITER_RANGE  */
    MZA_ITER_NEXT = 314,           /* MZA_ITER_NEXT  */
    MZA_ITER_BREAK = 315,          /* MZA_ITER_BREAK  */
    MZA_TRACE = 316,               /* MZA_TRACE  */
    MZA_ABORT = 317                /* MZA_ABORT  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{

  int iValue;
  char* sValue;
  MiniZinc::BytecodeStream::Instr bValue;
  std::list<int>* liValue;
  std::list<std::string>* sliValue;


};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int mza_yyparse (MZAContext& ctx);

#endif /* !YY_MZA_YY_USERS_DEKKER1_CODE_GITHUB_COM_MINIZINC_LIBMINIZINC_BYTE_BUILD_INCLUDE_MINIZINC_SUPPORT_MZA_PARSER_TAB_HH_INCLUDED  */
