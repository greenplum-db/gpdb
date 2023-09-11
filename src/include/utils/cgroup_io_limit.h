#ifndef CGROUP_IO_LIMIT_H
#define CGROUP_IO_LIMIT_H

#include "postgres.h"
#include "nodes/pg_list.h"

/* type for linux device id, use libc dev_t now.
 * bdi means backing device info.
 */
#ifdef __linux__
typedef dev_t bdi_t;
#define make_bdi(major, minor) makedev(major, minor)
#define bdi_major(bdi) major(bdi)
#define bdi_minor(bdi) minor(bdi)
#else
typedef uint64 bdi_t;
#endif


#define IO_LIMIT_MAX  (PG_UINT64_MAX)
#define IO_LIMIT_EMPTY (0)

/*
 * IOconfig represents the io.max of cgroup v2 io controller.
 * Fields: each field correspond to cgroup v2 io.max file.
 *	rbps: read bytes per second
 *	wbps: write bytes per second
 *	riops: read iops
 *	wiops: write iops
 */
typedef struct IOconifg
{
	// use uint64 for all fields, we can retrieve field by offset easily.
	uint64 rbps;
	uint64 wbps;
	uint64 riops;
	uint64 wiops;
} IOconfig;

/* the order must be same as struct IOconfig */
const extern char	*IOconfigFields[4];
/* the order must be same as struct IOconfigFields */
const extern char	*IOStatFields[4];

typedef struct IOconfigItem
{
	int offset;
	uint64 value;
} IOconfigItem;

/*
 * TblSpcIOLimit connects IOconfig and gpdb tablespace.
 * GPDB tablespace is a directory in filesystem, but the back of this directory
 * is one or multiple disks. Each disk has its own BDI, so there is a bdi_list
 * to save those bdi of disks.
 */
typedef struct TblSpcIOLimit
{
	Oid       tablespace_oid;

	/* for * and some filesystems, there are maybe multi block devices */
	List	  *bdi_list;

	IOconfig  *ioconfig;
} TblSpcIOLimit;

typedef struct IOLimitParserContext
{
	List *result;
	int normal_tablespce_cnt;
	int star_tablespace_cnt;
} IOLimitParserContext;

typedef struct IOLimitScannerState
{
	void *buffer;
	void *scanner;
} IOLimitScannerState;

typedef struct IOStatItems
{
	uint64 rbytes;
	uint64 wbytes;
	uint64 rios;
	uint64 wios;
} IOStatItems;

typedef struct IOStat
{
	Oid groupid;
	Oid tablespace;
	IOStatItems items;
} IOStat;

typedef struct IOStatHashEntry
{
	bdi_t id;
	IOStatItems items;
} IOStatHashEntry;

extern void io_limit_scanner_begin(IOLimitScannerState *state, const char *limit_str);
extern void io_limit_scanner_finish(IOLimitScannerState *state);

extern char *get_tablespace_path(Oid spcid);
extern bdi_t get_bdi_of_path(const char *ori_path);
extern int fill_bdi_list(TblSpcIOLimit *io_limit);

extern List *io_limit_parse(const char *limit_str);
extern void io_limit_free(List *limit_list);
extern void io_limit_validate(List *limit_list);
bool io_limit_value_validate(const char *field, const uint64 value, uint64 *max);

extern List  *get_iostat(Oid groupid, List *io_limit);
extern int  compare_iostat(const void *a, const void *b);
extern char *io_limit_dump(List *limit_list);
extern void clear_io_max(Oid groupid);

#endif
