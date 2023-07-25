#include "postgres.h"
#include "port.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "commands/tablespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_tablespace_d.h"
#include "utils/relcache.h"
#include "utils/cgroup_io_limit.h"

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
