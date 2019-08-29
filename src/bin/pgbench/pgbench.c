/*
 * pgbench.c
 *
 * A simple benchmark program for PostgreSQL
 * Originally written by Tatsuo Ishii and enhanced by many contributors.
 *
 * src/bin/pgbench/pgbench.c
 * Copyright (c) 2000-2019, PostgreSQL Global Development Group
 * ALL RIGHTS RESERVED;
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE AUTHOR OR DISTRIBUTORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND DISTRIBUTORS SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 */

#ifdef WIN32
#define FD_SETSIZE 1024			/* must set before winsock2.h is included */
#endif

#include "postgres_fe.h"
#include "common/int.h"
#include "common/logging.h"
#include "fe_utils/conditional.h"
#include "getopt_long.h"
#include "libpq-fe.h"
#include "portability/instr_time.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>		/* for getrlimit */
#endif

/* For testing, PGBENCH_USE_SELECT can be defined to force use of that code */
#if defined(HAVE_PPOLL) && !defined(PGBENCH_USE_SELECT)
#define POLL_USING_PPOLL
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#else							/* no ppoll(), so use select() */
#define POLL_USING_SELECT
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "pgbench.h"

#define ERRCODE_UNDEFINED_TABLE  "42P01"

/*
 * Hashing constants
 */
#define FNV_PRIME			UINT64CONST(0x100000001b3)
#define FNV_OFFSET_BASIS	UINT64CONST(0xcbf29ce484222325)
#define MM2_MUL				UINT64CONST(0xc6a4a7935bd1e995)
#define MM2_MUL_TIMES_8		UINT64CONST(0x35253c9ade8f4ca8)
#define MM2_ROT				47

/*
 * Multi-platform socket set implementations
 */

#ifdef POLL_USING_PPOLL
#define SOCKET_WAIT_METHOD "ppoll"

typedef struct socket_set
{
	int			maxfds;			/* allocated length of pollfds[] array */
	int			curfds;			/* number currently in use */
	struct pollfd pollfds[FLEXIBLE_ARRAY_MEMBER];
} socket_set;

#endif							/* POLL_USING_PPOLL */

#ifdef POLL_USING_SELECT
#define SOCKET_WAIT_METHOD "select"

typedef struct socket_set
{
	int			maxfd;			/* largest FD currently set in fds */
	fd_set		fds;
} socket_set;

#endif							/* POLL_USING_SELECT */

/*
 * Multi-platform pthread implementations
 */

#ifdef WIN32
/* Use native win32 threads on Windows */
typedef struct win32_pthread *pthread_t;
typedef int pthread_attr_t;

static int	pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
static int	pthread_join(pthread_t th, void **thread_return);
#elif defined(ENABLE_THREAD_SAFETY)
/* Use platform-dependent pthread capability */
#include <pthread.h>
#else
/* No threads implementation, use none (-j 1) */
#define pthread_t void *
#endif


/********************************************************************
 * some configurable parameters */

#define DEFAULT_INIT_STEPS "dtgvp"	/* default -I setting */

#define LOG_STEP_SECONDS	5	/* seconds between log messages */
#define DEFAULT_NXACTS	10		/* default nxacts */

#define MIN_GAUSSIAN_PARAM		2.0 /* minimum parameter for gauss */

#define MIN_ZIPFIAN_PARAM		1.001	/* minimum parameter for zipfian */
#define MAX_ZIPFIAN_PARAM		1000.0	/* maximum parameter for zipfian */

int			nxacts = 0;			/* number of transactions per client */
int			duration = 0;		/* duration in seconds */
int64		end_time = 0;		/* when to stop in micro seconds, under -T */

/*
 * scaling factor. for example, scale = 10 will make 1000000 tuples in
 * pgbench_accounts table.
 */
int			scale = 1;

/*
 * fillfactor. for example, fillfactor = 90 will use only 90 percent
 * space during inserts and leave 10 percent free.
 */
int			fillfactor = 100;

/*
 * use unlogged tables?
 */
bool		unlogged_tables = false;

/*
 * log sampling rate (1.0 = log everything, 0.0 = option not given)
 */
double		sample_rate = 0.0;

/*
 * When threads are throttled to a given rate limit, this is the target delay
 * to reach that rate in usec.  0 is the default and means no throttling.
 */
double		throttle_delay = 0;

/*
 * Transactions which take longer than this limit (in usec) are counted as
 * late, and reported as such, although they are completed anyway. When
 * throttling is enabled, execution time slots that are more than this late
 * are skipped altogether, and counted separately.
 */
int64		latency_limit = 0;

/*
 * tablespace selection
 */
char	   *tablespace = NULL;
char	   *index_tablespace = NULL;

/* random seed used to initialize base_random_sequence */
int64		random_seed = -1;

/*
 * end of configurable parameters
 *********************************************************************/

#define nbranches	1			/* Makes little sense to change this.  Change
								 * -s instead */
#define ntellers	10
#define naccounts	100000

/*
 * The scale factor at/beyond which 32bit integers are incapable of storing
 * 64bit values.
 *
 * Although the actual threshold is 21474, we use 20000 because it is easier to
 * document and remember, and isn't that far away from the real threshold.
 */
#define SCALE_32BIT_THRESHOLD 20000

bool		use_log;			/* log transaction latencies to a file */
bool		use_quiet;			/* quiet logging onto stderr */
int			agg_interval;		/* log aggregates instead of individual
								 * transactions */
bool		per_script_stats = false;	/* whether to collect stats per script */
int			progress = 0;		/* thread progress report every this seconds */
bool		progress_timestamp = false; /* progress report with Unix time */
int			nclients = 1;		/* number of clients */
int			nthreads = 1;		/* number of threads */
bool		is_connect;			/* establish connection for each transaction */
bool		report_per_command; /* report per-command latencies */
int			main_pid;			/* main process id used in log filename */

int			use_unique_key=1;	/* indexes will be primary key if set, otherwise non-unique indexes */

char	   *pghost = "";
char	   *pgport = "";
char	   *storage_clause = "appendonly=false";
char	   *login = NULL;
char	   *dbName;
char	   *logfile_prefix = NULL;
const char *progname;

#define WSEP '@'				/* weight separator */

volatile bool timer_exceeded = false;	/* flag from signal handler */

/*
 * Variable definitions.
 *
 * If a variable only has a string value, "svalue" is that value, and value is
 * "not set".  If the value is known, "value" contains the value (in any
 * variant).
 *
 * In this case "svalue" contains the string equivalent of the value, if we've
 * had occasion to compute that, or NULL if we haven't.
 */
typedef struct
{
	char	   *name;			/* variable's name */
	char	   *svalue;			/* its value in string form, if known */
	PgBenchValue value;			/* actual variable's value */
} Variable;

#define MAX_SCRIPTS		128		/* max number of SQL scripts allowed */
#define SHELL_COMMAND_SIZE	256 /* maximum size allowed for shell command */

/*
 * Simple data structure to keep stats about something.
 *
 * XXX probably the first value should be kept and used as an offset for
 * better numerical stability...
 */
typedef struct SimpleStats
{
	int64		count;			/* how many values were encountered */
	double		min;			/* the minimum seen */
	double		max;			/* the maximum seen */
	double		sum;			/* sum of values */
	double		sum2;			/* sum of squared values */
} SimpleStats;

/*
 * Data structure to hold various statistics: per-thread and per-script stats
 * are maintained and merged together.
 */
typedef struct StatsData
{
	time_t		start_time;		/* interval start time, for aggregates */
	int64		cnt;			/* number of transactions, including skipped */
	int64		skipped;		/* number of transactions skipped under --rate
								 * and --latency-limit */
	SimpleStats latency;
	SimpleStats lag;
} StatsData;

/*
 * Struct to keep random state.
 */
typedef struct RandomState
{
	unsigned short xseed[3];
} RandomState;

/* Various random sequences are initialized from this one. */
static RandomState base_random_sequence;

/*
 * Connection state machine states.
 */
typedef enum
{
	/*
	 * The client must first choose a script to execute.  Once chosen, it can
	 * either be throttled (state CSTATE_PREPARE_THROTTLE under --rate), start
	 * right away (state CSTATE_START_TX) or not start at all if the timer was
	 * exceeded (state CSTATE_FINISHED).
	 */
	CSTATE_CHOOSE_SCRIPT,

	/*
	 * CSTATE_START_TX performs start-of-transaction processing.  Establishes
	 * a new connection for the transaction in --connect mode, records the
	 * transaction start time, and proceed to the first command.
	 *
	 * Note: once a script is started, it will either error or run till its
	 * end, where it may be interrupted. It is not interrupted while running,
	 * so pgbench --time is to be understood as tx are allowed to start in
	 * that time, and will finish when their work is completed.
	 */
	CSTATE_START_TX,

	/*
	 * In CSTATE_PREPARE_THROTTLE state, we calculate when to begin the next
	 * transaction, and advance to CSTATE_THROTTLE.  CSTATE_THROTTLE state
	 * sleeps until that moment, then advances to CSTATE_START_TX, or
	 * CSTATE_FINISHED if the next transaction would start beyond the end of
	 * the run.
	 */
	CSTATE_PREPARE_THROTTLE,
	CSTATE_THROTTLE,

	/*
	 * We loop through these states, to process each command in the script:
	 *
	 * CSTATE_START_COMMAND starts the execution of a command.  On a SQL
	 * command, the command is sent to the server, and we move to
	 * CSTATE_WAIT_RESULT state.  On a \sleep meta-command, the timer is set,
	 * and we enter the CSTATE_SLEEP state to wait for it to expire. Other
	 * meta-commands are executed immediately.  If the command about to start
	 * is actually beyond the end of the script, advance to CSTATE_END_TX.
	 *
	 * CSTATE_WAIT_RESULT waits until we get a result set back from the server
	 * for the current command.
	 *
	 * CSTATE_SLEEP waits until the end of \sleep.
	 *
	 * CSTATE_END_COMMAND records the end-of-command timestamp, increments the
	 * command counter, and loops back to CSTATE_START_COMMAND state.
	 *
	 * CSTATE_SKIP_COMMAND is used by conditional branches which are not
	 * executed. It quickly skip commands that do not need any evaluation.
	 * This state can move forward several commands, till there is something
	 * to do or the end of the script.
	 */
	CSTATE_START_COMMAND,
	CSTATE_WAIT_RESULT,
	CSTATE_SLEEP,
	CSTATE_END_COMMAND,
	CSTATE_SKIP_COMMAND,

	/*
	 * CSTATE_END_TX performs end-of-transaction processing.  It calculates
	 * latency, and logs the transaction.  In --connect mode, it closes the
	 * current connection.
	 *
	 * Then either starts over in CSTATE_CHOOSE_SCRIPT, or enters
	 * CSTATE_FINISHED if we have no more work to do.
	 */
	CSTATE_END_TX,

	/*
	 * Final states.  CSTATE_ABORTED means that the script execution was
	 * aborted because a command failed, CSTATE_FINISHED means success.
	 */
	CSTATE_ABORTED,
	CSTATE_FINISHED
} ConnectionStateEnum;

/*
 * Connection state.
 */
typedef struct
{
	PGconn	   *con;			/* connection handle to DB */
	int			id;				/* client No. */
	ConnectionStateEnum state;	/* state machine's current state. */
	ConditionalStack cstack;	/* enclosing conditionals state */

	/*
	 * Separate randomness for each client. This is used for random functions
	 * PGBENCH_RANDOM_* during the execution of the script.
	 */
	RandomState cs_func_rs;

	int			use_file;		/* index in sql_script for this client */
	int			command;		/* command number in script */

	/* client variables */
	Variable   *variables;		/* array of variable definitions */
	int			nvariables;		/* number of variables */
	bool		vars_sorted;	/* are variables sorted by name? */

	/* various times about current transaction */
	int64		txn_scheduled;	/* scheduled start time of transaction (usec) */
	int64		sleep_until;	/* scheduled start time of next cmd (usec) */
	instr_time	txn_begin;		/* used for measuring schedule lag times */
	instr_time	stmt_begin;		/* used for measuring statement latencies */

	bool		prepared[MAX_SCRIPTS];	/* whether client prepared the script */

	/* per client collected stats */
	int64		cnt;			/* client transaction count, for -t */
	int			ecnt;			/* error count */
} CState;

/*
 * Thread state
 */
typedef struct
{
	int			tid;			/* thread id */
	pthread_t	thread;			/* thread handle */
	CState	   *state;			/* array of CState */
	int			nstate;			/* length of state[] */

	/*
	 * Separate randomness for each thread. Each thread option uses its own
	 * random state to make all of them independent of each other and
	 * therefore deterministic at the thread level.
	 */
	RandomState ts_choose_rs;	/* random state for selecting a script */
	RandomState ts_throttle_rs; /* random state for transaction throttling */
	RandomState ts_sample_rs;	/* random state for log sampling */

	int64		throttle_trigger;	/* previous/next throttling (us) */
	FILE	   *logfile;		/* where to log, or NULL */

	/* per thread collected stats */
	instr_time	start_time;		/* thread start time */
	instr_time	conn_time;
	StatsData	stats;
	int64		latency_late;	/* executed but late transactions */
} TState;

#define INVALID_THREAD		((pthread_t) 0)

/*
 * queries read from files
 */
#define SQL_COMMAND		1
#define META_COMMAND	2

/*
 * max number of backslash command arguments or SQL variables,
 * including the command or SQL statement itself
 */
#define MAX_ARGS		256

typedef enum MetaCommand
{
	META_NONE,					/* not a known meta-command */
	META_SET,					/* \set */
	META_SETSHELL,				/* \setshell */
	META_SHELL,					/* \shell */
	META_SLEEP,					/* \sleep */
	META_GSET,					/* \gset */
	META_IF,					/* \if */
	META_ELIF,					/* \elif */
	META_ELSE,					/* \else */
	META_ENDIF					/* \endif */
} MetaCommand;

typedef enum QueryMode
{
	QUERY_SIMPLE,				/* simple query */
	QUERY_EXTENDED,				/* extended query */
	QUERY_PREPARED,				/* extended query with prepared statements */
	NUM_QUERYMODE
} QueryMode;

static QueryMode querymode = QUERY_SIMPLE;
static const char *QUERYMODE[] = {"simple", "extended", "prepared"};

/*
 * struct Command represents one command in a script.
 *
 * lines		The raw, possibly multi-line command text.  Variable substitution
 *				not applied.
 * first_line	A short, single-line extract of 'lines', for error reporting.
 * type			SQL_COMMAND or META_COMMAND
 * meta			The type of meta-command, or META_NONE if command is SQL
 * argc			Number of arguments of the command, 0 if not yet processed.
 * argv			Command arguments, the first of which is the command or SQL
 *				string itself.  For SQL commands, after post-processing
 *				argv[0] is the same as 'lines' with variables substituted.
 * varprefix 	SQL commands terminated with \gset have this set
 *				to a non NULL value.  If nonempty, it's used to prefix the
 *				variable name that receives the value.
 * expr			Parsed expression, if needed.
 * stats		Time spent in this command.
 */
typedef struct Command
{
	PQExpBufferData lines;
	char	   *first_line;
	int			type;
	MetaCommand meta;
	int			argc;
	char	   *argv[MAX_ARGS];
	char	   *varprefix;
	PgBenchExpr *expr;
	SimpleStats stats;
} Command;

typedef struct ParsedScript
{
	const char *desc;			/* script descriptor (eg, file name) */
	int			weight;			/* selection weight */
	Command   **commands;		/* NULL-terminated array of Commands */
	StatsData	stats;			/* total time spent in script */
} ParsedScript;

static ParsedScript sql_script[MAX_SCRIPTS];	/* SQL script files */
static int	num_scripts;		/* number of scripts in sql_script[] */
static int64 total_weight = 0;

static int	debug = 0;			/* debug flag */

/* Builtin test scripts */
typedef struct BuiltinScript
{
	const char *name;			/* very short name for -b ... */
	const char *desc;			/* short description */
	const char *script;			/* actual pgbench script */
} BuiltinScript;

static const BuiltinScript builtin_script[] =
{
	{
		"tpcb-like",
		"<builtin: TPC-B (sort of)>",
		"\\set aid random(1, " CppAsString2(naccounts) " * :scale)\n"
		"\\set bid random(1, " CppAsString2(nbranches) " * :scale)\n"
		"\\set tid random(1, " CppAsString2(ntellers) " * :scale)\n"
		"\\set delta random(-5000, 5000)\n"
		"BEGIN;\n"
		"UPDATE pgbench_accounts SET abalance = abalance + :delta WHERE aid = :aid;\n"
		"SELECT abalance FROM pgbench_accounts WHERE aid = :aid;\n"
		"UPDATE pgbench_tellers SET tbalance = tbalance + :delta WHERE tid = :tid;\n"
		"UPDATE pgbench_branches SET bbalance = bbalance + :delta WHERE bid = :bid;\n"
		"INSERT INTO pgbench_history (tid, bid, aid, delta, mtime) VALUES (:tid, :bid, :aid, :delta, CURRENT_TIMESTAMP);\n"
		"END;\n"
	},
	{
		"simple-update",
		"<builtin: simple update>",
		"\\set aid random(1, " CppAsString2(naccounts) " * :scale)\n"
		"\\set bid random(1, " CppAsString2(nbranches) " * :scale)\n"
		"\\set tid random(1, " CppAsString2(ntellers) " * :scale)\n"
		"\\set delta random(-5000, 5000)\n"
		"BEGIN;\n"
		"UPDATE pgbench_accounts SET abalance = abalance + :delta WHERE aid = :aid;\n"
		"SELECT abalance FROM pgbench_accounts WHERE aid = :aid;\n"
		"INSERT INTO pgbench_history (tid, bid, aid, delta, mtime) VALUES (:tid, :bid, :aid, :delta, CURRENT_TIMESTAMP);\n"
		"END;\n"
	},
	{
		"select-only",
		"<builtin: select only>",
		"\\set aid random(1, " CppAsString2(naccounts) " * :scale)\n"
		"SELECT abalance FROM pgbench_accounts WHERE aid = :aid;\n"
	}
};


/* Function prototypes */
static void setNullValue(PgBenchValue *pv);
static void setBoolValue(PgBenchValue *pv, bool bval);
static void setIntValue(PgBenchValue *pv, int64 ival);
static void setDoubleValue(PgBenchValue *pv, double dval);
static bool evaluateExpr(CState *st, PgBenchExpr *expr,
						 PgBenchValue *retval);
static ConnectionStateEnum executeMetaCommand(CState *st, instr_time *now);
static void doLog(TState *thread, CState *st,
				  StatsData *agg, bool skipped, double latency, double lag);
static void processXactStats(TState *thread, CState *st, instr_time *now,
							 bool skipped, StatsData *agg);
static void addScript(ParsedScript script);
static void *threadRun(void *arg);
static void finishCon(CState *st);
static void setalarm(int seconds);
static socket_set *alloc_socket_set(int count);
static void free_socket_set(socket_set *sa);
static void clear_socket_set(socket_set *sa);
static void add_socket_to_set(socket_set *sa, int fd, int idx);
static int	wait_on_socket_set(socket_set *sa, int64 usecs);
static bool socket_has_input(socket_set *sa, int fd, int idx);


/* callback functions for our flex lexer */
static const PsqlScanCallbacks pgbench_callbacks = {
	NULL,						/* don't need get_variable functionality */
};


static void
usage(void)
{
	printf("%s is a benchmarking tool for PostgreSQL.\n\n"
		   "Usage:\n"
		   "  %s [OPTION]... [DBNAME]\n"
		   "\nInitialization options:\n"
		   "  -i, --initialize         invokes initialization mode\n"
		   "  -x STRING    append this string to the storage clause e.g. 'appendonly=true, orientation=column'\n"
		   "  -I, --init-steps=[dtgvpf]+ (default \"dtgvp\")\n"
		   "                           run selected initialization steps\n"
		   "  -F, --fillfactor=NUM     set fill factor\n"
		   "  -n, --no-vacuum          do not run VACUUM during initialization\n"
		   "  -q, --quiet              quiet logging (one message each 5 seconds)\n"
		   "  -s, --scale=NUM          scaling factor\n"
		   "  --foreign-keys           create foreign key constraints between tables\n"
		   "  --use-unique-keys        make the indexes that are created non-unique indexes\n"
		   "                           (default: unique)\n"
		   "  --index-tablespace=TABLESPACE\n"
		   "                           create indexes in the specified tablespace\n"
		   "  --tablespace=TABLESPACE  create tables in the specified tablespace\n"
		   "  --unlogged-tables        create tables as unlogged tables\n"
		   "\nOptions to select what to run:\n"
		   "  -b, --builtin=NAME[@W]   add builtin script NAME weighted at W (default: 1)\n"
		   "                           (use \"-b list\" to list available scripts)\n"
		   "  -f, --file=FILENAME[@W]  add script FILENAME weighted at W (default: 1)\n"
		   "  -N, --skip-some-updates  skip updates of pgbench_tellers and pgbench_branches\n"
		   "                           (same as \"-b simple-update\")\n"
		   "  -S, --select-only        perform SELECT-only transactions\n"
		   "                           (same as \"-b select-only\")\n"
		   "\nBenchmarking options:\n"
		   "  -c, --client=NUM         number of concurrent database clients (default: 1)\n"
		   "  -C, --connect            establish new connection for each transaction\n"
		   "  -D, --define=VARNAME=VALUE\n"
		   "                           define variable for use by custom script\n"
		   "  -j, --jobs=NUM           number of threads (default: 1)\n"
		   "  -l, --log                write transaction times to log file\n"
		   "  -L, --latency-limit=NUM  count transactions lasting more than NUM ms as late\n"
		   "  -M, --protocol=simple|extended|prepared\n"
		   "                           protocol for submitting queries (default: simple)\n"
		   "  -n, --no-vacuum          do not run VACUUM before tests\n"
		   "  -P, --progress=NUM       show thread progress report every NUM seconds\n"
		   "  -r, --report-latencies   report average latency per command\n"
		   "  -R, --rate=NUM           target rate in transactions per second\n"
		   "  -s, --scale=NUM          report this scale factor in output\n"
		   "  -t, --transactions=NUM   number of transactions each client runs (default: 10)\n"
		   "  -T, --time=NUM           duration of benchmark test in seconds\n"
		   "  -v, --vacuum-all         vacuum all four standard tables before tests\n"
		   "  --aggregate-interval=NUM aggregate data over NUM seconds\n"
		   "  --log-prefix=PREFIX      prefix for transaction time log file\n"
		   "                           (default: \"pgbench_log\")\n"
		   "  --progress-timestamp     use Unix epoch timestamps for progress\n"
		   "  --random-seed=SEED       set random seed (\"time\", \"rand\", integer)\n"
		   "  --sampling-rate=NUM      fraction of transactions to log (e.g., 0.01 for 1%%)\n"
		   "\nCommon options:\n"
		   "  -d, --debug              print debugging output\n"
		   "  -h, --host=HOSTNAME      database server host or socket directory\n"
		   "  -p, --port=PORT          database server port number\n"
		   "  -U, --username=USERNAME  connect as specified database user\n"
		   "  -V, --version            output version information, then exit\n"
		   "  -?, --help               show this help, then exit\n"
		   "\n"
		   "Report bugs to <bugs@greenplum.org>.\n",
		   progname, progname);
}

/* return whether str matches "^\s*[-+]?[0-9]+$" */
static bool
is_an_int(const char *str)
{
	const char *ptr = str;

	/* skip leading spaces; cast is consistent with strtoint64 */
	while (*ptr && isspace((unsigned char) *ptr))
		ptr++;

	/* skip sign */
	if (*ptr == '+' || *ptr == '-')
		ptr++;

	/* at least one digit */
	if (*ptr && !isdigit((unsigned char) *ptr))
		return false;

	/* eat all digits */
	while (*ptr && isdigit((unsigned char) *ptr))
		ptr++;

	/* must have reached end of string */
	return *ptr == '\0';
}


/*
 * strtoint64 -- convert a string to 64-bit integer
 *
 * This function is a slightly modified version of scanint8() from
 * src/backend/utils/adt/int8.c.
 *
 * The function returns whether the conversion worked, and if so
 * "*result" is set to the result.
 *
 * If not errorOK, an error message is also printed out on errors.
 */
bool
strtoint64(const char *str, bool errorOK, int64 *result)
{
	const char *ptr = str;
	int64		tmp = 0;
	bool		neg = false;

	/*
	 * Do our own scan, rather than relying on sscanf which might be broken
	 * for long long.
	 *
	 * As INT64_MIN can't be stored as a positive 64 bit integer, accumulate
	 * value as a negative number.
	 */

	/* skip leading spaces */
	while (*ptr && isspace((unsigned char) *ptr))
		ptr++;

	/* handle sign */
	if (*ptr == '-')
	{
		ptr++;
		neg = true;
	}
	else if (*ptr == '+')
		ptr++;

	/* require at least one digit */
	if (unlikely(!isdigit((unsigned char) *ptr)))
		goto invalid_syntax;

	/* process digits */
	while (*ptr && isdigit((unsigned char) *ptr))
	{
		int8		digit = (*ptr++ - '0');

		if (unlikely(pg_mul_s64_overflow(tmp, 10, &tmp)) ||
			unlikely(pg_sub_s64_overflow(tmp, digit, &tmp)))
			goto out_of_range;
	}

	/* allow trailing whitespace, but not other trailing chars */
	while (*ptr != '\0' && isspace((unsigned char) *ptr))
		ptr++;

	if (unlikely(*ptr != '\0'))
		goto invalid_syntax;

	if (!neg)
	{
		if (unlikely(tmp == PG_INT64_MIN))
			goto out_of_range;
		tmp = -tmp;
	}

	*result = tmp;
	return true;

out_of_range:
	if (!errorOK)
		fprintf(stderr,
				"value \"%s\" is out of range for type bigint\n", str);
	return false;

invalid_syntax:
	if (!errorOK)
		fprintf(stderr,
				"invalid input syntax for type bigint: \"%s\"\n", str);
	return false;
}

/* convert string to double, detecting overflows/underflows */
bool
strtodouble(const char *str, bool errorOK, double *dv)
{
	char	   *end;

	errno = 0;
	*dv = strtod(str, &end);

	if (unlikely(errno != 0))
	{
		if (!errorOK)
			fprintf(stderr,
					"value \"%s\" is out of range for type double\n", str);
		return false;
	}

	if (unlikely(end == str || *end != '\0'))
	{
		if (!errorOK)
			fprintf(stderr,
					"invalid input syntax for type double: \"%s\"\n", str);
		return false;
	}
	return true;
}

/*
 * Initialize a random state struct.
 *
 * We derive the seed from base_random_sequence, which must be set up already.
 */
static void
initRandomState(RandomState *random_state)
{
	random_state->xseed[0] = (unsigned short)
		(pg_jrand48(base_random_sequence.xseed) & 0xFFFF);
	random_state->xseed[1] = (unsigned short)
		(pg_jrand48(base_random_sequence.xseed) & 0xFFFF);
	random_state->xseed[2] = (unsigned short)
		(pg_jrand48(base_random_sequence.xseed) & 0xFFFF);
}

/*
 * Random number generator: uniform distribution from min to max inclusive.
 *
 * Although the limits are expressed as int64, you can't generate the full
 * int64 range in one call, because the difference of the limits mustn't
 * overflow int64.  In practice it's unwise to ask for more than an int32
 * range, because of the limited precision of pg_erand48().
 */
static int64
getrand(RandomState *random_state, int64 min, int64 max)
{
	/*
	 * Odd coding is so that min and max have approximately the same chance of
	 * being selected as do numbers between them.
	 *
	 * pg_erand48() is thread-safe and concurrent, which is why we use it
	 * rather than random(), which in glibc is non-reentrant, and therefore
	 * protected by a mutex, and therefore a bottleneck on machines with many
	 * CPUs.
	 */
	return min + (int64) ((max - min + 1) * pg_erand48(random_state->xseed));
}

/*
 * random number generator: exponential distribution from min to max inclusive.
 * the parameter is so that the density of probability for the last cut-off max
 * value is exp(-parameter).
 */
static int64
getExponentialRand(RandomState *random_state, int64 min, int64 max,
				   double parameter)
{
	double		cut,
				uniform,
				rand;

	/* abort if wrong parameter, but must really be checked beforehand */
	Assert(parameter > 0.0);
	cut = exp(-parameter);
	/* erand in [0, 1), uniform in (0, 1] */
	uniform = 1.0 - pg_erand48(random_state->xseed);

	/*
	 * inner expression in (cut, 1] (if parameter > 0), rand in [0, 1)
	 */
	Assert((1.0 - cut) != 0.0);
	rand = -log(cut + (1.0 - cut) * uniform) / parameter;
	/* return int64 random number within between min and max */
	return min + (int64) ((max - min + 1) * rand);
}

/* random number generator: gaussian distribution from min to max inclusive */
static int64
getGaussianRand(RandomState *random_state, int64 min, int64 max,
				double parameter)
{
	double		stdev;
	double		rand;

	/* abort if parameter is too low, but must really be checked beforehand */
	Assert(parameter >= MIN_GAUSSIAN_PARAM);

	/*
	 * Get user specified random number from this loop, with -parameter <
	 * stdev <= parameter
	 *
	 * This loop is executed until the number is in the expected range.
	 *
	 * As the minimum parameter is 2.0, the probability of looping is low:
	 * sqrt(-2 ln(r)) <= 2 => r >= e^{-2} ~ 0.135, then when taking the
	 * average sinus multiplier as 2/pi, we have a 8.6% looping probability in
	 * the worst case. For a parameter value of 5.0, the looping probability
	 * is about e^{-5} * 2 / pi ~ 0.43%.
	 */
	do
	{
		/*
		 * pg_erand48 generates [0,1), but for the basic version of the
		 * Box-Muller transform the two uniformly distributed random numbers
		 * are expected in (0, 1] (see
		 * https://en.wikipedia.org/wiki/Box-Muller_transform)
		 */
		double		rand1 = 1.0 - pg_erand48(random_state->xseed);
		double		rand2 = 1.0 - pg_erand48(random_state->xseed);

		/* Box-Muller basic form transform */
		double		var_sqrt = sqrt(-2.0 * log(rand1));

		stdev = var_sqrt * sin(2.0 * M_PI * rand2);

		/*
		 * we may try with cos, but there may be a bias induced if the
		 * previous value fails the test. To be on the safe side, let us try
		 * over.
		 */
	}
	while (stdev < -parameter || stdev >= parameter);

	/* stdev is in [-parameter, parameter), normalization to [0,1) */
	rand = (stdev + parameter) / (parameter * 2.0);

	/* return int64 random number within between min and max */
	return min + (int64) ((max - min + 1) * rand);
}

/*
 * random number generator: generate a value, such that the series of values
 * will approximate a Poisson distribution centered on the given value.
 *
 * Individual results are rounded to integers, though the center value need
 * not be one.
 */
static int64
getPoissonRand(RandomState *random_state, double center)
{
	/*
	 * Use inverse transform sampling to generate a value > 0, such that the
	 * expected (i.e. average) value is the given argument.
	 */
	double		uniform;

	/* erand in [0, 1), uniform in (0, 1] */
	uniform = 1.0 - pg_erand48(random_state->xseed);

	return (int64) (-log(uniform) * center + 0.5);
}

/*
 * Computing zipfian using rejection method, based on
 * "Non-Uniform Random Variate Generation",
 * Luc Devroye, p. 550-551, Springer 1986.
 *
 * This works for s > 1.0, but may perform badly for s very close to 1.0.
 */
static int64
computeIterativeZipfian(RandomState *random_state, int64 n, double s)
{
	double		b = pow(2.0, s - 1.0);
	double		x,
				t,
				u,
				v;

	/* Ensure n is sane */
	if (n <= 1)
		return 1;

	while (true)
	{
		/* random variates */
		u = pg_erand48(random_state->xseed);
		v = pg_erand48(random_state->xseed);

		x = floor(pow(u, -1.0 / (s - 1.0)));

		t = pow(1.0 + 1.0 / x, s - 1.0);
		/* reject if too large or out of bound */
		if (v * x * (t - 1.0) / (b - 1.0) <= t / b && x <= n)
			break;
	}
	return (int64) x;
}

/* random number generator: zipfian distribution from min to max inclusive */
static int64
getZipfianRand(RandomState *random_state, int64 min, int64 max, double s)
{
	int64		n = max - min + 1;

	/* abort if parameter is invalid */
	Assert(MIN_ZIPFIAN_PARAM <= s && s <= MAX_ZIPFIAN_PARAM);

	return min - 1 + computeIterativeZipfian(random_state, n, s);
}

/*
 * FNV-1a hash function
 */
static int64
getHashFnv1a(int64 val, uint64 seed)
{
	int64		result;
	int			i;

	result = FNV_OFFSET_BASIS ^ seed;
	for (i = 0; i < 8; ++i)
	{
		int32		octet = val & 0xff;

		val = val >> 8;
		result = result ^ octet;
		result = result * FNV_PRIME;
	}

	return result;
}

/*
 * Murmur2 hash function
 *
 * Based on original work of Austin Appleby
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
 */
static int64
getHashMurmur2(int64 val, uint64 seed)
{
	uint64		result = seed ^ MM2_MUL_TIMES_8;	/* sizeof(int64) */
	uint64		k = (uint64) val;

	k *= MM2_MUL;
	k ^= k >> MM2_ROT;
	k *= MM2_MUL;

	result ^= k;
	result *= MM2_MUL;

	result ^= result >> MM2_ROT;
	result *= MM2_MUL;
	result ^= result >> MM2_ROT;

	return (int64) result;
}

/*
 * Initialize the given SimpleStats struct to all zeroes
 */
static void
initSimpleStats(SimpleStats *ss)
{
	memset(ss, 0, sizeof(SimpleStats));
}

/*
 * Accumulate one value into a SimpleStats struct.
 */
static void
addToSimpleStats(SimpleStats *ss, double val)
{
	if (ss->count == 0 || val < ss->min)
		ss->min = val;
	if (ss->count == 0 || val > ss->max)
		ss->max = val;
	ss->count++;
	ss->sum += val;
	ss->sum2 += val * val;
}

/*
 * Merge two SimpleStats objects
 */
static void
mergeSimpleStats(SimpleStats *acc, SimpleStats *ss)
{
	if (acc->count == 0 || ss->min < acc->min)
		acc->min = ss->min;
	if (acc->count == 0 || ss->max > acc->max)
		acc->max = ss->max;
	acc->count += ss->count;
	acc->sum += ss->sum;
	acc->sum2 += ss->sum2;
}

/*
 * Initialize a StatsData struct to mostly zeroes, with its start time set to
 * the given value.
 */
static void
initStats(StatsData *sd, time_t start_time)
{
	sd->start_time = start_time;
	sd->cnt = 0;
	sd->skipped = 0;
	initSimpleStats(&sd->latency);
	initSimpleStats(&sd->lag);
}

/*
 * Accumulate one additional item into the given stats object.
 */
static void
accumStats(StatsData *stats, bool skipped, double lat, double lag)
{
	stats->cnt++;

	if (skipped)
	{
		/* no latency to record on skipped transactions */
		stats->skipped++;
	}
	else
	{
		addToSimpleStats(&stats->latency, lat);

		/* and possibly the same for schedule lag */
		if (throttle_delay)
			addToSimpleStats(&stats->lag, lag);
	}
}

/* call PQexec() and exit() on failure */
static void
executeStatement(PGconn *con, const char *sql)
{
	PGresult   *res;

	res = PQexec(con, sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "%s", PQerrorMessage(con));
		exit(1);
	}
	PQclear(res);
}

/* call PQexec() and complain, but without exiting, on failure */
static void
tryExecuteStatement(PGconn *con, const char *sql)
{
	PGresult   *res;

	res = PQexec(con, sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "%s", PQerrorMessage(con));
		fprintf(stderr, "(ignoring this error and continuing anyway)\n");
	}
	PQclear(res);
}

/* set up a connection to the backend */
static PGconn *
doConnect(void)
{
	PGconn	   *conn;
	bool		new_pass;
	static bool have_password = false;
	static char password[100];

	/*
	 * Start the connection.  Loop until we have a password if requested by
	 * backend.
	 */
	do
	{
#define PARAMS_ARRAY_SIZE	7

		const char *keywords[PARAMS_ARRAY_SIZE];
		const char *values[PARAMS_ARRAY_SIZE];

		keywords[0] = "host";
		values[0] = pghost;
		keywords[1] = "port";
		values[1] = pgport;
		keywords[2] = "user";
		values[2] = login;
		keywords[3] = "password";
		values[3] = have_password ? password : NULL;
		keywords[4] = "dbname";
		values[4] = dbName;
		keywords[5] = "fallback_application_name";
		values[5] = progname;
		keywords[6] = NULL;
		values[6] = NULL;

		new_pass = false;

		conn = PQconnectdbParams(keywords, values, true);

		if (!conn)
		{
			fprintf(stderr, "connection to database \"%s\" failed\n",
					dbName);
			return NULL;
		}

		if (PQstatus(conn) == CONNECTION_BAD &&
			PQconnectionNeedsPassword(conn) &&
			!have_password)
		{
			PQfinish(conn);
			simple_prompt("Password: ", password, sizeof(password), false);
			have_password = true;
			new_pass = true;
		}
	} while (new_pass);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "connection to database \"%s\" failed:\n%s",
				PQdb(conn), PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
	}

	return conn;
}

/* qsort comparator for Variable array */
static int
compareVariableNames(const void *v1, const void *v2)
{
	return strcmp(((const Variable *) v1)->name,
				  ((const Variable *) v2)->name);
}

/* Locate a variable by name; returns NULL if unknown */
static Variable *
lookupVariable(CState *st, char *name)
{
	Variable	key;

	/* On some versions of Solaris, bsearch of zero items dumps core */
	if (st->nvariables <= 0)
		return NULL;

	/* Sort if we have to */
	if (!st->vars_sorted)
	{
		qsort((void *) st->variables, st->nvariables, sizeof(Variable),
			  compareVariableNames);
		st->vars_sorted = true;
	}

	/* Now we can search */
	key.name = name;
	return (Variable *) bsearch((void *) &key,
								(void *) st->variables,
								st->nvariables,
								sizeof(Variable),
								compareVariableNames);
}

/* Get the value of a variable, in string form; returns NULL if unknown */
static char *
getVariable(CState *st, char *name)
{
	Variable   *var;
	char		stringform[64];

	var = lookupVariable(st, name);
	if (var == NULL)
		return NULL;			/* not found */

	if (var->svalue)
		return var->svalue;		/* we have it in string form */

	/* We need to produce a string equivalent of the value */
	Assert(var->value.type != PGBT_NO_VALUE);
	if (var->value.type == PGBT_NULL)
		snprintf(stringform, sizeof(stringform), "NULL");
	else if (var->value.type == PGBT_BOOLEAN)
		snprintf(stringform, sizeof(stringform),
				 "%s", var->value.u.bval ? "true" : "false");
	else if (var->value.type == PGBT_INT)
		snprintf(stringform, sizeof(stringform),
				 INT64_FORMAT, var->value.u.ival);
	else if (var->value.type == PGBT_DOUBLE)
		snprintf(stringform, sizeof(stringform),
				 "%.*g", DBL_DIG, var->value.u.dval);
	else						/* internal error, unexpected type */
		Assert(0);
	var->svalue = pg_strdup(stringform);
	return var->svalue;
}

/* Try to convert variable to a value; return false on failure */
static bool
makeVariableValue(Variable *var)
{
	size_t		slen;

	if (var->value.type != PGBT_NO_VALUE)
		return true;			/* no work */

	slen = strlen(var->svalue);

	if (slen == 0)
		/* what should it do on ""? */
		return false;

	if (pg_strcasecmp(var->svalue, "null") == 0)
	{
		setNullValue(&var->value);
	}

	/*
	 * accept prefixes such as y, ye, n, no... but not for "o". 0/1 are
	 * recognized later as an int, which is converted to bool if needed.
	 */
	else if (pg_strncasecmp(var->svalue, "true", slen) == 0 ||
			 pg_strncasecmp(var->svalue, "yes", slen) == 0 ||
			 pg_strcasecmp(var->svalue, "on") == 0)
	{
		setBoolValue(&var->value, true);
	}
	else if (pg_strncasecmp(var->svalue, "false", slen) == 0 ||
			 pg_strncasecmp(var->svalue, "no", slen) == 0 ||
			 pg_strcasecmp(var->svalue, "off") == 0 ||
			 pg_strcasecmp(var->svalue, "of") == 0)
	{
		setBoolValue(&var->value, false);
	}
	else if (is_an_int(var->svalue))
	{
		/* if it looks like an int, it must be an int without overflow */
		int64		iv;

		if (!strtoint64(var->svalue, false, &iv))
			return false;

		setIntValue(&var->value, iv);
	}
	else						/* type should be double */
	{
		double		dv;

		if (!strtodouble(var->svalue, true, &dv))
		{
			fprintf(stderr,
					"malformed variable \"%s\" value: \"%s\"\n",
					var->name, var->svalue);
			return false;
		}
		setDoubleValue(&var->value, dv);
	}
	return true;
}

/*
 * Check whether a variable's name is allowed.
 *
 * We allow any non-ASCII character, as well as ASCII letters, digits, and
 * underscore.
 *
 * Keep this in sync with the definitions of variable name characters in
 * "src/fe_utils/psqlscan.l", "src/bin/psql/psqlscanslash.l" and
 * "src/bin/pgbench/exprscan.l".  Also see parseVariable(), below.
 *
 * Note: this static function is copied from "src/bin/psql/variables.c"
 * but changed to disallow variable names starting with a digit.
 */
static bool
valid_variable_name(const char *name)
{
	const unsigned char *ptr = (const unsigned char *) name;

	/* Mustn't be zero-length */
	if (*ptr == '\0')
		return false;

	/* must not start with [0-9] */
	if (IS_HIGHBIT_SET(*ptr) ||
		strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
			   "_", *ptr) != NULL)
		ptr++;
	else
		return false;

	/* remaining characters can include [0-9] */
	while (*ptr)
	{
		if (IS_HIGHBIT_SET(*ptr) ||
			strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
				   "_0123456789", *ptr) != NULL)
			ptr++;
		else
			return false;
	}

	return true;
}

/*
 * Lookup a variable by name, creating it if need be.
 * Caller is expected to assign a value to the variable.
 * Returns NULL on failure (bad name).
 */
static Variable *
lookupCreateVariable(CState *st, const char *context, char *name)
{
	Variable   *var;

	var = lookupVariable(st, name);
	if (var == NULL)
	{
		Variable   *newvars;

		/*
		 * Check for the name only when declaring a new variable to avoid
		 * overhead.
		 */
		if (!valid_variable_name(name))
		{
			fprintf(stderr, "%s: invalid variable name: \"%s\"\n",
					context, name);
			return NULL;
		}

		/* Create variable at the end of the array */
		if (st->variables)
			newvars = (Variable *) pg_realloc(st->variables,
											  (st->nvariables + 1) * sizeof(Variable));
		else
			newvars = (Variable *) pg_malloc(sizeof(Variable));

		st->variables = newvars;

		var = &newvars[st->nvariables];

		var->name = pg_strdup(name);
		var->svalue = NULL;
		/* caller is expected to initialize remaining fields */

		st->nvariables++;
		/* we don't re-sort the array till we have to */
		st->vars_sorted = false;
	}

	return var;
}

/* Assign a string value to a variable, creating it if need be */
/* Returns false on failure (bad name) */
static bool
putVariable(CState *st, const char *context, char *name, const char *value)
{
	Variable   *var;
	char	   *val;

	var = lookupCreateVariable(st, context, name);
	if (!var)
		return false;

	/* dup then free, in case value is pointing at this variable */
	val = pg_strdup(value);

	if (var->svalue)
		free(var->svalue);
	var->svalue = val;
	var->value.type = PGBT_NO_VALUE;

	return true;
}

/* Assign a value to a variable, creating it if need be */
/* Returns false on failure (bad name) */
static bool
putVariableValue(CState *st, const char *context, char *name,
				 const PgBenchValue *value)
{
	Variable   *var;

	var = lookupCreateVariable(st, context, name);
	if (!var)
		return false;

	if (var->svalue)
		free(var->svalue);
	var->svalue = NULL;
	var->value = *value;

	return true;
}

/* Assign an integer value to a variable, creating it if need be */
/* Returns false on failure (bad name) */
static bool
putVariableInt(CState *st, const char *context, char *name, int64 value)
{
	PgBenchValue val;

	setIntValue(&val, value);
	return putVariableValue(st, context, name, &val);
}

/*
 * Parse a possible variable reference (:varname).
 *
 * "sql" points at a colon.  If what follows it looks like a valid
 * variable name, return a malloc'd string containing the variable name,
 * and set *eaten to the number of characters consumed (including the colon).
 * Otherwise, return NULL.
 */
static char *
parseVariable(const char *sql, int *eaten)
{
	int			i = 1;			/* starting at 1 skips the colon */
	char	   *name;

	/* keep this logic in sync with valid_variable_name() */
	if (IS_HIGHBIT_SET(sql[i]) ||
		strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
			   "_", sql[i]) != NULL)
		i++;
	else
		return NULL;

	while (IS_HIGHBIT_SET(sql[i]) ||
		   strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
				  "_0123456789", sql[i]) != NULL)
		i++;

	name = pg_malloc(i);
	memcpy(name, &sql[1], i - 1);
	name[i - 1] = '\0';

	*eaten = i;
	return name;
}

static char *
replaceVariable(char **sql, char *param, int len, char *value)
{
	int			valueln = strlen(value);

	if (valueln > len)
	{
		size_t		offset = param - *sql;

		*sql = pg_realloc(*sql, strlen(*sql) - len + valueln + 1);
		param = *sql + offset;
	}

	if (valueln != len)
		memmove(param + valueln, param + len, strlen(param + len) + 1);
	memcpy(param, value, valueln);

	return param + valueln;
}

static char *
assignVariables(CState *st, char *sql)
{
	char	   *p,
			   *name,
			   *val;

	p = sql;
	while ((p = strchr(p, ':')) != NULL)
	{
		int			eaten;

		name = parseVariable(p, &eaten);
		if (name == NULL)
		{
			while (*p == ':')
			{
				p++;
			}
			continue;
		}

		val = getVariable(st, name);
		free(name);
		if (val == NULL)
		{
			p++;
			continue;
		}

		p = replaceVariable(&sql, p, eaten, val);
	}

	return sql;
}

static void
getQueryParams(CState *st, const Command *command, const char **params)
{
	int			i;

	for (i = 0; i < command->argc - 1; i++)
		params[i] = getVariable(st, command->argv[i + 1]);
}

static char *
valueTypeName(PgBenchValue *pval)
{
	if (pval->type == PGBT_NO_VALUE)
		return "none";
	else if (pval->type == PGBT_NULL)
		return "null";
	else if (pval->type == PGBT_INT)
		return "int";
	else if (pval->type == PGBT_DOUBLE)
		return "double";
	else if (pval->type == PGBT_BOOLEAN)
		return "boolean";
	else
	{
		/* internal error, should never get there */
		Assert(false);
		return NULL;
	}
}

/* get a value as a boolean, or tell if there is a problem */
static bool
coerceToBool(PgBenchValue *pval, bool *bval)
{
	if (pval->type == PGBT_BOOLEAN)
	{
		*bval = pval->u.bval;
		return true;
	}
	else						/* NULL, INT or DOUBLE */
	{
		fprintf(stderr, "cannot coerce %s to boolean\n", valueTypeName(pval));
		*bval = false;			/* suppress uninitialized-variable warnings */
		return false;
	}
}

/*
 * Return true or false from an expression for conditional purposes.
 * Non zero numerical values are true, zero and NULL are false.
 */
static bool
valueTruth(PgBenchValue *pval)
{
	switch (pval->type)
	{
		case PGBT_NULL:
			return false;
		case PGBT_BOOLEAN:
			return pval->u.bval;
		case PGBT_INT:
			return pval->u.ival != 0;
		case PGBT_DOUBLE:
			return pval->u.dval != 0.0;
		default:
			/* internal error, unexpected type */
			Assert(0);
			return false;
	}
}

/* get a value as an int, tell if there is a problem */
static bool
coerceToInt(PgBenchValue *pval, int64 *ival)
{
	if (pval->type == PGBT_INT)
	{
		*ival = pval->u.ival;
		return true;
	}
	else if (pval->type == PGBT_DOUBLE)
	{
		double		dval = rint(pval->u.dval);

		if (isnan(dval) || !FLOAT8_FITS_IN_INT64(dval))
		{
			fprintf(stderr, "double to int overflow for %f\n", dval);
			return false;
		}
		*ival = (int64) dval;
		return true;
	}
	else						/* BOOLEAN or NULL */
	{
		fprintf(stderr, "cannot coerce %s to int\n", valueTypeName(pval));
		return false;
	}
}

/* get a value as a double, or tell if there is a problem */
static bool
coerceToDouble(PgBenchValue *pval, double *dval)
{
	if (pval->type == PGBT_DOUBLE)
	{
		*dval = pval->u.dval;
		return true;
	}
	else if (pval->type == PGBT_INT)
	{
		*dval = (double) pval->u.ival;
		return true;
	}
	else						/* BOOLEAN or NULL */
	{
		fprintf(stderr, "cannot coerce %s to double\n", valueTypeName(pval));
		return false;
	}
}

/* assign a null value */
static void
setNullValue(PgBenchValue *pv)
{
	pv->type = PGBT_NULL;
	pv->u.ival = 0;
}

/* assign a boolean value */
static void
setBoolValue(PgBenchValue *pv, bool bval)
{
	pv->type = PGBT_BOOLEAN;
	pv->u.bval = bval;
}

/* assign an integer value */
static void
setIntValue(PgBenchValue *pv, int64 ival)
{
	pv->type = PGBT_INT;
	pv->u.ival = ival;
}

/* assign a double value */
static void
setDoubleValue(PgBenchValue *pv, double dval)
{
	pv->type = PGBT_DOUBLE;
	pv->u.dval = dval;
}

static bool
isLazyFunc(PgBenchFunction func)
{
	return func == PGBENCH_AND || func == PGBENCH_OR || func == PGBENCH_CASE;
}

/* lazy evaluation of some functions */
static bool
evalLazyFunc(CState *st,
			 PgBenchFunction func, PgBenchExprLink *args, PgBenchValue *retval)
{
	PgBenchValue a1,
				a2;
	bool		ba1,
				ba2;

	Assert(isLazyFunc(func) && args != NULL && args->next != NULL);

	/* args points to first condition */
	if (!evaluateExpr(st, args->expr, &a1))
		return false;

	/* second condition for AND/OR and corresponding branch for CASE */
	args = args->next;

	switch (func)
	{
		case PGBENCH_AND:
			if (a1.type == PGBT_NULL)
			{
				setNullValue(retval);
				return true;
			}

			if (!coerceToBool(&a1, &ba1))
				return false;

			if (!ba1)
			{
				setBoolValue(retval, false);
				return true;
			}

			if (!evaluateExpr(st, args->expr, &a2))
				return false;

			if (a2.type == PGBT_NULL)
			{
				setNullValue(retval);
				return true;
			}
			else if (!coerceToBool(&a2, &ba2))
				return false;
			else
			{
				setBoolValue(retval, ba2);
				return true;
			}

			return true;

		case PGBENCH_OR:

			if (a1.type == PGBT_NULL)
			{
				setNullValue(retval);
				return true;
			}

			if (!coerceToBool(&a1, &ba1))
				return false;

			if (ba1)
			{
				setBoolValue(retval, true);
				return true;
			}

			if (!evaluateExpr(st, args->expr, &a2))
				return false;

			if (a2.type == PGBT_NULL)
			{
				setNullValue(retval);
				return true;
			}
			else if (!coerceToBool(&a2, &ba2))
				return false;
			else
			{
				setBoolValue(retval, ba2);
				return true;
			}

		case PGBENCH_CASE:
			/* when true, execute branch */
			if (valueTruth(&a1))
				return evaluateExpr(st, args->expr, retval);

			/* now args contains next condition or final else expression */
			args = args->next;

			/* final else case? */
			if (args->next == NULL)
				return evaluateExpr(st, args->expr, retval);

			/* no, another when, proceed */
			return evalLazyFunc(st, PGBENCH_CASE, args, retval);

		default:
			/* internal error, cannot get here */
			Assert(0);
			break;
	}
	return false;
}

/* maximum number of function arguments */
#define MAX_FARGS 16

/*
 * Recursive evaluation of standard functions,
 * which do not require lazy evaluation.
 */
static bool
evalStandardFunc(CState *st,
				 PgBenchFunction func, PgBenchExprLink *args,
				 PgBenchValue *retval)
{
	/* evaluate all function arguments */
	int			nargs = 0;
	PgBenchValue vargs[MAX_FARGS];
	PgBenchExprLink *l = args;
	bool		has_null = false;

	/* Some compiler (gcc-12) may raise warning about uninitialized variable */
	memset(vargs, 0, sizeof(vargs));

	for (nargs = 0; nargs < MAX_FARGS && l != NULL; nargs++, l = l->next)
	{
		if (!evaluateExpr(st, l->expr, &vargs[nargs]))
			return false;
		has_null |= vargs[nargs].type == PGBT_NULL;
	}

	if (l != NULL)
	{
		fprintf(stderr,
				"too many function arguments, maximum is %d\n", MAX_FARGS);
		return false;
	}

	/* NULL arguments */
	if (has_null && func != PGBENCH_IS && func != PGBENCH_DEBUG)
	{
		setNullValue(retval);
		return true;
	}

	/* then evaluate function */
	switch (func)
	{
			/* overloaded operators */
		case PGBENCH_ADD:
		case PGBENCH_SUB:
		case PGBENCH_MUL:
		case PGBENCH_DIV:
		case PGBENCH_MOD:
		case PGBENCH_EQ:
		case PGBENCH_NE:
		case PGBENCH_LE:
		case PGBENCH_LT:
			{
				PgBenchValue *lval = &vargs[0],
						   *rval = &vargs[1];

				Assert(nargs == 2);

				/* overloaded type management, double if some double */
				if ((lval->type == PGBT_DOUBLE ||
					 rval->type == PGBT_DOUBLE) && func != PGBENCH_MOD)
				{
					double		ld,
								rd;

					if (!coerceToDouble(lval, &ld) ||
						!coerceToDouble(rval, &rd))
						return false;

					switch (func)
					{
						case PGBENCH_ADD:
							setDoubleValue(retval, ld + rd);
							return true;

						case PGBENCH_SUB:
							setDoubleValue(retval, ld - rd);
							return true;

						case PGBENCH_MUL:
							setDoubleValue(retval, ld * rd);
							return true;

						case PGBENCH_DIV:
							setDoubleValue(retval, ld / rd);
							return true;

						case PGBENCH_EQ:
							setBoolValue(retval, ld == rd);
							return true;

						case PGBENCH_NE:
							setBoolValue(retval, ld != rd);
							return true;

						case PGBENCH_LE:
							setBoolValue(retval, ld <= rd);
							return true;

						case PGBENCH_LT:
							setBoolValue(retval, ld < rd);
							return true;

						default:
							/* cannot get here */
							Assert(0);
					}
				}
				else			/* we have integer operands, or % */
				{
					int64		li,
								ri,
								res;

					if (!coerceToInt(lval, &li) ||
						!coerceToInt(rval, &ri))
						return false;

					switch (func)
					{
						case PGBENCH_ADD:
							if (pg_add_s64_overflow(li, ri, &res))
							{
								fprintf(stderr, "bigint add out of range\n");
								return false;
							}
							setIntValue(retval, res);
							return true;

						case PGBENCH_SUB:
							if (pg_sub_s64_overflow(li, ri, &res))
							{
								fprintf(stderr, "bigint sub out of range\n");
								return false;
							}
							setIntValue(retval, res);
							return true;

						case PGBENCH_MUL:
							if (pg_mul_s64_overflow(li, ri, &res))
							{
								fprintf(stderr, "bigint mul out of range\n");
								return false;
							}
							setIntValue(retval, res);
							return true;

						case PGBENCH_EQ:
							setBoolValue(retval, li == ri);
							return true;

						case PGBENCH_NE:
							setBoolValue(retval, li != ri);
							return true;

						case PGBENCH_LE:
							setBoolValue(retval, li <= ri);
							return true;

						case PGBENCH_LT:
							setBoolValue(retval, li < ri);
							return true;

						case PGBENCH_DIV:
						case PGBENCH_MOD:
							if (ri == 0)
							{
								fprintf(stderr, "division by zero\n");
								return false;
							}
							/* special handling of -1 divisor */
							if (ri == -1)
							{
								if (func == PGBENCH_DIV)
								{
									/* overflow check (needed for INT64_MIN) */
									if (li == PG_INT64_MIN)
									{
										fprintf(stderr, "bigint div out of range\n");
										return false;
									}
									else
										setIntValue(retval, -li);
								}
								else
									setIntValue(retval, 0);
								return true;
							}
							/* else divisor is not -1 */
							if (func == PGBENCH_DIV)
								setIntValue(retval, li / ri);
							else	/* func == PGBENCH_MOD */
								setIntValue(retval, li % ri);

							return true;

						default:
							/* cannot get here */
							Assert(0);
					}
				}

				Assert(0);
				return false;	/* NOTREACHED */
			}

			/* integer bitwise operators */
		case PGBENCH_BITAND:
		case PGBENCH_BITOR:
		case PGBENCH_BITXOR:
		case PGBENCH_LSHIFT:
		case PGBENCH_RSHIFT:
			{
				int64		li,
							ri;

				if (!coerceToInt(&vargs[0], &li) || !coerceToInt(&vargs[1], &ri))
					return false;

				if (func == PGBENCH_BITAND)
					setIntValue(retval, li & ri);
				else if (func == PGBENCH_BITOR)
					setIntValue(retval, li | ri);
				else if (func == PGBENCH_BITXOR)
					setIntValue(retval, li ^ ri);
				else if (func == PGBENCH_LSHIFT)
					setIntValue(retval, li << ri);
				else if (func == PGBENCH_RSHIFT)
					setIntValue(retval, li >> ri);
				else			/* cannot get here */
					Assert(0);

				return true;
			}

			/* logical operators */
		case PGBENCH_NOT:
			{
				bool		b;

				if (!coerceToBool(&vargs[0], &b))
					return false;

				setBoolValue(retval, !b);
				return true;
			}

			/* no arguments */
		case PGBENCH_PI:
			setDoubleValue(retval, M_PI);
			return true;

			/* 1 overloaded argument */
		case PGBENCH_ABS:
			{
				PgBenchValue *varg = &vargs[0];

				Assert(nargs == 1);

				if (varg->type == PGBT_INT)
				{
					int64		i = varg->u.ival;

					setIntValue(retval, i < 0 ? -i : i);
				}
				else
				{
					double		d = varg->u.dval;

					Assert(varg->type == PGBT_DOUBLE);
					setDoubleValue(retval, d < 0.0 ? -d : d);
				}

				return true;
			}

		case PGBENCH_DEBUG:
			{
				PgBenchValue *varg = &vargs[0];

				Assert(nargs == 1);

				fprintf(stderr, "debug(script=%d,command=%d): ",
						st->use_file, st->command + 1);

				if (varg->type == PGBT_NULL)
					fprintf(stderr, "null\n");
				else if (varg->type == PGBT_BOOLEAN)
					fprintf(stderr, "boolean %s\n", varg->u.bval ? "true" : "false");
				else if (varg->type == PGBT_INT)
					fprintf(stderr, "int " INT64_FORMAT "\n", varg->u.ival);
				else if (varg->type == PGBT_DOUBLE)
					fprintf(stderr, "double %.*g\n", DBL_DIG, varg->u.dval);
				else			/* internal error, unexpected type */
					Assert(0);

				*retval = *varg;

				return true;
			}

			/* 1 double argument */
		case PGBENCH_DOUBLE:
		case PGBENCH_SQRT:
		case PGBENCH_LN:
		case PGBENCH_EXP:
			{
				double		dval;

				Assert(nargs == 1);

				if (!coerceToDouble(&vargs[0], &dval))
					return false;

				if (func == PGBENCH_SQRT)
					dval = sqrt(dval);
				else if (func == PGBENCH_LN)
					dval = log(dval);
				else if (func == PGBENCH_EXP)
					dval = exp(dval);
				/* else is cast: do nothing */

				setDoubleValue(retval, dval);
				return true;
			}

			/* 1 int argument */
		case PGBENCH_INT:
			{
				int64		ival;

				Assert(nargs == 1);

				if (!coerceToInt(&vargs[0], &ival))
					return false;

				setIntValue(retval, ival);
				return true;
			}

			/* variable number of arguments */
		case PGBENCH_LEAST:
		case PGBENCH_GREATEST:
			{
				bool		havedouble;
				int			i;

				Assert(nargs >= 1);

				/* need double result if any input is double */
				havedouble = false;
				for (i = 0; i < nargs; i++)
				{
					if (vargs[i].type == PGBT_DOUBLE)
					{
						havedouble = true;
						break;
					}
				}
				if (havedouble)
				{
					double		extremum;

					if (!coerceToDouble(&vargs[0], &extremum))
						return false;
					for (i = 1; i < nargs; i++)
					{
						double		dval;

						if (!coerceToDouble(&vargs[i], &dval))
							return false;
						if (func == PGBENCH_LEAST)
							extremum = Min(extremum, dval);
						else
							extremum = Max(extremum, dval);
					}
					setDoubleValue(retval, extremum);
				}
				else
				{
					int64		extremum;

					if (!coerceToInt(&vargs[0], &extremum))
						return false;
					for (i = 1; i < nargs; i++)
					{
						int64		ival;

						if (!coerceToInt(&vargs[i], &ival))
							return false;
						if (func == PGBENCH_LEAST)
							extremum = Min(extremum, ival);
						else
							extremum = Max(extremum, ival);
					}
					setIntValue(retval, extremum);
				}
				return true;
			}

			/* random functions */
		case PGBENCH_RANDOM:
		case PGBENCH_RANDOM_EXPONENTIAL:
		case PGBENCH_RANDOM_GAUSSIAN:
		case PGBENCH_RANDOM_ZIPFIAN:
			{
				int64		imin,
							imax,
							delta;

				Assert(nargs >= 2);

				if (!coerceToInt(&vargs[0], &imin) ||
					!coerceToInt(&vargs[1], &imax))
					return false;

				/* check random range */
				if (unlikely(imin > imax))
				{
					fprintf(stderr, "empty range given to random\n");
					return false;
				}
				else if (unlikely(pg_sub_s64_overflow(imax, imin, &delta) ||
								  pg_add_s64_overflow(delta, 1, &delta)))
				{
					/* prevent int overflows in random functions */
					fprintf(stderr, "random range is too large\n");
					return false;
				}

				if (func == PGBENCH_RANDOM)
				{
					Assert(nargs == 2);
					setIntValue(retval, getrand(&st->cs_func_rs, imin, imax));
				}
				else			/* gaussian & exponential */
				{
					double		param;

					Assert(nargs == 3);

					if (!coerceToDouble(&vargs[2], &param))
						return false;

					if (func == PGBENCH_RANDOM_GAUSSIAN)
					{
						if (param < MIN_GAUSSIAN_PARAM)
						{
							fprintf(stderr,
									"gaussian parameter must be at least %f "
									"(not %f)\n", MIN_GAUSSIAN_PARAM, param);
							return false;
						}

						setIntValue(retval,
									getGaussianRand(&st->cs_func_rs,
													imin, imax, param));
					}
					else if (func == PGBENCH_RANDOM_ZIPFIAN)
					{
						if (param < MIN_ZIPFIAN_PARAM || param > MAX_ZIPFIAN_PARAM)
						{
							fprintf(stderr,
									"zipfian parameter must be in range [%.3f, %.0f]"
									" (not %f)\n",
									MIN_ZIPFIAN_PARAM, MAX_ZIPFIAN_PARAM, param);
							return false;
						}

						setIntValue(retval,
									getZipfianRand(&st->cs_func_rs, imin, imax, param));
					}
					else		/* exponential */
					{
						if (param <= 0.0)
						{
							fprintf(stderr,
									"exponential parameter must be greater than zero"
									" (not %f)\n", param);
							return false;
						}

						setIntValue(retval,
									getExponentialRand(&st->cs_func_rs,
													   imin, imax, param));
					}
				}

				return true;
			}

		case PGBENCH_POW:
			{
				PgBenchValue *lval = &vargs[0];
				PgBenchValue *rval = &vargs[1];
				double		ld,
							rd;

				Assert(nargs == 2);

				if (!coerceToDouble(lval, &ld) ||
					!coerceToDouble(rval, &rd))
					return false;

				setDoubleValue(retval, pow(ld, rd));

				return true;
			}

		case PGBENCH_IS:
			{
				Assert(nargs == 2);

				/*
				 * note: this simple implementation is more permissive than
				 * SQL
				 */
				setBoolValue(retval,
							 vargs[0].type == vargs[1].type &&
							 vargs[0].u.bval == vargs[1].u.bval);
				return true;
			}

			/* hashing */
		case PGBENCH_HASH_FNV1A:
		case PGBENCH_HASH_MURMUR2:
			{
				int64		val,
							seed;

				Assert(nargs == 2);

				if (!coerceToInt(&vargs[0], &val) ||
					!coerceToInt(&vargs[1], &seed))
					return false;

				if (func == PGBENCH_HASH_MURMUR2)
					setIntValue(retval, getHashMurmur2(val, seed));
				else if (func == PGBENCH_HASH_FNV1A)
					setIntValue(retval, getHashFnv1a(val, seed));
				else
					/* cannot get here */
					Assert(0);

				return true;
			}

		default:
			/* cannot get here */
			Assert(0);
			/* dead code to avoid a compiler warning */
			return false;
	}
}

/* evaluate some function */
static bool
evalFunc(CState *st,
		 PgBenchFunction func, PgBenchExprLink *args, PgBenchValue *retval)
{
	if (isLazyFunc(func))
		return evalLazyFunc(st, func, args, retval);
	else
		return evalStandardFunc(st, func, args, retval);
}

/*
 * Recursive evaluation of an expression in a pgbench script
 * using the current state of variables.
 * Returns whether the evaluation was ok,
 * the value itself is returned through the retval pointer.
 */
static bool
evaluateExpr(CState *st, PgBenchExpr *expr, PgBenchValue *retval)
{
	switch (expr->etype)
	{
		case ENODE_CONSTANT:
			{
				*retval = expr->u.constant;
				return true;
			}

		case ENODE_VARIABLE:
			{
				Variable   *var;

				if ((var = lookupVariable(st, expr->u.variable.varname)) == NULL)
				{
					fprintf(stderr, "undefined variable \"%s\"\n",
							expr->u.variable.varname);
					return false;
				}

				if (!makeVariableValue(var))
					return false;

				*retval = var->value;
				return true;
			}

		case ENODE_FUNCTION:
			return evalFunc(st,
							expr->u.function.function,
							expr->u.function.args,
							retval);

		default:
			/* internal error which should never occur */
			fprintf(stderr, "unexpected enode type in evaluation: %d\n",
					expr->etype);
			exit(1);
	}
}

/*
 * Convert command name to meta-command enum identifier
 */
static MetaCommand
getMetaCommand(const char *cmd)
{
	MetaCommand mc;

	if (cmd == NULL)
		mc = META_NONE;
	else if (pg_strcasecmp(cmd, "set") == 0)
		mc = META_SET;
	else if (pg_strcasecmp(cmd, "setshell") == 0)
		mc = META_SETSHELL;
	else if (pg_strcasecmp(cmd, "shell") == 0)
		mc = META_SHELL;
	else if (pg_strcasecmp(cmd, "sleep") == 0)
		mc = META_SLEEP;
	else if (pg_strcasecmp(cmd, "if") == 0)
		mc = META_IF;
	else if (pg_strcasecmp(cmd, "elif") == 0)
		mc = META_ELIF;
	else if (pg_strcasecmp(cmd, "else") == 0)
		mc = META_ELSE;
	else if (pg_strcasecmp(cmd, "endif") == 0)
		mc = META_ENDIF;
	else if (pg_strcasecmp(cmd, "gset") == 0)
		mc = META_GSET;
	else
		mc = META_NONE;
	return mc;
}

/*
 * Run a shell command. The result is assigned to the variable if not NULL.
 * Return true if succeeded, or false on error.
 */
static bool
runShellCommand(CState *st, char *variable, char **argv, int argc)
{
	char		command[SHELL_COMMAND_SIZE];
	int			i,
				len = 0;
	FILE	   *fp;
	char		res[64];
	char	   *endptr;
	int			retval;

	/*----------
	 * Join arguments with whitespace separators. Arguments starting with
	 * exactly one colon are treated as variables:
	 *	name - append a string "name"
	 *	:var - append a variable named 'var'
	 *	::name - append a string ":name"
	 *----------
	 */
	for (i = 0; i < argc; i++)
	{
		char	   *arg;
		int			arglen;

		if (argv[i][0] != ':')
		{
			arg = argv[i];		/* a string literal */
		}
		else if (argv[i][1] == ':')
		{
			arg = argv[i] + 1;	/* a string literal starting with colons */
		}
		else if ((arg = getVariable(st, argv[i] + 1)) == NULL)
		{
			fprintf(stderr, "%s: undefined variable \"%s\"\n",
					argv[0], argv[i]);
			return false;
		}

		arglen = strlen(arg);
		if (len + arglen + (i > 0 ? 1 : 0) >= SHELL_COMMAND_SIZE - 1)
		{
			fprintf(stderr, "%s: shell command is too long\n", argv[0]);
			return false;
		}

		if (i > 0)
			command[len++] = ' ';
		memcpy(command + len, arg, arglen);
		len += arglen;
	}

	command[len] = '\0';

	/* Fast path for non-assignment case */
	if (variable == NULL)
	{
		if (system(command))
		{
			if (!timer_exceeded)
				fprintf(stderr, "%s: could not launch shell command\n", argv[0]);
			return false;
		}
		return true;
	}

	/* Execute the command with pipe and read the standard output. */
	if ((fp = popen(command, "r")) == NULL)
	{
		fprintf(stderr, "%s: could not launch shell command\n", argv[0]);
		return false;
	}
	if (fgets(res, sizeof(res), fp) == NULL)
	{
		if (!timer_exceeded)
			fprintf(stderr, "%s: could not read result of shell command\n", argv[0]);
		(void) pclose(fp);
		return false;
	}
	if (pclose(fp) < 0)
	{
		fprintf(stderr, "%s: could not close shell command\n", argv[0]);
		return false;
	}

	/* Check whether the result is an integer and assign it to the variable */
	retval = (int) strtol(res, &endptr, 10);
	while (*endptr != '\0' && isspace((unsigned char) *endptr))
		endptr++;
	if (*res == '\0' || *endptr != '\0')
	{
		fprintf(stderr, "%s: shell command must return an integer (not \"%s\")\n",
				argv[0], res);
		return false;
	}
	if (!putVariableInt(st, "setshell", variable, retval))
		return false;

#ifdef DEBUG
	printf("shell parameter name: \"%s\", value: \"%s\"\n", argv[1], res);
#endif
	return true;
}

#define MAX_PREPARE_NAME		32
static void
preparedStatementName(char *buffer, int file, int state)
{
	sprintf(buffer, "P%d_%d", file, state);
}

static void
commandFailed(CState *st, const char *cmd, const char *message)
{
	fprintf(stderr,
			"client %d aborted in command %d (%s) of script %d; %s\n",
			st->id, st->command, cmd, st->use_file, message);
}

/* return a script number with a weighted choice. */
static int
chooseScript(TState *thread)
{
	int			i = 0;
	int64		w;

	if (num_scripts == 1)
		return 0;

	w = getrand(&thread->ts_choose_rs, 0, total_weight - 1);
	do
	{
		w -= sql_script[i++].weight;
	} while (w >= 0);

	return i - 1;
}

/* Send a SQL command, using the chosen querymode */
static bool
sendCommand(CState *st, Command *command)
{
	int			r;

	if (querymode == QUERY_SIMPLE)
	{
		char	   *sql;

		sql = pg_strdup(command->argv[0]);
		sql = assignVariables(st, sql);

		if (debug)
			fprintf(stderr, "client %d sending %s\n", st->id, sql);
		r = PQsendQuery(st->con, sql);
		free(sql);
	}
	else if (querymode == QUERY_EXTENDED)
	{
		const char *sql = command->argv[0];
		const char *params[MAX_ARGS];

		getQueryParams(st, command, params);

		if (debug)
			fprintf(stderr, "client %d sending %s\n", st->id, sql);
		r = PQsendQueryParams(st->con, sql, command->argc - 1,
							  NULL, params, NULL, NULL, 0);
	}
	else if (querymode == QUERY_PREPARED)
	{
		char		name[MAX_PREPARE_NAME];
		const char *params[MAX_ARGS];

		if (!st->prepared[st->use_file])
		{
			int			j;
			Command   **commands = sql_script[st->use_file].commands;

			for (j = 0; commands[j] != NULL; j++)
			{
				PGresult   *res;
				char		name[MAX_PREPARE_NAME];

				if (commands[j]->type != SQL_COMMAND)
					continue;
				preparedStatementName(name, st->use_file, j);
				res = PQprepare(st->con, name,
								commands[j]->argv[0], commands[j]->argc - 1, NULL);
				if (PQresultStatus(res) != PGRES_COMMAND_OK)
					fprintf(stderr, "%s", PQerrorMessage(st->con));
				PQclear(res);
			}
			st->prepared[st->use_file] = true;
		}

		getQueryParams(st, command, params);
		preparedStatementName(name, st->use_file, st->command);

		if (debug)
			fprintf(stderr, "client %d sending %s\n", st->id, name);
		r = PQsendQueryPrepared(st->con, name, command->argc - 1,
								params, NULL, NULL, 0);
	}
	else						/* unknown sql mode */
		r = 0;

	if (r == 0)
	{
		if (debug)
			fprintf(stderr, "client %d could not send %s\n",
					st->id, command->argv[0]);
		st->ecnt++;
		return false;
	}
	else
		return true;
}

/*
 * Process query response from the backend.
 *
 * If varprefix is not NULL, it's the variable name prefix where to store
 * the results of the *last* command.
 *
 * Returns true if everything is A-OK, false if any error occurs.
 */
static bool
readCommandResponse(CState *st, char *varprefix)
{
	PGresult   *res;
	PGresult   *next_res;
	int			qrynum = 0;

	res = PQgetResult(st->con);

	while (res != NULL)
	{
		bool		is_last;

		/* peek at the next result to know whether the current is last */
		next_res = PQgetResult(st->con);
		is_last = (next_res == NULL);

		switch (PQresultStatus(res))
		{
			case PGRES_COMMAND_OK:	/* non-SELECT commands */
			case PGRES_EMPTY_QUERY: /* may be used for testing no-op overhead */
				if (is_last && varprefix != NULL)
				{
					fprintf(stderr,
							"client %d script %d command %d query %d: expected one row, got %d\n",
							st->id, st->use_file, st->command, qrynum, 0);
					goto error;
				}
				break;

			case PGRES_TUPLES_OK:
				if (is_last && varprefix != NULL)
				{
					if (PQntuples(res) != 1)
					{
						fprintf(stderr,
								"client %d script %d command %d query %d: expected one row, got %d\n",
								st->id, st->use_file, st->command, qrynum, PQntuples(res));
						goto error;
					}

					/* store results into variables */
					for (int fld = 0; fld < PQnfields(res); fld++)
					{
						char	   *varname = PQfname(res, fld);

						/* allocate varname only if necessary, freed below */
						if (*varprefix != '\0')
							varname = psprintf("%s%s", varprefix, varname);

						/* store result as a string */
						if (!putVariable(st, "gset", varname,
										 PQgetvalue(res, 0, fld)))
						{
							/* internal error */
							fprintf(stderr,
									"client %d script %d command %d query %d: error storing into variable %s\n",
									st->id, st->use_file, st->command, qrynum,
									varname);
							goto error;
						}

						if (*varprefix != '\0')
							pg_free(varname);
					}
				}
				/* otherwise the result is simply thrown away by PQclear below */
				break;

			default:
				/* anything else is unexpected */
				fprintf(stderr,
						"client %d script %d aborted in command %d query %d: %s",
						st->id, st->use_file, st->command, qrynum,
						PQerrorMessage(st->con));
				goto error;
		}

		PQclear(res);
		qrynum++;
		res = next_res;
	}

	if (qrynum == 0)
	{
		fprintf(stderr, "client %d command %d: no results\n", st->id, st->command);
		st->ecnt++;
		return false;
	}

	return true;

error:
	st->ecnt++;
	PQclear(res);
	PQclear(next_res);
	do
	{
		res = PQgetResult(st->con);
		PQclear(res);
	} while (res);

	return false;
}

/*
 * Parse the argument to a \sleep command, and return the requested amount
 * of delay, in microseconds.  Returns true on success, false on error.
 */
static bool
evaluateSleep(CState *st, int argc, char **argv, int *usecs)
{
	char	   *var;
	int			usec;

	if (*argv[1] == ':')
	{
		if ((var = getVariable(st, argv[1] + 1)) == NULL)
		{
			fprintf(stderr, "%s: undefined variable \"%s\"\n",
					argv[0], argv[1]);
			return false;
		}
		usec = atoi(var);
	}
	else
		usec = atoi(argv[1]);

	if (argc > 2)
	{
		if (pg_strcasecmp(argv[2], "ms") == 0)
			usec *= 1000;
		else if (pg_strcasecmp(argv[2], "s") == 0)
			usec *= 1000000;
	}
	else
		usec *= 1000000;

	*usecs = usec;
	return true;
}

/*
 * Advance the state machine of a connection.
 */
static void
advanceConnectionState(TState *thread, CState *st, StatsData *agg)
{
	instr_time	now;

	/*
	 * gettimeofday() isn't free, so we get the current timestamp lazily the
	 * first time it's needed, and reuse the same value throughout this
	 * function after that.  This also ensures that e.g. the calculated
	 * latency reported in the log file and in the totals are the same. Zero
	 * means "not set yet".  Reset "now" when we execute shell commands or
	 * expressions, which might take a non-negligible amount of time, though.
	 */
	INSTR_TIME_SET_ZERO(now);

	/*
	 * Loop in the state machine, until we have to wait for a result from the
	 * server or have to sleep for throttling or \sleep.
	 *
	 * Note: In the switch-statement below, 'break' will loop back here,
	 * meaning "continue in the state machine".  Return is used to return to
	 * the caller, giving the thread the opportunity to advance another
	 * client.
	 */
	for (;;)
	{
		Command    *command;

		switch (st->state)
		{
				/* Select transaction (script) to run.  */
			case CSTATE_CHOOSE_SCRIPT:
				st->use_file = chooseScript(thread);
				Assert(conditional_stack_empty(st->cstack));

				if (debug)
					fprintf(stderr, "client %d executing script \"%s\"\n", st->id,
							sql_script[st->use_file].desc);

				/*
				 * If time is over, we're done; otherwise, get ready to start
				 * a new transaction, or to get throttled if that's requested.
				 */
				st->state = timer_exceeded ? CSTATE_FINISHED :
					throttle_delay > 0 ? CSTATE_PREPARE_THROTTLE : CSTATE_START_TX;
				break;

				/* Start new transaction (script) */
			case CSTATE_START_TX:

				/* establish connection if needed, i.e. under --connect */
				if (st->con == NULL)
				{
					instr_time	start;

					INSTR_TIME_SET_CURRENT_LAZY(now);
					start = now;
					if ((st->con = doConnect()) == NULL)
					{
						fprintf(stderr, "client %d aborted while establishing connection\n",
								st->id);
						st->state = CSTATE_ABORTED;
						break;
					}
					INSTR_TIME_SET_CURRENT(now);
					INSTR_TIME_ACCUM_DIFF(thread->conn_time, now, start);

					/* Reset session-local state */
					memset(st->prepared, 0, sizeof(st->prepared));
				}

				/* record transaction start time */
				INSTR_TIME_SET_CURRENT_LAZY(now);
				st->txn_begin = now;

				/*
				 * When not throttling, this is also the transaction's
				 * scheduled start time.
				 */
				if (!throttle_delay)
					st->txn_scheduled = INSTR_TIME_GET_MICROSEC(now);

				/* Begin with the first command */
				st->state = CSTATE_START_COMMAND;
				st->command = 0;
				break;

				/*
				 * Handle throttling once per transaction by sleeping.
				 */
			case CSTATE_PREPARE_THROTTLE:

				/*
				 * Generate a delay such that the series of delays will
				 * approximate a Poisson distribution centered on the
				 * throttle_delay time.
				 *
				 * If transactions are too slow or a given wait is shorter
				 * than a transaction, the next transaction will start right
				 * away.
				 */
				Assert(throttle_delay > 0);

				thread->throttle_trigger +=
					getPoissonRand(&thread->ts_throttle_rs, throttle_delay);
				st->txn_scheduled = thread->throttle_trigger;

				/*
				 * If --latency-limit is used, and this slot is already late
				 * so that the transaction will miss the latency limit even if
				 * it completed immediately, skip this time slot and schedule
				 * to continue running on the next slot that isn't late yet.
				 * But don't iterate beyond the -t limit, if one is given.
				 */
				if (latency_limit)
				{
					int64		now_us;

					INSTR_TIME_SET_CURRENT_LAZY(now);
					now_us = INSTR_TIME_GET_MICROSEC(now);

					while (thread->throttle_trigger < now_us - latency_limit &&
						   (nxacts <= 0 || st->cnt < nxacts))
					{
						processXactStats(thread, st, &now, true, agg);
						/* next rendez-vous */
						thread->throttle_trigger +=
							getPoissonRand(&thread->ts_throttle_rs, throttle_delay);
						st->txn_scheduled = thread->throttle_trigger;
					}

					/*
					 * stop client if -t was exceeded in the previous skip
					 * loop
					 */
					if (nxacts > 0 && st->cnt >= nxacts)
					{
						st->state = CSTATE_FINISHED;
						break;
					}
				}

				/*
				 * stop client if next transaction is beyond pgbench end of
				 * execution; otherwise, throttle it.
				 */
				st->state = end_time > 0 && st->txn_scheduled > end_time ?
					CSTATE_FINISHED : CSTATE_THROTTLE;
				break;

				/*
				 * Wait until it's time to start next transaction.
				 */
			case CSTATE_THROTTLE:
				INSTR_TIME_SET_CURRENT_LAZY(now);

				if (INSTR_TIME_GET_MICROSEC(now) < st->txn_scheduled)
					return;		/* still sleeping, nothing to do here */

				/* done sleeping, but don't start transaction if we're done */
				st->state = timer_exceeded ? CSTATE_FINISHED : CSTATE_START_TX;
				break;

				/*
				 * Send a command to server (or execute a meta-command)
				 */
			case CSTATE_START_COMMAND:
				command = sql_script[st->use_file].commands[st->command];

				/* Transition to script end processing if done */
				if (command == NULL)
				{
					st->state = CSTATE_END_TX;
					break;
				}

				/* record begin time of next command, and initiate it */
				if (report_per_command)
				{
					INSTR_TIME_SET_CURRENT_LAZY(now);
					st->stmt_begin = now;
				}

				/* Execute the command */
				if (command->type == SQL_COMMAND)
				{
					if (!sendCommand(st, command))
					{
						commandFailed(st, "SQL", "SQL command send failed");
						st->state = CSTATE_ABORTED;
					}
					else
						st->state = CSTATE_WAIT_RESULT;
				}
				else if (command->type == META_COMMAND)
				{
					/*-----
					 * Possible state changes when executing meta commands:
					 * - on errors CSTATE_ABORTED
					 * - on sleep CSTATE_SLEEP
					 * - else CSTATE_END_COMMAND
					 */
					st->state = executeMetaCommand(st, &now);
				}

				/*
				 * We're now waiting for an SQL command to complete, or
				 * finished processing a metacommand, or need to sleep, or
				 * something bad happened.
				 */
				Assert(st->state == CSTATE_WAIT_RESULT ||
					   st->state == CSTATE_END_COMMAND ||
					   st->state == CSTATE_SLEEP ||
					   st->state == CSTATE_ABORTED);
				break;

				/*
				 * non executed conditional branch
				 */
			case CSTATE_SKIP_COMMAND:
				Assert(!conditional_active(st->cstack));
				/* quickly skip commands until something to do... */
				while (true)
				{
					Command    *command;

					command = sql_script[st->use_file].commands[st->command];

					/* cannot reach end of script in that state */
					Assert(command != NULL);

					/*
					 * if this is conditional related, update conditional
					 * state
					 */
					if (command->type == META_COMMAND &&
						(command->meta == META_IF ||
						 command->meta == META_ELIF ||
						 command->meta == META_ELSE ||
						 command->meta == META_ENDIF))
					{
						switch (conditional_stack_peek(st->cstack))
						{
							case IFSTATE_FALSE:
								if (command->meta == META_IF ||
									command->meta == META_ELIF)
								{
									/* we must evaluate the condition */
									st->state = CSTATE_START_COMMAND;
								}
								else if (command->meta == META_ELSE)
								{
									/* we must execute next command */
									conditional_stack_poke(st->cstack,
														   IFSTATE_ELSE_TRUE);
									st->state = CSTATE_START_COMMAND;
									st->command++;
								}
								else if (command->meta == META_ENDIF)
								{
									Assert(!conditional_stack_empty(st->cstack));
									conditional_stack_pop(st->cstack);
									if (conditional_active(st->cstack))
										st->state = CSTATE_START_COMMAND;

									/*
									 * else state remains in
									 * CSTATE_SKIP_COMMAND
									 */
									st->command++;
								}
								break;

							case IFSTATE_IGNORED:
							case IFSTATE_ELSE_FALSE:
								if (command->meta == META_IF)
									conditional_stack_push(st->cstack,
														   IFSTATE_IGNORED);
								else if (command->meta == META_ENDIF)
								{
									Assert(!conditional_stack_empty(st->cstack));
									conditional_stack_pop(st->cstack);
									if (conditional_active(st->cstack))
										st->state = CSTATE_START_COMMAND;
								}
								/* could detect "else" & "elif" after "else" */
								st->command++;
								break;

							case IFSTATE_NONE:
							case IFSTATE_TRUE:
							case IFSTATE_ELSE_TRUE:
							default:

								/*
								 * inconsistent if inactive, unreachable dead
								 * code
								 */
								Assert(false);
						}
					}
					else
					{
						/* skip and consider next */
						st->command++;
					}

					if (st->state != CSTATE_SKIP_COMMAND)
						/* out of quick skip command loop */
						break;
				}
				break;

				/*
				 * Wait for the current SQL command to complete
				 */
			case CSTATE_WAIT_RESULT:
				if (debug)
					fprintf(stderr, "client %d receiving\n", st->id);
				if (!PQconsumeInput(st->con))
				{
					/* there's something wrong */
					commandFailed(st, "SQL", "perhaps the backend died while processing");
					st->state = CSTATE_ABORTED;
					break;
				}
				if (PQisBusy(st->con))
					return;		/* don't have the whole result yet */

				/* store or discard the query results */
				if (readCommandResponse(st, sql_script[st->use_file].commands[st->command]->varprefix))
					st->state = CSTATE_END_COMMAND;
				else
					st->state = CSTATE_ABORTED;
				break;

				/*
				 * Wait until sleep is done. This state is entered after a
				 * \sleep metacommand. The behavior is similar to
				 * CSTATE_THROTTLE, but proceeds to CSTATE_START_COMMAND
				 * instead of CSTATE_START_TX.
				 */
			case CSTATE_SLEEP:
				INSTR_TIME_SET_CURRENT_LAZY(now);
				if (INSTR_TIME_GET_MICROSEC(now) < st->sleep_until)
					return;		/* still sleeping, nothing to do here */
				/* Else done sleeping. */
				st->state = CSTATE_END_COMMAND;
				break;

				/*
				 * End of command: record stats and proceed to next command.
				 */
			case CSTATE_END_COMMAND:

				/*
				 * command completed: accumulate per-command execution times
				 * in thread-local data structure, if per-command latencies
				 * are requested.
				 */
				if (report_per_command)
				{
					Command    *command;

					INSTR_TIME_SET_CURRENT_LAZY(now);

					command = sql_script[st->use_file].commands[st->command];
					/* XXX could use a mutex here, but we choose not to */
					addToSimpleStats(&command->stats,
									 INSTR_TIME_GET_DOUBLE(now) -
									 INSTR_TIME_GET_DOUBLE(st->stmt_begin));
				}

				/* Go ahead with next command, to be executed or skipped */
				st->command++;
				st->state = conditional_active(st->cstack) ?
					CSTATE_START_COMMAND : CSTATE_SKIP_COMMAND;
				break;

				/*
				 * End of transaction (end of script, really).
				 */
			case CSTATE_END_TX:

				/* transaction finished: calculate latency and do log */
				processXactStats(thread, st, &now, false, agg);

				/*
				 * missing \endif... cannot happen if CheckConditional was
				 * okay
				 */
				Assert(conditional_stack_empty(st->cstack));

				if (is_connect)
				{
					finishCon(st);
					INSTR_TIME_SET_ZERO(now);
				}

				if ((st->cnt >= nxacts && duration <= 0) || timer_exceeded)
				{
					/* script completed */
					st->state = CSTATE_FINISHED;
					break;
				}

				/* next transaction (script) */
				st->state = CSTATE_CHOOSE_SCRIPT;

				/*
				 * Ensure that we always return on this point, so as to avoid
				 * an infinite loop if the script only contains meta commands.
				 */
				return;

				/*
				 * Final states.  Close the connection if it's still open.
				 */
			case CSTATE_ABORTED:
			case CSTATE_FINISHED:
				finishCon(st);
				return;
		}
	}
}

/*
 * Subroutine for advanceConnectionState -- initiate or execute the current
 * meta command, and return the next state to set.
 *
 * *now is updated to the current time, unless the command is expected to
 * take no time to execute.
 */
static ConnectionStateEnum
executeMetaCommand(CState *st, instr_time *now)
{
	Command    *command = sql_script[st->use_file].commands[st->command];
	int			argc;
	char	  **argv;

	Assert(command != NULL && command->type == META_COMMAND);

	argc = command->argc;
	argv = command->argv;

	if (debug)
	{
		fprintf(stderr, "client %d executing \\%s", st->id, argv[0]);
		for (int i = 1; i < argc; i++)
			fprintf(stderr, " %s", argv[i]);
		fprintf(stderr, "\n");
	}

	if (command->meta == META_SLEEP)
	{
		int			usec;

		/*
		 * A \sleep doesn't execute anything, we just get the delay from the
		 * argument, and enter the CSTATE_SLEEP state.  (The per-command
		 * latency will be recorded in CSTATE_SLEEP state, not here, after the
		 * delay has elapsed.)
		 */
		if (!evaluateSleep(st, argc, argv, &usec))
		{
			commandFailed(st, "sleep", "execution of meta-command failed");
			return CSTATE_ABORTED;
		}

		INSTR_TIME_SET_CURRENT_LAZY(*now);
		st->sleep_until = INSTR_TIME_GET_MICROSEC(*now) + usec;
		return CSTATE_SLEEP;
	}
	else if (command->meta == META_SET)
	{
		PgBenchExpr *expr = command->expr;
		PgBenchValue result;

		if (!evaluateExpr(st, expr, &result))
		{
			commandFailed(st, argv[0], "evaluation of meta-command failed");
			return CSTATE_ABORTED;
		}

		if (!putVariableValue(st, argv[0], argv[1], &result))
		{
			commandFailed(st, "set", "assignment of meta-command failed");
			return CSTATE_ABORTED;
		}
	}
	else if (command->meta == META_IF)
	{
		/* backslash commands with an expression to evaluate */
		PgBenchExpr *expr = command->expr;
		PgBenchValue result;
		bool		cond;

		if (!evaluateExpr(st, expr, &result))
		{
			commandFailed(st, argv[0], "evaluation of meta-command failed");
			return CSTATE_ABORTED;
		}

		cond = valueTruth(&result);
		conditional_stack_push(st->cstack, cond ? IFSTATE_TRUE : IFSTATE_FALSE);
	}
	else if (command->meta == META_ELIF)
	{
		/* backslash commands with an expression to evaluate */
		PgBenchExpr *expr = command->expr;
		PgBenchValue result;
		bool		cond;

		if (conditional_stack_peek(st->cstack) == IFSTATE_TRUE)
		{
			/* elif after executed block, skip eval and wait for endif. */
			conditional_stack_poke(st->cstack, IFSTATE_IGNORED);
			return CSTATE_END_COMMAND;
		}

		if (!evaluateExpr(st, expr, &result))
		{
			commandFailed(st, argv[0], "evaluation of meta-command failed");
			return CSTATE_ABORTED;
		}

		cond = valueTruth(&result);
		Assert(conditional_stack_peek(st->cstack) == IFSTATE_FALSE);
		conditional_stack_poke(st->cstack, cond ? IFSTATE_TRUE : IFSTATE_FALSE);
	}
	else if (command->meta == META_ELSE)
	{
		switch (conditional_stack_peek(st->cstack))
		{
			case IFSTATE_TRUE:
				conditional_stack_poke(st->cstack, IFSTATE_ELSE_FALSE);
				break;
			case IFSTATE_FALSE: /* inconsistent if active */
			case IFSTATE_IGNORED:	/* inconsistent if active */
			case IFSTATE_NONE:	/* else without if */
			case IFSTATE_ELSE_TRUE: /* else after else */
			case IFSTATE_ELSE_FALSE:	/* else after else */
			default:
				/* dead code if conditional check is ok */
				Assert(false);
		}
	}
	else if (command->meta == META_ENDIF)
	{
		Assert(!conditional_stack_empty(st->cstack));
		conditional_stack_pop(st->cstack);
	}
	else if (command->meta == META_SETSHELL)
	{
		if (!runShellCommand(st, argv[1], argv + 2, argc - 2))
		{
			commandFailed(st, "setshell", "execution of meta-command failed");
			return CSTATE_ABORTED;
		}
	}
	else if (command->meta == META_SHELL)
	{
		if (!runShellCommand(st, NULL, argv + 1, argc - 1))
		{
			commandFailed(st, "shell", "execution of meta-command failed");
			return CSTATE_ABORTED;
		}
	}

	/*
	 * executing the expression or shell command might have taken a
	 * non-negligible amount of time, so reset 'now'
	 */
	INSTR_TIME_SET_ZERO(*now);

	return CSTATE_END_COMMAND;
}

/*
 * Print log entry after completing one transaction.
 *
 * We print Unix-epoch timestamps in the log, so that entries can be
 * correlated against other logs.  On some platforms this could be obtained
 * from the instr_time reading the caller has, but rather than get entangled
 * with that, we just eat the cost of an extra syscall in all cases.
 */
static void
doLog(TState *thread, CState *st,
	  StatsData *agg, bool skipped, double latency, double lag)
{
	FILE	   *logfile = thread->logfile;

	Assert(use_log);

	/*
	 * Skip the log entry if sampling is enabled and this row doesn't belong
	 * to the random sample.
	 */
	if (sample_rate != 0.0 &&
		pg_erand48(thread->ts_sample_rs.xseed) > sample_rate)
		return;

	/* should we aggregate the results or not? */
	if (agg_interval > 0)
	{
		/*
		 * Loop until we reach the interval of the current moment, and print
		 * any empty intervals in between (this may happen with very low tps,
		 * e.g. --rate=0.1).
		 */
		time_t		now = time(NULL);

		while (agg->start_time + agg_interval <= now)
		{
			/* print aggregated report to logfile */
			fprintf(logfile, "%ld " INT64_FORMAT " %.0f %.0f %.0f %.0f",
					(long) agg->start_time,
					agg->cnt,
					agg->latency.sum,
					agg->latency.sum2,
					agg->latency.min,
					agg->latency.max);
			if (throttle_delay)
			{
				fprintf(logfile, " %.0f %.0f %.0f %.0f",
						agg->lag.sum,
						agg->lag.sum2,
						agg->lag.min,
						agg->lag.max);
				if (latency_limit)
					fprintf(logfile, " " INT64_FORMAT, agg->skipped);
			}
			fputc('\n', logfile);

			/* reset data and move to next interval */
			initStats(agg, agg->start_time + agg_interval);
		}

		/* accumulate the current transaction */
		accumStats(agg, skipped, latency, lag);
	}
	else
	{
		/* no, print raw transactions */
		struct timeval tv;

		gettimeofday(&tv, NULL);
		if (skipped)
			fprintf(logfile, "%d " INT64_FORMAT " skipped %d %ld %ld",
					st->id, st->cnt, st->use_file,
					(long) tv.tv_sec, (long) tv.tv_usec);
		else
			fprintf(logfile, "%d " INT64_FORMAT " %.0f %d %ld %ld",
					st->id, st->cnt, latency, st->use_file,
					(long) tv.tv_sec, (long) tv.tv_usec);
		if (throttle_delay)
			fprintf(logfile, " %.0f", lag);
		fputc('\n', logfile);
	}
}

/*
 * Accumulate and report statistics at end of a transaction.
 *
 * (This is also called when a transaction is late and thus skipped.
 * Note that even skipped transactions are counted in the "cnt" fields.)
 */
static void
processXactStats(TState *thread, CState *st, instr_time *now,
				 bool skipped, StatsData *agg)
{
	double		latency = 0.0,
				lag = 0.0;
	bool		thread_details = progress || throttle_delay || latency_limit,
				detailed = thread_details || use_log || per_script_stats;

	if (detailed && !skipped)
	{
		INSTR_TIME_SET_CURRENT_LAZY(*now);

		/* compute latency & lag */
		latency = INSTR_TIME_GET_MICROSEC(*now) - st->txn_scheduled;
		lag = INSTR_TIME_GET_MICROSEC(st->txn_begin) - st->txn_scheduled;
	}

	if (thread_details)
	{
		/* keep detailed thread stats */
		accumStats(&thread->stats, skipped, latency, lag);

		/* count transactions over the latency limit, if needed */
		if (latency_limit && latency > latency_limit)
			thread->latency_late++;
	}
	else
	{
		/* no detailed stats, just count */
		thread->stats.cnt++;
	}

	/* client stat is just counting */
	st->cnt++;

	if (use_log)
		doLog(thread, st, agg, skipped, latency, lag);

	/* XXX could use a mutex here, but we choose not to */
	if (per_script_stats)
		accumStats(&sql_script[st->use_file].stats, skipped, latency, lag);
}


/* discard connections */
static void
disconnect_all(CState *state, int length)
{
	int			i;

	for (i = 0; i < length; i++)
		finishCon(&state[i]);
}

/*
 * Remove old pgbench tables, if any exist
 */
static void
initDropTables(PGconn *con)
{
	fprintf(stderr, "dropping old tables...\n");

	/*
	 * We drop all the tables in one command, so that whether there are
	 * foreign key dependencies or not doesn't matter.
	 */
	executeStatement(con, "drop table if exists "
					 "pgbench_accounts, "
					 "pgbench_branches, "
					 "pgbench_history, "
					 "pgbench_tellers");
}

/*
 * Create pgbench's standard tables
 */
static void
initCreateTables(PGconn *con)
{
	/*
	 * Note: TPC-B requires at least 100 bytes per row, and the "filler"
	 * fields in these table declarations were intended to comply with that.
	 * The pgbench_accounts table complies with that because the "filler"
	 * column is set to blank-padded empty string. But for all other tables
	 * the columns default to NULL and so don't actually take any space.  We
	 * could fix that by giving them non-null default values.  However, that
	 * would completely break comparability of pgbench results with prior
	 * versions. Since pgbench has never pretended to be fully TPC-B compliant
	 * anyway, we stick with the historical behavior.
	 */
	struct ddlinfo
	{
		const char *table;		/* table name */
		const char *smcols;		/* column decls if accountIDs are 32 bits */
		const char *bigcols;	/* column decls if accountIDs are 64 bits */
		int			declare_fillfactor;
		char	   *distributed_col;
	};
	static const struct ddlinfo DDLs[] = {
		{
			"pgbench_history",
			"tid int,bid int,aid    int,delta int,mtime timestamp,filler char(22)",
			"tid int,bid int,aid bigint,delta int,mtime timestamp,filler char(22)",
			0
			, "bid"
		},
		{
			"pgbench_tellers",
			"tid int not null,bid int,tbalance int,filler char(84)",
			"tid int not null,bid int,tbalance int,filler char(84)",
			1,
			"tid"
		},
		{
			"pgbench_accounts",
			"aid    int not null,bid int,abalance int,filler char(84)",
			"aid bigint not null,bid int,abalance int,filler char(84)",
			1
			, "aid"
		},
		{
			"pgbench_branches",
			"bid int not null,bbalance int,filler char(88)",
			"bid int not null,bbalance int,filler char(88)",
			1
			, "bid"
		}
	};
	int			i;

	fprintf(stderr, "creating tables...\n");

	for (i = 0; i < lengthof(DDLs); i++)
	{
		char		opts[256];
		char		buffer[256];
		const struct ddlinfo *ddl = &DDLs[i];
		const char *cols;

		/* Construct new create table statement. */
		opts[0] = '\0';
		if (ddl->declare_fillfactor)
			snprintf(opts + strlen(opts), sizeof(opts) - strlen(opts),
					 " with (fillfactor=%d, %s)",
					 fillfactor, storage_clause);
		else
			snprintf(opts + strlen(opts), sizeof(opts) - strlen(opts),
					 " with (%s)",
					 storage_clause);
		if (tablespace != NULL)
		{
			char	   *escape_tablespace;

			escape_tablespace = PQescapeIdentifier(con, tablespace,
												   strlen(tablespace));
			snprintf(opts + strlen(opts), sizeof(opts) - strlen(opts),
					 " tablespace %s", escape_tablespace);
			PQfreemem(escape_tablespace);
		}
		snprintf(opts + strlen(opts), sizeof(opts) - strlen(opts),
				 " distributed by (%s)",
				 ddl->distributed_col);

		cols = (scale >= SCALE_32BIT_THRESHOLD) ? ddl->bigcols : ddl->smcols;

		snprintf(buffer, sizeof(buffer), "create%s table %s(%s)%s",
				 unlogged_tables ? " unlogged" : "",
				 ddl->table, cols, opts);

		executeStatement(con, buffer);
	}
}

/*
 * Fill the standard tables with some data
 */
static void
initGenerateData(PGconn *con)
{
	char		sql[256];
	PGresult   *res;
	int			i;
	int64		k;

	/* used to track elapsed time and estimate of the remaining time */
	instr_time	start,
				diff;
	double		elapsed_sec,
				remaining_sec;
	int			log_interval = 1;

	fprintf(stderr, "generating data...\n");

	/*
	 * we do all of this in one transaction to enable the backend's
	 * data-loading optimizations
	 */
	executeStatement(con, "begin");

	/*
	 * truncate away any old data, in one command in case there are foreign
	 * keys
	 */
	executeStatement(con, "truncate table "
					 "pgbench_accounts, "
					 "pgbench_branches, "
					 "pgbench_history, "
					 "pgbench_tellers");

	/*
	 * fill branches, tellers, accounts in that order in case foreign keys
	 * already exist
	 */
	for (i = 0; i < nbranches * scale; i++)
	{
		/* "filler" column defaults to NULL */
		snprintf(sql, sizeof(sql),
				 "insert into pgbench_branches(bid,bbalance) values(%d,0)",
				 i + 1);
		executeStatement(con, sql);
	}

	for (i = 0; i < ntellers * scale; i++)
	{
		/* "filler" column defaults to NULL */
		snprintf(sql, sizeof(sql),
				 "insert into pgbench_tellers(tid,bid,tbalance) values (%d,%d,0)",
				 i + 1, i / ntellers + 1);
		executeStatement(con, sql);
	}

	/*
	 * accounts is big enough to be worth using COPY and tracking runtime
	 */
	res = PQexec(con, "copy pgbench_accounts from stdin");
	if (PQresultStatus(res) != PGRES_COPY_IN)
	{
		fprintf(stderr, "%s", PQerrorMessage(con));
		exit(1);
	}
	PQclear(res);

	INSTR_TIME_SET_CURRENT(start);

	for (k = 0; k < (int64) naccounts * scale; k++)
	{
		int64		j = k + 1;

		/* "filler" column defaults to blank padded empty string */
		snprintf(sql, sizeof(sql),
				 INT64_FORMAT "\t" INT64_FORMAT "\t%d\t\n",
				 j, k / naccounts + 1, 0);
		if (PQputline(con, sql))
		{
			fprintf(stderr, "PQputline failed\n");
			exit(1);
		}

		/*
		 * If we want to stick with the original logging, print a message each
		 * 100k inserted rows.
		 */
		if ((!use_quiet) && (j % 100000 == 0))
		{
			INSTR_TIME_SET_CURRENT(diff);
			INSTR_TIME_SUBTRACT(diff, start);

			elapsed_sec = INSTR_TIME_GET_DOUBLE(diff);
			remaining_sec = ((double) scale * naccounts - j) * elapsed_sec / j;

			fprintf(stderr, INT64_FORMAT " of " INT64_FORMAT " tuples (%d%%) done (elapsed %.2f s, remaining %.2f s)\n",
					j, (int64) naccounts * scale,
					(int) (((int64) j * 100) / (naccounts * (int64) scale)),
					elapsed_sec, remaining_sec);
		}
		/* let's not call the timing for each row, but only each 100 rows */
		else if (use_quiet && (j % 100 == 0))
		{
			INSTR_TIME_SET_CURRENT(diff);
			INSTR_TIME_SUBTRACT(diff, start);

			elapsed_sec = INSTR_TIME_GET_DOUBLE(diff);
			remaining_sec = ((double) scale * naccounts - j) * elapsed_sec / j;

			/* have we reached the next interval (or end)? */
			if ((j == scale * naccounts) || (elapsed_sec >= log_interval * LOG_STEP_SECONDS))
			{
				fprintf(stderr, INT64_FORMAT " of " INT64_FORMAT " tuples (%d%%) done (elapsed %.2f s, remaining %.2f s)\n",
						j, (int64) naccounts * scale,
						(int) (((int64) j * 100) / (naccounts * (int64) scale)), elapsed_sec, remaining_sec);

				/* skip to the next interval */
				log_interval = (int) ceil(elapsed_sec / LOG_STEP_SECONDS);
			}
		}

	}
	if (PQputline(con, "\\.\n"))
	{
		fprintf(stderr, "very last PQputline failed\n");
		exit(1);
	}
	if (PQendcopy(con))
	{
		fprintf(stderr, "PQendcopy failed\n");
		exit(1);
	}

	executeStatement(con, "commit");
}

/*
 * Invoke vacuum on the standard tables
 */
static void
initVacuum(PGconn *con)
{
	fprintf(stderr, "vacuuming...\n");
	executeStatement(con, "vacuum analyze pgbench_branches");
	executeStatement(con, "vacuum analyze pgbench_tellers");
	executeStatement(con, "vacuum analyze pgbench_accounts");
	executeStatement(con, "vacuum analyze pgbench_history");
}

/*
 * Create primary keys on the standard tables
 */
static void
initCreatePKeys(PGconn *con)
{
	static const char *const DDLINDEXes[] = {
		"alter table pgbench_branches add primary key (bid)",
		"alter table pgbench_tellers add primary key (tid)",
		"alter table pgbench_accounts add primary key (aid)"
	};
	static const char *const NON_UNIQUE_INDEX_DDLINDEXes[] = {
		"CREATE INDEX branch_idx ON pgbench_branches (bid) ",
		"CREATE INDEX teller_idx ON pgbench_tellers (tid) ",
		"CREATE INDEX account_idx ON pgbench_accounts (aid) "
	};
	StaticAssertStmt(lengthof(DDLINDEXes) == lengthof(NON_UNIQUE_INDEX_DDLINDEXes),
					 "NON_UNIQUE_INDEX_DDLINDEXes must have same size as DDLINDEXes");
	int			i;

	fprintf(stderr, "creating primary keys...\n");
	for (i = 0; i < lengthof(DDLINDEXes); i++)
	{
		char		buffer[256];

		if (use_unique_key)
			strlcpy(buffer, DDLINDEXes[i], sizeof(buffer));
		else
			strlcpy(buffer, NON_UNIQUE_INDEX_DDLINDEXes[i], sizeof(buffer));

		if (index_tablespace != NULL)
		{
			char	   *escape_tablespace;

			escape_tablespace = PQescapeIdentifier(con, index_tablespace,
												   strlen(index_tablespace));
			snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
					 " using index tablespace %s", escape_tablespace);
			PQfreemem(escape_tablespace);
		}

		executeStatement(con, buffer);
	}
}

/*
 * Create foreign key constraints between the standard tables
 */
static void
initCreateFKeys(PGconn *con)
{
	static const char *const DDLKEYs[] = {
		"alter table pgbench_tellers add constraint pgbench_tellers_bid_fkey foreign key (bid) references pgbench_branches",
		"alter table pgbench_accounts add constraint pgbench_accounts_bid_fkey foreign key (bid) references pgbench_branches",
		"alter table pgbench_history add constraint pgbench_history_bid_fkey foreign key (bid) references pgbench_branches",
		"alter table pgbench_history add constraint pgbench_history_tid_fkey foreign key (tid) references pgbench_tellers",
		"alter table pgbench_history add constraint pgbench_history_aid_fkey foreign key (aid) references pgbench_accounts"
	};
	int			i;

	fprintf(stderr, "creating foreign keys...\n");
	for (i = 0; i < lengthof(DDLKEYs); i++)
	{
		executeStatement(con, DDLKEYs[i]);
	}
}

/*
 * Validate an initialization-steps string
 *
 * (We could just leave it to runInitSteps() to fail if there are wrong
 * characters, but since initialization can take awhile, it seems friendlier
 * to check during option parsing.)
 */
static void
checkInitSteps(const char *initialize_steps)
{
	const char *step;

	if (initialize_steps[0] == '\0')
	{
		fprintf(stderr, "no initialization steps specified\n");
		exit(1);
	}

	for (step = initialize_steps; *step != '\0'; step++)
	{
		if (strchr("dtgvpf ", *step) == NULL)
		{
			fprintf(stderr, "unrecognized initialization step \"%c\"\n",
					*step);
			fprintf(stderr, "allowed steps are: \"d\", \"t\", \"g\", \"v\", \"p\", \"f\"\n");
			exit(1);
		}
	}
}

/*
 * Invoke each initialization step in the given string
 */
static void
runInitSteps(const char *initialize_steps)
{
	PGconn	   *con;
	const char *step;

	if ((con = doConnect()) == NULL)
		exit(1);

	for (step = initialize_steps; *step != '\0'; step++)
	{
		switch (*step)
		{
			case 'd':
				initDropTables(con);
				break;
			case 't':
				initCreateTables(con);
				break;
			case 'g':
				initGenerateData(con);
				break;
			case 'v':
				initVacuum(con);
				break;
			case 'p':
				initCreatePKeys(con);
				break;
			case 'f':
				initCreateFKeys(con);
				break;
			case ' ':
				break;			/* ignore */
			default:
				fprintf(stderr, "unrecognized initialization step \"%c\"\n",
						*step);
				PQfinish(con);
				exit(1);
		}
	}

	fprintf(stderr, "done.\n");
	PQfinish(con);
}

/*
 * Replace :param with $n throughout the command's SQL text, which
 * is a modifiable string in cmd->lines.
 */
static bool
parseQuery(Command *cmd)
{
	char	   *sql,
			   *p;

	cmd->argc = 1;

	p = sql = pg_strdup(cmd->lines.data);
	while ((p = strchr(p, ':')) != NULL)
	{
		char		var[13];
		char	   *name;
		int			eaten;

		name = parseVariable(p, &eaten);
		if (name == NULL)
		{
			while (*p == ':')
			{
				p++;
			}
			continue;
		}

		/*
		 * cmd->argv[0] is the SQL statement itself, so the max number of
		 * arguments is one less than MAX_ARGS
		 */
		if (cmd->argc >= MAX_ARGS)
		{
			fprintf(stderr, "statement has too many arguments (maximum is %d): %s\n",
					MAX_ARGS - 1, cmd->lines.data);
			pg_free(name);
			return false;
		}

		sprintf(var, "$%d", cmd->argc);
		p = replaceVariable(&sql, p, eaten, var);

		cmd->argv[cmd->argc] = name;
		cmd->argc++;
	}

	Assert(cmd->argv[0] == NULL);
	cmd->argv[0] = sql;
	return true;
}

/*
 * syntax error while parsing a script (in practice, while parsing a
 * backslash command, because we don't detect syntax errors in SQL)
 *
 * source: source of script (filename or builtin-script ID)
 * lineno: line number within script (count from 1)
 * line: whole line of backslash command, if available
 * command: backslash command name, if available
 * msg: the actual error message
 * more: optional extra message
 * column: zero-based column number, or -1 if unknown
 */
void
syntax_error(const char *source, int lineno,
			 const char *line, const char *command,
			 const char *msg, const char *more, int column)
{
	fprintf(stderr, "%s:%d: %s", source, lineno, msg);
	if (more != NULL)
		fprintf(stderr, " (%s)", more);
	if (column >= 0 && line == NULL)
		fprintf(stderr, " at column %d", column + 1);
	if (command != NULL)
		fprintf(stderr, " in command \"%s\"", command);
	fprintf(stderr, "\n");
	if (line != NULL)
	{
		fprintf(stderr, "%s\n", line);
		if (column >= 0)
		{
			int			i;

			for (i = 0; i < column; i++)
				fprintf(stderr, " ");
			fprintf(stderr, "^ error found here\n");
		}
	}
	exit(1);
}

/*
 * Return a pointer to the start of the SQL command, after skipping over
 * whitespace and "--" comments.
 * If the end of the string is reached, return NULL.
 */
static char *
skip_sql_comments(char *sql_command)
{
	char	   *p = sql_command;

	/* Skip any leading whitespace, as well as "--" style comments */
	for (;;)
	{
		if (isspace((unsigned char) *p))
			p++;
		else if (strncmp(p, "--", 2) == 0)
		{
			p = strchr(p, '\n');
			if (p == NULL)
				return NULL;
			p++;
		}
		else
			break;
	}

	/* NULL if there's nothing but whitespace and comments */
	if (*p == '\0')
		return NULL;

	return p;
}

/*
 * Parse a SQL command; return a Command struct, or NULL if it's a comment
 *
 * On entry, psqlscan.l has collected the command into "buf", so we don't
 * really need to do much here except check for comments and set up a Command
 * struct.
 */
static Command *
create_sql_command(PQExpBuffer buf, const char *source)
{
	Command    *my_command;
	char	   *p = skip_sql_comments(buf->data);

	if (p == NULL)
		return NULL;

	/* Allocate and initialize Command structure */
	my_command = (Command *) pg_malloc(sizeof(Command));
	initPQExpBuffer(&my_command->lines);
	appendPQExpBufferStr(&my_command->lines, p);
	my_command->first_line = NULL;	/* this is set later */
	my_command->type = SQL_COMMAND;
	my_command->meta = META_NONE;
	my_command->argc = 0;
	memset(my_command->argv, 0, sizeof(my_command->argv));
	my_command->varprefix = NULL;	/* allocated later, if needed */
	my_command->expr = NULL;
	initSimpleStats(&my_command->stats);

	return my_command;
}

/* Free a Command structure and associated data */
static void
free_command(Command *command)
{
	termPQExpBuffer(&command->lines);
	if (command->first_line)
		pg_free(command->first_line);
	for (int i = 0; i < command->argc; i++)
		pg_free(command->argv[i]);
	if (command->varprefix)
		pg_free(command->varprefix);

	/*
	 * It should also free expr recursively, but this is currently not needed
	 * as only gset commands (which do not have an expression) are freed.
	 */
	pg_free(command);
}

/*
 * Once an SQL command is fully parsed, possibly by accumulating several
 * parts, complete other fields of the Command structure.
 */
static void
postprocess_sql_command(Command *my_command)
{
	char		buffer[128];

	Assert(my_command->type == SQL_COMMAND);

	/* Save the first line for error display. */
	strlcpy(buffer, my_command->lines.data, sizeof(buffer));
	buffer[strcspn(buffer, "\n\r")] = '\0';
	my_command->first_line = pg_strdup(buffer);

	/* parse query if necessary */
	switch (querymode)
	{
		case QUERY_SIMPLE:
			my_command->argv[0] = my_command->lines.data;
			my_command->argc++;
			break;
		case QUERY_EXTENDED:
		case QUERY_PREPARED:
			if (!parseQuery(my_command))
				exit(1);
			break;
		default:
			exit(1);
	}
}

/*
 * Parse a backslash command; return a Command struct, or NULL if comment
 *
 * At call, we have scanned only the initial backslash.
 */
static Command *
process_backslash_command(PsqlScanState sstate, const char *source)
{
	Command    *my_command;
	PQExpBufferData word_buf;
	int			word_offset;
	int			offsets[MAX_ARGS];	/* offsets of argument words */
	int			start_offset;
	int			lineno;
	int			j;

	initPQExpBuffer(&word_buf);

	/* Remember location of the backslash */
	start_offset = expr_scanner_offset(sstate) - 1;
	lineno = expr_scanner_get_lineno(sstate, start_offset);

	/* Collect first word of command */
	if (!expr_lex_one_word(sstate, &word_buf, &word_offset))
	{
		termPQExpBuffer(&word_buf);
		return NULL;
	}

	/* Allocate and initialize Command structure */
	my_command = (Command *) pg_malloc0(sizeof(Command));
	my_command->type = META_COMMAND;
	my_command->argc = 0;
	initSimpleStats(&my_command->stats);

	/* Save first word (command name) */
	j = 0;
	offsets[j] = word_offset;
	my_command->argv[j++] = pg_strdup(word_buf.data);
	my_command->argc++;

	/* ... and convert it to enum form */
	my_command->meta = getMetaCommand(my_command->argv[0]);

	if (my_command->meta == META_SET ||
		my_command->meta == META_IF ||
		my_command->meta == META_ELIF)
	{
		yyscan_t	yyscanner;

		/* For \set, collect var name */
		if (my_command->meta == META_SET)
		{
			if (!expr_lex_one_word(sstate, &word_buf, &word_offset))
				syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
							 "missing argument", NULL, -1);

			offsets[j] = word_offset;
			my_command->argv[j++] = pg_strdup(word_buf.data);
			my_command->argc++;
		}

		/* then for all parse the expression */
		yyscanner = expr_scanner_init(sstate, source, lineno, start_offset,
									  my_command->argv[0]);

		if (expr_yyparse(yyscanner) != 0)
		{
			/* dead code: exit done from syntax_error called by yyerror */
			exit(1);
		}

		my_command->expr = expr_parse_result;

		/* Save line, trimming any trailing newline */
		my_command->first_line =
			expr_scanner_get_substring(sstate,
									   start_offset,
									   expr_scanner_offset(sstate),
									   true);

		expr_scanner_finish(yyscanner);

		termPQExpBuffer(&word_buf);

		return my_command;
	}

	/* For all other commands, collect remaining words. */
	while (expr_lex_one_word(sstate, &word_buf, &word_offset))
	{
		/*
		 * my_command->argv[0] is the command itself, so the max number of
		 * arguments is one less than MAX_ARGS
		 */
		if (j >= MAX_ARGS)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "too many arguments", NULL, -1);

		offsets[j] = word_offset;
		my_command->argv[j++] = pg_strdup(word_buf.data);
		my_command->argc++;
	}

	/* Save line, trimming any trailing newline */
	my_command->first_line =
		expr_scanner_get_substring(sstate,
								   start_offset,
								   expr_scanner_offset(sstate),
								   true);

	if (my_command->meta == META_SLEEP)
	{
		if (my_command->argc < 2)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "missing argument", NULL, -1);

		if (my_command->argc > 3)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "too many arguments", NULL,
						 offsets[3] - start_offset);

		/*
		 * Split argument into number and unit to allow "sleep 1ms" etc. We
		 * don't have to terminate the number argument with null because it
		 * will be parsed with atoi, which ignores trailing non-digit
		 * characters.
		 */
		if (my_command->argc == 2 && my_command->argv[1][0] != ':')
		{
			char	   *c = my_command->argv[1];

			while (isdigit((unsigned char) *c))
				c++;
			if (*c)
			{
				my_command->argv[2] = c;
				offsets[2] = offsets[1] + (c - my_command->argv[1]);
				my_command->argc = 3;
			}
		}

		if (my_command->argc == 3)
		{
			if (pg_strcasecmp(my_command->argv[2], "us") != 0 &&
				pg_strcasecmp(my_command->argv[2], "ms") != 0 &&
				pg_strcasecmp(my_command->argv[2], "s") != 0)
				syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
							 "unrecognized time unit, must be us, ms or s",
							 my_command->argv[2], offsets[2] - start_offset);
		}
	}
	else if (my_command->meta == META_SETSHELL)
	{
		if (my_command->argc < 3)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "missing argument", NULL, -1);
	}
	else if (my_command->meta == META_SHELL)
	{
		if (my_command->argc < 2)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "missing command", NULL, -1);
	}
	else if (my_command->meta == META_ELSE || my_command->meta == META_ENDIF)
	{
		if (my_command->argc != 1)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "unexpected argument", NULL, -1);
	}
	else if (my_command->meta == META_GSET)
	{
		if (my_command->argc > 2)
			syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
						 "too many arguments", NULL, -1);
	}
	else
	{
		/* my_command->meta == META_NONE */
		syntax_error(source, lineno, my_command->first_line, my_command->argv[0],
					 "invalid command", NULL, -1);
	}

	termPQExpBuffer(&word_buf);

	return my_command;
}

static void
ConditionError(const char *desc, int cmdn, const char *msg)
{
	fprintf(stderr,
			"condition error in script \"%s\" command %d: %s\n",
			desc, cmdn, msg);
	exit(1);
}

/*
 * Partial evaluation of conditionals before recording and running the script.
 */
static void
CheckConditional(ParsedScript ps)
{
	/* statically check conditional structure */
	ConditionalStack cs = conditional_stack_create();
	int			i;

	for (i = 0; ps.commands[i] != NULL; i++)
	{
		Command    *cmd = ps.commands[i];

		if (cmd->type == META_COMMAND)
		{
			switch (cmd->meta)
			{
				case META_IF:
					conditional_stack_push(cs, IFSTATE_FALSE);
					break;
				case META_ELIF:
					if (conditional_stack_empty(cs))
						ConditionError(ps.desc, i + 1, "\\elif without matching \\if");
					if (conditional_stack_peek(cs) == IFSTATE_ELSE_FALSE)
						ConditionError(ps.desc, i + 1, "\\elif after \\else");
					break;
				case META_ELSE:
					if (conditional_stack_empty(cs))
						ConditionError(ps.desc, i + 1, "\\else without matching \\if");
					if (conditional_stack_peek(cs) == IFSTATE_ELSE_FALSE)
						ConditionError(ps.desc, i + 1, "\\else after \\else");
					conditional_stack_poke(cs, IFSTATE_ELSE_FALSE);
					break;
				case META_ENDIF:
					if (!conditional_stack_pop(cs))
						ConditionError(ps.desc, i + 1, "\\endif without matching \\if");
					break;
				default:
					/* ignore anything else... */
					break;
			}
		}
	}
	if (!conditional_stack_empty(cs))
		ConditionError(ps.desc, i + 1, "\\if without matching \\endif");
	conditional_stack_destroy(cs);
}

/*
 * Parse a script (either the contents of a file, or a built-in script)
 * and add it to the list of scripts.
 */
static void
ParseScript(const char *script, const char *desc, int weight)
{
	ParsedScript ps;
	PsqlScanState sstate;
	PQExpBufferData line_buf;
	int			alloc_num;
	int			index;
	int			lineno;
	int			start_offset;

#define COMMANDS_ALLOC_NUM 128
	alloc_num = COMMANDS_ALLOC_NUM;

	/* Initialize all fields of ps */
	ps.desc = desc;
	ps.weight = weight;
	ps.commands = (Command **) pg_malloc(sizeof(Command *) * alloc_num);
	initStats(&ps.stats, 0);

	/* Prepare to parse script */
	sstate = psql_scan_create(&pgbench_callbacks);

	/*
	 * Ideally, we'd scan scripts using the encoding and stdstrings settings
	 * we get from a DB connection.  However, without major rearrangement of
	 * pgbench's argument parsing, we can't have a DB connection at the time
	 * we parse scripts.  Using SQL_ASCII (encoding 0) should work well enough
	 * with any backend-safe encoding, though conceivably we could be fooled
	 * if a script file uses a client-only encoding.  We also assume that
	 * stdstrings should be true, which is a bit riskier.
	 */
	psql_scan_setup(sstate, script, strlen(script), 0, true);
	start_offset = expr_scanner_offset(sstate) - 1;

	initPQExpBuffer(&line_buf);

	index = 0;

	for (;;)
	{
		PsqlScanResult sr;
		promptStatus_t prompt;
		Command    *command = NULL;

		resetPQExpBuffer(&line_buf);
		lineno = expr_scanner_get_lineno(sstate, start_offset);

		sr = psql_scan(sstate, &line_buf, &prompt);

		/* If we collected a new SQL command, process that */
		command = create_sql_command(&line_buf, desc);

		/* store new command */
		if (command)
			ps.commands[index++] = command;

		/* If we reached a backslash, process that */
		if (sr == PSCAN_BACKSLASH)
		{
			command = process_backslash_command(sstate, desc);

			if (command)
			{
				/*
				 * If this is gset, merge into the preceding command. (We
				 * don't use a command slot in this case).
				 */
				if (command->meta == META_GSET)
				{
					Command    *cmd;

					if (index == 0)
						syntax_error(desc, lineno, NULL, NULL,
									 "\\gset must follow a SQL command",
									 NULL, -1);

					cmd = ps.commands[index - 1];

					if (cmd->type != SQL_COMMAND ||
						cmd->varprefix != NULL)
						syntax_error(desc, lineno, NULL, NULL,
									 "\\gset must follow a SQL command",
									 cmd->first_line, -1);

					/* get variable prefix */
					if (command->argc <= 1 || command->argv[1][0] == '\0')
						cmd->varprefix = pg_strdup("");
					else
						cmd->varprefix = pg_strdup(command->argv[1]);

					/* cleanup unused command */
					free_command(command);

					continue;
				}

				/* Attach any other backslash command as a new command */
				ps.commands[index++] = command;
			}
		}

		/*
		 * Since we used a command slot, allocate more if needed.  Note we
		 * always allocate one more in order to accommodate the NULL
		 * terminator below.
		 */
		if (index >= alloc_num)
		{
			alloc_num += COMMANDS_ALLOC_NUM;
			ps.commands = (Command **)
				pg_realloc(ps.commands, sizeof(Command *) * alloc_num);
		}

		/* Done if we reached EOF */
		if (sr == PSCAN_INCOMPLETE || sr == PSCAN_EOL)
			break;
	}

	ps.commands[index] = NULL;

	addScript(ps);

	termPQExpBuffer(&line_buf);
	psql_scan_finish(sstate);
	psql_scan_destroy(sstate);
}

/*
 * Read the entire contents of file fd, and return it in a malloc'd buffer.
 *
 * The buffer will typically be larger than necessary, but we don't care
 * in this program, because we'll free it as soon as we've parsed the script.
 */
static char *
read_file_contents(FILE *fd)
{
	char	   *buf;
	size_t		buflen = BUFSIZ;
	size_t		used = 0;

	buf = (char *) pg_malloc(buflen);

	for (;;)
	{
		size_t		nread;

		nread = fread(buf + used, 1, BUFSIZ, fd);
		used += nread;
		/* If fread() read less than requested, must be EOF or error */
		if (nread < BUFSIZ)
			break;
		/* Enlarge buf so we can read some more */
		buflen += BUFSIZ;
		buf = (char *) pg_realloc(buf, buflen);
	}
	/* There is surely room for a terminator */
	buf[used] = '\0';

	return buf;
}

/*
 * Given a file name, read it and add its script to the list.
 * "-" means to read stdin.
 * NB: filename must be storage that won't disappear.
 */
static void
process_file(const char *filename, int weight)
{
	FILE	   *fd;
	char	   *buf;

	/* Slurp the file contents into "buf" */
	if (strcmp(filename, "-") == 0)
		fd = stdin;
	else if ((fd = fopen(filename, "r")) == NULL)
	{
		fprintf(stderr, "could not open file \"%s\": %s\n",
				filename, strerror(errno));
		exit(1);
	}

	buf = read_file_contents(fd);

	if (ferror(fd))
	{
		fprintf(stderr, "could not read file \"%s\": %s\n",
				filename, strerror(errno));
		exit(1);
	}

	if (fd != stdin)
		fclose(fd);

	ParseScript(buf, filename, weight);

	free(buf);
}

/* Parse the given builtin script and add it to the list. */
static void
process_builtin(const BuiltinScript *bi, int weight)
{
	ParseScript(bi->script, bi->desc, weight);
}

/* show available builtin scripts */
static void
listAvailableScripts(void)
{
	int			i;

	fprintf(stderr, "Available builtin scripts:\n");
	for (i = 0; i < lengthof(builtin_script); i++)
		fprintf(stderr, "\t%s\n", builtin_script[i].name);
	fprintf(stderr, "\n");
}

/* return builtin script "name" if unambiguous, fails if not found */
static const BuiltinScript *
findBuiltin(const char *name)
{
	int			i,
				found = 0,
				len = strlen(name);
	const BuiltinScript *result = NULL;

	for (i = 0; i < lengthof(builtin_script); i++)
	{
		if (strncmp(builtin_script[i].name, name, len) == 0)
		{
			result = &builtin_script[i];
			found++;
		}
	}

	/* ok, unambiguous result */
	if (found == 1)
		return result;

	/* error cases */
	if (found == 0)
		fprintf(stderr, "no builtin script found for name \"%s\"\n", name);
	else						/* found > 1 */
		fprintf(stderr,
				"ambiguous builtin name: %d builtin scripts found for prefix \"%s\"\n", found, name);

	listAvailableScripts();
	exit(1);
}

/*
 * Determine the weight specification from a script option (-b, -f), if any,
 * and return it as an integer (1 is returned if there's no weight).  The
 * script name is returned in *script as a malloc'd string.
 */
static int
parseScriptWeight(const char *option, char **script)
{
	char	   *sep;
	int			weight;

	if ((sep = strrchr(option, WSEP)))
	{
		int			namelen = sep - option;
		long		wtmp;
		char	   *badp;

		/* generate the script name */
		*script = pg_malloc(namelen + 1);
		strncpy(*script, option, namelen);
		(*script)[namelen] = '\0';

		/* process digits of the weight spec */
		errno = 0;
		wtmp = strtol(sep + 1, &badp, 10);
		if (errno != 0 || badp == sep + 1 || *badp != '\0')
		{
			fprintf(stderr, "invalid weight specification: %s\n", sep);
			exit(1);
		}
		if (wtmp > INT_MAX || wtmp < 0)
		{
			fprintf(stderr,
					"weight specification out of range (0 .. %u): " INT64_FORMAT "\n",
					INT_MAX, (int64) wtmp);
			exit(1);
		}
		weight = wtmp;
	}
	else
	{
		*script = pg_strdup(option);
		weight = 1;
	}

	return weight;
}

/* append a script to the list of scripts to process */
static void
addScript(ParsedScript script)
{
	if (script.commands == NULL || script.commands[0] == NULL)
	{
		fprintf(stderr, "empty command list for script \"%s\"\n", script.desc);
		exit(1);
	}

	if (num_scripts >= MAX_SCRIPTS)
	{
		fprintf(stderr, "at most %d SQL scripts are allowed\n", MAX_SCRIPTS);
		exit(1);
	}

	CheckConditional(script);

	sql_script[num_scripts] = script;
	num_scripts++;
}

/*
 * Print progress report.
 *
 * On entry, *last and *last_report contain the statistics and time of last
 * progress report.  On exit, they are updated with the new stats.
 */
static void
printProgressReport(TState *threads, int64 test_start, int64 now,
					StatsData *last, int64 *last_report)
{
	/* generate and show report */
	int64		run = now - *last_report,
				ntx;
	double		tps,
				total_run,
				latency,
				sqlat,
				lag,
				stdev;
	char		tbuf[315];
	StatsData	cur;

	/*
	 * Add up the statistics of all threads.
	 *
	 * XXX: No locking.  There is no guarantee that we get an atomic snapshot
	 * of the transaction count and latencies, so these figures can well be
	 * off by a small amount.  The progress report's purpose is to give a
	 * quick overview of how the test is going, so that shouldn't matter too
	 * much.  (If a read from a 64-bit integer is not atomic, you might get a
	 * "torn" read and completely bogus latencies though!)
	 */
	initStats(&cur, 0);
	for (int i = 0; i < nthreads; i++)
	{
		mergeSimpleStats(&cur.latency, &threads[i].stats.latency);
		mergeSimpleStats(&cur.lag, &threads[i].stats.lag);
		cur.cnt += threads[i].stats.cnt;
		cur.skipped += threads[i].stats.skipped;
	}

	/* we count only actually executed transactions */
	ntx = (cur.cnt - cur.skipped) - (last->cnt - last->skipped);
	total_run = (now - test_start) / 1000000.0;
	tps = 1000000.0 * ntx / run;
	if (ntx > 0)
	{
		latency = 0.001 * (cur.latency.sum - last->latency.sum) / ntx;
		sqlat = 1.0 * (cur.latency.sum2 - last->latency.sum2) / ntx;
		stdev = 0.001 * sqrt(sqlat - 1000000.0 * latency * latency);
		lag = 0.001 * (cur.lag.sum - last->lag.sum) / ntx;
	}
	else
	{
		latency = sqlat = stdev = lag = 0;
	}

	if (progress_timestamp)
	{
		/*
		 * On some platforms the current system timestamp is available in
		 * now_time, but rather than get entangled with that, we just eat the
		 * cost of an extra syscall in all cases.
		 */
		struct timeval tv;

		gettimeofday(&tv, NULL);
		snprintf(tbuf, sizeof(tbuf), "%ld.%03ld s",
				 (long) tv.tv_sec, (long) (tv.tv_usec / 1000));
	}
	else
	{
		/* round seconds are expected, but the thread may be late */
		snprintf(tbuf, sizeof(tbuf), "%.1f s", total_run);
	}

	fprintf(stderr,
			"progress: %s, %.1f tps, lat %.3f ms stddev %.3f",
			tbuf, tps, latency, stdev);

	if (throttle_delay)
	{
		fprintf(stderr, ", lag %.3f ms", lag);
		if (latency_limit)
			fprintf(stderr, ", " INT64_FORMAT " skipped",
					cur.skipped - last->skipped);
	}
	fprintf(stderr, "\n");

	*last = cur;
	*last_report = now;
}

static void
printSimpleStats(const char *prefix, SimpleStats *ss)
{
	if (ss->count > 0)
	{
		double		latency = ss->sum / ss->count;
		double		stddev = sqrt(ss->sum2 / ss->count - latency * latency);

		printf("%s average = %.3f ms\n", prefix, 0.001 * latency);
		printf("%s stddev = %.3f ms\n", prefix, 0.001 * stddev);
	}
}

/* print out results */
static void
printResults(StatsData *total, instr_time total_time,
			 instr_time conn_total_time, int64 latency_late)
{
	double		time_include,
				tps_include,
				tps_exclude;
	int64		ntx = total->cnt - total->skipped;

	time_include = INSTR_TIME_GET_DOUBLE(total_time);

	/* tps is about actually executed transactions */
	tps_include = ntx / time_include;
	tps_exclude = ntx /
		(time_include - (INSTR_TIME_GET_DOUBLE(conn_total_time) / nclients));

	/* Report test parameters. */
	printf("transaction type: %s\n",
		   num_scripts == 1 ? sql_script[0].desc : "multiple scripts");
	printf("scaling factor: %d\n", scale);
	printf("query mode: %s\n", QUERYMODE[querymode]);
	printf("number of clients: %d\n", nclients);
	printf("number of threads: %d\n", nthreads);
	if (duration <= 0)
	{
		printf("number of transactions per client: %d\n", nxacts);
		printf("number of transactions actually processed: " INT64_FORMAT "/%d\n",
			   ntx, nxacts * nclients);
	}
	else
	{
		printf("duration: %d s\n", duration);
		printf("number of transactions actually processed: " INT64_FORMAT "\n",
			   ntx);
	}

	/* Remaining stats are nonsensical if we failed to execute any xacts */
	if (total->cnt <= 0)
		return;

	if (throttle_delay && latency_limit)
		printf("number of transactions skipped: " INT64_FORMAT " (%.3f %%)\n",
			   total->skipped,
			   100.0 * total->skipped / total->cnt);

	if (latency_limit)
		printf("number of transactions above the %.1f ms latency limit: " INT64_FORMAT "/" INT64_FORMAT " (%.3f %%)\n",
			   latency_limit / 1000.0, latency_late, ntx,
			   (ntx > 0) ? 100.0 * latency_late / ntx : 0.0);

	if (throttle_delay || progress || latency_limit)
		printSimpleStats("latency", &total->latency);
	else
	{
		/* no measurement, show average latency computed from run time */
		printf("latency average = %.3f ms\n",
			   1000.0 * time_include * nclients / total->cnt);
	}

	if (throttle_delay)
	{
		/*
		 * Report average transaction lag under rate limit throttling.  This
		 * is the delay between scheduled and actual start times for the
		 * transaction.  The measured lag may be caused by thread/client load,
		 * the database load, or the Poisson throttling process.
		 */
		printf("rate limit schedule lag: avg %.3f (max %.3f) ms\n",
			   0.001 * total->lag.sum / total->cnt, 0.001 * total->lag.max);
	}

	printf("tps = %f (including connections establishing)\n", tps_include);
	printf("tps = %f (excluding connections establishing)\n", tps_exclude);

	/* Report per-script/command statistics */
	if (per_script_stats || report_per_command)
	{
		int			i;

		for (i = 0; i < num_scripts; i++)
		{
			if (per_script_stats)
			{
				StatsData  *sstats = &sql_script[i].stats;

				printf("SQL script %d: %s\n"
					   " - weight: %d (targets %.1f%% of total)\n"
					   " - " INT64_FORMAT " transactions (%.1f%% of total, tps = %f)\n",
					   i + 1, sql_script[i].desc,
					   sql_script[i].weight,
					   100.0 * sql_script[i].weight / total_weight,
					   sstats->cnt,
					   100.0 * sstats->cnt / total->cnt,
					   (sstats->cnt - sstats->skipped) / time_include);

				if (throttle_delay && latency_limit && sstats->cnt > 0)
					printf(" - number of transactions skipped: " INT64_FORMAT " (%.3f%%)\n",
						   sstats->skipped,
						   100.0 * sstats->skipped / sstats->cnt);

				printSimpleStats(" - latency", &sstats->latency);
			}

			/* Report per-command latencies */
			if (report_per_command)
			{
				Command   **commands;

				if (per_script_stats)
					printf(" - statement latencies in milliseconds:\n");
				else
					printf("statement latencies in milliseconds:\n");

				for (commands = sql_script[i].commands;
					 *commands != NULL;
					 commands++)
				{
					SimpleStats *cstats = &(*commands)->stats;

					printf("   %11.3f  %s\n",
						   (cstats->count > 0) ?
						   1000.0 * cstats->sum / cstats->count : 0.0,
						   (*commands)->first_line);
				}
			}
		}
	}
}

/*
 * Set up a random seed according to seed parameter (NULL means default),
 * and initialize base_random_sequence for use in initializing other sequences.
 */
static bool
set_random_seed(const char *seed)
{
	uint64		iseed;

	if (seed == NULL || strcmp(seed, "time") == 0)
	{
		/* rely on current time */
		instr_time	now;

		INSTR_TIME_SET_CURRENT(now);
		iseed = (uint64) INSTR_TIME_GET_MICROSEC(now);
	}
	else if (strcmp(seed, "rand") == 0)
	{
		/* use some "strong" random source */
		if (!pg_strong_random(&iseed, sizeof(iseed)))
		{
			fprintf(stderr, "could not generate random seed.\n");
			return false;
		}
	}
	else
	{
		/* parse unsigned-int seed value */
		unsigned long ulseed;
		char		garbage;

		/* Don't try to use UINT64_FORMAT here; it might not work for sscanf */
		if (sscanf(seed, "%lu%c", &ulseed, &garbage) != 1)
		{
			fprintf(stderr,
					"unrecognized random seed option \"%s\": expecting an unsigned integer, \"time\" or \"rand\"\n",
					seed);
			return false;
		}
		iseed = (uint64) ulseed;
	}

	if (seed != NULL)
		fprintf(stderr, "setting random seed to " UINT64_FORMAT "\n", iseed);
	random_seed = iseed;

	/* Fill base_random_sequence with low-order bits of seed */
	base_random_sequence.xseed[0] = iseed & 0xFFFF;
	base_random_sequence.xseed[1] = (iseed >> 16) & 0xFFFF;
	base_random_sequence.xseed[2] = (iseed >> 32) & 0xFFFF;

	return true;
}

int
main(int argc, char **argv)
{
	static struct option long_options[] = {
		/* systematic long/short named options */
		{"builtin", required_argument, NULL, 'b'},
		{"client", required_argument, NULL, 'c'},
		{"connect", no_argument, NULL, 'C'},
		{"debug", no_argument, NULL, 'd'},
		{"define", required_argument, NULL, 'D'},
		{"file", required_argument, NULL, 'f'},
		{"fillfactor", required_argument, NULL, 'F'},
		{"host", required_argument, NULL, 'h'},
		{"initialize", no_argument, NULL, 'i'},
		{"init-steps", required_argument, NULL, 'I'},
		{"jobs", required_argument, NULL, 'j'},
		{"log", no_argument, NULL, 'l'},
		{"latency-limit", required_argument, NULL, 'L'},
		{"no-vacuum", no_argument, NULL, 'n'},
		{"port", required_argument, NULL, 'p'},
		{"progress", required_argument, NULL, 'P'},
		{"protocol", required_argument, NULL, 'M'},
		{"quiet", no_argument, NULL, 'q'},
		{"report-latencies", no_argument, NULL, 'r'},
		{"rate", required_argument, NULL, 'R'},
		{"scale", required_argument, NULL, 's'},
		{"select-only", no_argument, NULL, 'S'},
		{"skip-some-updates", no_argument, NULL, 'N'},
		{"time", required_argument, NULL, 'T'},
		{"transactions", required_argument, NULL, 't'},
		{"username", required_argument, NULL, 'U'},
		{"vacuum-all", no_argument, NULL, 'v'},
		/* long-named only options */
		{"unlogged-tables", no_argument, NULL, 1},
		{"tablespace", required_argument, NULL, 2},
		{"index-tablespace", required_argument, NULL, 3},
		{"sampling-rate", required_argument, NULL, 4},
		{"aggregate-interval", required_argument, NULL, 5},
		{"progress-timestamp", no_argument, NULL, 6},
		{"log-prefix", required_argument, NULL, 7},
		{"foreign-keys", no_argument, NULL, 8},
		{"random-seed", required_argument, NULL, 9},
		{"use-unique-keys", no_argument, &use_unique_key, 1},
		{NULL, 0, NULL, 0}
	};

	int			c;
	bool		is_init_mode = false;	/* initialize mode? */
	char	   *initialize_steps = NULL;
	bool		foreign_keys = false;
	bool		is_no_vacuum = false;
	bool		do_vacuum_accounts = false; /* vacuum accounts table? */
	int			optindex;
	bool		scale_given = false;

	bool		benchmarking_option_set = false;
	bool		initialization_option_set = false;
	bool		internal_script_used = false;

	CState	   *state;			/* status of clients */
	TState	   *threads;		/* array of thread */

	instr_time	start_time;		/* start up time */
	instr_time	total_time;
	instr_time	conn_total_time;
	int64		latency_late = 0;
	StatsData	stats;
	int			weight;

	int			i;
	int			nclients_dealt;

#ifdef HAVE_GETRLIMIT
	struct rlimit rlim;
#endif

	PGconn	   *con;
	PGresult   *res;
	char	   *env;

	int			exit_code = 0;

	pg_logging_init(argv[0]);
	progname = get_progname(argv[0]);

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage();
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pgbench (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	if ((env = getenv("PGHOST")) != NULL && *env != '\0')
		pghost = env;
	if ((env = getenv("PGPORT")) != NULL && *env != '\0')
		pgport = env;
	else if ((env = getenv("PGUSER")) != NULL && *env != '\0')
		login = env;

	state = (CState *) pg_malloc0(sizeof(CState));

	/* set random seed early, because it may be used while parsing scripts. */
	if (!set_random_seed(getenv("PGBENCH_RANDOM_SEED")))
	{
		fprintf(stderr, "error while setting random seed from PGBENCH_RANDOM_SEED environment variable\n");
		exit(1);
	}

	while ((c = getopt_long(argc, argv, "iI:h:nvp:dqb:SNc:j:Crs:t:T:U:lf:D:F:M:P:R:L:x:", long_options, &optindex)) != -1)
	{
		char	   *script;

		switch (c)
		{
			case 'i':
				is_init_mode = true;
				break;
			case 'x':
				storage_clause = optarg;
				break;
			case 'I':
				if (initialize_steps)
					pg_free(initialize_steps);
				initialize_steps = pg_strdup(optarg);
				checkInitSteps(initialize_steps);
				initialization_option_set = true;
				break;
			case 'h':
				pghost = pg_strdup(optarg);
				break;
			case 'n':
				is_no_vacuum = true;
				break;
			case 'v':
				benchmarking_option_set = true;
				do_vacuum_accounts = true;
				break;
			case 'p':
				pgport = pg_strdup(optarg);
				break;
			case 'd':
				debug++;
				break;
			case 'c':
				benchmarking_option_set = true;
				nclients = atoi(optarg);
				if (nclients <= 0)
				{
					fprintf(stderr, "invalid number of clients: \"%s\"\n",
							optarg);
					exit(1);
				}
#ifdef HAVE_GETRLIMIT
#ifdef RLIMIT_NOFILE			/* most platforms use RLIMIT_NOFILE */
				if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
#else							/* but BSD doesn't ... */
				if (getrlimit(RLIMIT_OFILE, &rlim) == -1)
#endif							/* RLIMIT_NOFILE */
				{
					fprintf(stderr, "getrlimit failed: %s\n", strerror(errno));
					exit(1);
				}
				if (rlim.rlim_cur < nclients + 3)
				{
					fprintf(stderr, "need at least %d open files, but system limit is %ld\n",
							nclients + 3, (long) rlim.rlim_cur);
					fprintf(stderr, "Reduce number of clients, or use limit/ulimit to increase the system limit.\n");
					exit(1);
				}
#endif							/* HAVE_GETRLIMIT */
				break;
			case 'j':			/* jobs */
				benchmarking_option_set = true;
				nthreads = atoi(optarg);
				if (nthreads <= 0)
				{
					fprintf(stderr, "invalid number of threads: \"%s\"\n",
							optarg);
					exit(1);
				}
#ifndef ENABLE_THREAD_SAFETY
				if (nthreads != 1)
				{
					fprintf(stderr, "threads are not supported on this platform; use -j1\n");
					exit(1);
				}
#endif							/* !ENABLE_THREAD_SAFETY */
				break;
			case 'C':
				benchmarking_option_set = true;
				is_connect = true;
				break;
			case 'r':
				benchmarking_option_set = true;
				report_per_command = true;
				break;
			case 's':
				scale_given = true;
				scale = atoi(optarg);
				if (scale <= 0)
				{
					fprintf(stderr, "invalid scaling factor: \"%s\"\n", optarg);
					exit(1);
				}
				break;
			case 't':
				benchmarking_option_set = true;
				nxacts = atoi(optarg);
				if (nxacts <= 0)
				{
					fprintf(stderr, "invalid number of transactions: \"%s\"\n",
							optarg);
					exit(1);
				}
				break;
			case 'T':
				benchmarking_option_set = true;
				duration = atoi(optarg);
				if (duration <= 0)
				{
					fprintf(stderr, "invalid duration: \"%s\"\n", optarg);
					exit(1);
				}
				break;
			case 'U':
				login = pg_strdup(optarg);
				break;
			case 'l':
				benchmarking_option_set = true;
				use_log = true;
				break;
			case 'q':
				initialization_option_set = true;
				use_quiet = true;
				break;
			case 'b':
				if (strcmp(optarg, "list") == 0)
				{
					listAvailableScripts();
					exit(0);
				}
				weight = parseScriptWeight(optarg, &script);
				process_builtin(findBuiltin(script), weight);
				benchmarking_option_set = true;
				internal_script_used = true;
				break;
			case 'S':
				process_builtin(findBuiltin("select-only"), 1);
				benchmarking_option_set = true;
				internal_script_used = true;
				break;
			case 'N':
				process_builtin(findBuiltin("simple-update"), 1);
				benchmarking_option_set = true;
				internal_script_used = true;
				break;
			case 'f':
				weight = parseScriptWeight(optarg, &script);
				process_file(script, weight);
				benchmarking_option_set = true;
				break;
			case 'D':
				{
					char	   *p;

					benchmarking_option_set = true;

					if ((p = strchr(optarg, '=')) == NULL || p == optarg || *(p + 1) == '\0')
					{
						fprintf(stderr, "invalid variable definition: \"%s\"\n",
								optarg);
						exit(1);
					}

					*p++ = '\0';
					if (!putVariable(&state[0], "option", optarg, p))
						exit(1);
				}
				break;
			case 'F':
				initialization_option_set = true;
				fillfactor = atoi(optarg);
				if (fillfactor < 10 || fillfactor > 100)
				{
					fprintf(stderr, "invalid fillfactor: \"%s\"\n", optarg);
					exit(1);
				}
				break;
			case 'M':
				benchmarking_option_set = true;
				for (querymode = 0; querymode < NUM_QUERYMODE; querymode++)
					if (strcmp(optarg, QUERYMODE[querymode]) == 0)
						break;
				if (querymode >= NUM_QUERYMODE)
				{
					fprintf(stderr, "invalid query mode (-M): \"%s\"\n",
							optarg);
					exit(1);
				}
				break;
			case 'P':
				benchmarking_option_set = true;
				progress = atoi(optarg);
				if (progress <= 0)
				{
					fprintf(stderr, "invalid thread progress delay: \"%s\"\n",
							optarg);
					exit(1);
				}
				break;
			case 'R':
				{
					/* get a double from the beginning of option value */
					double		throttle_value = atof(optarg);

					benchmarking_option_set = true;

					if (throttle_value <= 0.0)
					{
						fprintf(stderr, "invalid rate limit: \"%s\"\n", optarg);
						exit(1);
					}
					/* Invert rate limit into per-transaction delay in usec */
					throttle_delay = 1000000.0 / throttle_value;
				}
				break;
			case 'L':
				{
					double		limit_ms = atof(optarg);

					if (limit_ms <= 0.0)
					{
						fprintf(stderr, "invalid latency limit: \"%s\"\n",
								optarg);
						exit(1);
					}
					benchmarking_option_set = true;
					latency_limit = (int64) (limit_ms * 1000);
				}
				break;
			case 1:				/* unlogged-tables */
				initialization_option_set = true;
				unlogged_tables = true;
				break;
			case 2:				/* tablespace */
				initialization_option_set = true;
				tablespace = pg_strdup(optarg);
				break;
			case 3:				/* index-tablespace */
				initialization_option_set = true;
				index_tablespace = pg_strdup(optarg);
				break;
			case 4:				/* sampling-rate */
				benchmarking_option_set = true;
				sample_rate = atof(optarg);
				if (sample_rate <= 0.0 || sample_rate > 1.0)
				{
					fprintf(stderr, "invalid sampling rate: \"%s\"\n", optarg);
					exit(1);
				}
				break;
			case 5:				/* aggregate-interval */
				benchmarking_option_set = true;
				agg_interval = atoi(optarg);
				if (agg_interval <= 0)
				{
					fprintf(stderr, "invalid number of seconds for aggregation: \"%s\"\n",
							optarg);
					exit(1);
				}
				break;
			case 6:				/* progress-timestamp */
				progress_timestamp = true;
				benchmarking_option_set = true;
				break;
			case 7:				/* log-prefix */
				benchmarking_option_set = true;
				logfile_prefix = pg_strdup(optarg);
				break;
			case 8:				/* foreign-keys */
				initialization_option_set = true;
				foreign_keys = true;
				break;
			case 9:				/* random-seed */
				benchmarking_option_set = true;
				if (!set_random_seed(optarg))
				{
					fprintf(stderr, "error while setting random seed from --random-seed option\n");
					exit(1);
				}
				break;
			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
				break;
		}
	}

	/* set default script if none */
	if (num_scripts == 0 && !is_init_mode)
	{
		process_builtin(findBuiltin("tpcb-like"), 1);
		benchmarking_option_set = true;
		internal_script_used = true;
	}

	/* complete SQL command initialization and compute total weight */
	for (i = 0; i < num_scripts; i++)
	{
		Command   **commands = sql_script[i].commands;

		for (int j = 0; commands[j] != NULL; j++)
			if (commands[j]->type == SQL_COMMAND)
				postprocess_sql_command(commands[j]);

		/* cannot overflow: weight is 32b, total_weight 64b */
		total_weight += sql_script[i].weight;
	}

	if (total_weight == 0 && !is_init_mode)
	{
		fprintf(stderr, "total script weight must not be zero\n");
		exit(1);
	}

	/* show per script stats if several scripts are used */
	if (num_scripts > 1)
		per_script_stats = true;

	/*
	 * Don't need more threads than there are clients.  (This is not merely an
	 * optimization; throttle_delay is calculated incorrectly below if some
	 * threads have no clients assigned to them.)
	 */
	if (nthreads > nclients)
		nthreads = nclients;

	/*
	 * Convert throttle_delay to a per-thread delay time.  Note that this
	 * might be a fractional number of usec, but that's OK, since it's just
	 * the center of a Poisson distribution of delays.
	 */
	throttle_delay *= nthreads;

	if (argc > optind)
		dbName = argv[optind++];
	else
	{
		if ((env = getenv("PGDATABASE")) != NULL && *env != '\0')
			dbName = env;
		else if (login != NULL && *login != '\0')
			dbName = login;
		else
			dbName = "";
	}

	if (optind < argc)
	{
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	if (is_init_mode)
	{
		if (benchmarking_option_set)
		{
			fprintf(stderr, "some of the specified options cannot be used in initialization (-i) mode\n");
			exit(1);
		}

		if (initialize_steps == NULL)
			initialize_steps = pg_strdup(DEFAULT_INIT_STEPS);

		if (is_no_vacuum)
		{
			/* Remove any vacuum step in initialize_steps */
			char	   *p;

			while ((p = strchr(initialize_steps, 'v')) != NULL)
				*p = ' ';
		}

		if (foreign_keys)
		{
			/* Add 'f' to end of initialize_steps, if not already there */
			if (strchr(initialize_steps, 'f') == NULL)
			{
				initialize_steps = (char *)
					pg_realloc(initialize_steps,
							   strlen(initialize_steps) + 2);
				strcat(initialize_steps, "f");
			}
		}

		runInitSteps(initialize_steps);
		exit(0);
	}
	else
	{
		if (initialization_option_set)
		{
			fprintf(stderr, "some of the specified options cannot be used in benchmarking mode\n");
			exit(1);
		}
	}

	if (nxacts > 0 && duration > 0)
	{
		fprintf(stderr, "specify either a number of transactions (-t) or a duration (-T), not both\n");
		exit(1);
	}

	/* Use DEFAULT_NXACTS if neither nxacts nor duration is specified. */
	if (nxacts <= 0 && duration <= 0)
		nxacts = DEFAULT_NXACTS;

	/* --sampling-rate may be used only with -l */
	if (sample_rate > 0.0 && !use_log)
	{
		fprintf(stderr, "log sampling (--sampling-rate) is allowed only when logging transactions (-l)\n");
		exit(1);
	}

	/* --sampling-rate may not be used with --aggregate-interval */
	if (sample_rate > 0.0 && agg_interval > 0)
	{
		fprintf(stderr, "log sampling (--sampling-rate) and aggregation (--aggregate-interval) cannot be used at the same time\n");
		exit(1);
	}

	if (agg_interval > 0 && !use_log)
	{
		fprintf(stderr, "log aggregation is allowed only when actually logging transactions\n");
		exit(1);
	}

	if (!use_log && logfile_prefix)
	{
		fprintf(stderr, "log file prefix (--log-prefix) is allowed only when logging transactions (-l)\n");
		exit(1);
	}

	if (duration > 0 && agg_interval > duration)
	{
		fprintf(stderr, "number of seconds for aggregation (%d) must not be higher than test duration (%d)\n", agg_interval, duration);
		exit(1);
	}

	if (duration > 0 && agg_interval > 0 && duration % agg_interval != 0)
	{
		fprintf(stderr, "duration (%d) must be a multiple of aggregation interval (%d)\n", duration, agg_interval);
		exit(1);
	}

	if (progress_timestamp && progress == 0)
	{
		fprintf(stderr, "--progress-timestamp is allowed only under --progress\n");
		exit(1);
	}

	/*
	 * save main process id in the global variable because process id will be
	 * changed after fork.
	 */
	main_pid = (int) getpid();

	if (nclients > 1)
	{
		state = (CState *) pg_realloc(state, sizeof(CState) * nclients);
		memset(state + 1, 0, sizeof(CState) * (nclients - 1));

		/* copy any -D switch values to all clients */
		for (i = 1; i < nclients; i++)
		{
			int			j;

			state[i].id = i;
			for (j = 0; j < state[0].nvariables; j++)
			{
				Variable   *var = &state[0].variables[j];

				if (var->value.type != PGBT_NO_VALUE)
				{
					if (!putVariableValue(&state[i], "startup",
										  var->name, &var->value))
						exit(1);
				}
				else
				{
					if (!putVariable(&state[i], "startup",
									 var->name, var->svalue))
						exit(1);
				}
			}
		}
	}

	/* other CState initializations */
	for (i = 0; i < nclients; i++)
	{
		state[i].cstack = conditional_stack_create();
		initRandomState(&state[i].cs_func_rs);
	}

	if (debug)
	{
		if (duration <= 0)
			printf("pghost: %s pgport: %s nclients: %d nxacts: %d dbName: %s\n",
				   pghost, pgport, nclients, nxacts, dbName);
		else
			printf("pghost: %s pgport: %s nclients: %d duration: %d dbName: %s\n",
				   pghost, pgport, nclients, duration, dbName);
	}

	/* opening connection... */
	con = doConnect();
	if (con == NULL)
		exit(1);

	if (internal_script_used)
	{
		/*
		 * get the scaling factor that should be same as count(*) from
		 * pgbench_branches if this is not a custom query
		 */
		res = PQexec(con, "select count(*) from pgbench_branches");
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			char	   *sqlState = PQresultErrorField(res, PG_DIAG_SQLSTATE);

			fprintf(stderr, "%s", PQerrorMessage(con));
			if (sqlState && strcmp(sqlState, ERRCODE_UNDEFINED_TABLE) == 0)
			{
				fprintf(stderr, "Perhaps you need to do initialization (\"pgbench -i\") in database \"%s\"\n", PQdb(con));
			}

			exit(1);
		}
		scale = atoi(PQgetvalue(res, 0, 0));
		if (scale < 0)
		{
			fprintf(stderr, "invalid count(*) from pgbench_branches: \"%s\"\n",
					PQgetvalue(res, 0, 0));
			exit(1);
		}
		PQclear(res);

		/* warn if we override user-given -s switch */
		if (scale_given)
			fprintf(stderr,
					"scale option ignored, using count from pgbench_branches table (%d)\n",
					scale);
	}

	/*
	 * :scale variables normally get -s or database scale, but don't override
	 * an explicit -D switch
	 */
	if (lookupVariable(&state[0], "scale") == NULL)
	{
		for (i = 0; i < nclients; i++)
		{
			if (!putVariableInt(&state[i], "startup", "scale", scale))
				exit(1);
		}
	}

	/*
	 * Define a :client_id variable that is unique per connection. But don't
	 * override an explicit -D switch.
	 */
	if (lookupVariable(&state[0], "client_id") == NULL)
	{
		for (i = 0; i < nclients; i++)
			if (!putVariableInt(&state[i], "startup", "client_id", i))
				exit(1);
	}

	/* set default seed for hash functions */
	if (lookupVariable(&state[0], "default_seed") == NULL)
	{
		uint64		seed =
		((uint64) pg_jrand48(base_random_sequence.xseed) & 0xFFFFFFFF) |
		(((uint64) pg_jrand48(base_random_sequence.xseed) & 0xFFFFFFFF) << 32);

		for (i = 0; i < nclients; i++)
			if (!putVariableInt(&state[i], "startup", "default_seed", (int64) seed))
				exit(1);
	}

	/* set random seed unless overwritten */
	if (lookupVariable(&state[0], "random_seed") == NULL)
	{
		for (i = 0; i < nclients; i++)
			if (!putVariableInt(&state[i], "startup", "random_seed", random_seed))
				exit(1);
	}

	if (!is_no_vacuum)
	{
		fprintf(stderr, "starting vacuum...");
		tryExecuteStatement(con, "vacuum pgbench_branches");
		tryExecuteStatement(con, "vacuum pgbench_tellers");
		tryExecuteStatement(con, "truncate pgbench_history");
		fprintf(stderr, "end.\n");

		if (do_vacuum_accounts)
		{
			fprintf(stderr, "starting vacuum pgbench_accounts...");
			tryExecuteStatement(con, "vacuum analyze pgbench_accounts");
			fprintf(stderr, "end.\n");
		}
	}
	PQfinish(con);

	/* set up thread data structures */
	threads = (TState *) pg_malloc(sizeof(TState) * nthreads);
	nclients_dealt = 0;

	for (i = 0; i < nthreads; i++)
	{
		TState	   *thread = &threads[i];

		thread->tid = i;
		thread->state = &state[nclients_dealt];
		thread->nstate =
			(nclients - nclients_dealt + nthreads - i - 1) / (nthreads - i);
		initRandomState(&thread->ts_choose_rs);
		initRandomState(&thread->ts_throttle_rs);
		initRandomState(&thread->ts_sample_rs);
		thread->logfile = NULL; /* filled in later */
		thread->latency_late = 0;
		initStats(&thread->stats, 0);

		nclients_dealt += thread->nstate;
	}

	/* all clients must be assigned to a thread */
	Assert(nclients_dealt == nclients);

	/* get start up time */
	INSTR_TIME_SET_CURRENT(start_time);

	/* set alarm if duration is specified. */
	if (duration > 0)
		setalarm(duration);

	/* start threads */
#ifdef ENABLE_THREAD_SAFETY
	for (i = 0; i < nthreads; i++)
	{
		TState	   *thread = &threads[i];

		INSTR_TIME_SET_CURRENT(thread->start_time);

		/* compute when to stop */
		if (duration > 0)
			end_time = INSTR_TIME_GET_MICROSEC(thread->start_time) +
				(int64) 1000000 * duration;

		/* the first thread (i = 0) is executed by main thread */
		if (i > 0)
		{
			int			err = pthread_create(&thread->thread, NULL, threadRun, thread);

			if (err != 0 || thread->thread == INVALID_THREAD)
			{
				fprintf(stderr, "could not create thread: %s\n", strerror(err));
				exit(1);
			}
		}
		else
		{
			thread->thread = INVALID_THREAD;
		}
	}
#else
	INSTR_TIME_SET_CURRENT(threads[0].start_time);
	/* compute when to stop */
	if (duration > 0)
		end_time = INSTR_TIME_GET_MICROSEC(threads[0].start_time) +
			(int64) 1000000 * duration;
	threads[0].thread = INVALID_THREAD;
#endif							/* ENABLE_THREAD_SAFETY */

	/* wait for threads and accumulate results */
	initStats(&stats, 0);
	INSTR_TIME_SET_ZERO(conn_total_time);
	for (i = 0; i < nthreads; i++)
	{
		TState	   *thread = &threads[i];

#ifdef ENABLE_THREAD_SAFETY
		if (threads[i].thread == INVALID_THREAD)
			/* actually run this thread directly in the main thread */
			(void) threadRun(thread);
		else
			/* wait of other threads. should check that 0 is returned? */
			pthread_join(thread->thread, NULL);
#else
		(void) threadRun(thread);
#endif							/* ENABLE_THREAD_SAFETY */

		for (int j = 0; j < thread->nstate; j++)
			if (thread->state[j].state != CSTATE_FINISHED)
				exit_code = 2;

		/* aggregate thread level stats */
		mergeSimpleStats(&stats.latency, &thread->stats.latency);
		mergeSimpleStats(&stats.lag, &thread->stats.lag);
		stats.cnt += thread->stats.cnt;
		stats.skipped += thread->stats.skipped;
		latency_late += thread->latency_late;
		INSTR_TIME_ADD(conn_total_time, thread->conn_time);
	}
	disconnect_all(state, nclients);

	/*
	 * XXX We compute results as though every client of every thread started
	 * and finished at the same time.  That model can diverge noticeably from
	 * reality for a short benchmark run involving relatively many threads.
	 * The first thread may process notably many transactions before the last
	 * thread begins.  Improving the model alone would bring limited benefit,
	 * because performance during those periods of partial thread count can
	 * easily exceed steady state performance.  This is one of the many ways
	 * short runs convey deceptive performance figures.
	 */
	INSTR_TIME_SET_CURRENT(total_time);
	INSTR_TIME_SUBTRACT(total_time, start_time);
	printResults(&stats, total_time, conn_total_time, latency_late);

	if (exit_code != 0)
		fprintf(stderr, "Run was aborted; the above results are incomplete.\n");

	return exit_code;
}

static void *
threadRun(void *arg)
{
	TState	   *thread = (TState *) arg;
	CState	   *state = thread->state;
	instr_time	start,
				end;
	int			nstate = thread->nstate;
	int			remains = nstate;	/* number of remaining clients */
	socket_set *sockets = alloc_socket_set(nstate);
	int			i;

	/* for reporting progress: */
	int64		thread_start = INSTR_TIME_GET_MICROSEC(thread->start_time);
	int64		last_report = thread_start;
	int64		next_report = last_report + (int64) progress * 1000000;
	StatsData	last,
				aggs;

	/*
	 * Initialize throttling rate target for all of the thread's clients.  It
	 * might be a little more accurate to reset thread->start_time here too.
	 * The possible drift seems too small relative to typical throttle delay
	 * times to worry about it.
	 */
	INSTR_TIME_SET_CURRENT(start);
	thread->throttle_trigger = INSTR_TIME_GET_MICROSEC(start);

	INSTR_TIME_SET_ZERO(thread->conn_time);

	initStats(&aggs, time(NULL));
	last = aggs;

	/* open log file if requested */
	if (use_log)
	{
		char		logpath[MAXPGPATH];
		char	   *prefix = logfile_prefix ? logfile_prefix : "pgbench_log";

		if (thread->tid == 0)
			snprintf(logpath, sizeof(logpath), "%s.%d", prefix, main_pid);
		else
			snprintf(logpath, sizeof(logpath), "%s.%d.%d", prefix, main_pid, thread->tid);

		thread->logfile = fopen(logpath, "w");

		if (thread->logfile == NULL)
		{
			fprintf(stderr, "could not open logfile \"%s\": %s\n",
					logpath, strerror(errno));
			goto done;
		}
	}

	if (!is_connect)
	{
		/* make connections to the database before starting */
		for (i = 0; i < nstate; i++)
		{
			if ((state[i].con = doConnect()) == NULL)
				goto done;
		}
	}

	/* time after thread and connections set up */
	INSTR_TIME_SET_CURRENT(thread->conn_time);
	INSTR_TIME_SUBTRACT(thread->conn_time, thread->start_time);

	/* explicitly initialize the state machines */
	for (i = 0; i < nstate; i++)
	{
		state[i].state = CSTATE_CHOOSE_SCRIPT;
	}

	/* loop till all clients have terminated */
	while (remains > 0)
	{
		int			nsocks;		/* number of sockets to be waited for */
		int64		min_usec;
		int64		now_usec = 0;	/* set this only if needed */

		/*
		 * identify which client sockets should be checked for input, and
		 * compute the nearest time (if any) at which we need to wake up.
		 */
		clear_socket_set(sockets);
		nsocks = 0;
		min_usec = PG_INT64_MAX;
		for (i = 0; i < nstate; i++)
		{
			CState	   *st = &state[i];

			if (st->state == CSTATE_SLEEP || st->state == CSTATE_THROTTLE)
			{
				/* a nap from the script, or under throttling */
				int64		this_usec;

				/* get current time if needed */
				if (now_usec == 0)
				{
					instr_time	now;

					INSTR_TIME_SET_CURRENT(now);
					now_usec = INSTR_TIME_GET_MICROSEC(now);
				}

				/* min_usec should be the minimum delay across all clients */
				this_usec = (st->state == CSTATE_SLEEP ?
							 st->sleep_until : st->txn_scheduled) - now_usec;
				if (min_usec > this_usec)
					min_usec = this_usec;
			}
			else if (st->state == CSTATE_WAIT_RESULT)
			{
				/*
				 * waiting for result from server - nothing to do unless the
				 * socket is readable
				 */
				int			sock = PQsocket(st->con);

				if (sock < 0)
				{
					fprintf(stderr, "invalid socket: %s",
							PQerrorMessage(st->con));
					goto done;
				}

				add_socket_to_set(sockets, sock, nsocks++);
			}
			else if (st->state != CSTATE_ABORTED &&
					 st->state != CSTATE_FINISHED)
			{
				/*
				 * This client thread is ready to do something, so we don't
				 * want to wait.  No need to examine additional clients.
				 */
				min_usec = 0;
				break;
			}
		}

		/* also wake up to print the next progress report on time */
		if (progress && min_usec > 0 && thread->tid == 0)
		{
			/* get current time if needed */
			if (now_usec == 0)
			{
				instr_time	now;

				INSTR_TIME_SET_CURRENT(now);
				now_usec = INSTR_TIME_GET_MICROSEC(now);
			}

			if (now_usec >= next_report)
				min_usec = 0;
			else if ((next_report - now_usec) < min_usec)
				min_usec = next_report - now_usec;
		}

		/*
		 * If no clients are ready to execute actions, sleep until we receive
		 * data on some client socket or the timeout (if any) elapses.
		 */
		if (min_usec > 0)
		{
			int			rc = 0;

			if (min_usec != PG_INT64_MAX)
			{
				if (nsocks > 0)
				{
					rc = wait_on_socket_set(sockets, min_usec);
				}
				else			/* nothing active, simple sleep */
				{
					pg_usleep(min_usec);
				}
			}
			else				/* no explicit delay, wait without timeout */
			{
				rc = wait_on_socket_set(sockets, 0);
			}

			if (rc < 0)
			{
				if (errno == EINTR)
				{
					/* On EINTR, go back to top of loop */
					continue;
				}
				/* must be something wrong */
				fprintf(stderr, "%s() failed: %s\n", SOCKET_WAIT_METHOD, strerror(errno));
				goto done;
			}
		}
		else
		{
			/* min_usec <= 0, i.e. something needs to be executed now */

			/* If we didn't wait, don't try to read any data */
			clear_socket_set(sockets);
		}

		/* ok, advance the state machine of each connection */
		nsocks = 0;
		for (i = 0; i < nstate; i++)
		{
			CState	   *st = &state[i];

			if (st->state == CSTATE_WAIT_RESULT)
			{
				/* don't call advanceConnectionState unless data is available */
				int			sock = PQsocket(st->con);

				if (sock < 0)
				{
					fprintf(stderr, "invalid socket: %s",
							PQerrorMessage(st->con));
					goto done;
				}

				if (!socket_has_input(sockets, sock, nsocks++))
					continue;
			}
			else if (st->state == CSTATE_FINISHED ||
					 st->state == CSTATE_ABORTED)
			{
				/* this client is done, no need to consider it anymore */
				continue;
			}

			advanceConnectionState(thread, st, &aggs);

			/*
			 * If advanceConnectionState changed client to finished state,
			 * that's one less client that remains.
			 */
			if (st->state == CSTATE_FINISHED || st->state == CSTATE_ABORTED)
				remains--;
		}

		/* progress report is made by thread 0 for all threads */
		if (progress && thread->tid == 0)
		{
			instr_time	now_time;
			int64		now;

			INSTR_TIME_SET_CURRENT(now_time);
			now = INSTR_TIME_GET_MICROSEC(now_time);
			if (now >= next_report)
			{
				/*
				 * Horrible hack: this relies on the thread pointer we are
				 * passed to be equivalent to threads[0], that is the first
				 * entry of the threads array.  That is why this MUST be done
				 * by thread 0 and not any other.
				 */
				printProgressReport(thread, thread_start, now,
									&last, &last_report);

				/*
				 * Ensure that the next report is in the future, in case
				 * pgbench/postgres got stuck somewhere.
				 */
				do
				{
					next_report += (int64) progress * 1000000;
				} while (now >= next_report);
			}
		}
	}

done:
	INSTR_TIME_SET_CURRENT(start);
	disconnect_all(state, nstate);
	INSTR_TIME_SET_CURRENT(end);
	INSTR_TIME_ACCUM_DIFF(thread->conn_time, end, start);
	if (thread->logfile)
	{
		if (agg_interval > 0)
		{
			/* log aggregated but not yet reported transactions */
			doLog(thread, state, &aggs, false, 0, 0);
		}
		fclose(thread->logfile);
		thread->logfile = NULL;
	}
	free_socket_set(sockets);
	return NULL;
}

static void
finishCon(CState *st)
{
	if (st->con != NULL)
	{
		PQfinish(st->con);
		st->con = NULL;
	}
}

/*
 * Support for duration option: set timer_exceeded after so many seconds.
 */

#ifndef WIN32

static void
handle_sig_alarm(SIGNAL_ARGS)
{
	timer_exceeded = true;
}

static void
setalarm(int seconds)
{
	pqsignal(SIGALRM, handle_sig_alarm);
	alarm(seconds);
}

#else							/* WIN32 */

static VOID CALLBACK
win32_timer_callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	timer_exceeded = true;
}

static void
setalarm(int seconds)
{
	HANDLE		queue;
	HANDLE		timer;

	/* This function will be called at most once, so we can cheat a bit. */
	queue = CreateTimerQueue();
	if (seconds > ((DWORD) -1) / 1000 ||
		!CreateTimerQueueTimer(&timer, queue,
							   win32_timer_callback, NULL, seconds * 1000, 0,
							   WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE))
	{
		fprintf(stderr, "failed to set timer\n");
		exit(1);
	}
}

#endif							/* WIN32 */


/*
 * These functions provide an abstraction layer that hides the syscall
 * we use to wait for input on a set of sockets.
 *
 * Currently there are two implementations, based on ppoll(2) and select(2).
 * ppoll() is preferred where available due to its typically higher ceiling
 * on the number of usable sockets.  We do not use the more-widely-available
 * poll(2) because it only offers millisecond timeout resolution, which could
 * be problematic with high --rate settings.
 *
 * Function APIs:
 *
 * alloc_socket_set: allocate an empty socket set with room for up to
 *		"count" sockets.
 *
 * free_socket_set: deallocate a socket set.
 *
 * clear_socket_set: reset a socket set to empty.
 *
 * add_socket_to_set: add socket with indicated FD to slot "idx" in the
 *		socket set.  Slots must be filled in order, starting with 0.
 *
 * wait_on_socket_set: wait for input on any socket in set, or for timeout
 *		to expire.  timeout is measured in microseconds; 0 means wait forever.
 *		Returns result code of underlying syscall (>=0 if OK, else see errno).
 *
 * socket_has_input: after waiting, call this to see if given socket has
 *		input.  fd and idx parameters should match some previous call to
 *		add_socket_to_set.
 *
 * Note that wait_on_socket_set destructively modifies the state of the
 * socket set.  After checking for input, caller must apply clear_socket_set
 * and add_socket_to_set again before waiting again.
 */

#ifdef POLL_USING_PPOLL

static socket_set *
alloc_socket_set(int count)
{
	socket_set *sa;

	sa = (socket_set *) pg_malloc0(offsetof(socket_set, pollfds) +
								   sizeof(struct pollfd) * count);
	sa->maxfds = count;
	sa->curfds = 0;
	return sa;
}

static void
free_socket_set(socket_set *sa)
{
	pg_free(sa);
}

static void
clear_socket_set(socket_set *sa)
{
	sa->curfds = 0;
}

static void
add_socket_to_set(socket_set *sa, int fd, int idx)
{
	Assert(idx < sa->maxfds && idx == sa->curfds);
	sa->pollfds[idx].fd = fd;
	sa->pollfds[idx].events = POLLIN;
	sa->pollfds[idx].revents = 0;
	sa->curfds++;
}

static int
wait_on_socket_set(socket_set *sa, int64 usecs)
{
	if (usecs > 0)
	{
		struct timespec timeout;

		timeout.tv_sec = usecs / 1000000;
		timeout.tv_nsec = (usecs % 1000000) * 1000;
		return ppoll(sa->pollfds, sa->curfds, &timeout, NULL);
	}
	else
	{
		return ppoll(sa->pollfds, sa->curfds, NULL, NULL);
	}
}

static bool
socket_has_input(socket_set *sa, int fd, int idx)
{
	/*
	 * In some cases, threadRun will apply clear_socket_set and then try to
	 * apply socket_has_input anyway with arguments that it used before that,
	 * or might've used before that except that it exited its setup loop
	 * early.  Hence, if the socket set is empty, silently return false
	 * regardless of the parameters.  If it's not empty, we can Assert that
	 * the parameters match a previous call.
	 */
	if (sa->curfds == 0)
		return false;

	Assert(idx < sa->curfds && sa->pollfds[idx].fd == fd);
	return (sa->pollfds[idx].revents & POLLIN) != 0;
}

#endif							/* POLL_USING_PPOLL */

#ifdef POLL_USING_SELECT

static socket_set *
alloc_socket_set(int count)
{
	return (socket_set *) pg_malloc0(sizeof(socket_set));
}

static void
free_socket_set(socket_set *sa)
{
	pg_free(sa);
}

static void
clear_socket_set(socket_set *sa)
{
	FD_ZERO(&sa->fds);
	sa->maxfd = -1;
}

static void
add_socket_to_set(socket_set *sa, int fd, int idx)
{
	if (fd < 0 || fd >= FD_SETSIZE)
	{
		/*
		 * Doing a hard exit here is a bit grotty, but it doesn't seem worth
		 * complicating the API to make it less grotty.
		 */
		fprintf(stderr, "too many client connections for select()\n");
		exit(1);
	}
	FD_SET(fd, &sa->fds);
	if (fd > sa->maxfd)
		sa->maxfd = fd;
}

static int
wait_on_socket_set(socket_set *sa, int64 usecs)
{
	if (usecs > 0)
	{
		struct timeval timeout;

		timeout.tv_sec = usecs / 1000000;
		timeout.tv_usec = usecs % 1000000;
		return select(sa->maxfd + 1, &sa->fds, NULL, NULL, &timeout);
	}
	else
	{
		return select(sa->maxfd + 1, &sa->fds, NULL, NULL, NULL);
	}
}

static bool
socket_has_input(socket_set *sa, int fd, int idx)
{
	return (FD_ISSET(fd, &sa->fds) != 0);
}

#endif							/* POLL_USING_SELECT */


/* partial pthread implementation for Windows */

#ifdef WIN32

typedef struct win32_pthread
{
	HANDLE		handle;
	void	   *(*routine) (void *);
	void	   *arg;
	void	   *result;
} win32_pthread;

static unsigned __stdcall
win32_pthread_run(void *arg)
{
	win32_pthread *th = (win32_pthread *) arg;

	th->result = th->routine(th->arg);

	return 0;
}

static int
pthread_create(pthread_t *thread,
			   pthread_attr_t *attr,
			   void *(*start_routine) (void *),
			   void *arg)
{
	int			save_errno;
	win32_pthread *th;

	th = (win32_pthread *) pg_malloc(sizeof(win32_pthread));
	th->routine = start_routine;
	th->arg = arg;
	th->result = NULL;

	th->handle = (HANDLE) _beginthreadex(NULL, 0, win32_pthread_run, th, 0, NULL);
	if (th->handle == NULL)
	{
		save_errno = errno;
		free(th);
		return save_errno;
	}

	*thread = th;
	return 0;
}

static int
pthread_join(pthread_t th, void **thread_return)
{
	if (th == NULL || th->handle == NULL)
		return errno = EINVAL;

	if (WaitForSingleObject(th->handle, INFINITE) != WAIT_OBJECT_0)
	{
		_dosmaperr(GetLastError());
		return errno;
	}

	if (thread_return)
		*thread_return = th->result;

	CloseHandle(th->handle);
	free(th);
	return 0;
}

#endif							/* WIN32 */
