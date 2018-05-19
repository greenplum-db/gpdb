/*
 *	pg_upgrade.h
 *
 *	Portions Copyright (c) 2016, Pivotal Software Inc
 *	Copyright (c) 2010-2011, PostgreSQL Global Development Group
 *	contrib/pg_upgrade/pg_upgrade.h
 */

#include "postgres.h"

#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "libpq-fe.h"

/* Allocate for null byte */
#define USER_NAME_SIZE		128

#define MAX_STRING			1024
#define LINE_ALLOC			4096
#define QUERY_ALLOC			8192

#define NUMERIC_ALLOC 100

#define MIGRATOR_API_VERSION	1

#define MESSAGE_WIDTH		"60"

#define OVERWRITE_MESSAGE	"  %-" MESSAGE_WIDTH "." MESSAGE_WIDTH "s\r"
#define GET_MAJOR_VERSION(v)	((v) / 100)

#define ALL_DUMP_FILE		"pg_upgrade_dump_all.sql"
/* contains both global db information and CREATE DATABASE commands */
#define GLOBALS_DUMP_FILE	"pg_upgrade_dump_globals.sql"
#define DB_DUMP_FILE		"pg_upgrade_dump_db.sql"

#define GLOBAL_OIDS_DUMP_FILE "pg_upgrade_dump_global_oids.sql"
#define DB_OIDS_DUMP_FILE_MASK	"pg_upgrade_dump_%u_oids.sql"


/* needs to be kept in sync with pg_class.h */
#define RELSTORAGE_EXTERNAL	'x'

#ifndef WIN32
#define pg_copy_file		copy_file
#define pg_mv_file			rename
#define pg_link_file		link
#define PATH_SEPARATOR      '/'
#define RM_CMD				"rm -f"
#define RMDIR_CMD			"rm -rf"
#define SCRIPT_EXT			"sh"
#else
#define pg_copy_file		CopyFile
#define pg_mv_file			pgrename
#define pg_link_file		win32_pghardlink
#define sleep(x)			Sleep(x * 1000)
#define PATH_SEPARATOR      '\\'
#define RM_CMD				"DEL /q"
#define RMDIR_CMD			"RMDIR /s/q"
#define SCRIPT_EXT			"bat"
#define EXE_EXT				".exe"
#endif

#if defined(WIN32) && !defined(__CYGWIN__)

		/*
		 * XXX This does not work for all terminal environments or for output
		 * containing non-ASCII characters; see comments in simple_prompt().
		 */
#define DEVTTY	"con"
#else
#define DEVTTY	"/dev/tty"
#endif

#define CLUSTER_NAME(cluster)	((cluster) == &old_cluster ? "old" : \
								 (cluster) == &new_cluster ? "new" : "none")

#define atooid(x)  ((Oid) strtoul((x), NULL, 10))

/* OID system catalog preservation added during PG 9.0 development */
#define TABLE_SPACE_SUBDIRS_CAT_VER 201001111
/* postmaster/postgres -b (binary_upgrade) flag added during PG 9.1 development */
/* In GPDB, it was introduced during GPDB 5.0 development. */
#define BINARY_UPGRADE_SERVER_FLAG_CAT_VER 301607301

/*
 * Extra information stored for each Append-only table.
 * This is used to transfer the information from the auxiliary
 * AO table to the new cluster.
 */

/* To hold contents of pg_visimap_<oid> */
typedef struct
{
	int			segno;
	int64		first_row_no;
	char	   *visimap;		/* text representation of the "bit varying" field */
} AOVisiMapInfo;

typedef struct
{
	int			segno;
	int			columngroup_no;
	int64		first_row_no;
	char	   *minipage;		/* text representation of the "bit varying" field */
} AOBlkDir;

/* To hold contents of pg_aoseg_<oid> */
typedef struct
{
	int			segno;
	int64		eof;
	int64		tupcount;
	int64		varblockcount;
	int64		eofuncompressed;
	int64		modcount;
	int16		version;
	int16		state;
} AOSegInfo;

/* To hold contents of pf_aocsseg_<oid> */
typedef struct
{
	int         segno;
	int64		tupcount;
	int64		varblockcount;
	char       *vpinfo;
	int64		modcount;
	int16		state;
	int16		version;
} AOCSSegInfo;

typedef struct
{
	int16		attlen;
	char		attalign;
	bool		is_numeric;
} AttInfo;

/*
 * Each relation is represented by a relinfo structure.
 */
typedef struct
{
	char		nspname[NAMEDATALEN];	/* namespace name */
	char		relname[NAMEDATALEN];	/* relation name */
	Oid			reloid;			/* relation oid */
	char		relstorage;
	Oid			relfilenode;	/* relation relfile node */
	/* relation tablespace path, or "" for the cluster default */
	char		tablespace[MAXPGPATH];

	/* Extra information for append-only tables */
	AOSegInfo  *aosegments;
	AOCSSegInfo *aocssegments;
	int			naosegments;
	AOVisiMapInfo *aovisimaps;
	int			naovisimaps;
	AOBlkDir   *aoblkdirs;
	int			naoblkdirs;

	/* Extra information for heap tables */
	bool		gpdb4_heap_conversion_needed;
	bool		has_numerics;
	AttInfo	   *atts;
	int			natts;
} RelInfo;

typedef struct
{
	RelInfo    *rels;
	int			nrels;
} RelInfoArr;

typedef enum
{
	HEAP,
	AO,
	AOCS,
	FSM
} RelType;

/*
 * The following structure represents a relation mapping.
 */
typedef struct
{
	char		old_dir[MAXPGPATH];
	char		new_dir[MAXPGPATH];

	/*
	 * old/new relfilenodes might differ for pg_largeobject(_metadata) indexes
	 * due to VACUUM FULL or REINDEX.  Other relfilenodes are preserved.
	 */
	Oid			old_relfilenode;
	Oid			new_relfilenode;
	/* the rest are used only for logging and error reporting */
	char		nspname[NAMEDATALEN];	/* namespaces */
	char		relname[NAMEDATALEN];

	/* GPDB */
	bool		missing_seg0_ok;

	RelType		type;			/* Type of relation */

	/* Extra information for heap tables */
	bool		gpdb4_heap_conversion_needed;
	bool		has_numerics;
	AttInfo	   *atts;
	int			natts;
} FileNameMap;

/*
 * Structure to store database information
 */
typedef struct
{
	Oid			db_oid;			/* oid of the database */
	char		db_name[NAMEDATALEN];	/* database name */
	char		db_tblspace[MAXPGPATH]; /* database default tablespace path */
	RelInfoArr	rel_arr;		/* array of all user relinfos */

	char	   *reserved_oids;	/* as a string */
} DbInfo;

typedef struct
{
	DbInfo	   *dbs;			/* array of db infos */
	int			ndbs;			/* number of db infos */
} DbInfoArr;

/*
 * The following structure is used to hold pg_control information.
 * Rather than using the backend's control structure we use our own
 * structure to avoid pg_control version issues between releases.
 */
typedef struct
{
	uint32		ctrl_ver;
	uint32		cat_ver;
	uint32		logid;
	uint32		nxtlogseg;
	uint32		chkpnt_tli;
	uint32		chkpnt_nxtxid;
	uint32		chkpnt_nxtoid;
	uint32		align;
	uint32		blocksz;
	uint32		largesz;
	uint32		walsz;
	uint32		walseg;
	uint32		ident;
	uint32		index;
	uint32		toast;
	bool		date_is_int;
	bool		float8_pass_by_value;
	bool		data_checksum_version;
	char	   *lc_collate;
	char	   *lc_ctype;
	char	   *encoding;
} ControlData;

/*
 * Enumeration to denote link modes
 */
typedef enum
{
	TRANSFER_MODE_COPY,
	TRANSFER_MODE_LINK
} transferMode;

/*
 * Enumeration to denote checksum modes
 */
typedef enum
{
	CHECKSUM_NONE = 0,
	CHECKSUM_ADD,
	CHECKSUM_REMOVE
} checksumMode;

/*
 * Enumeration to denote pg_log modes
 */
typedef enum
{
	PG_INFO,
	PG_REPORT,
	PG_WARNING,
	PG_FATAL,
	PG_DEBUG
} eLogType;


typedef long pgpid_t;

/*
 * Enumeration for operations in the progress report
 */
typedef enum
{
	CHECK,
	SCHEMA_DUMP,
	SCHEMA_RESTORE,
	FILE_MAP,
	FILE_COPY,
	FIXUP,
	ABORT,
	DONE
} progress_type;

/*
 * cluster
 *
 *	information about each cluster
 */
typedef struct
{
	ControlData controldata;	/* pg_control information */
	DbInfoArr	dbarr;			/* dbinfos array */
	char	   *pgdata;			/* pathname for cluster's $PGDATA directory */
	char	   *bindir;			/* pathname for cluster's executable directory */
	unsigned short port;		/* port number where postmaster is waiting */
	uint32		major_version;	/* PG_VERSION of cluster */
	char		major_version_str[64];	/* string PG_VERSION of cluster */
	Oid			pg_database_oid;	/* OID of pg_database relation */
	char	   *libpath;		/* pathname for cluster's pkglibdir */
	char	   *tablespace_suffix;		/* directory specification */

	char	   *global_reserved_oids; /* OID preassign calls for shared objects */
} ClusterInfo;


/*
 *	LogOpts
*/
typedef struct
{
	char	   *filename;		/* name of log file (may be /dev/null) */
	FILE	   *fd;				/* log FILE */
	bool		debug;			/* TRUE -> log more information */
	FILE	   *debug_fd;		/* debug-level log FILE */
	bool		verbose;		/* TRUE -> be verbose in messages */

	/* GPDB */
	bool		progress;		/* TRUE -> file based progress queue */
} LogOpts;


/*
 *	UserOpts
*/
typedef struct
{
	bool		check;			/* TRUE -> ask user for permission to make
								 * changes */
	transferMode transfer_mode; /* copy files or link them? */

	/* GPDB */
	bool		dispatcher_mode; /* TRUE -> upgrading QD node */
	checksumMode checksum_mode; /* TRUE -> calculate and add checksums to
								 * data pages */
} UserOpts;


/*
 * OSInfo
 */
typedef struct
{
	const char *progname;		/* complete pathname for this program */
	char	   *exec_path;		/* full path to my executable */
	char	   *user;			/* username for clusters */
	char		cwd[MAXPGPATH]; /* current working directory, used for output */
	char	  **tablespaces;	/* tablespaces */
	int			num_tablespaces;
	char	  **libraries;		/* loadable libraries */
	int			num_libraries;
	ClusterInfo *running_cluster;
} OSInfo;


/*
 * Global variables
 */
extern LogOpts log_opts;
extern UserOpts user_opts;
extern ClusterInfo old_cluster,
			new_cluster;
extern OSInfo os_info;
extern char scandir_file_pattern[];


/* check.c */

void		output_check_banner(bool *live_check);
void check_old_cluster(bool live_check,
				  char **sequence_script_file_name);
void		check_new_cluster(void);
void		report_clusters_compatible(void);
void		issue_warnings(char *sequence_script_file_name);
void		output_completion_banner(char *deletion_script_file_name);
void		check_cluster_versions(void);
void		check_cluster_compatibility(bool live_check);
void		create_script_for_old_cluster_deletion(char **deletion_script_file_name);


/* controldata.c */

void		get_control_data(ClusterInfo *cluster, bool live_check);
void check_control_data(ControlData *oldctrl,
				   ControlData *newctrl);


/* dump.c */

void		generate_old_dump(void);
void		split_old_dump(void);


/* exec.c */

int exec_prog(bool throw_error,
		  const char *cmd,...);
void		verify_directories(void);
bool		is_server_running(const char *datadir);
void		rename_old_pg_control(void);


/* file.c */

#ifdef PAGE_CONVERSION
typedef const char *(*pluginStartup) (uint16 migratorVersion,
								uint16 *pluginVersion, uint16 newPageVersion,
								   uint16 oldPageVersion, void **pluginData);
typedef const char *(*pluginConvertFile) (void *pluginData,
								   const char *dstName, const char *srcName);
typedef const char *(*pluginConvertPage) (void *pluginData,
								   const char *dstPage, const char *srcPage);
typedef const char *(*pluginShutdown) (void *pluginData);

typedef struct
{
	uint16		oldPageVersion; /* Page layout version of the old cluster		*/
	uint16		newPageVersion; /* Page layout version of the new cluster		*/
	uint16		pluginVersion;	/* API version of converter plugin */
	void	   *pluginData;		/* Plugin data (set by plugin) */
	pluginStartup startup;		/* Pointer to plugin's startup function */
	pluginConvertFile convertFile;		/* Pointer to plugin's file converter
										 * function */
	pluginConvertPage convertPage;		/* Pointer to plugin's page converter
										 * function */
	pluginShutdown shutdown;	/* Pointer to plugin's shutdown function */
} pageCnvCtx;

const char *setupPageConverter(pageCnvCtx **result);
#else
/* dummy */
typedef void *pageCnvCtx;
#endif

int			dir_matching_filenames(const struct dirent * scan_ent);
int pg_scandir(const char *dirname, struct dirent *** namelist,
		   int (*selector) (const struct dirent *));
const char *copyAndUpdateFile(pageCnvCtx *pageConverter, const char *src,
				  const char *dst, bool force);
const char *linkAndUpdateFile(pageCnvCtx *pageConverter, const char *src,
				  const char *dst);

void		check_hard_link(void);
void rewriteHeapPageChecksum(const char *fromfile, const char *tofile,
							 const char *schemaName, const char *relName);

/* function.c */

void		install_support_functions_in_new_db(const char *db_name);
void		uninstall_support_functions_from_new_cluster(void);
void		get_loadable_libraries(void);
void		check_loadable_libraries(void);

/* info.c */

FileNameMap *gen_db_file_maps(DbInfo *old_db,
				 DbInfo *new_db, int *nmaps, const char *old_pgdata,
				 const char *new_pgdata);
void		get_db_and_rel_infos(ClusterInfo *cluster);
void		free_db_and_rel_infos(DbInfoArr *db_arr);
void print_maps(FileNameMap *maps, int n,
		   const char *db_name);

/* option.c */

void		parseCommandLine(int argc, char *argv[]);

/* relfilenode.c */

void		get_pg_database_relfilenode(ClusterInfo *cluster);
const char *transfer_all_new_dbs(DbInfoArr *olddb_arr,
				   DbInfoArr *newdb_arr, char *old_pgdata, char *new_pgdata);

/* aotable.c */
void		restore_aosegment_tables(void);

/* gpdb4_heap_convert.c */
const char *convert_gpdb4_heap_file(const char *src, const char *dst,
									bool has_numerics, AttInfo *atts, int natts);

/* tablespace.c */

void		init_tablespaces(void);


/* server.c */

PGconn	   *connectToServer(ClusterInfo *cluster, const char *db_name);
PGresult   *executeQueryOrDie(PGconn *conn, const char *fmt,...);

void		start_postmaster(ClusterInfo *cluster);
void		stop_postmaster(bool fast);
uint32		get_major_server_version(ClusterInfo *cluster);
void		check_pghost_envvar(void);


/* util.c */

char	   *quote_identifier(const char *s);
int			get_user_info(char **user_name);
void		check_ok(void);
void		report_status(eLogType type, const char *fmt,...);
void		pg_log(eLogType type, char *fmt,...);
void		prep_status(const char *fmt,...);
void		check_ok(void);
char	   *pg_strdup(const char *s);
void	   *pg_malloc(int size);
void		pg_free(void *ptr);
const char *getErrorText(int errNum);
unsigned int str2uint(const char *str);
void		pg_putenv(const char *var, const char *val);
void 		report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...);
void		close_progress(void);


/* version.c */

void new_9_0_populate_pg_largeobject_metadata(ClusterInfo *cluster,
										 bool check_mode);
void new_gpdb5_0_invalidate_indexes(bool check_mode);
void new_gpdb_invalidate_bitmap_indexes(bool check_mode);

/* version_old_8_3.c */

void		old_8_3_check_for_name_data_type_usage(ClusterInfo *cluster);
void		old_8_3_check_for_tsquery_usage(ClusterInfo *cluster);
void		old_8_3_check_ltree_usage(ClusterInfo *cluster);
void		old_8_3_rebuild_tsvector_tables(ClusterInfo *cluster, bool check_mode);
void		old_8_3_invalidate_hash_gin_indexes(ClusterInfo *cluster, bool check_mode);
void old_8_3_invalidate_bpchar_pattern_ops_indexes(ClusterInfo *cluster,
											  bool check_mode);
char	   *old_8_3_create_sequence_script(ClusterInfo *cluster);

/* version_old_gpdb4.c */
void old_GPDB4_check_for_money_data_type_usage(ClusterInfo *cluster);
void old_GPDB4_check_no_free_aoseg(ClusterInfo *cluster);

/* oid_dump.c */
void dump_new_oids(void);
void get_old_oids(void);
void slurp_oid_files(void);

/*
 * Hack to make backend macros that check for assertions to work.
 */
#ifdef AssertMacro
#undef AssertMacro
#endif
#define AssertMacro(condition) ((void) true)
#ifdef Assert
#undef Assert
#endif
#define Assert(condition) ((void) (true || (condition)))
