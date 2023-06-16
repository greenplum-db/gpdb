%define api.pure true
%define api.prefix {io_limit_yy}

%code top {
	#include "postgres.h"
	#include "access/heapam.h"
	#include "access/relscan.h"
	#include "access/table.h"
	#include "access/tableam.h"
	#include "commands/tablespace.h"
	#include "catalog/pg_tablespace.h"
	#include "catalog/pg_tablespace_d.h"
	#include "utils/relcache.h"

	#include <sys/stat.h>
	#include <sys/sysinfo.h>
	#include <sys/types.h>
	#include <sys/sysmacros.h>
	#include <mntent.h>
	#include <libgen.h>

	#define YYMALLOC palloc
	#define YYFREE	 pfree
	#define YYERROR_VERBOSE 1

	const int	IOconfigTotalFields = 4;
	/* the order must be same as struct IOconfig */
	const char	*IOconfigFields[4] = { "rbps", "wbps", "riops", "wiops" };
}

%code requires {
	#include "utils/cgroup_io_limit.h"

	typedef struct IOconfigItem
	{
		int offset;
		uint64 value;
	} IOconfigItem;

	typedef struct IO_LIMIT_PARSER_CONTEXT
	{
		List *result;
		int normal_tablespce_cnt;
		int star_tablespace_cnt;
	} IO_LIMIT_PARSER_CONTEXT;
}

%union {
	char *str;
	uint64 integer;
	IOconfig *ioconfig;
	TblSpcIOLimit *tblspciolimit;
	List *list;
	IOconfigItem *ioconfigitem;
}


%parse-param { IO_LIMIT_PARSER_CONTEXT *context }

%param { void *scanner }

%code {
	bdi_t get_bdi_of_path(const char *ori_path);
	void fill_bdi_list(TblSpcIOLimit *io_limit, List *result);
	int io_limit_yylex(void *lval, void *scanner);
	int io_limit_yyerror(IO_LIMIT_PARSER_CONTEXT *parser_context, void *scanner, const char *message);
}

%token <str> ID IOLIMIT_CONFIG_DELIM TABLESPACE_IO_CONFIG_START IOCONFIG_DELIM VALUE_MAX IO_KEY
%token <integer> VALUE

%type <str> tablespace_name
%type <integer> io_value
%type <ioconfig> ioconfigs
%type <tblspciolimit> tablespace_io_config
%type <list> iolimit_config_string start
%type <ioconfigitem> ioconfig

%destructor { pfree($$); } <str> <integer> <ioconfig> <ioconfigitem>
%destructor {
	pfree($$->ioconfig);
	list_free_deep($$->bdi_list);
	pfree($$);
} <tblspciolimit>

%%

start: iolimit_config_string
	   {
			context->result = $$ = $1;
			return 0;
	   }

iolimit_config_string: tablespace_io_config
					   {
							List *l = NIL;
							fill_bdi_list($1, NIL );

							$$ = lappend(l, $1);
					   }
					 | iolimit_config_string IOLIMIT_CONFIG_DELIM tablespace_io_config
					   {
							fill_bdi_list($3, $1);

							$$ = lappend($1, $3);
					   }

tablespace_name: ID  { $$ = $1; } | '*' { $$ = "*"; }

tablespace_io_config: tablespace_name TABLESPACE_IO_CONFIG_START ioconfigs
					  {
							TblSpcIOLimit *tblspciolimit = (TblSpcIOLimit *)palloc0(sizeof(TblSpcIOLimit));

							if (strcmp($1, "*") == 0)
							{
								if (context->normal_tablespce_cnt > 0)
									ereport(ERROR,
											(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
											errmsg("io limit: tablespace '*' cannot be used with other tablespaces")));
								tblspciolimit->tablespace_oid = InvalidOid;
								context->star_tablespace_cnt++;
							}
							else
							{
								if (context->star_tablespace_cnt > 0)
									ereport(ERROR,
											(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
											errmsg("io limit: tablespace '*' cannot be used with other tablespaces")));
								tblspciolimit->tablespace_oid = get_tablespace_oid($1, false);
								context->normal_tablespce_cnt++;
							}

							tblspciolimit->ioconfig = $3;

							$$ = tblspciolimit;
					  }

ioconfigs: ioconfig
		   {
				IOconfig *config = (IOconfig *)palloc0(sizeof(IOconfig));
				uint64 *config_var = (uint64 *)config;
				if (config == NULL)
					io_limit_yyerror(NIL, NULL, "io limit: cannot allocate memory");

				*(config_var + $1->offset) = $1->value;
				$$ = config;
		   }
		 | ioconfigs IOCONFIG_DELIM ioconfig
		   {
				uint64 *config_var = (uint64 *)$1;
				*(config_var + $3->offset) = $3->value;
				$$ = $1;
		   }

ioconfig: IO_KEY '=' io_value
		  {
			IOconfigItem *item = (IOconfigItem *)palloc0(sizeof(IOconfigItem));
			if (item == NULL)
				io_limit_yyerror(NIL, NULL, "io limit: cannot allocate memory");

			item->value = $3;
			for (int i = 0;i < IOconfigTotalFields; ++i)
				if (strcmp($1, IOconfigFields[i]) == 0)
					item->offset = i;

			$$ = item;
		  }

io_value: VALUE
		  {
			if ($1 < 2)
				io_limit_yyerror(NIL, NULL, "io limit: value cannot smaller than 2");

			$$ = $1;
		  }
		| VALUE_MAX { $$ = -1; }

%%

int io_limit_yyerror(IO_LIMIT_PARSER_CONTEXT *parser_context, void *scanner, const char *message)
{
	ereport(ERROR, \
		(errcode(ERRCODE_SYNTAX_ERROR), \
		errmsg("%s", message))); \
	return 0;
}

/*
 * Fill bdi_list in TblSpcIOLimit.
 * Fill bdi_list according to TblSpcIOLimit->tablespace_oid, and check bdi
 * collision with other TblSpcIOLimit.
 */
void fill_bdi_list(TblSpcIOLimit *iolimit, List *result)
{
	if (iolimit->tablespace_oid == InvalidOid)
	{
		Relation rel = table_open(TableSpaceRelationId, AccessShareLock);
		TableScanDesc scan = table_beginscan_catalog(rel, 0, NULL);
		HeapTuple tuple;
		/*
		 * scan all tablespace and get bdi
		 */
		iolimit->bdi_list = NULL;
		while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
		{
			char *ts_dir;
			char data_dir[1024];
			bdi_t *bdi;
			Form_pg_tablespace spaceform = (Form_pg_tablespace) GETSTRUCT(tuple);
			Oid dbid = MyDatabaseId;
			if (spaceform->oid == GLOBALTABLESPACE_OID)
				dbid = 0;

			ts_dir = GetDatabasePath(dbid, spaceform->oid);
			sprintf(data_dir, "%s/%s", DataDir, ts_dir);

			bdi = (bdi_t *)palloc0(sizeof(bdi_t));
			*bdi = get_bdi_of_path(data_dir);
			iolimit->bdi_list = lappend(iolimit->bdi_list, bdi);
		}
		table_endscan(scan);
		table_close(rel, AccessShareLock);
	}
	else
	{
		char *ts_dir;
		char data_dir[1024];
		Oid dbid = MyDatabaseId;
		bdi_t *bdi;
		if (iolimit->tablespace_oid == GLOBALTABLESPACE_OID)
			dbid = 0;

		ts_dir = GetDatabasePath(dbid, iolimit->tablespace_oid);
		sprintf(data_dir, "%s/%s", DataDir, ts_dir);

		bdi = (bdi_t *)palloc0(sizeof(bdi_t));
		*bdi = get_bdi_of_path(data_dir);

		ListCell      *limit_cell;
		ListCell      *bdi_cell;
		/* check duplicate disk */
		foreach (limit_cell, result)
		{
			TblSpcIOLimit *otherLimit = (TblSpcIOLimit *)lfirst(limit_cell);
			foreach (bdi_cell, otherLimit->bdi_list)
			{
				if (*bdi == *((bdi_t *)lfirst(bdi_cell)))
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("io limit: tablespaces of io limit must located at different disks")));
				}
			}
		}
		iolimit->bdi_list = lappend(iolimit->bdi_list, bdi);
	}
}

/*
 * Get BDI of a path.
 *
 * First find mountpoint from /proc/self/mounts, then find bdi of mountpoints.
 * Check this bdi is a disk or a partition(via check 'start' file in
 * /sys/deb/block/{bdi}), if it is a disk, just return it, if not, find the disk
 * and return it's bdi.
 */
bdi_t
get_bdi_of_path(const char *ori_path)
{
	struct mntent *mnt;

	char path[1024];
	realpath(ori_path, path);

	FILE *fp = setmntent("/proc/self/mounts", "r");

	size_t max_match_cnt = 0;
	struct mntent match_mnt;

	/* find mount point of path */
	while ((mnt = getmntent(fp)) != NULL)
	{
		size_t dir_len = strlen(mnt->mnt_dir);

		if (strstr(path, mnt->mnt_dir) != NULL && strncmp(path, mnt->mnt_dir, dir_len) == 0)
		{
			if (dir_len > max_match_cnt)
			{
				max_match_cnt = dir_len;
				match_mnt     = *mnt;
			}
		}
	}
	fclose(fp);

	struct stat sb;
	if (stat(match_mnt.mnt_fsname, &sb) == -1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("cannot find disk of %s\n", path)));
	}

	int maj = major(sb.st_rdev);
	int min = minor(sb.st_rdev);

	char sysfs_path[64];
	sprintf(sysfs_path, "/sys/dev/block/%d:%d", maj, min);

	char sysfs_path_start[64];
	sprintf(sysfs_path_start, "%s/start", sysfs_path);

	if (access(sysfs_path_start, F_OK) == -1)
		return make_bdi(maj, min);

	char real_path[1024];
	realpath(sysfs_path, real_path);
	dirname(real_path);

	sprintf(real_path, "%s/dev", real_path);

	FILE *f = fopen(real_path, "r");
	if (f == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("cannot find disk of %s\n", path)));
	}

	int parent_maj;
	int parent_min;
	fscanf(f, "%d:%d", &parent_maj, &parent_min);
	fclose(f);

	return make_bdi(parent_maj, parent_min);
}

List *io_limit_parse(const char *limit_str)
{
	List *result = NIL;
	IO_LIMIT_PARSER_CONTEXT *context = (IO_LIMIT_PARSER_CONTEXT *)palloc0(sizeof(IO_LIMIT_PARSER_CONTEXT));

	IO_LIMIT_PARSER_STATE *state = io_limit_begin_scan(limit_str);
	if (io_limit_yyparse(context, state->scanner) != 0)
		io_limit_yyerror(context, state->scanner, "io limit: parse error");

	io_limit_end_scan(state);

	result = context->result;
	pfree(context);

	return result;
}
