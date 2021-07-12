/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30706

/* Bison version string.  */
#define YYBISON_VERSION "3.7.6"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         mza_yyparse
#define yylex           mza_yylex
#define yyerror         mza_yyerror
#define yydebug         mza_yydebug
#define yynerrs         mza_yynerrs

/* First part of user prologue.  */

#include <cstdio>
#include <list>

#include <minizinc/interpreter.hh>

//Anonymous struct for when yyparse is exported
typedef struct MZAContext MZAContext;
#include <minizinc/support/mza_parser.tab.hh>

using namespace MiniZinc;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE mza_yy_scan_string ( const char* yy_str );

extern int yylex(YYSTYPE*, YYLTYPE*);
extern FILE* yyin;

struct ProcPatch {
  int code;
  BytecodeProc::Mode mode;
  std::vector<std::pair<int, std::pair<std::string, int>>> patch;
  ProcPatch(int code0, BytecodeProc::Mode mode0, std::vector<std::pair<int, std::pair<std::string, int>>> patch0)
  : code(code0), mode(mode0), patch(std::move(patch0)) {}
};

typedef struct MZAContext {
  std::vector<BytecodeProc>& procs;
  std::unordered_map<std::string, int>& proc_map;
  int& max_glob;
  std::vector<ProcPatch> to_patch;
  BytecodeStream proc_body;
  std::unordered_map<std::string, int> labels;
  std::vector<std::pair<int, std::string>> patch_labels;
  std::vector<std::pair<int, std::pair<std::string, int>>> patch_procs;
} MZAContext;

void yyerror(YYLTYPE* location, MZAContext& ctx, const char* s);


# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include <minizinc/support/mza_parser.tab.hh>
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_MZA_INT = 3,                    /* MZA_INT  */
  YYSYMBOL_MZA_REG = 4,                    /* MZA_REG  */
  YYSYMBOL_MZA_MODE = 5,                   /* MZA_MODE  */
  YYSYMBOL_MZA_CTX = 6,                    /* MZA_CTX  */
  YYSYMBOL_MZA_ID = 7,                     /* MZA_ID  */
  YYSYMBOL_MZA_COLON = 8,                  /* ":"  */
  YYSYMBOL_MZA_DELAY = 9,                  /* "D"  */
  YYSYMBOL_MZA_GLOBAL = 10,                /* "GLOBAL"  */
  YYSYMBOL_MZA_ADDI = 11,                  /* MZA_ADDI  */
  YYSYMBOL_MZA_SUBI = 12,                  /* MZA_SUBI  */
  YYSYMBOL_MZA_MULI = 13,                  /* MZA_MULI  */
  YYSYMBOL_MZA_DIVI = 14,                  /* MZA_DIVI  */
  YYSYMBOL_MZA_MODI = 15,                  /* MZA_MODI  */
  YYSYMBOL_MZA_INCI = 16,                  /* MZA_INCI  */
  YYSYMBOL_MZA_DECI = 17,                  /* MZA_DECI  */
  YYSYMBOL_MZA_IMMI = 18,                  /* MZA_IMMI  */
  YYSYMBOL_MZA_CLEAR = 19,                 /* MZA_CLEAR  */
  YYSYMBOL_MZA_LOAD_GLOBAL = 20,           /* MZA_LOAD_GLOBAL  */
  YYSYMBOL_MZA_STORE_GLOBAL = 21,          /* MZA_STORE_GLOBAL  */
  YYSYMBOL_MZA_MOV = 22,                   /* MZA_MOV  */
  YYSYMBOL_MZA_JMP = 23,                   /* MZA_JMP  */
  YYSYMBOL_MZA_JMPIF = 24,                 /* MZA_JMPIF  */
  YYSYMBOL_MZA_JMPIFNOT = 25,              /* MZA_JMPIFNOT  */
  YYSYMBOL_MZA_EQI = 26,                   /* MZA_EQI  */
  YYSYMBOL_MZA_LTI = 27,                   /* MZA_LTI  */
  YYSYMBOL_MZA_LEI = 28,                   /* MZA_LEI  */
  YYSYMBOL_MZA_AND = 29,                   /* MZA_AND  */
  YYSYMBOL_MZA_OR = 30,                    /* MZA_OR  */
  YYSYMBOL_MZA_NOT = 31,                   /* MZA_NOT  */
  YYSYMBOL_MZA_XOR = 32,                   /* MZA_XOR  */
  YYSYMBOL_MZA_ISPAR = 33,                 /* MZA_ISPAR  */
  YYSYMBOL_MZA_ISEMPTY = 34,               /* MZA_ISEMPTY  */
  YYSYMBOL_MZA_LENGTH = 35,                /* MZA_LENGTH  */
  YYSYMBOL_MZA_GET_VEC = 36,               /* MZA_GET_VEC  */
  YYSYMBOL_MZA_GET_ARRAY = 37,             /* MZA_GET_ARRAY  */
  YYSYMBOL_MZA_LB = 38,                    /* MZA_LB  */
  YYSYMBOL_MZA_UB = 39,                    /* MZA_UB  */
  YYSYMBOL_MZA_DOM = 40,                   /* MZA_DOM  */
  YYSYMBOL_MZA_MAKE_SET = 41,              /* MZA_MAKE_SET  */
  YYSYMBOL_MZA_DIFF = 42,                  /* MZA_DIFF  */
  YYSYMBOL_MZA_INTERSECTION = 43,          /* MZA_INTERSECTION  */
  YYSYMBOL_MZA_UNION = 44,                 /* MZA_UNION  */
  YYSYMBOL_MZA_INTERSECT_DOMAIN = 45,      /* MZA_INTERSECT_DOMAIN  */
  YYSYMBOL_MZA_OPEN_AGGREGATION = 46,      /* MZA_OPEN_AGGREGATION  */
  YYSYMBOL_MZA_CLOSE_AGGREGATION = 47,     /* MZA_CLOSE_AGGREGATION  */
  YYSYMBOL_MZA_SIMPLIFY_LIN = 48,          /* MZA_SIMPLIFY_LIN  */
  YYSYMBOL_MZA_PUSH = 49,                  /* MZA_PUSH  */
  YYSYMBOL_MZA_POP = 50,                   /* MZA_POP  */
  YYSYMBOL_MZA_POST = 51,                  /* MZA_POST  */
  YYSYMBOL_MZA_RET = 52,                   /* MZA_RET  */
  YYSYMBOL_MZA_CALL = 53,                  /* MZA_CALL  */
  YYSYMBOL_MZA_BUILTIN = 54,               /* MZA_BUILTIN  */
  YYSYMBOL_MZA_TCALL = 55,                 /* MZA_TCALL  */
  YYSYMBOL_MZA_ITER_ARRAY = 56,            /* MZA_ITER_ARRAY  */
  YYSYMBOL_MZA_ITER_VEC = 57,              /* MZA_ITER_VEC  */
  YYSYMBOL_MZA_ITER_RANGE = 58,            /* MZA_ITER_RANGE  */
  YYSYMBOL_MZA_ITER_NEXT = 59,             /* MZA_ITER_NEXT  */
  YYSYMBOL_MZA_ITER_BREAK = 60,            /* MZA_ITER_BREAK  */
  YYSYMBOL_MZA_TRACE = 61,                 /* MZA_TRACE  */
  YYSYMBOL_MZA_ABORT = 62,                 /* MZA_ABORT  */
  YYSYMBOL_YYACCEPT = 63,                  /* $accept  */
  YYSYMBOL_procedures = 64,                /* procedures  */
  YYSYMBOL_procedure = 65,                 /* procedure  */
  YYSYMBOL_mode = 66,                      /* mode  */
  YYSYMBOL_delay = 67,                     /* delay  */
  YYSYMBOL_instructions = 68,              /* instructions  */
  YYSYMBOL_labeled_instr = 69,             /* labeled_instr  */
  YYSYMBOL_labels = 70,                    /* labels  */
  YYSYMBOL_label = 71,                     /* label  */
  YYSYMBOL_instruction = 72,               /* instruction  */
  YYSYMBOL_instrIR = 73,                   /* instrIR  */
  YYSYMBOL_instrI = 74,                    /* instrI  */
  YYSYMBOL_instrR = 75,                    /* instrR  */
  YYSYMBOL_instrRR = 76,                   /* instrRR  */
  YYSYMBOL_instrRS = 77,                   /* instrRS  */
  YYSYMBOL_instrRRR = 78,                  /* instrRRR  */
  YYSYMBOL_instrRRS = 79,                  /* instrRRS  */
  YYSYMBOL_instrRRIRRR = 80,               /* instrRRIRRR  */
  YYSYMBOL_instrS = 81,                    /* instrS  */
  YYSYMBOL_instr = 82,                     /* instr  */
  YYSYMBOL_registers = 83                  /* registers  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   105

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  63
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  21
/* YYNRULES -- Number of rules.  */
#define YYNRULES  80
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  121

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   317


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   154,   154,   156,   159,   205,   206,   209,   210,   212,
     214,   217,   226,   227,   230,   233,   238,   244,   251,   257,
     264,   272,   281,   289,   300,   307,   320,   331,   340,   347,
     353,   360,   368,   376,   388,   391,   394,   395,   396,   397,
     398,   399,   400,   403,   404,   405,   406,   407,   408,   409,
     410,   411,   412,   415,   416,   417,   418,   421,   422,   423,
     424,   425,   426,   427,   428,   429,   430,   431,   432,   433,
     434,   435,   436,   439,   442,   445,   448,   449,   450,   454,
     455
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "MZA_INT", "MZA_REG",
  "MZA_MODE", "MZA_CTX", "MZA_ID", "\":\"", "\"D\"", "\"GLOBAL\"",
  "MZA_ADDI", "MZA_SUBI", "MZA_MULI", "MZA_DIVI", "MZA_MODI", "MZA_INCI",
  "MZA_DECI", "MZA_IMMI", "MZA_CLEAR", "MZA_LOAD_GLOBAL",
  "MZA_STORE_GLOBAL", "MZA_MOV", "MZA_JMP", "MZA_JMPIF", "MZA_JMPIFNOT",
  "MZA_EQI", "MZA_LTI", "MZA_LEI", "MZA_AND", "MZA_OR", "MZA_NOT",
  "MZA_XOR", "MZA_ISPAR", "MZA_ISEMPTY", "MZA_LENGTH", "MZA_GET_VEC",
  "MZA_GET_ARRAY", "MZA_LB", "MZA_UB", "MZA_DOM", "MZA_MAKE_SET",
  "MZA_DIFF", "MZA_INTERSECTION", "MZA_UNION", "MZA_INTERSECT_DOMAIN",
  "MZA_OPEN_AGGREGATION", "MZA_CLOSE_AGGREGATION", "MZA_SIMPLIFY_LIN",
  "MZA_PUSH", "MZA_POP", "MZA_POST", "MZA_RET", "MZA_CALL", "MZA_BUILTIN",
  "MZA_TCALL", "MZA_ITER_ARRAY", "MZA_ITER_VEC", "MZA_ITER_RANGE",
  "MZA_ITER_NEXT", "MZA_ITER_BREAK", "MZA_TRACE", "MZA_ABORT", "$accept",
  "procedures", "procedure", "mode", "delay", "instructions",
  "labeled_instr", "labels", "label", "instruction", "instrIR", "instrI",
  "instrR", "instrRR", "instrRS", "instrRRR", "instrRRS", "instrRRIRRR",
  "instrS", "instr", "registers", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317
};
#endif

#define YYPACT_NINF (-32)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-5)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
     -32,    58,   -32,    -6,   -32,    -5,    -3,   -32,    57,    52,
     -32,   -32,    59,   -32,    -7,    54,   -32,   -32,   -32,   -32,
     -32,   -32,   -32,   -32,   -32,    60,    61,   -32,   -32,   -32,
     -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,
     -32,   -32,    65,   -32,   -32,   -32,   -32,   -32,   -32,   -32,
     -32,    50,   -32,   -32,   -32,   -32,   -32,   -32,    64,    63,
      66,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,
      69,    70,    71,    72,    73,    74,    78,    79,    67,   -32,
     -32,    80,    82,   -32,   -32,   -32,   -32,    81,   -32,    83,
      85,   -32,   -32,    87,    86,    88,    90,    91,   -32,   -32,
     -32,    92,    84,    92,    94,   -32,   -32,   -32,    95,    93,
      98,   -32,   -32,   -32,   -32,   -32,    99,    92,   100,   101,
     -32
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       2,     0,     1,     0,     3,     0,     5,     6,     0,     7,
       8,     9,    12,    10,     0,     0,    57,    58,    59,    60,
      61,    36,    37,    34,    52,     0,     0,    43,    75,    53,
      54,    62,    63,    64,    65,    66,    44,    67,    45,    46,
      47,    68,     0,    50,    49,    51,    48,    69,    70,    71,
      72,     0,    77,    74,    38,    39,    40,    76,     0,     0,
       0,    55,    56,    73,    41,    35,    42,    78,    13,    11,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    15,
      14,     0,     0,    79,    28,    29,    30,     0,    79,     0,
       0,    16,    18,     0,     0,     0,     0,     0,    24,    31,
      32,    33,     0,    26,     0,    17,    19,    20,     0,     0,
       0,    80,    79,    27,    22,    21,     0,    25,     0,     0,
      23
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,
     -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,   -32,
     -31
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     1,     4,     8,    11,    12,    13,    14,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
     101
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      15,     5,     7,     6,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    84,   103,     2,    -4,
       9,    10,    80,    81,     0,    82,     3,    -4,    83,    87,
      88,    89,    90,    91,    98,    92,    93,    94,    95,    85,
      86,   117,    96,    97,    99,   100,     0,   112,   102,   105,
     104,   106,   108,   107,   109,   110,   111,   113,     0,   114,
     115,   116,     0,   118,   119,   120
};

static const yytype_int8 yycheck[] =
{
       7,     7,     5,     8,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     6,    88,     0,     0,
       3,     9,     8,     3,    -1,     4,     8,     8,     3,     5,
       7,     5,     3,     3,     7,     4,     4,     4,     4,    29,
      30,   112,     4,     4,     4,     3,    -1,     3,     7,     4,
       7,     4,     4,     7,     4,     4,     4,     3,    -1,     4,
       7,     3,    -1,     4,     4,     4
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    64,     0,     8,    65,     7,     8,     5,    66,     3,
       9,    67,    68,    69,    70,     7,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
       8,     3,     4,     3,     6,    29,    30,     5,     7,     5,
       3,     3,     4,     4,     4,     4,     4,     4,     7,     4,
       3,    83,     7,    83,     7,     4,     4,     7,     4,     4,
       4,     4,     3,     3,     4,     7,     3,    83,     4,     4,
       4
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    63,    64,    64,    65,    66,    66,    67,    67,    68,
      68,    69,    70,    70,    71,    72,    72,    72,    72,    72,
      72,    72,    72,    72,    72,    72,    72,    72,    72,    72,
      72,    72,    72,    72,    73,    74,    75,    75,    75,    75,
      75,    75,    75,    76,    76,    76,    76,    76,    76,    76,
      76,    76,    76,    77,    77,    77,    77,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    79,    80,    81,    82,    82,    82,    83,
      83
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     7,     0,     1,     0,     1,     0,
       2,     2,     0,     2,     2,     1,     2,     3,     2,     3,
       3,     4,     4,     7,     2,     5,     3,     4,     2,     2,
       2,     3,     3,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, ctx, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YY_LOCATION_PRINT
#  if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#   define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

#  else
#   define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#  endif
# endif /* !defined YY_LOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, ctx); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, MZAContext& ctx)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (ctx);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, MZAContext& ctx)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, ctx);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, MZAContext& ctx)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), ctx);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, ctx); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, MZAContext& ctx)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (ctx);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (MZAContext& ctx)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 4: /* procedure: ":" MZA_ID ":" mode MZA_INT delay instructions  */
    {
      // Patch jumps with recorded labels
      for (auto& cl : ctx.patch_labels) {
        if (ctx.labels.find(cl.second) == ctx.labels.end()) {
          throw Error("Error: label " + cl.second + " not found\n");
        }
        ctx.proc_body.patchAddress(cl.first, ctx.labels[cl.second]);
      }
      ctx.labels.clear();
      ctx.patch_labels.clear();

      ctx.proc_body.setNArgs((yyvsp[-2].iValue));

      // Add ABORT instruction for error management
      if (ctx.proc_body.size() > 0) {
        ctx.proc_body.addInstr(BytecodeStream::ABORT);
      }
      // Store procedure in the correct place
      auto mode = static_cast<BytecodeProc::Mode>((yyvsp[-3].iValue));
      auto it = ctx.proc_map.find((yyvsp[-5].sValue));
      if (it != ctx.proc_map.end()) {
        BytecodeProc& bcp = ctx.procs[it->second];
        if (bcp.mode[mode].size() > 0) {
          throw Error("Error: procedure " + std::string((yyvsp[-5].sValue)) + " already defined before with the same mode\n");
        }
        if (bcp.nargs != (yyvsp[-2].iValue) ) {
          throw Error("Error: procedure " + std::string((yyvsp[-5].sValue)) + " already defined before with different number of arguments\n");
        }
        bcp.mode[(yyvsp[-3].iValue)] = ctx.proc_body;
        ctx.to_patch.emplace_back(it->second, mode, ctx.patch_procs);
      } else {
        BytecodeProc bcp;
        bcp.name = (yyvsp[-5].sValue);
        bcp.mode[mode] = ctx.proc_body;
        bcp.nargs = (yyvsp[-2].iValue);
        bcp.delay = (yyvsp[-1].iValue);
        ctx.proc_map.emplace((yyvsp[-5].sValue), ctx.procs.size());
        ctx.to_patch.emplace_back(ctx.procs.size(), mode, ctx.patch_procs);
        ctx.procs.push_back(bcp);
      }
      ctx.proc_body = BytecodeStream();
      ctx.patch_procs.clear();
    }
    break;

  case 5: /* mode: %empty  */
               { (yyval.iValue) = BytecodeProc::FUN; }
    break;

  case 7: /* delay: %empty  */
                { (yyval.iValue) = 0; }
    break;

  case 8: /* delay: "D"  */
                { (yyval.iValue) = 1; }
    break;

  case 11: /* labeled_instr: labels instruction  */
    {
      for (auto l : *(yyvsp[-1].sliValue)) {
        ctx.labels.emplace(l, (yyvsp[0].iValue));
      }
      delete (yyvsp[-1].sliValue);
    }
    break;

  case 12: /* labels: %empty  */
                { (yyval.sliValue) = new std::list<std::string>(); }
    break;

  case 13: /* labels: labels label  */
                 { (yyvsp[-1].sliValue)->push_back((yyvsp[0].sValue)); (yyval.sliValue) = (yyvsp[-1].sliValue); }
    break;

  case 15: /* instruction: instr  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[0].bValue));
    }
    break;

  case 16: /* instruction: instrI MZA_INT  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.proc_body.addIntVal((yyvsp[0].iValue));
    }
    break;

  case 17: /* instruction: instrIR MZA_INT MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addIntVal((yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 18: /* instruction: instrR MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 19: /* instruction: instrRR MZA_REG MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 20: /* instruction: instrRS MZA_REG MZA_ID  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.patch_labels.emplace_back(ctx.proc_body.size(), (yyvsp[0].sValue));
      ctx.proc_body.addSmallInt(0);
    }
    break;

  case 21: /* instruction: instrRRS MZA_REG MZA_REG MZA_ID  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-3].bValue));
      ctx.proc_body.addReg((yyvsp[-2].iValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.patch_labels.emplace_back(ctx.proc_body.size(), (yyvsp[0].sValue));
      ctx.proc_body.addSmallInt(0);
    }
    break;

  case 22: /* instruction: instrRRR MZA_REG MZA_REG MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-3].bValue));
      ctx.proc_body.addReg((yyvsp[-2].iValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 23: /* instruction: instrRRIRRR MZA_REG MZA_REG MZA_INT MZA_REG MZA_REG MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-6].bValue));
      ctx.proc_body.addReg((yyvsp[-5].iValue));
      ctx.proc_body.addReg((yyvsp[-4].iValue));
      ctx.proc_body.addIntVal((yyvsp[-3].iValue));
      ctx.proc_body.addReg((yyvsp[-2].iValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 24: /* instruction: instrS MZA_ID  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.patch_labels.emplace_back(ctx.proc_body.size(), (yyvsp[0].sValue));
      ctx.proc_body.addSmallInt(0);
    }
    break;

  case 25: /* instruction: MZA_CALL MZA_MODE MZA_ID MZA_INT registers  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-4].bValue));
      ctx.proc_body.addCharVal((yyvsp[-3].iValue));
      ctx.patch_procs.emplace_back(ctx.proc_body.size(), std::make_pair((yyvsp[-2].sValue), (yyvsp[0].liValue)->size()));
      ctx.proc_body.addSmallInt(0);
      ctx.proc_body.addCharVal((yyvsp[-1].iValue));
      for (auto arg : *(yyvsp[0].liValue)) {
        ctx.proc_body.addReg(arg);
      }
      delete (yyvsp[0].liValue);
    }
    break;

  case 26: /* instruction: MZA_BUILTIN MZA_ID registers  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.patch_procs.emplace_back(ctx.proc_body.size(), std::make_pair((yyvsp[-1].sValue), (yyvsp[0].liValue)->size()));
      ctx.proc_body.addSmallInt(0);
      for (auto arg : *(yyvsp[0].liValue)) {
        ctx.proc_body.addReg(arg);
      }
      delete (yyvsp[0].liValue);
    }
    break;

  case 27: /* instruction: MZA_TCALL MZA_MODE MZA_ID MZA_INT  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-3].bValue));
      ctx.proc_body.addCharVal((yyvsp[-2].iValue));
      ctx.patch_procs.emplace_back(ctx.proc_body.size(), std::make_pair((yyvsp[-1].sValue), -1));
      ctx.proc_body.addSmallInt(0);
      ctx.proc_body.addCharVal((yyvsp[0].iValue));
    }
    break;

  case 28: /* instruction: MZA_OPEN_AGGREGATION MZA_CTX  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.proc_body.addCharVal((yyvsp[0].iValue));
    }
    break;

  case 29: /* instruction: MZA_OPEN_AGGREGATION MZA_AND  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.proc_body.addCharVal(AggregationCtx::VCTX_AND);
    }
    break;

  case 30: /* instruction: MZA_OPEN_AGGREGATION MZA_OR  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-1].bValue));
      ctx.proc_body.addCharVal(AggregationCtx::VCTX_OR);
    }
    break;

  case 31: /* instruction: MZA_LOAD_GLOBAL MZA_INT MZA_REG  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue), true);
      ctx.max_glob = std::max(ctx.max_glob, (yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue));
    }
    break;

  case 32: /* instruction: MZA_STORE_GLOBAL MZA_REG MZA_INT  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addReg((yyvsp[-1].iValue));
      ctx.proc_body.addReg((yyvsp[0].iValue), true);
      ctx.max_glob = std::max(ctx.max_glob, (yyvsp[0].iValue));
    }
    break;

  case 33: /* instruction: MZA_GET_ARRAY MZA_INT registers  */
    {
      (yyval.iValue) = ctx.proc_body.size();
      ctx.proc_body.addInstr((yyvsp[-2].bValue));
      ctx.proc_body.addIntVal((yyvsp[-1].iValue));
      for (auto arg : *(yyvsp[0].liValue)) {
        ctx.proc_body.addReg(arg);
      }
      delete (yyvsp[0].liValue);
    }
    break;

  case 79: /* registers: %empty  */
                { (yyval.liValue) = new std::list<int>(); }
    break;

  case 80: /* registers: registers MZA_REG  */
                      { (yyvsp[-1].liValue)->push_back((yyvsp[0].iValue)); (yyval.liValue) = (yyvsp[-1].liValue); }
    break;



      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, ctx, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          goto yyexhaustedlab;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, ctx);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, ctx);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if 1
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, ctx, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, ctx);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, ctx);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}



#include <minizinc/interpreter/primitives.hh>
#include <minizinc/interpreter.hh>

void yyerror(YYLTYPE* location, MZAContext& ctx, const char* s) {
  std::ostringstream oss;
  oss << "Cannot parse MiniZinc assembly in line " << location->first_line << ": " << std::string(s);
  throw Error(oss.str());
}

std::pair<int, std::vector<BytecodeProc>> parse_mza(const std::string& assembly_str) {
  std::vector<BytecodeProc> procs;
  std::unordered_map<std::string, int> proc_map;
  int max_glob;
  // Initialise first slots with
  for (PrimitiveMap::Primitive* p : primitiveMap()) {
    BytecodeProc bcp;
    bcp.name = p->name();
    bcp.nargs = p->n_args();
    bcp.delay = false;
    procs.push_back(bcp);
    proc_map.emplace(bcp.name, p->ident());
  }
  mza_yy_scan_string(assembly_str.c_str());
  MZAContext ctx = MZAContext{procs, proc_map, max_glob};
  int err = yyparse(ctx);
  if (err != 0) {
    throw std::runtime_error("Cannot parse MiniZinc assembly: " + std::to_string(err));
  }
  for (auto& p : ctx.to_patch) {
    int code = p.code;
    BytecodeProc::Mode mode = p.mode;
    for (auto& patch : p.patch) {
      int& nargs = patch.second.second;
      std::string& name = patch.second.first;

      if (nargs >= 0 && nargs != procs[proc_map[name]].nargs) {
          throw Error("Error: number of arguments in call to " + patch.second.first + " at position "
                      + std::to_string(patch.second.second) + " has an invalid number of arguments.\n");
      }
      procs[code].mode[mode].patchAddress(patch.first, proc_map[patch.second.first]);
    }
  }
  return {max_glob, procs};
}
