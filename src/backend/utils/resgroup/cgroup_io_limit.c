#include "postgres.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/indexing.h"
#include "catalog/pg_resgroup_d.h"
#include "catalog/pg_resgroupcapability_d.h"
#include "common/string.h"
#include "commands/tablespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_tablespace_d.h"
#include "commands/resgroupcmds.h"
#include "commands/tablespace.h"
#include "common/string.h"
#include "fmgr.h"
#include "port.h"
#include "storage/fd.h"
#include "utils/cgroup.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/palloc.h"
#include "utils/relcache.h"
#include "utils/resgroup.h"
#include "utils/cgroup_io_limit.h"
#include <utils/cgroup.h>

#ifndef __linux__
#error  cgroup is only available on linux
#endif

#include <libgen.h>
#include <limits.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

const char *IOconfigFields[4] = {"rbps", "wbps", "riops", "wiops"};
const char	*IOStatFields[4] = {"rbytes", "wbytes", "rios", "wios"};

static int bdi_cmp(const void *a, const void *b);

typedef struct BDICmp
{
	Oid ts;
	bdi_t bdi;
} BDICmp;

/*
 * Why not use 'return a - b' directly?
 * Because bdi_t is unsigned long now, larger than int. And the
 * implementation of bdi_t maybe changes in the future.
 */
static int
bdi_cmp(const void *a, const void *b)
{
	BDICmp x = *(BDICmp *)a;
	BDICmp y = *(BDICmp *)b;

	if (x.bdi < y.bdi)
		return -1;
	if (x.bdi > y.bdi)
		return 1;

	return 0;
}

/*
 * Validate io limit after parse.
 * The main work of validate process:
 *  1. fill bdi_list of TblSpcIOLimit.
 *  2. check duplicate bdi.
 */
void
io_limit_validate(List *limit_list)
{
	ListCell *limit_cell;
	int bdi_count = 0;
	int i = 0;
	BDICmp *bdi_array;
	bool is_star = false;

	foreach(limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);
		bdi_count += fill_bdi_list(limit);

		if (limit->tablespace_oid == InvalidOid)
			is_star = true;
	}

	bdi_array = (BDICmp *) palloc(bdi_count * sizeof(BDICmp));
	/* fill bdi list and check wbps/rbps range */
	foreach(limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);
		ListCell	  *bdi_cell;

		foreach (bdi_cell, limit->bdi_list)
		{
			bdi_array[i].bdi = *(bdi_t *)lfirst(bdi_cell);
			bdi_array[i].ts = limit->tablespace_oid;
			i++;
		}
	}

	Assert(i == bdi_count);

	/* check duplicate bdi */
	if (is_star)
		return;

	qsort(bdi_array, bdi_count, sizeof(BDICmp), bdi_cmp);
	for (i = 0; i < bdi_count - 1; ++i)
	{
		if (bdi_array[i].bdi == bdi_array[i + 1].bdi)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("io limit: tablespaces of io limit must locate at different disks, tablespace '%s' and '%s' have the same disk identifier.",
							get_tablespace_name(bdi_array[i].ts),
							get_tablespace_name(bdi_array[i + 1].ts))),
					errhint("either omit these tablespaces from the IO limit or mount them separately"));
		}
	}

	pfree(bdi_array);
}

/*
 * Fill bdi_list in TblSpcIOLimit.
 * Fill bdi_list according to TblSpcIOLimit->tablespace_oid.
 *
 * Return bdi count of tablespace.
 */
int
fill_bdi_list(TblSpcIOLimit *iolimit)
{
	int result_cnt = 0;

	/* caller should init the bdi_list */
	Assert(iolimit->bdi_list == NULL);

	if (iolimit->tablespace_oid == InvalidOid)
	{
		Relation rel = table_open(TableSpaceRelationId, AccessShareLock);
		TableScanDesc scan = table_beginscan_catalog(rel, 0, NULL);
		HeapTuple tuple;
		/*
		 * scan all tablespaces and get bdi
		 */
		while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
		{
			bdi_t *bdi;
			Form_pg_tablespace spaceform = (Form_pg_tablespace) GETSTRUCT(tuple);

			bdi = (bdi_t *)palloc0(sizeof(bdi_t));
			*bdi = get_bdi_of_path(get_tablespace_path(spaceform->oid));
			iolimit->bdi_list = lappend(iolimit->bdi_list, bdi);
			result_cnt++;
		}
		table_endscan(scan);
		table_close(rel, AccessShareLock);
	}
	else
	{
		bdi_t *bdi;

		bdi = (bdi_t *)palloc0(sizeof(bdi_t));
		*bdi = get_bdi_of_path(get_tablespace_path(iolimit->tablespace_oid));

		iolimit->bdi_list = lappend(iolimit->bdi_list, bdi);
		result_cnt++;
	}

	return result_cnt;
}

/*
 * Get BDI of a path.
 *
 * First find mountpoint from /proc/self/mounts, then find bdi of mountpoints.
 * Check this bdi is a disk or a partition(via check 'start' file in
 * /sys/dev/block/{bdi}), if it is a disk, just return it, if not, find the disk
 * and return it's bdi.
 */
bdi_t
get_bdi_of_path(const char *ori_path)
{
	int maj;
	int min;
	size_t max_match_len = 0;
	struct mntent *mnt;
	struct mntent result;
	struct mntent match_mnt = {};
	/* default size of glibc */
	char mntent_buffer[PATH_MAX];
	char sysfs_path[PATH_MAX];
	char sysfs_path_start[PATH_MAX];
	char real_path[PATH_MAX];
	char path[PATH_MAX];

	char *res = realpath(ori_path, path);
	if (res == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("io limit: cannot find realpath of %s, details: %m.", ori_path)));

	FILE *fp = setmntent("/proc/self/mounts", "r");

	/* find mount point of path */
	while ((mnt = getmntent_r(fp, &result, mntent_buffer, sizeof(mntent_buffer))) != NULL)
	{
		size_t dir_len = strlen(mnt->mnt_dir);

		if (strstr(path, mnt->mnt_dir) != NULL && strncmp(path, mnt->mnt_dir, dir_len) == 0)
		{
			if (dir_len > max_match_len)
			{
				max_match_len = dir_len;
				match_mnt.mnt_passno = mnt->mnt_passno;
				match_mnt.mnt_freq = mnt->mnt_freq;

				/* copy string */
				if (match_mnt.mnt_fsname != NULL)
					pfree(match_mnt.mnt_fsname);
				match_mnt.mnt_fsname = pstrdup(mnt->mnt_fsname);

				if (match_mnt.mnt_dir != NULL)
					pfree(match_mnt.mnt_dir);
				match_mnt.mnt_dir = pstrdup(mnt->mnt_dir);

				if (match_mnt.mnt_type != NULL)
					pfree(match_mnt.mnt_type);
				match_mnt.mnt_type = pstrdup(mnt->mnt_type);

				if (match_mnt.mnt_opts != NULL)
					pfree(match_mnt.mnt_opts);
				match_mnt.mnt_opts = pstrdup(mnt->mnt_opts);
			}
		}
	}
	endmntent(fp);

	struct stat sb;
	if (stat(match_mnt.mnt_fsname, &sb) == -1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("cannot find disk of %s, details: %m", path),
				 errhint("mount point of %s is: %s", path, match_mnt.mnt_fsname)));
	}
	pfree(match_mnt.mnt_fsname);
	pfree(match_mnt.mnt_dir);
	pfree(match_mnt.mnt_type);
	pfree(match_mnt.mnt_opts);

	maj = major(sb.st_rdev);
	min = minor(sb.st_rdev);

	snprintf(sysfs_path, sizeof(sysfs_path), "/sys/dev/block/%d:%d", maj, min);

	snprintf(sysfs_path_start, sizeof(sysfs_path_start), "%s/start", sysfs_path);

	if (access(sysfs_path_start, F_OK) == -1)
		return make_bdi(maj, min);

	res = realpath(sysfs_path, real_path);
	if (res == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("io limit: cannot find realpath of %s, details: %m.", sysfs_path)));
	}

	dirname(real_path);

	snprintf(real_path + strlen(real_path), sizeof(real_path) - strlen(real_path), "/dev");

	FILE *f = fopen(real_path, "r");
	if (f == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("cannot find disk of %s\n", path)));
	}

	int parent_maj;
	int parent_min;
	int scan_result = fscanf(f, "%d:%d", &parent_maj, &parent_min);
	if (scan_result < 2)
	{
		fclose(f);
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("io limit: cannot read block device id from %s, details: %m.", real_path)));
	}
	fclose(f);

	return make_bdi(parent_maj, parent_min);
}


bool
io_limit_value_validate(const char *field, const uint64 value, uint64 *max)
{
	const uint64 ULMAX = ULLONG_MAX / 1024 / 1024;
	const uint32 UMAX = UINT_MAX;

	*max = 0;

	if (strcmp(field, "rbps") == 0 || strcmp(field, "wbps") == 0)
	{
		*max = ULMAX;
		return value == IO_LIMIT_MAX || !(value > ULMAX || value < 2);
	}
	else if (strcmp(field, "riops") == 0 || strcmp(field, "wiops") == 0)
	{
		*max = (uint64) UMAX;
		return value == IO_LIMIT_MAX || !(value > UMAX || value < 2);
	}

	return false;
}

char *
get_tablespace_path(Oid spcid)
{
	if (spcid == InvalidOid)
		return NULL;

	if (spcid == DEFAULTTABLESPACE_OID ||
		spcid == GLOBALTABLESPACE_OID)
	{
		Oid dbid = MyDatabaseId;

		if (spcid == GLOBALTABLESPACE_OID)
			dbid = 0;

		return GetDatabasePath(dbid, spcid);
	}

	return psprintf("pg_tblspc/%u", spcid);
}

void
io_limit_free(List *limit_list)
{
	ListCell *cell;

	foreach (cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(cell);
		list_free_deep(limit->bdi_list);
		pfree(limit->ioconfig);
	}

	list_free_deep(limit_list);
}

/*
 * Get content from io.stat of cgroup.
 *
 * Use group oid to find the cgroup path, and then use
 * parsed io limit objects to collect data for tablespaces.
 *
 * params:
 *	groupid: resource group oid
 *	io_limit: parsed io_limit str objects
 *	result: array of IOStat to save statistics
 *
 * Return the count of result IOStat
 *
 */
List *
get_iostat(Oid groupid, List *io_limit)
{
	List *result = NIL;

	HTAB *io_stat_hash = NULL;
	HASHCTL ctl;

	char io_stat_path[PATH_MAX];
	ListCell *cell;
	StringInfo line = makeStringInfo();
	FILE *f;

	buildPath(groupid, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN, "io.stat",
			  io_stat_path, sizeof(io_stat_path));
	f = AllocateFile(io_stat_path, "r");
	if (f == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("io limit: cannot read %s, details: %m.", io_stat_path)));

	/*
	 * parse file content.
	 * content example:
	 * "8:16 rbytes=1459200 wbytes=314773504 rios=192 wios=353 ..."
	 */
	memset(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(bdi_t);
	ctl.entrysize = sizeof(IOStatHashEntry);
	ctl.hcxt = CurrentMemoryContext;
	io_stat_hash =
		hash_create("hash table for bdi -> io stat", list_length(io_limit), &ctl,
					HASH_ELEM | HASH_CONTEXT);

	while (pg_get_line_append(f, line))
	{
		uint32 maj, min;
		bdi_t bdi;
		IOStatHashEntry *entry;
		char *t;

		char *str = (char *) line->data;
		int res = sscanf(str, "%u:%u", &maj, &min);
		if (res != 2)
		{
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("io limit: cannot parse content from '%s', details: %m.",
							str)));
		}

		bdi = make_bdi(maj, min);
		entry = hash_search(io_stat_hash, (void *)&bdi, HASH_ENTER, NULL);
		memset(entry, 0, sizeof(IOStatHashEntry));
		entry->id = bdi;

		t = str;
		while (true)
		{
			char key[64];
			int i;

			t = strstr(t, " ");
			if (t == NULL)
				break;
			t++;

			for (i = 0; i < lengthof(IOStatFields); i++)
			{
				if (strncmp(IOStatFields[i], t, strlen(IOStatFields[i])) == 0)
				{
					uint64 *value = (uint64 *) &entry->items;
					sprintf(key, "%s=%%lu", IOStatFields[i]);
					sscanf(t, key, value + i);
					break;
				}
			}
		}

		initStringInfo(line);
	}
	FreeFile(f);

	foreach (cell, io_limit)
	{
		ListCell *bdi_cell;
		IOStat *stat = (IOStat *) palloc0(sizeof(IOStat));

		TblSpcIOLimit *limit = (TblSpcIOLimit *) lfirst(cell);
		fill_bdi_list(limit);
		stat->tablespace = limit->tablespace_oid;
		stat->groupid = groupid;

		foreach (bdi_cell, limit->bdi_list)
		{
			bdi_t *bdi = (bdi_t *)lfirst(bdi_cell);
			IOStatHashEntry *entry =
				hash_search(io_stat_hash, (void *)bdi, HASH_FIND, NULL);

			if (entry != NULL)
			{
				stat->items.wbytes += entry->items.wbytes;
				stat->items.rbytes += entry->items.rbytes;
				stat->items.rios += entry->items.rios;
				stat->items.wios += entry->items.wios;
			}
		}

		result = lappend(result, stat);
	}

	hash_destroy(io_stat_hash);
	io_limit_free(io_limit);

	return result;
}

/*
 * sort a list of IOStat
 */
int
compare_iostat(const void *x, const void *y)
{
	IOStat *a = (IOStat *) lfirst(*(ListCell **) x);
	IOStat *b = (IOStat *) lfirst(*(ListCell **) y);
	if (a->groupid != b->groupid)
	{
		if (a->groupid < b->groupid)
			return -1;
		return 1;
	}

	if (a->tablespace != b->tablespace)
	{
		if (a->tablespace < b->tablespace)
			return -1;
		return 1;
	}

	return 0;
}

char *
io_limit_dump(List *limit_list)
{
	ListCell *cell;

	StringInfo result = makeStringInfo();

	foreach(cell, limit_list)
	{
		int i;
		int fields_length = lengthof(IOconfigFields);
		TblSpcIOLimit *limit = (TblSpcIOLimit *) lfirst(cell);
		uint64 *value = (uint64 *) limit->ioconfig;

		if (limit->tablespace_oid != InvalidOid)
			appendStringInfo(result, "%u:", limit->tablespace_oid);
		else
			appendStringInfo(result, "*:");

		for(i = 0; i < fields_length; i++)
		{
			if ((*(value + i) != IO_LIMIT_MAX) && (*(value + i) != IO_LIMIT_EMPTY))
				appendStringInfo(result, "%s=%lu", IOconfigFields[i], *(value + i));
			else
				appendStringInfo(result, "%s=max", IOconfigFields[i]);

			if (i + 1 != fields_length)
				appendStringInfo(result, ",");
		}

		if (cell != limit_list->tail)
			appendStringInfo(result, ";");
	}

	return result->data;
}

void
clear_io_max(Oid groupid)
{
	FILE *f;
	StringInfo line			 = makeStringInfo();
	StringInfo result		 = makeStringInfo();
	List	   *result_lines = NIL;
	ListCell   *cell;
	char	   path[MAX_CGROUP_PATHLEN];
	buildPath(groupid, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN, "io.max", path, MAX_CGROUP_PATHLEN);

	f = AllocateFile(path, "r");
	/* pg_get_line_buf will reset line each time */
	while (pg_get_line_buf(f, line))
	{
		uint32 maj, min;
		int i;
		char *str = line->data;

		sscanf(str, "%u:%u", &maj, &min);

		appendStringInfo(result, "%u:%u", maj, min);
		for (i = 0; i < lengthof(IOconfigFields); i++)
			appendStringInfo(result, " %s=max", IOconfigFields[i]);

		result_lines = lappend(result_lines, (void *) pstrdup(result->data));

		resetStringInfo(result);
	}
	FreeFile(f);
	pfree(line->data);
	pfree(result->data);
	pfree(line);
	pfree(result);

	foreach(cell, result_lines)
	{
		char *str = (char *)lfirst(cell);
		writeStr(groupid, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN, "io.max", str);
	}

	list_free_deep(result_lines);
}
