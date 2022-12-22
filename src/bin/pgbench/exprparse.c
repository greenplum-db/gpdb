/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         expr_yyparse
#define yylex           expr_yylex
#define yyerror         expr_yyerror
#define yydebug         expr_yydebug
#define yynerrs         expr_yynerrs


/* Copy the first part of user declarations.  */
#line 1 "exprparse.y" /* yacc.c:339  */

/*-------------------------------------------------------------------------
 *
 * exprparse.y
 *	  bison grammar for a simple expression syntax
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/bin/pgbench/exprparse.y
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include "pgbench.h"

#define PGBENCH_NARGS_VARIABLE	(-1)
#define PGBENCH_NARGS_CASE		(-2)
#define PGBENCH_NARGS_HASH		(-3)

PgBenchExpr *expr_parse_result;

static PgBenchExprList *make_elist(PgBenchExpr *exp, PgBenchExprList *list);
static PgBenchExpr *make_null_constant(void);
static PgBenchExpr *make_boolean_constant(bool bval);
static PgBenchExpr *make_integer_constant(int64 ival);
static PgBenchExpr *make_double_constant(double dval);
static PgBenchExpr *make_variable(char *varname);
static PgBenchExpr *make_op(yyscan_t yyscanner, const char *operator,
		PgBenchExpr *lexpr, PgBenchExpr *rexpr);
static PgBenchExpr *make_uop(yyscan_t yyscanner, const char *operator, PgBenchExpr *expr);
static int	find_func(yyscan_t yyscanner, const char *fname);
static PgBenchExpr *make_func(yyscan_t yyscanner, int fnumber, PgBenchExprList *args);
static PgBenchExpr *make_case(yyscan_t yyscanner, PgBenchExprList *when_then_list, PgBenchExpr *else_part);


#line 111 "exprparse.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif


/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int expr_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    NULL_CONST = 258,
    INTEGER_CONST = 259,
    MAXINT_PLUS_ONE_CONST = 260,
    DOUBLE_CONST = 261,
    BOOLEAN_CONST = 262,
    VARIABLE = 263,
    FUNCTION = 264,
    AND_OP = 265,
    OR_OP = 266,
    NOT_OP = 267,
    NE_OP = 268,
    LE_OP = 269,
    GE_OP = 270,
    LS_OP = 271,
    RS_OP = 272,
    IS_OP = 273,
    CASE_KW = 274,
    WHEN_KW = 275,
    THEN_KW = 276,
    ELSE_KW = 277,
    END_KW = 278,
    ISNULL_OP = 279,
    NOTNULL_OP = 280,
    UNARY = 281
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 48 "exprparse.y" /* yacc.c:355  */

	int64		ival;
	double		dval;
	bool		bval;
	char	   *str;
	PgBenchExpr *expr;
	PgBenchExprList *elist;

#line 184 "exprparse.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int expr_yyparse (yyscan_t yyscanner);



/* Copy the second part of user declarations.  */

#line 200 "exprparse.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

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

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
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


#if ! defined yyoverflow || YYERROR_VERBOSE

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
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   320

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  42
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  7
/* YYNRULES -- Number of rules.  */
#define YYNRULES  47
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  88

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   281

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    30,     2,    37,    31,     2,
      40,    41,    35,    33,    39,    34,     2,    36,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      26,    28,    27,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    29,     2,    32,     2,     2,     2,
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
      25,    38
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,    82,    82,    84,    85,    86,    89,    90,    92,    95,
      98,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     120,   121,   125,   126,   131,   135,   141,   142,   143,   144,
     146,   147,   148,   152,   153,   156,   157,   159
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NULL_CONST", "INTEGER_CONST",
  "MAXINT_PLUS_ONE_CONST", "DOUBLE_CONST", "BOOLEAN_CONST", "VARIABLE",
  "FUNCTION", "AND_OP", "OR_OP", "NOT_OP", "NE_OP", "LE_OP", "GE_OP",
  "LS_OP", "RS_OP", "IS_OP", "CASE_KW", "WHEN_KW", "THEN_KW", "ELSE_KW",
  "END_KW", "ISNULL_OP", "NOTNULL_OP", "'<'", "'>'", "'='", "'|'", "'#'",
  "'&'", "'~'", "'+'", "'-'", "'*'", "'/'", "'%'", "UNARY", "','", "'('",
  "')'", "$accept", "result", "elist", "expr", "when_then_list",
  "case_control", "function", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,    60,    62,    61,   124,
      35,    38,   126,    43,    45,    42,    47,    37,   281,    44,
      40,    41
};
# endif

#define YYPACT_NINF -33

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-33)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  (!!((Yytable_value) == (-1)))

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      64,   -33,   -33,   -33,   -33,   -33,   -33,    64,   -19,    64,
      64,    46,    64,    13,   205,   -33,   -22,   258,    64,    -6,
      11,   -33,   -33,   -33,    92,   -33,    64,    64,    64,    64,
      64,    64,    64,     3,   -33,   -33,    64,    64,    64,    64,
      64,    64,    64,    64,    64,    64,    64,    64,   121,    64,
      64,   -33,   -33,   258,   233,   283,   283,   283,    11,    11,
     -33,   -33,     5,   283,   283,   283,    11,    11,    11,    -9,
      -9,   -33,   -33,   -33,   -32,   205,    64,   149,   177,   -33,
     -33,    64,   -33,   205,    64,   -33,   205,   205
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    36,    38,    39,    37,    40,    47,     0,     0,     0,
       0,     0,     0,     0,     2,    42,     0,    11,     0,     0,
      10,     7,     9,     8,     0,     1,     0,     0,     0,     0,
       0,     0,     0,     0,    30,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     3,     0,     0,
       0,    45,     6,    28,    29,    22,    18,    20,    26,    27,
      32,    34,     0,    17,    19,    21,    24,    25,    23,    12,
      13,    14,    15,    16,     0,     4,     0,     0,     0,    33,
      35,     0,    41,    44,     0,    46,     5,    43
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -33,   -33,   -33,    -7,   -33,   -33,   -33
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,    13,    74,    14,    19,    15,    16
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      17,    18,    20,    21,    23,    24,    60,    81,    79,    82,
      61,    48,    80,    25,    49,    62,    50,    51,    47,    53,
      54,    55,    56,    57,    58,    59,    44,    45,    46,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      75,     0,    77,    78,    42,    43,    44,    45,    46,     1,
       2,    22,     3,     4,     5,     6,     0,     0,     7,     0,
       0,     0,     0,     0,     0,     8,     0,     1,     2,    83,
       3,     4,     5,     6,    86,     0,     7,    87,     9,    10,
      11,     0,     0,     8,     0,     0,    12,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,     0,
       0,     0,    26,    27,    12,    28,    29,    30,    31,    32,
      33,     0,     0,     0,     0,     0,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,    44,    45,    46,
       0,    26,    27,    52,    28,    29,    30,    31,    32,    33,
       0,     0,    76,     0,     0,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,    44,    45,    46,    26,
      27,     0,    28,    29,    30,    31,    32,    33,     0,     0,
      84,     0,     0,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,    44,    45,    46,    26,    27,     0,
      28,    29,    30,    31,    32,    33,     0,     0,     0,     0,
      85,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,    44,    45,    46,    26,    27,     0,    28,    29,
      30,    31,    32,    33,     0,     0,     0,     0,     0,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
      44,    45,    46,    26,     0,     0,    28,    29,    30,    31,
      32,    33,     0,     0,     0,     0,     0,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,    44,    45,
      46,    28,    29,    30,    31,    32,    33,     0,     0,     0,
       0,     0,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,    44,    45,    46,    -1,    -1,    -1,    31,
      32,     0,     0,     0,     0,     0,     0,     0,     0,    -1,
      -1,    -1,    39,    40,    41,     0,    42,    43,    44,    45,
      46
};

static const yytype_int8 yycheck[] =
{
       7,    20,     9,    10,    11,    12,     3,    39,     3,    41,
       7,    18,     7,     0,    20,    12,    22,    23,    40,    26,
      27,    28,    29,    30,    31,    32,    35,    36,    37,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    -1,    49,    50,    33,    34,    35,    36,    37,     3,
       4,     5,     6,     7,     8,     9,    -1,    -1,    12,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    -1,     3,     4,    76,
       6,     7,     8,     9,    81,    -1,    12,    84,    32,    33,
      34,    -1,    -1,    19,    -1,    -1,    40,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    34,    -1,
      -1,    -1,    10,    11,    40,    13,    14,    15,    16,    17,
      18,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,    27,
      28,    29,    30,    31,    -1,    33,    34,    35,    36,    37,
      -1,    10,    11,    41,    13,    14,    15,    16,    17,    18,
      -1,    -1,    21,    -1,    -1,    24,    25,    26,    27,    28,
      29,    30,    31,    -1,    33,    34,    35,    36,    37,    10,
      11,    -1,    13,    14,    15,    16,    17,    18,    -1,    -1,
      21,    -1,    -1,    24,    25,    26,    27,    28,    29,    30,
      31,    -1,    33,    34,    35,    36,    37,    10,    11,    -1,
      13,    14,    15,    16,    17,    18,    -1,    -1,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    -1,
      33,    34,    35,    36,    37,    10,    11,    -1,    13,    14,
      15,    16,    17,    18,    -1,    -1,    -1,    -1,    -1,    24,
      25,    26,    27,    28,    29,    30,    31,    -1,    33,    34,
      35,    36,    37,    10,    -1,    -1,    13,    14,    15,    16,
      17,    18,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    -1,    33,    34,    35,    36,
      37,    13,    14,    15,    16,    17,    18,    -1,    -1,    -1,
      -1,    -1,    24,    25,    26,    27,    28,    29,    30,    31,
      -1,    33,    34,    35,    36,    37,    13,    14,    15,    16,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    26,
      27,    28,    29,    30,    31,    -1,    33,    34,    35,    36,
      37
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     4,     6,     7,     8,     9,    12,    19,    32,
      33,    34,    40,    43,    45,    47,    48,    45,    20,    46,
      45,    45,     5,    45,    45,     0,    10,    11,    13,    14,
      15,    16,    17,    18,    24,    25,    26,    27,    28,    29,
      30,    31,    33,    34,    35,    36,    37,    40,    45,    20,
      22,    23,    41,    45,    45,    45,    45,    45,    45,    45,
       3,     7,    12,    45,    45,    45,    45,    45,    45,    45,
      45,    45,    45,    45,    44,    45,    21,    45,    45,     3,
       7,    39,    41,    45,    21,    23,    45,    45
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    42,    43,    44,    44,    44,    45,    45,    45,    45,
      45,    45,    45,    45,    45,    45,    45,    45,    45,    45,
      45,    45,    45,    45,    45,    45,    45,    45,    45,    45,
      45,    45,    45,    45,    45,    45,    45,    45,    45,    45,
      45,    45,    45,    46,    46,    47,    47,    48
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     1,     3,     3,     2,     2,     2,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       2,     2,     3,     4,     3,     4,     1,     1,     1,     1,
       1,     4,     1,     5,     4,     3,     5,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
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
      yyerror (yyscanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



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

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, yyscanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t yyscanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yyscanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t yyscanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yyscanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, yyscan_t yyscanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , yyscanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, yyscanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
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
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
            /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
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
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
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
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
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
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, yyscan_t yyscanner)
{
  YYUSE (yyvaluep);
  YYUSE (yyscanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (yyscan_t yyscanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, yyscanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
| yyreduce -- Do a reduction.  |
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


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 82 "exprparse.y" /* yacc.c:1646  */
    { expr_parse_result = (yyvsp[0].expr); }
#line 1384 "exprparse.c" /* yacc.c:1646  */
    break;

  case 3:
#line 84 "exprparse.y" /* yacc.c:1646  */
    { (yyval.elist) = NULL; }
#line 1390 "exprparse.c" /* yacc.c:1646  */
    break;

  case 4:
#line 85 "exprparse.y" /* yacc.c:1646  */
    { (yyval.elist) = make_elist((yyvsp[0].expr), NULL); }
#line 1396 "exprparse.c" /* yacc.c:1646  */
    break;

  case 5:
#line 86 "exprparse.y" /* yacc.c:1646  */
    { (yyval.elist) = make_elist((yyvsp[0].expr), (yyvsp[-2].elist)); }
#line 1402 "exprparse.c" /* yacc.c:1646  */
    break;

  case 6:
#line 89 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = (yyvsp[-1].expr); }
#line 1408 "exprparse.c" /* yacc.c:1646  */
    break;

  case 7:
#line 90 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = (yyvsp[0].expr); }
#line 1414 "exprparse.c" /* yacc.c:1646  */
    break;

  case 8:
#line 92 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "-",
										   make_integer_constant(0), (yyvsp[0].expr)); }
#line 1421 "exprparse.c" /* yacc.c:1646  */
    break;

  case 9:
#line 96 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_integer_constant(PG_INT64_MIN); }
#line 1427 "exprparse.c" /* yacc.c:1646  */
    break;

  case 10:
#line 98 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "#",
										   make_integer_constant(~INT64CONST(0)), (yyvsp[0].expr)); }
#line 1434 "exprparse.c" /* yacc.c:1646  */
    break;

  case 11:
#line 100 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_uop(yyscanner, "!not", (yyvsp[0].expr)); }
#line 1440 "exprparse.c" /* yacc.c:1646  */
    break;

  case 12:
#line 101 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "+", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1446 "exprparse.c" /* yacc.c:1646  */
    break;

  case 13:
#line 102 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "-", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1452 "exprparse.c" /* yacc.c:1646  */
    break;

  case 14:
#line 103 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "*", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1458 "exprparse.c" /* yacc.c:1646  */
    break;

  case 15:
#line 104 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "/", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1464 "exprparse.c" /* yacc.c:1646  */
    break;

  case 16:
#line 105 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "mod", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1470 "exprparse.c" /* yacc.c:1646  */
    break;

  case 17:
#line 106 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1476 "exprparse.c" /* yacc.c:1646  */
    break;

  case 18:
#line 107 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<=", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1482 "exprparse.c" /* yacc.c:1646  */
    break;

  case 19:
#line 108 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<", (yyvsp[0].expr), (yyvsp[-2].expr)); }
#line 1488 "exprparse.c" /* yacc.c:1646  */
    break;

  case 20:
#line 109 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<=", (yyvsp[0].expr), (yyvsp[-2].expr)); }
#line 1494 "exprparse.c" /* yacc.c:1646  */
    break;

  case 21:
#line 110 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "=", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1500 "exprparse.c" /* yacc.c:1646  */
    break;

  case 22:
#line 111 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<>", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1506 "exprparse.c" /* yacc.c:1646  */
    break;

  case 23:
#line 112 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "&", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1512 "exprparse.c" /* yacc.c:1646  */
    break;

  case 24:
#line 113 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "|", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1518 "exprparse.c" /* yacc.c:1646  */
    break;

  case 25:
#line 114 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "#", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1524 "exprparse.c" /* yacc.c:1646  */
    break;

  case 26:
#line 115 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "<<", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1530 "exprparse.c" /* yacc.c:1646  */
    break;

  case 27:
#line 116 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, ">>", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1536 "exprparse.c" /* yacc.c:1646  */
    break;

  case 28:
#line 117 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "!and", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1542 "exprparse.c" /* yacc.c:1646  */
    break;

  case 29:
#line 118 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "!or", (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1548 "exprparse.c" /* yacc.c:1646  */
    break;

  case 30:
#line 120 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-1].expr), make_null_constant()); }
#line 1554 "exprparse.c" /* yacc.c:1646  */
    break;

  case 31:
#line 121 "exprparse.y" /* yacc.c:1646  */
    {
								(yyval.expr) = make_uop(yyscanner, "!not",
											  make_op(yyscanner, "!is", (yyvsp[-1].expr), make_null_constant()));
							}
#line 1563 "exprparse.c" /* yacc.c:1646  */
    break;

  case 32:
#line 125 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-2].expr), make_null_constant()); }
#line 1569 "exprparse.c" /* yacc.c:1646  */
    break;

  case 33:
#line 127 "exprparse.y" /* yacc.c:1646  */
    {
								(yyval.expr) = make_uop(yyscanner, "!not",
											  make_op(yyscanner, "!is", (yyvsp[-3].expr), make_null_constant()));
							}
#line 1578 "exprparse.c" /* yacc.c:1646  */
    break;

  case 34:
#line 132 "exprparse.y" /* yacc.c:1646  */
    {
								(yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-2].expr), make_boolean_constant((yyvsp[0].bval)));
							}
#line 1586 "exprparse.c" /* yacc.c:1646  */
    break;

  case 35:
#line 136 "exprparse.y" /* yacc.c:1646  */
    {
								(yyval.expr) = make_uop(yyscanner, "!not",
											  make_op(yyscanner, "!is", (yyvsp[-3].expr), make_boolean_constant((yyvsp[0].bval))));
							}
#line 1595 "exprparse.c" /* yacc.c:1646  */
    break;

  case 36:
#line 141 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_null_constant(); }
#line 1601 "exprparse.c" /* yacc.c:1646  */
    break;

  case 37:
#line 142 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_boolean_constant((yyvsp[0].bval)); }
#line 1607 "exprparse.c" /* yacc.c:1646  */
    break;

  case 38:
#line 143 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_integer_constant((yyvsp[0].ival)); }
#line 1613 "exprparse.c" /* yacc.c:1646  */
    break;

  case 39:
#line 144 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_double_constant((yyvsp[0].dval)); }
#line 1619 "exprparse.c" /* yacc.c:1646  */
    break;

  case 40:
#line 146 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_variable((yyvsp[0].str)); }
#line 1625 "exprparse.c" /* yacc.c:1646  */
    break;

  case 41:
#line 147 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_func(yyscanner, (yyvsp[-3].ival), (yyvsp[-1].elist)); }
#line 1631 "exprparse.c" /* yacc.c:1646  */
    break;

  case 42:
#line 148 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = (yyvsp[0].expr); }
#line 1637 "exprparse.c" /* yacc.c:1646  */
    break;

  case 43:
#line 152 "exprparse.y" /* yacc.c:1646  */
    { (yyval.elist) = make_elist((yyvsp[0].expr), make_elist((yyvsp[-2].expr), (yyvsp[-4].elist))); }
#line 1643 "exprparse.c" /* yacc.c:1646  */
    break;

  case 44:
#line 153 "exprparse.y" /* yacc.c:1646  */
    { (yyval.elist) = make_elist((yyvsp[0].expr), make_elist((yyvsp[-2].expr), NULL)); }
#line 1649 "exprparse.c" /* yacc.c:1646  */
    break;

  case 45:
#line 156 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_case(yyscanner, (yyvsp[-1].elist), make_null_constant()); }
#line 1655 "exprparse.c" /* yacc.c:1646  */
    break;

  case 46:
#line 157 "exprparse.y" /* yacc.c:1646  */
    { (yyval.expr) = make_case(yyscanner, (yyvsp[-3].elist), (yyvsp[-1].expr)); }
#line 1661 "exprparse.c" /* yacc.c:1646  */
    break;

  case 47:
#line 159 "exprparse.y" /* yacc.c:1646  */
    { (yyval.ival) = find_func(yyscanner, (yyvsp[0].str)); pg_free((yyvsp[0].str)); }
#line 1667 "exprparse.c" /* yacc.c:1646  */
    break;


#line 1671 "exprparse.c" /* yacc.c:1646  */
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
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (yyscanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yyscanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



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
                      yytoken, &yylval, yyscanner);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

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

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yyscanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (yyscanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, yyscanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, yyscanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 162 "exprparse.y" /* yacc.c:1906  */


static PgBenchExpr *
make_null_constant(void)
{
	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	expr->etype = ENODE_CONSTANT;
	expr->u.constant.type = PGBT_NULL;
	expr->u.constant.u.ival = 0;
	return expr;
}

static PgBenchExpr *
make_integer_constant(int64 ival)
{
	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	expr->etype = ENODE_CONSTANT;
	expr->u.constant.type = PGBT_INT;
	expr->u.constant.u.ival = ival;
	return expr;
}

static PgBenchExpr *
make_double_constant(double dval)
{
	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	expr->etype = ENODE_CONSTANT;
	expr->u.constant.type = PGBT_DOUBLE;
	expr->u.constant.u.dval = dval;
	return expr;
}

static PgBenchExpr *
make_boolean_constant(bool bval)
{
	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	expr->etype = ENODE_CONSTANT;
	expr->u.constant.type = PGBT_BOOLEAN;
	expr->u.constant.u.bval = bval;
	return expr;
}

static PgBenchExpr *
make_variable(char *varname)
{
	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	expr->etype = ENODE_VARIABLE;
	expr->u.variable.varname = varname;
	return expr;
}

/* binary operators */
static PgBenchExpr *
make_op(yyscan_t yyscanner, const char *operator,
		PgBenchExpr *lexpr, PgBenchExpr *rexpr)
{
	return make_func(yyscanner, find_func(yyscanner, operator),
					 make_elist(rexpr, make_elist(lexpr, NULL)));
}

/* unary operator */
static PgBenchExpr *
make_uop(yyscan_t yyscanner, const char *operator, PgBenchExpr *expr)
{
	return make_func(yyscanner, find_func(yyscanner, operator), make_elist(expr, NULL));
}

/*
 * List of available functions:
 * - fname: function name, "!..." for special internal functions
 * - nargs: number of arguments. Special cases:
 *			- PGBENCH_NARGS_VARIABLE is a special value for least & greatest
 *			  meaning #args >= 1;
 *			- PGBENCH_NARGS_CASE is for the "CASE WHEN ..." function, which
 *			  has #args >= 3 and odd;
 * 			- PGBENCH_NARGS_HASH is for hash functions, which have one required
 *			  and one optional argument;
 * - tag: function identifier from PgBenchFunction enum
 */
static const struct
{
	const char *fname;
	int			nargs;
	PgBenchFunction tag;
}	PGBENCH_FUNCTIONS[] =
{
	/* parsed as operators, executed as functions */
	{
		"+", 2, PGBENCH_ADD
	},
	{
		"-", 2, PGBENCH_SUB
	},
	{
		"*", 2, PGBENCH_MUL
	},
	{
		"/", 2, PGBENCH_DIV
	},
	{
		"mod", 2, PGBENCH_MOD
	},
	/* actual functions */
	{
		"abs", 1, PGBENCH_ABS
	},
	{
		"least", PGBENCH_NARGS_VARIABLE, PGBENCH_LEAST
	},
	{
		"greatest", PGBENCH_NARGS_VARIABLE, PGBENCH_GREATEST
	},
	{
		"debug", 1, PGBENCH_DEBUG
	},
	{
		"pi", 0, PGBENCH_PI
	},
	{
		"sqrt", 1, PGBENCH_SQRT
	},
	{
		"ln", 1, PGBENCH_LN
	},
	{
		"exp", 1, PGBENCH_EXP
	},
	{
		"int", 1, PGBENCH_INT
	},
	{
		"double", 1, PGBENCH_DOUBLE
	},
	{
		"random", 2, PGBENCH_RANDOM
	},
	{
		"random_gaussian", 3, PGBENCH_RANDOM_GAUSSIAN
	},
	{
		"random_exponential", 3, PGBENCH_RANDOM_EXPONENTIAL
	},
	{
		"random_zipfian", 3, PGBENCH_RANDOM_ZIPFIAN
	},
	{
		"pow", 2, PGBENCH_POW
	},
	{
		"power", 2, PGBENCH_POW
	},
	/* logical operators */
	{
		"!and", 2, PGBENCH_AND
	},
	{
		"!or", 2, PGBENCH_OR
	},
	{
		"!not", 1, PGBENCH_NOT
	},
	/* bitwise integer operators */
	{
		"&", 2, PGBENCH_BITAND
	},
	{
		"|", 2, PGBENCH_BITOR
	},
	{
		"#", 2, PGBENCH_BITXOR
	},
	{
		"<<", 2, PGBENCH_LSHIFT
	},
	{
		">>", 2, PGBENCH_RSHIFT
	},
	/* comparison operators */
	{
		"=", 2, PGBENCH_EQ
	},
	{
		"<>", 2, PGBENCH_NE
	},
	{
		"<=", 2, PGBENCH_LE
	},
	{
		"<", 2, PGBENCH_LT
	},
	{
		"!is", 2, PGBENCH_IS
	},
	/* "case when ... then ... else ... end" construction */
	{
		"!case_end", PGBENCH_NARGS_CASE, PGBENCH_CASE
	},
	{
		"hash", PGBENCH_NARGS_HASH, PGBENCH_HASH_MURMUR2
	},
	{
		"hash_murmur2", PGBENCH_NARGS_HASH, PGBENCH_HASH_MURMUR2
	},
	{
		"hash_fnv1a", PGBENCH_NARGS_HASH, PGBENCH_HASH_FNV1A
	},
	/* keep as last array element */
	{
		NULL, 0, 0
	}
};

/*
 * Find a function from its name
 *
 * return the index of the function from the PGBENCH_FUNCTIONS array
 * or fail if the function is unknown.
 */
static int
find_func(yyscan_t yyscanner, const char *fname)
{
	int			i = 0;

	while (PGBENCH_FUNCTIONS[i].fname)
	{
		if (pg_strcasecmp(fname, PGBENCH_FUNCTIONS[i].fname) == 0)
			return i;
		i++;
	}

	expr_yyerror_more(yyscanner, "unexpected function name", fname);

	/* not reached */
	return -1;
}

/* Expression linked list builder */
static PgBenchExprList *
make_elist(PgBenchExpr *expr, PgBenchExprList *list)
{
	PgBenchExprLink *cons;

	if (list == NULL)
	{
		list = pg_malloc(sizeof(PgBenchExprList));
		list->head = NULL;
		list->tail = NULL;
	}

	cons = pg_malloc(sizeof(PgBenchExprLink));
	cons->expr = expr;
	cons->next = NULL;

	if (list->head == NULL)
		list->head = cons;
	else
		list->tail->next = cons;

	list->tail = cons;

	return list;
}

/* Return the length of an expression list */
static int
elist_length(PgBenchExprList *list)
{
	PgBenchExprLink *link = list != NULL ? list->head : NULL;
	int			len = 0;

	for (; link != NULL; link = link->next)
		len++;

	return len;
}

/* Build function call expression */
static PgBenchExpr *
make_func(yyscan_t yyscanner, int fnumber, PgBenchExprList *args)
{
	int len = elist_length(args);

	PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

	Assert(fnumber >= 0);

	/* validate arguments number including few special cases */
	switch (PGBENCH_FUNCTIONS[fnumber].nargs)
	{
		/* check at least one arg for least & greatest */
		case PGBENCH_NARGS_VARIABLE:
			if (len == 0)
				expr_yyerror_more(yyscanner, "at least one argument expected",
								  PGBENCH_FUNCTIONS[fnumber].fname);
			break;

		/* case (when ... then ...)+ (else ...)? end */
		case PGBENCH_NARGS_CASE:
			/* 'else' branch is always present, but could be a NULL-constant */
			if (len < 3 || len % 2 != 1)
				expr_yyerror_more(yyscanner,
								  "odd and >= 3 number of arguments expected",
								  "case control structure");
			break;

		/* hash functions with optional seed argument */
		case PGBENCH_NARGS_HASH:
			if (len < 1 || len > 2)
				expr_yyerror_more(yyscanner, "unexpected number of arguments",
								  PGBENCH_FUNCTIONS[fnumber].fname);

			if (len == 1)
			{
				PgBenchExpr *var = make_variable("default_seed");
				args = make_elist(var, args);
			}
			break;

		/* common case: positive arguments number */
		default:
			Assert(PGBENCH_FUNCTIONS[fnumber].nargs >= 0);

			if (PGBENCH_FUNCTIONS[fnumber].nargs != len)
				expr_yyerror_more(yyscanner, "unexpected number of arguments",
								  PGBENCH_FUNCTIONS[fnumber].fname);
	}

	expr->etype = ENODE_FUNCTION;
	expr->u.function.function = PGBENCH_FUNCTIONS[fnumber].tag;

	/* only the link is used, the head/tail is not useful anymore */
	expr->u.function.args = args != NULL ? args->head : NULL;
	if (args)
		pg_free(args);

	return expr;
}

static PgBenchExpr *
make_case(yyscan_t yyscanner, PgBenchExprList *when_then_list, PgBenchExpr *else_part)
{
	return make_func(yyscanner,
					 find_func(yyscanner, "!case_end"),
					 make_elist(else_part, when_then_list));
}

/*
 * exprscan.l is compiled as part of exprparse.y.  Currently, this is
 * unavoidable because exprparse does not create a .h file to export
 * its token symbols.  If these files ever grow large enough to be
 * worth compiling separately, that could be fixed; but for now it
 * seems like useless complication.
 */

/* First, get rid of "#define yyscan_t" from pgbench.h */
#undef yyscan_t
/* ... and the yylval macro, which flex will have its own definition for */
#undef yylval

#include "exprscan.c"
