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

#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <mntent.h>
#include <libgen.h>

const int	IOconfigTotalFields = 4;
const char	*IOconfigFields[4] = { "rbps", "wbps", "riops", "wiops" };

/*
 * Why not use 'return a -b' directly?
 * Because bdi_t is unsigned long now, larger than int. And the
 * implementation of bdi_t maybe changes in the future.
 */
static int
bdi_cmp(const void *a, const void *b)
{
	int x = *(bdi_t *)a;
	int y = *(bdi_t *)b;

	if (x < y)
		return -1;
	if (x > y)
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
	bdi_t *bdi_array;

	foreach (limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);
		bdi_count += fill_bdi_list(limit);
	}

	bdi_array = (bdi_t *) palloc(bdi_count * sizeof(bdi_t));
	foreach (limit_cell, limit_list)
	{
		TblSpcIOLimit *limit = (TblSpcIOLimit *)lfirst(limit_cell);


		ListCell      *bdi_cell;
		foreach (bdi_cell, limit->bdi_list)
			bdi_array[i++] = *(bdi_t *)lfirst(bdi_cell);
	}

	/* check duplicate bdi */
	pg_qsort(bdi_array, bdi_count, sizeof(bdi_t), bdi_cmp);
	for (int i = 0; i < bdi_count - 1; ++i)
	{
		if (bdi_array[i] == bdi_array[i + 1])
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("io limit: tablespaces of io limit must located at different disks")));
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
int fill_bdi_list(TblSpcIOLimit *iolimit)
{
	int result_cnt = 0;

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
			char data_path[1024];
			bdi_t *bdi;
			Form_pg_tablespace spaceform = (Form_pg_tablespace) GETSTRUCT(tuple);
			Oid dbid = MyDatabaseId;
			if (spaceform->oid == GLOBALTABLESPACE_OID)
				dbid = 0;

			ts_dir = GetDatabasePath(dbid, spaceform->oid);
			sprintf(data_path, "%s/%s", DataDir, ts_dir);

			bdi = (bdi_t *)palloc0(sizeof(bdi_t));
			*bdi = get_bdi_of_path(data_path);
			iolimit->bdi_list = lappend(iolimit->bdi_list, bdi);
			result_cnt++;
		}
		table_endscan(scan);
		table_close(rel, AccessShareLock);
	}
	else
	{
		char *ts_dir;
		char data_path[1024];
		Oid dbid = MyDatabaseId;
		bdi_t *bdi;
		if (iolimit->tablespace_oid == GLOBALTABLESPACE_OID)
			dbid = 0;

		ts_dir = GetDatabasePath(dbid, iolimit->tablespace_oid);
		sprintf(data_path, "%s/%s", DataDir, ts_dir);

		bdi = (bdi_t *)palloc0(sizeof(bdi_t));
		*bdi = get_bdi_of_path(data_path);

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
