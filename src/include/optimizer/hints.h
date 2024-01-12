/*
 * Copied from pg_hint_plan.c
 */

extern "C" {
#include "postgres.h"
#include "nodes/pathnodes.h"
#include "utils/guc.h"
}

/* hint keyword of enum type*/
typedef enum HintKeyword
{
	HINT_KEYWORD_SEQSCAN,
	HINT_KEYWORD_INDEXSCAN,
	HINT_KEYWORD_INDEXSCANREGEXP,
	HINT_KEYWORD_BITMAPSCAN,
	HINT_KEYWORD_BITMAPSCANREGEXP,
	HINT_KEYWORD_TIDSCAN,
	HINT_KEYWORD_NOSEQSCAN,
	HINT_KEYWORD_NOINDEXSCAN,
	HINT_KEYWORD_NOBITMAPSCAN,
	HINT_KEYWORD_NOTIDSCAN,
	HINT_KEYWORD_INDEXONLYSCAN,
	HINT_KEYWORD_INDEXONLYSCANREGEXP,
	HINT_KEYWORD_NOINDEXONLYSCAN,

	HINT_KEYWORD_NESTLOOP,
	HINT_KEYWORD_MERGEJOIN,
	HINT_KEYWORD_HASHJOIN,
	HINT_KEYWORD_NONESTLOOP,
	HINT_KEYWORD_NOMERGEJOIN,
	HINT_KEYWORD_NOHASHJOIN,

	HINT_KEYWORD_LEADING,
	HINT_KEYWORD_SET,
	HINT_KEYWORD_ROWS,
	HINT_KEYWORD_PARALLEL,

	HINT_KEYWORD_UNRECOGNIZED
} HintKeyword;

#define SCAN_HINT_ACCEPTS_INDEX_NAMES(kw) \
	(kw == HINT_KEYWORD_INDEXSCAN ||			\
	 kw == HINT_KEYWORD_INDEXSCANREGEXP ||		\
	 kw == HINT_KEYWORD_INDEXONLYSCAN ||		\
	 kw == HINT_KEYWORD_INDEXONLYSCANREGEXP ||	\
	 kw == HINT_KEYWORD_BITMAPSCAN ||				\
	 kw == HINT_KEYWORD_BITMAPSCANREGEXP)


typedef struct Hint Hint;
typedef struct HintState HintState;

typedef Hint *(*HintCreateFunction) (const char *hint_str,
									 const char *keyword,
									 HintKeyword hint_keyword);
typedef void (*HintDeleteFunction) (Hint *hint);
typedef void (*HintDescFunction) (Hint *hint, StringInfo buf, bool nolf);
typedef int (*HintCmpFunction) (const Hint *a, const Hint *b);
typedef const char *(*HintParseFunction) (Hint *hint, HintState *hstate,
										  Query *parse, const char *str);

/* hint types */
#define NUM_HINT_TYPE	6
typedef enum HintType
{
	HINT_TYPE_SCAN_METHOD,
	HINT_TYPE_JOIN_METHOD,
	HINT_TYPE_LEADING,
	HINT_TYPE_SET,
	HINT_TYPE_ROWS,
	HINT_TYPE_PARALLEL
} HintType;

typedef enum HintTypeBitmap
{
	HINT_BM_SCAN_METHOD = 1,
	HINT_BM_PARALLEL = 2
} HintTypeBitmap;

/* hint status */
typedef enum HintStatus
{
	HINT_STATE_NOTUSED = 0,		/* specified relation not used in query */
	HINT_STATE_USED,			/* hint is used */
	HINT_STATE_DUPLICATION,		/* specified hint duplication */
	HINT_STATE_ERROR			/* execute error (parse error does not include
								 * it) */
} HintStatus;

/* common data for all hints. */
struct Hint
{
	const char		   *hint_str;		/* must not do pfree */
	const char		   *keyword;		/* must not do pfree */
	HintKeyword			hint_keyword;
	HintType			type;
	HintStatus			state;
	HintDeleteFunction	delete_func;
	HintDescFunction	desc_func;
	HintCmpFunction		cmp_func;
	HintParseFunction	parse_func;
};

/* scan method hints */
typedef struct ScanMethodHint
{
	Hint			base;
	char		   *relname;
	List		   *indexnames;
	bool			regexp;
	unsigned char	enforce_mask;
} ScanMethodHint;

typedef struct ParentIndexInfo
{
	bool		indisunique;
	Oid			method;
	List	   *column_names;
	char	   *expression_str;
	Oid		   *indcollation;
	Oid		   *opclass;
	int16	   *indoption;
	char	   *indpred_str;
} ParentIndexInfo;

/* join method hints */
typedef struct JoinMethodHint
{
	Hint			base;
	int				nrels;
	int				inner_nrels;
	char		  **relnames;
	unsigned char	enforce_mask;
	Relids			joinrelids;
	Relids			inner_joinrelids;
} JoinMethodHint;

/* join order hints */
typedef struct OuterInnerRels
{
	char   *relation;
	List   *outer_inner_pair;
} OuterInnerRels;

typedef struct LeadingHint
{
	Hint			base;
	List		   *relations;	/* relation names specified in Leading hint */
	OuterInnerRels *outer_inner;
} LeadingHint;

/* change a run-time parameter hints */
typedef struct SetHint
{
	Hint	base;
	char   *name;				/* name of variable */
	char   *value;
	List   *words;
} SetHint;

/* rows hints */
typedef enum RowsValueType {
	RVT_ABSOLUTE,		/* Rows(... #1000) */
	RVT_ADD,			/* Rows(... +1000) */
	RVT_SUB,			/* Rows(... -1000) */
	RVT_MULTI,			/* Rows(... *1.2) */
} RowsValueType;
typedef struct RowsHint
{
	Hint			base;
	int				nrels;
	int				inner_nrels;
	char		  **relnames;
	Relids			joinrelids;
	Relids			inner_joinrelids;
	char		   *rows_str;
	RowsValueType	value_type;
	double			rows;
} RowsHint;

/* parallel hints */
typedef struct ParallelHint
{
	Hint			base;
	char		   *relname;
	char		   *nworkers_str;	/* original string of nworkers */
	int				nworkers;		/* num of workers specified by Worker */
	bool			force_parallel;	/* force parallel scan */
} ParallelHint;

/*
 * Describes a context of hint processing.
 */
struct HintState
{
	char		   *hint_str;			/* original hint string */

	/* all hint */
	int				nall_hints;			/* # of valid all hints */
	int				max_all_hints;		/* # of slots for all hints */
	Hint		  **all_hints;			/* parsed all hints */

	/* # of each hints */
	int				num_hints[NUM_HINT_TYPE];

	/* for scan method hints */
	ScanMethodHint **scan_hints;		/* parsed scan hints */

	/* Initial values of parameters  */
	int				init_scan_mask;		/* enable_* mask */
	int				init_nworkers;		/* max_parallel_workers_per_gather */
	/* min_parallel_table_scan_size*/
	int				init_min_para_tablescan_size;
	/* min_parallel_index_scan_size*/
	int				init_min_para_indexscan_size;
	double			init_paratup_cost;	/* parallel_tuple_cost */
	double			init_parasetup_cost;/* parallel_setup_cost */

	PlannerInfo	   *current_root;		/* PlannerInfo for the followings */
	Index			parent_relid;		/* inherit parent of table relid */
	ScanMethodHint *parent_scan_hint;	/* scan hint for the parent */
	ParallelHint   *parent_parallel_hint; /* parallel hint for the parent */
	List		   *parent_index_infos; /* list of parent table's index */

	JoinMethodHint **join_hints;		/* parsed join hints */
	int				init_join_mask;		/* initial value join parameter */
	List		  **join_hint_level;
	LeadingHint	  **leading_hint;		/* parsed Leading hints */
	SetHint		  **set_hints;			/* parsed Set hints */
	GucContext		context;			/* which GUC parameters can we set? */
	RowsHint	  **rows_hints;			/* parsed Rows hints */
	ParallelHint  **parallel_hints;		/* parsed Parallel hints */
};
