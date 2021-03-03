/*-------------------------------------------------------------------------
 *
 * guc_tables.h
 *		Declarations of tables used by GUC.
 *
 * See src/backend/utils/misc/README for design notes.
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *	  src/include/utils/guc_tables.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef GUC_TABLES_H
#define GUC_TABLES_H 1

#include "utils/guc.h"

/*
 * GUC supports these types of variables:
 */
enum config_type
{
	PGC_BOOL,
	PGC_INT,
	PGC_REAL,
	PGC_STRING,
	PGC_ENUM
};

union config_var_val
{
	bool		boolval;
	int			intval;
	double		realval;
	char	   *stringval;
	int			enumval;
};

/*
 * The actual value of a GUC variable can include a malloc'd opaque struct
 * "extra", which is created by its check_hook and used by its assign_hook.
 */
typedef struct config_var_value
{
	union config_var_val val;
	void	   *extra;
} config_var_value;

/*
 * Groupings to help organize all the run-time options for display
 *
 * Note: When you modify this, you need to modify config_group_names[]
 *       as well, which is located in guc.c.
 */
enum config_group
{
	UNGROUPED,
	FILE_LOCATIONS,
	CONN_AUTH,
	CONN_AUTH_SETTINGS,
	CONN_AUTH_AUTH,
	CONN_AUTH_SSL,

	EXTERNAL_TABLES,                    /*CDB*/
	APPENDONLY_TABLES,                  /*CDB*/
	RESOURCES,
	RESOURCES_MEM,
	RESOURCES_DISK,
	RESOURCES_KERNEL,
	RESOURCES_VACUUM_DELAY,
	RESOURCES_BGWRITER,
	RESOURCES_ASYNCHRONOUS,
	RESOURCES_MGM,
	WAL,
	WAL_SETTINGS,
	WAL_CHECKPOINTS,
	WAL_ARCHIVING,
	WAL_ARCHIVE_RECOVERY,
	WAL_RECOVERY_TARGET,
	REPLICATION,
	REPLICATION_SENDING,
	REPLICATION_MASTER,
	REPLICATION_STANDBY,
	REPLICATION_SUBSCRIBERS,
	QUERY_TUNING,
	QUERY_TUNING_METHOD,
	QUERY_TUNING_COST,
	QUERY_TUNING_OTHER,

	LOGGING,
	LOGGING_WHERE,
	LOGGING_WHEN,
	LOGGING_WHAT,
	PROCESS_TITLE,
	STATS,

    STATS_ANALYZE,                      /*CDB*/
	STATS_MONITORING,
	STATS_COLLECTOR,
	AUTOVACUUM,
	CLIENT_CONN,

	CLIENT_CONN_STATEMENT,
	CLIENT_CONN_LOCALE,
	CLIENT_CONN_PRELOAD,
	CLIENT_CONN_OTHER,
	LOCK_MANAGEMENT,
	COMPAT_OPTIONS,

	COMPAT_OPTIONS_PREVIOUS,
	COMPAT_OPTIONS_CLIENT,
    COMPAT_OPTIONS_IGNORED,             /*CDB*/
	ERROR_HANDLING_OPTIONS,
    GP_ARRAY_CONFIGURATION,            /*CDB*/
    GP_ARRAY_TUNING,                   /*CDB*/

    GP_WORKER_IDENTITY,                /*CDB*/
	GP_ERROR_HANDLING,				   /*CDB*/
	PRESET_OPTIONS,
	CUSTOM_OPTIONS,
	DEVELOPER_OPTIONS,

	/*
	 * GPDB: deprecated GUCs. In this group, the GUCs are still functioning,
	 * but we don't recommend customers to use them. They may be defunct in
	 * the future release.
	 */
	DEPRECATED_OPTIONS,

	/*
	 * GPDB: defunct GUCs. In this group, the GUCs are defunct. The GUCs are still
	 * there, but attempting to change their values will not have any effects.
	 */
	DEFUNCT_OPTIONS,






	___CONFIG_GROUP_COUNT /* sentinel to indicate end of enumeration */
};

/*
 * Stack entry for saving the state a variable had prior to an uncommitted
 * transactional change
 */
typedef enum
{
	/* This is almost GucAction, but we need a fourth state for SET+LOCAL */
	GUC_SAVE,					/* entry caused by function SET option */
	GUC_SET,					/* entry caused by plain SET command */
	GUC_LOCAL,					/* entry caused by SET LOCAL command */
	GUC_SET_LOCAL				/* entry caused by SET then SET LOCAL */
} GucStackState;

typedef struct guc_stack
{
	struct guc_stack *prev;		/* previous stack item, if any */
	int			nest_level;		/* nesting depth at which we made entry */
	GucStackState state;		/* see enum above */
	GucSource	source;			/* source of the prior value */
	/* masked value's source must be PGC_S_SESSION, so no need to store it */
	GucContext	scontext;		/* context that set the prior value */
	GucContext	masked_scontext;	/* context that set the masked value */
	config_var_value prior;		/* previous value of variable */
	config_var_value masked;	/* SET value in a GUC_SET_LOCAL entry */
} GucStack;

/*
 * Generic fields applicable to all types of variables
 *
 * The short description should be less than 80 chars in length. Some
 * applications may use the long description as well, and will append
 * it to the short description. (separated by a newline or '. ')
 *
 * Note that sourcefile/sourceline are kept here, and not pushed into stacked
 * values, although in principle they belong with some stacked value if the
 * active value is session- or transaction-local.  This is to avoid bloating
 * stack entries.  We know they are only relevant when source == PGC_S_FILE.
 */
struct config_generic
{
	/* constant fields, must be set correctly in initial value: */
	const char *name;			/* name of variable - MUST BE FIRST */
	GucContext	context;		/* context required to set the variable */
	enum config_group group;	/* to help organize variables by function */
	const char *short_desc;		/* short desc. of this variable's purpose */
	const char *long_desc;		/* long desc. of this variable's purpose */
	int			flags;			/* flag bits, see guc.h */
	/* variable fields, initialized at runtime: */
	enum config_type vartype;	/* type of variable (set only at startup) */
	int			status;			/* status bits, see below */
	GucSource	source;			/* source of the current actual value */
	GucSource	reset_source;	/* source of the reset_value */
	GucContext	scontext;		/* context that set the current value */
	GucContext	reset_scontext; /* context that set the reset value */
	GucStack   *stack;			/* stacked prior values */
	void	   *extra;			/* "extra" pointer for current actual value */
	char	   *sourcefile;		/* file current setting is from (NULL if not
								 * set in config file) */
	int			sourceline;		/* line in source file */
};

/* bit values in status field */
#define GUC_IS_IN_FILE		0x0001	/* found it in config file */
/*
 * Caution: the GUC_IS_IN_FILE bit is transient state for ProcessConfigFile.
 * Do not assume that its value represents useful information elsewhere.
 */
#define GUC_PENDING_RESTART 0x0002

/* upper limit for GUC variables measured in kilobytes of memory */
/* note that various places assume the byte size fits in a "long" variable */
#if SIZEOF_SIZE_T > 4 && SIZEOF_LONG > 4
#define MAX_KILOBYTES	INT_MAX
#else
#define MAX_KILOBYTES	(INT_MAX / 1024)
#endif

/* GUC records for specific variable types */

struct config_bool
{
	struct config_generic gen;
	/* constant fields, must be set correctly in initial value: */
	bool	   *variable;
	bool		boot_val;
	GucBoolCheckHook check_hook;
	GucBoolAssignHook assign_hook;
	GucShowHook show_hook;
	/* variable fields, initialized at runtime: */
	bool		reset_val;
	void	   *reset_extra;
};

struct config_int
{
	struct config_generic gen;
	/* constant fields, must be set correctly in initial value: */
	int		   *variable;
	int			boot_val;
	int			min;
	int			max;
	GucIntCheckHook check_hook;
	GucIntAssignHook assign_hook;
	GucShowHook show_hook;
	/* variable fields, initialized at runtime: */
	int			reset_val;
	void	   *reset_extra;
};

struct config_real
{
	struct config_generic gen;
	/* constant fields, must be set correctly in initial value: */
	double	   *variable;
	double		boot_val;
	double		min;
	double		max;
	GucRealCheckHook check_hook;
	GucRealAssignHook assign_hook;
	GucShowHook show_hook;
	/* variable fields, initialized at runtime: */
	double		reset_val;
	void	   *reset_extra;
};

struct config_string
{
	struct config_generic gen;
	/* constant fields, must be set correctly in initial value: */
	char	  **variable;
	const char *boot_val;
	GucStringCheckHook check_hook;
	GucStringAssignHook assign_hook;
	GucShowHook show_hook;
	/* variable fields, initialized at runtime: */
	char	   *reset_val;
	void	   *reset_extra;
};

struct config_enum
{
	struct config_generic gen;
	/* constant fields, must be set correctly in initial value: */
	int		   *variable;
	int			boot_val;
	const struct config_enum_entry *options;
	GucEnumCheckHook check_hook;
	GucEnumAssignHook assign_hook;
	GucShowHook show_hook;
	/* variable fields, initialized at runtime: */
	int			reset_val;
	void	   *reset_extra;
};

/* constant tables corresponding to enums above and in guc.h */
extern const char *const config_group_names[];
extern const char *const config_type_names[];
extern const char *const GucContext_Names[];
extern const char *const GucSource_Names[];

/* get the current set of variables */
extern struct config_generic **get_guc_variables(void);
extern int get_num_guc_variables(void);

extern void build_guc_variables(void);

/* search in enum options */
extern const char *config_enum_lookup_by_value(struct config_enum *record, int val);
extern bool config_enum_lookup_by_name(struct config_enum *record,
									   const char *value, int *retval);
extern struct config_generic **get_explain_guc_options(int *num);

extern bool parse_int(const char *value, int *result, int flags, const char **hintmsg);

/* guc_gp.c needs this from guc.c */
extern const struct config_enum_entry server_message_level_options[];

/* guc_gp.c exports these for guc.c */
extern struct config_bool ConfigureNamesBool_gp[];
extern struct config_int ConfigureNamesInt_gp[];
extern struct config_real ConfigureNamesReal_gp[];
extern struct config_string ConfigureNamesString_gp[];
extern struct config_enum ConfigureNamesEnum_gp[];

extern void gpdb_assign_sync_flag(struct config_generic **guc_variables, int size, bool predefine);

#endif							/* GUC_TABLES_H */
