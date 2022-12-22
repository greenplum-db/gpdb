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
#define yyparse         jsonpath_yyparse
#define yylex           jsonpath_yylex
#define yyerror         jsonpath_yyerror
#define yydebug         jsonpath_yydebug
#define yynerrs         jsonpath_yynerrs


/* Copy the first part of user declarations.  */
#line 1 "jsonpath_gram.y" /* yacc.c:339  */

/*-------------------------------------------------------------------------
 *
 * jsonpath_gram.y
 *	 Grammar definitions for jsonpath datatype
 *
 * Transforms tokenized jsonpath into tree of JsonPathParseItem structs.
 *
 * Copyright (c) 2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	src/backend/utils/adt/jsonpath_gram.y
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "regex/regex.h"
#include "utils/builtins.h"
#include "utils/jsonpath.h"

/* struct JsonPathString is shared between scan and gram */
typedef struct JsonPathString
{
	char	   *val;
	int			len;
	int			total;
}			JsonPathString;

union YYSTYPE;

/* flex 2.5.4 doesn't bother with a decl for this */
int	jsonpath_yylex(union YYSTYPE *yylval_param);
int	jsonpath_yyparse(JsonPathParseResult **result);
void jsonpath_yyerror(JsonPathParseResult **result, const char *message);

static JsonPathParseItem *makeItemType(JsonPathItemType type);
static JsonPathParseItem *makeItemString(JsonPathString *s);
static JsonPathParseItem *makeItemVariable(JsonPathString *s);
static JsonPathParseItem *makeItemKey(JsonPathString *s);
static JsonPathParseItem *makeItemNumeric(JsonPathString *s);
static JsonPathParseItem *makeItemBool(bool val);
static JsonPathParseItem *makeItemBinary(JsonPathItemType type,
										 JsonPathParseItem *la,
										 JsonPathParseItem *ra);
static JsonPathParseItem *makeItemUnary(JsonPathItemType type,
										JsonPathParseItem *a);
static JsonPathParseItem *makeItemList(List *list);
static JsonPathParseItem *makeIndexArray(List *list);
static JsonPathParseItem *makeAny(int first, int last);
static JsonPathParseItem *makeItemLikeRegex(JsonPathParseItem *expr,
											JsonPathString *pattern,
											JsonPathString *flags);

/*
 * Bison doesn't allocate anything that needs to live across parser calls,
 * so we can easily have it use palloc instead of malloc.  This prevents
 * memory leaks if we error out during parsing.  Note this only works with
 * bison >= 2.0.  However, in bison 1.875 the default is to use alloca()
 * if possible, so there's not really much problem anyhow, at least if
 * you're building with gcc.
 */
#define YYMALLOC palloc
#define YYFREE   pfree


#line 144 "jsonpath_gram.c" /* yacc.c:339  */

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
# define YYERROR_VERBOSE 1
#endif


/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int jsonpath_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    TO_P = 258,
    NULL_P = 259,
    TRUE_P = 260,
    FALSE_P = 261,
    IS_P = 262,
    UNKNOWN_P = 263,
    EXISTS_P = 264,
    IDENT_P = 265,
    STRING_P = 266,
    NUMERIC_P = 267,
    INT_P = 268,
    VARIABLE_P = 269,
    OR_P = 270,
    AND_P = 271,
    NOT_P = 272,
    LESS_P = 273,
    LESSEQUAL_P = 274,
    EQUAL_P = 275,
    NOTEQUAL_P = 276,
    GREATEREQUAL_P = 277,
    GREATER_P = 278,
    ANY_P = 279,
    STRICT_P = 280,
    LAX_P = 281,
    LAST_P = 282,
    STARTS_P = 283,
    WITH_P = 284,
    LIKE_REGEX_P = 285,
    FLAG_P = 286,
    ABS_P = 287,
    SIZE_P = 288,
    TYPE_P = 289,
    FLOOR_P = 290,
    DOUBLE_P = 291,
    CEILING_P = 292,
    KEYVALUE_P = 293,
    UMINUS = 294
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 80 "jsonpath_gram.y" /* yacc.c:355  */

	JsonPathString		str;
	List			   *elems;	/* list of JsonPathParseItem */
	List			   *indexs;	/* list of integers */
	JsonPathParseItem  *value;
	JsonPathParseResult *result;
	JsonPathItemType	optype;
	bool				boolean;
	int					integer;

#line 232 "jsonpath_gram.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int jsonpath_yyparse (JsonPathParseResult **result);



/* Copy the second part of user declarations.  */

#line 248 "jsonpath_gram.c" /* yacc.c:358  */

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
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   234

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  56
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  21
/* YYNRULES -- Number of rules.  */
#define YYNRULES  99
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  137

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   294

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,    47,    43,     2,     2,
      45,    46,    41,    39,    49,    40,    54,    42,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    55,    48,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    50,     2,    51,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    52,     2,    53,     2,     2,     2,     2,
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
      35,    36,    37,    38,    44
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   128,   128,   133,   137,   138,   142,   143,   144,   148,
     149,   150,   151,   152,   153,   154,   158,   159,   160,   161,
     162,   163,   167,   168,   172,   173,   174,   175,   176,   177,
     179,   181,   182,   187,   188,   192,   193,   194,   195,   199,
     200,   201,   202,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   218,   219,   223,   224,   228,   229,   233,   234,
     238,   239,   240,   245,   246,   247,   248,   249,   250,   254,
     258,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   284,   285,   286,   287,   288,   289,   290
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TO_P", "NULL_P", "TRUE_P", "FALSE_P",
  "IS_P", "UNKNOWN_P", "EXISTS_P", "IDENT_P", "STRING_P", "NUMERIC_P",
  "INT_P", "VARIABLE_P", "OR_P", "AND_P", "NOT_P", "LESS_P", "LESSEQUAL_P",
  "EQUAL_P", "NOTEQUAL_P", "GREATEREQUAL_P", "GREATER_P", "ANY_P",
  "STRICT_P", "LAX_P", "LAST_P", "STARTS_P", "WITH_P", "LIKE_REGEX_P",
  "FLAG_P", "ABS_P", "SIZE_P", "TYPE_P", "FLOOR_P", "DOUBLE_P",
  "CEILING_P", "KEYVALUE_P", "'+'", "'-'", "'*'", "'/'", "'%'", "UMINUS",
  "'('", "')'", "'$'", "'@'", "','", "'['", "']'", "'{'", "'}'", "'.'",
  "'?'", "$accept", "result", "expr_or_predicate", "mode", "scalar_value",
  "comp_op", "delimited_predicate", "predicate", "starts_with_initial",
  "path_primary", "accessor_expr", "expr", "index_elem", "index_list",
  "array_accessor", "any_level", "any_path", "accessor_op", "key",
  "key_name", "method", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,    43,
      45,    42,    47,    37,   294,    40,    41,    36,    64,    44,
      91,    93,   123,   125,    46,    63
};
# endif

#define YYPACT_NINF -46

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-46)))

#define YYTABLE_NINF -100

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      67,   -46,   -46,     8,    51,   -46,   -46,   -46,   -46,   -26,
     -46,   -46,   -46,   -46,    -3,   -46,   113,   113,    51,   -46,
     -46,   -46,   -46,   -46,    17,   -46,   -45,   191,   113,    51,
     -46,    51,   -46,   -46,    15,   162,    51,    51,    68,   138,
     -25,   -46,   -46,   -46,   -46,   -46,   -46,   -46,   -46,    42,
      14,   113,   113,   113,   113,   113,   113,    89,    20,   191,
      29,     4,   -45,    13,   -46,    -5,    -2,   -46,   -23,   -46,
     -46,   -46,   -46,   -46,   -46,   -46,   -46,   -46,    24,   -46,
     -46,   -46,   -46,   -46,   -46,   -46,    32,    38,    49,    52,
      59,    61,    69,   -46,   -46,   -46,   -46,    75,    51,     7,
      74,    60,    60,   -46,   -46,   -46,    46,   -46,   -46,   -45,
     104,   -46,   -46,   -46,   113,   113,   -46,   -11,    76,    54,
     -46,   -46,   -46,   110,   -46,    46,   -46,   -46,   -46,     0,
     -46,   -46,   -46,   -11,   -46,    70,   -46
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       8,     6,     7,     0,     0,     1,    10,    11,    12,     0,
       9,    13,    14,    15,     0,    38,     0,     0,     0,    36,
      37,     2,    35,    24,     5,    39,    43,     4,     0,     0,
      28,     0,    45,    46,     0,     0,     0,     0,     0,     0,
       0,    65,    42,    18,    20,    16,    17,    21,    19,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    22,    44,    27,    26,     0,    52,    54,     0,    72,
      73,    74,    75,    76,    77,    78,    70,    71,    60,    79,
      80,    88,    89,    90,    91,    92,    81,    82,    83,    84,
      85,    86,    87,    64,    66,    63,    69,     0,     0,     0,
      31,    47,    48,    49,    50,    51,    25,    23,    22,     0,
       0,    41,    40,    56,     0,     0,    57,     0,     0,     0,
      33,    34,    30,     0,    29,    53,    55,    58,    59,     0,
      67,    68,    32,     0,    61,     0,    62
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -46,   -46,   -46,   -46,   -46,   -46,   119,   -14,   -46,   -46,
     -46,    -4,    19,   -46,   -46,     3,   -46,   -19,   -46,   -46,
     -46
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     3,    21,     4,    22,    56,    23,    24,   122,    25,
      26,    59,    67,    68,    41,   129,    94,   111,    95,    96,
      97
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      27,   114,   127,   133,    34,    38,     9,    42,     5,    39,
      40,   110,    32,    33,    35,    58,   128,    60,   120,    28,
      98,   121,    63,    64,    57,   100,   115,    35,   116,    37,
      36,    37,    36,    37,    66,    36,    37,    51,    52,    53,
      54,    55,    29,   112,    36,    37,   113,   101,   102,   103,
     104,   105,   106,   134,    38,     6,     7,     8,    39,    40,
       9,    61,    10,    11,    12,    13,   108,    -3,    14,    36,
      37,    99,     6,     7,     8,   109,   117,   -93,    15,    10,
      11,    12,    13,   -94,   119,    51,    52,    53,    54,    55,
      16,    17,     1,     2,   -95,    15,    18,   -96,    19,    20,
     131,    53,    54,    55,   -97,   123,   -98,    16,    17,    65,
     125,    66,   124,    31,   -99,    19,    20,     6,     7,     8,
     118,   132,   130,   136,    10,    11,    12,    13,    51,    52,
      53,    54,    55,    30,   126,   107,   135,     0,     0,     0,
      15,    69,    70,    71,    72,    73,    74,    75,    76,    77,
       0,     0,    16,    17,     0,     0,     0,     0,    31,     0,
      19,    20,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,     0,     0,    93,
      43,    44,    45,    46,    47,    48,     0,     0,     0,     0,
      49,     0,    50,     0,     0,     0,     0,     0,     0,     0,
       0,    51,    52,    53,    54,    55,     0,     0,    62,    43,
      44,    45,    46,    47,    48,     0,     0,     0,     0,    49,
       0,    50,     0,     0,     0,     0,     0,     0,     0,     0,
      51,    52,    53,    54,    55
};

static const yytype_int16 yycheck[] =
{
       4,     3,    13,     3,    18,    50,     9,    26,     0,    54,
      55,     7,    16,    17,    18,    29,    27,    31,    11,    45,
      45,    14,    36,    37,    28,    11,    49,    31,    51,    16,
      15,    16,    15,    16,    38,    15,    16,    39,    40,    41,
      42,    43,    45,    62,    15,    16,    51,    51,    52,    53,
      54,    55,    56,    53,    50,     4,     5,     6,    54,    55,
       9,    46,    11,    12,    13,    14,    46,     0,    17,    15,
      16,    29,     4,     5,     6,    46,    52,    45,    27,    11,
      12,    13,    14,    45,    98,    39,    40,    41,    42,    43,
      39,    40,    25,    26,    45,    27,    45,    45,    47,    48,
      46,    41,    42,    43,    45,    31,    45,    39,    40,    41,
     114,   115,     8,    45,    45,    47,    48,     4,     5,     6,
      45,    11,    46,    53,    11,    12,    13,    14,    39,    40,
      41,    42,    43,    14,   115,    46,   133,    -1,    -1,    -1,
      27,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      -1,    -1,    39,    40,    -1,    -1,    -1,    -1,    45,    -1,
      47,    48,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    -1,    -1,    41,
      18,    19,    20,    21,    22,    23,    -1,    -1,    -1,    -1,
      28,    -1,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    39,    40,    41,    42,    43,    -1,    -1,    46,    18,
      19,    20,    21,    22,    23,    -1,    -1,    -1,    -1,    28,
      -1,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      39,    40,    41,    42,    43
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    25,    26,    57,    59,     0,     4,     5,     6,     9,
      11,    12,    13,    14,    17,    27,    39,    40,    45,    47,
      48,    58,    60,    62,    63,    65,    66,    67,    45,    45,
      62,    45,    67,    67,    63,    67,    15,    16,    50,    54,
      55,    70,    73,    18,    19,    20,    21,    22,    23,    28,
      30,    39,    40,    41,    42,    43,    61,    67,    63,    67,
      63,    46,    46,    63,    63,    41,    67,    68,    69,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    41,    72,    74,    75,    76,    45,    29,
      11,    67,    67,    67,    67,    67,    67,    46,    46,    46,
       7,    73,    73,    51,     3,    49,    51,    52,    45,    63,
      11,    14,    64,    31,     8,    67,    68,    13,    27,    71,
      46,    46,    11,     3,    53,    71,    53
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    56,    57,    57,    58,    58,    59,    59,    59,    60,
      60,    60,    60,    60,    60,    60,    61,    61,    61,    61,
      61,    61,    62,    62,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    64,    64,    65,    65,    65,    65,    66,
      66,    66,    66,    67,    67,    67,    67,    67,    67,    67,
      67,    67,    68,    68,    69,    69,    70,    70,    71,    71,
      72,    72,    72,    73,    73,    73,    73,    73,    73,    74,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    76,    76,    76,    76,    76,    76,    76
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     0,     1,     1,     1,     1,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     3,     4,     1,     3,     3,     3,     2,     5,
       4,     3,     5,     1,     1,     1,     1,     1,     1,     1,
       4,     4,     2,     1,     3,     2,     2,     3,     3,     3,
       3,     3,     1,     3,     1,     3,     3,     3,     1,     1,
       1,     4,     6,     2,     2,     1,     2,     4,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1
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
      yyerror (result, YY_("syntax error: cannot back up")); \
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
                  Type, Value, result); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, JsonPathParseResult **result)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (result);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, JsonPathParseResult **result)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, result);
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, JsonPathParseResult **result)
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
                                              , result);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, result); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, JsonPathParseResult **result)
{
  YYUSE (yyvaluep);
  YYUSE (result);
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
yyparse (JsonPathParseResult **result)
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
      yychar = yylex (&yylval);
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
#line 128 "jsonpath_gram.y" /* yacc.c:1646  */
    {
										*result = palloc(sizeof(JsonPathParseResult));
										(*result)->expr = (yyvsp[0].value);
										(*result)->lax = (yyvsp[-1].boolean);
									}
#line 1459 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 3:
#line 133 "jsonpath_gram.y" /* yacc.c:1646  */
    { *result = NULL; }
#line 1465 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 4:
#line 137 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1471 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 5:
#line 138 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1477 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 6:
#line 142 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.boolean) = false; }
#line 1483 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 7:
#line 143 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.boolean) = true; }
#line 1489 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 8:
#line 144 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.boolean) = true; }
#line 1495 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 9:
#line 148 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemString(&(yyvsp[0].str)); }
#line 1501 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 10:
#line 149 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemString(NULL); }
#line 1507 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 11:
#line 150 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBool(true); }
#line 1513 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 12:
#line 151 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBool(false); }
#line 1519 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 13:
#line 152 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemNumeric(&(yyvsp[0].str)); }
#line 1525 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 14:
#line 153 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemNumeric(&(yyvsp[0].str)); }
#line 1531 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 15:
#line 154 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemVariable(&(yyvsp[0].str)); }
#line 1537 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 16:
#line 158 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiEqual; }
#line 1543 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 17:
#line 159 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiNotEqual; }
#line 1549 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 18:
#line 160 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiLess; }
#line 1555 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 19:
#line 161 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiGreater; }
#line 1561 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 20:
#line 162 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiLessOrEqual; }
#line 1567 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 21:
#line 163 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiGreaterOrEqual; }
#line 1573 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 22:
#line 167 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[-1].value); }
#line 1579 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 23:
#line 168 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiExists, (yyvsp[-1].value)); }
#line 1585 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 24:
#line 172 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1591 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 25:
#line 173 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary((yyvsp[-1].optype), (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1597 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 26:
#line 174 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiAnd, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1603 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 27:
#line 175 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiOr, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1609 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 28:
#line 176 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiNot, (yyvsp[0].value)); }
#line 1615 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 29:
#line 178 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiIsUnknown, (yyvsp[-3].value)); }
#line 1621 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 30:
#line 180 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiStartsWith, (yyvsp[-3].value), (yyvsp[0].value)); }
#line 1627 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 31:
#line 181 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemLikeRegex((yyvsp[-2].value), &(yyvsp[0].str), NULL); }
#line 1633 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 32:
#line 183 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemLikeRegex((yyvsp[-4].value), &(yyvsp[-2].str), &(yyvsp[0].str)); }
#line 1639 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 33:
#line 187 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemString(&(yyvsp[0].str)); }
#line 1645 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 34:
#line 188 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemVariable(&(yyvsp[0].str)); }
#line 1651 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 35:
#line 192 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1657 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 36:
#line 193 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType(jpiRoot); }
#line 1663 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 37:
#line 194 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType(jpiCurrent); }
#line 1669 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 38:
#line 195 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType(jpiLast); }
#line 1675 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 39:
#line 199 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.elems) = list_make1((yyvsp[0].value)); }
#line 1681 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 40:
#line 200 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.elems) = list_make2((yyvsp[-2].value), (yyvsp[0].value)); }
#line 1687 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 41:
#line 201 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.elems) = list_make2((yyvsp[-2].value), (yyvsp[0].value)); }
#line 1693 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 42:
#line 202 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.elems) = lappend((yyvsp[-1].elems), (yyvsp[0].value)); }
#line 1699 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 43:
#line 206 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemList((yyvsp[0].elems)); }
#line 1705 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 44:
#line 207 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[-1].value); }
#line 1711 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 45:
#line 208 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiPlus, (yyvsp[0].value)); }
#line 1717 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 46:
#line 209 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiMinus, (yyvsp[0].value)); }
#line 1723 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 47:
#line 210 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiAdd, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1729 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 48:
#line 211 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiSub, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1735 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 49:
#line 212 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiMul, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1741 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 50:
#line 213 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiDiv, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1747 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 51:
#line 214 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiMod, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1753 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 52:
#line 218 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiSubscript, (yyvsp[0].value), NULL); }
#line 1759 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 53:
#line 219 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemBinary(jpiSubscript, (yyvsp[-2].value), (yyvsp[0].value)); }
#line 1765 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 54:
#line 223 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.indexs) = list_make1((yyvsp[0].value)); }
#line 1771 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 55:
#line 224 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.indexs) = lappend((yyvsp[-2].indexs), (yyvsp[0].value)); }
#line 1777 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 56:
#line 228 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType(jpiAnyArray); }
#line 1783 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 57:
#line 229 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeIndexArray((yyvsp[-1].indexs)); }
#line 1789 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 58:
#line 233 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.integer) = pg_atoi((yyvsp[0].str).val, 4, 0); }
#line 1795 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 59:
#line 234 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.integer) = -1; }
#line 1801 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 60:
#line 238 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeAny(0, -1); }
#line 1807 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 61:
#line 239 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeAny((yyvsp[-1].integer), (yyvsp[-1].integer)); }
#line 1813 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 62:
#line 241 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeAny((yyvsp[-3].integer), (yyvsp[-1].integer)); }
#line 1819 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 63:
#line 245 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1825 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 64:
#line 246 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType(jpiAnyKey); }
#line 1831 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 65:
#line 247 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1837 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 66:
#line 248 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = (yyvsp[0].value); }
#line 1843 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 67:
#line 249 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemType((yyvsp[-2].optype)); }
#line 1849 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 68:
#line 250 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemUnary(jpiFilter, (yyvsp[-1].value)); }
#line 1855 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 69:
#line 254 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.value) = makeItemKey(&(yyvsp[0].str)); }
#line 1861 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 93:
#line 284 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiAbs; }
#line 1867 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 94:
#line 285 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiSize; }
#line 1873 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 95:
#line 286 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiType; }
#line 1879 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 96:
#line 287 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiFloor; }
#line 1885 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 97:
#line 288 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiDouble; }
#line 1891 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 98:
#line 289 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiCeiling; }
#line 1897 "jsonpath_gram.c" /* yacc.c:1646  */
    break;

  case 99:
#line 290 "jsonpath_gram.y" /* yacc.c:1646  */
    { (yyval.optype) = jpiKeyValue; }
#line 1903 "jsonpath_gram.c" /* yacc.c:1646  */
    break;


#line 1907 "jsonpath_gram.c" /* yacc.c:1646  */
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
      yyerror (result, YY_("syntax error"));
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
        yyerror (result, yymsgp);
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
                      yytoken, &yylval, result);
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
                  yystos[yystate], yyvsp, result);
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
  yyerror (result, YY_("memory exhausted"));
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
                  yytoken, &yylval, result);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, result);
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
#line 292 "jsonpath_gram.y" /* yacc.c:1906  */


/*
 * The helper functions below allocate and fill JsonPathParseItem's of various
 * types.
 */

static JsonPathParseItem *
makeItemType(JsonPathItemType type)
{
	JsonPathParseItem  *v = palloc(sizeof(*v));

	CHECK_FOR_INTERRUPTS();

	v->type = type;
	v->next = NULL;

	return v;
}

static JsonPathParseItem *
makeItemString(JsonPathString *s)
{
	JsonPathParseItem  *v;

	if (s == NULL)
	{
		v = makeItemType(jpiNull);
	}
	else
	{
		v = makeItemType(jpiString);
		v->value.string.val = s->val;
		v->value.string.len = s->len;
	}

	return v;
}

static JsonPathParseItem *
makeItemVariable(JsonPathString *s)
{
	JsonPathParseItem  *v;

	v = makeItemType(jpiVariable);
	v->value.string.val = s->val;
	v->value.string.len = s->len;

	return v;
}

static JsonPathParseItem *
makeItemKey(JsonPathString *s)
{
	JsonPathParseItem  *v;

	v = makeItemString(s);
	v->type = jpiKey;

	return v;
}

static JsonPathParseItem *
makeItemNumeric(JsonPathString *s)
{
	JsonPathParseItem  *v;

	v = makeItemType(jpiNumeric);
	v->value.numeric =
		DatumGetNumeric(DirectFunctionCall3(numeric_in,
											CStringGetDatum(s->val),
											ObjectIdGetDatum(InvalidOid),
											Int32GetDatum(-1)));

	return v;
}

static JsonPathParseItem *
makeItemBool(bool val)
{
	JsonPathParseItem  *v = makeItemType(jpiBool);

	v->value.boolean = val;

	return v;
}

static JsonPathParseItem *
makeItemBinary(JsonPathItemType type, JsonPathParseItem *la, JsonPathParseItem *ra)
{
	JsonPathParseItem  *v = makeItemType(type);

	v->value.args.left = la;
	v->value.args.right = ra;

	return v;
}

static JsonPathParseItem *
makeItemUnary(JsonPathItemType type, JsonPathParseItem *a)
{
	JsonPathParseItem  *v;

	if (type == jpiPlus && a->type == jpiNumeric && !a->next)
		return a;

	if (type == jpiMinus && a->type == jpiNumeric && !a->next)
	{
		v = makeItemType(jpiNumeric);
		v->value.numeric =
			DatumGetNumeric(DirectFunctionCall1(numeric_uminus,
												NumericGetDatum(a->value.numeric)));
		return v;
	}

	v = makeItemType(type);

	v->value.arg = a;

	return v;
}

static JsonPathParseItem *
makeItemList(List *list)
{
	JsonPathParseItem  *head,
					   *end;
	ListCell		   *cell = list_head(list);

	head = end = (JsonPathParseItem *) lfirst(cell);

	if (!lnext(cell))
		return head;

	/* append items to the end of already existing list */
	while (end->next)
		end = end->next;

	for_each_cell(cell, lnext(cell))
	{
		JsonPathParseItem *c = (JsonPathParseItem *) lfirst(cell);

		end->next = c;
		end = c;
	}

	return head;
}

static JsonPathParseItem *
makeIndexArray(List *list)
{
	JsonPathParseItem  *v = makeItemType(jpiIndexArray);
	ListCell		   *cell;
	int					i = 0;

	Assert(list_length(list) > 0);
	v->value.array.nelems = list_length(list);

	v->value.array.elems = palloc(sizeof(v->value.array.elems[0]) *
								  v->value.array.nelems);

	foreach(cell, list)
	{
		JsonPathParseItem  *jpi = lfirst(cell);

		Assert(jpi->type == jpiSubscript);

		v->value.array.elems[i].from = jpi->value.args.left;
		v->value.array.elems[i++].to = jpi->value.args.right;
	}

	return v;
}

static JsonPathParseItem *
makeAny(int first, int last)
{
	JsonPathParseItem  *v = makeItemType(jpiAny);

	v->value.anybounds.first = (first >= 0) ? first : PG_UINT32_MAX;
	v->value.anybounds.last = (last >= 0) ? last : PG_UINT32_MAX;

	return v;
}

static JsonPathParseItem *
makeItemLikeRegex(JsonPathParseItem *expr, JsonPathString *pattern,
				  JsonPathString *flags)
{
	JsonPathParseItem  *v = makeItemType(jpiLikeRegex);
	int					i;
	int					cflags = REG_ADVANCED;

	v->value.like_regex.expr = expr;
	v->value.like_regex.pattern = pattern->val;
	v->value.like_regex.patternlen = pattern->len;
	v->value.like_regex.flags = 0;

	for (i = 0; flags && i < flags->len; i++)
	{
		switch (flags->val[i])
		{
			case 'i':
				v->value.like_regex.flags |= JSP_REGEX_ICASE;
				cflags |= REG_ICASE;
				break;
			case 's':
				v->value.like_regex.flags &= ~JSP_REGEX_MLINE;
				v->value.like_regex.flags |= JSP_REGEX_SLINE;
				cflags |= REG_NEWLINE;
				break;
			case 'm':
				v->value.like_regex.flags &= ~JSP_REGEX_SLINE;
				v->value.like_regex.flags |= JSP_REGEX_MLINE;
				cflags &= ~REG_NEWLINE;
				break;
			case 'x':
				v->value.like_regex.flags |= JSP_REGEX_WSPACE;
				cflags |= REG_EXPANDED;
				break;
			case 'q':
				v->value.like_regex.flags |= JSP_REGEX_QUOTE;
				if (!(v->value.like_regex.flags & (JSP_REGEX_MLINE | JSP_REGEX_SLINE | JSP_REGEX_WSPACE)))
				{
					cflags &= ~REG_ADVANCED;
					cflags |= REG_QUOTE;
				}
				break;
			default:
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid input syntax for type %s", "jsonpath"),
						 errdetail("unrecognized flag character \"%.*s\" in LIKE_REGEX predicate",
								   pg_mblen(flags->val + i), flags->val + i)));
				break;
		}
	}

	/* check regex validity */
	(void) RE_compile_and_cache(cstring_to_text_with_len(pattern->val,
														 pattern->len),
								cflags, DEFAULT_COLLATION_OID);

	return v;
}

/*
 * jsonpath_scan.l is compiled as part of jsonpath_gram.y.  Currently, this is
 * unavoidable because jsonpath_gram does not create a .h file to export its
 * token symbols.  If these files ever grow large enough to be worth compiling
 * separately, that could be fixed; but for now it seems like useless
 * complication.
 */

#include "jsonpath_scan.c"
