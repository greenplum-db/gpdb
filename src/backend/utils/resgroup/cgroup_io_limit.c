#include "postgres.h"
#include "fmgr.h"
#include "port.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/genam.h"
#include "access/table.h"
#include "access/tableam.h"
#include "commands/resgroupcmds.h"
#include "commands/tablespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_tablespace_d.h"
#include "catalog/pg_resgroup_d.h"
#include "catalog/pg_resgroupcapability_d.h"
#include "catalog/indexing.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/relcache.h"
#include "utils/cgroup_io_limit.h"
#include "utils/cgroup.h"
#include "utils/resgroup.h"

#include <limits.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <mntent.h>
#include <libgen.h>
#include <unistd.h>

const char	*IOconfigFields[4] = { "rbps", "wbps", "riops", "wiops" };

static int bdi_cmp(const void *a, const void *b);
static void ioconfig_validate(IOconfig *config);

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

	foreach (limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);
		bdi_count += fill_bdi_list(limit);

		if (limit->tablespace_oid == InvalidOid)
			is_star = true;
	}

	bdi_array = (BDICmp *) palloc(bdi_count * sizeof(BDICmp));
	/* fill bdi list and check wbps/rbps range */
	foreach (limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);
		ListCell      *bdi_cell;

		ioconfig_validate(limit->ioconfig);

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
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("io limit: cannot find realpath of %s, details: %m.", ori_path)));
	}

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

static void
ioconfig_validate(IOconfig *config)
{
	const uint64 ULMAX = ULLONG_MAX / 1024 / 1024;
	const uint32 UMAX = UINT_MAX;

	if (config->rbps != IO_LIMIT_MAX && (config->rbps > ULMAX || config->rbps < 2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("io limit: rbps must in range [2, %lu] or equal 0", ULMAX)));

	if (config->wbps != IO_LIMIT_MAX && (config->wbps > ULMAX || config->wbps < 2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("io limit: wbps must in range [2, %lu] or equal 0", ULMAX)));

	if (config->wiops != IO_LIMIT_MAX && (config->wiops > UMAX || config->wiops < 2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("io limit: wiops must in range [2, %u] or equal 0", UMAX)));

	if (config->riops != IO_LIMIT_MAX && (config->riops > UMAX || config->riops < 2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("io limit: riops must in range [2, %u] or equal 0", UMAX)));
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
		TblSpcIOLimit *limit = (TblSpcIOLimit *) lfirst(cell);
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
int
get_iostat(Oid groupid, List *io_limit, IOStat *result)
{
#define MAX_LINE 1024

	int count = 0;

	HTAB *io_stat_hash = NULL;
	HASHCTL ctl;

	char io_stat_path[PATH_MAX];
	char tmp_line[MAX_LINE] = {};
	ListCell *cell;
	List *lines = NIL;
	StringInfo line = makeStringInfo();
	FILE *f;


	buildPath(groupid, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN, "io.stat", io_stat_path, sizeof(io_stat_path));
	f = fopen(io_stat_path, "r");
	if (f == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("io limit: cannot read %s, details: %m.", io_stat_path)));
	}

	/*
	 * read all lines at a time
	 */
	while (fgets(tmp_line, MAX_LINE, f))
	{
		appendStringInfoString(line, tmp_line);
		if (tmp_line[strlen(tmp_line) - 1] == '\n')
		{
			lines = lappend(lines, line->data);
			initStringInfo(line);
		}
	}
	fclose(f);

	/*
	 * parse file content.
	 * content example:
	 * "8:16 rbytes=1459200 wbytes=314773504 rios=192 wios=353 ..."
	 */
	memset(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(bdi_t);
	ctl.entrysize = sizeof(IOStatHashEntry);
	ctl.hcxt = CurrentMemoryContext;
	io_stat_hash = hash_create("hash table for bdi -> io stat", list_length(io_limit),
							  &ctl, HASH_ELEM | HASH_CONTEXT);
	foreach(cell, lines)
	{
		uint64 maj, min, wbytes = 0, rbytes = 0, rios = 0, wios = 0;
		bdi_t bdi;
		IOStatHashEntry *entry;

		char *str = (char *) lfirst(cell);
		int res = sscanf(str, "%lu:%lu rbytes=%lu wbytes=%lu rios=%lu wios=%lu",
			   &maj, &min, &rbytes, &wbytes, &rios, &wios);

		if (res == EOF)
		{
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					errmsg("io limit: cannot parse content from '%s', details: %m.", str)));
		}

		bdi = make_bdi(maj, min);
		entry = hash_search(io_stat_hash, (void *) &bdi, HASH_ENTER, NULL);
		if (entry == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					errmsg("io limit: cannot insert into hash table")));
		}

		entry->id = bdi;
		entry->items.rbytes = rbytes;
		entry->items.wbytes = wbytes;
		entry->items.rios = rios;
		entry->items.wios = wios;
	}

	/* construct result list */
	foreach(cell, io_limit)
	{
		ListCell *bdi_cell;
		IOStat *stat = result + count;

		TblSpcIOLimit *limit = (TblSpcIOLimit *) lfirst(cell);
		fill_bdi_list(limit);
		stat->tablespace = limit->tablespace_oid;
		stat->groupid = groupid;

		foreach(bdi_cell, limit->bdi_list)
		{
			bdi_t *bdi = (bdi_t *) lfirst(bdi_cell);
			IOStatHashEntry *entry = hash_search(io_stat_hash, (void *) bdi, HASH_FIND, NULL);

			if (entry != NULL)
			{
				stat->items.wbytes += entry->items.wbytes;
				stat->items.rbytes += entry->items.rbytes;
				stat->items.rios += entry->items.rios;
				stat->items.wios += entry->items.wios;
			}
		}

		count++;
	}

	hash_destroy(io_stat_hash);
	io_limit_free(io_limit);
	list_free_deep(lines);

	return count;
}

int
compare_iostat(const void *x, const void *y)
{
	IOStat *a = (IOStat *)x;
	IOStat *b = (IOStat *)y;
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
