#ifndef CGROUP_IO_LIMIT_H
#define CGROUP_IO_LIMIT_H

#include "postgres.h"
#include "nodes/pg_list.h"

/* type for linux device id, use libc dev_t now.
 * bdi means backing device info.
 */
typedef dev_t bdi_t;

#define make_bdi(major, minor) makedev(major, minor)
#define bdi_major(bdi) major(bdi)
#define bdi_minor(bdi) minor(bdi)

/*
 * IOconfig represents the io.max of cgroup v2 io controller.
 * Fiels: each field correspond to cgroup v2 io.max file.
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

typedef struct IO_LIMIT_PARSER_STATE
{
	void *state;
	void *scanner;
} IO_LIMIT_PARSER_STATE;

IO_LIMIT_PARSER_STATE *io_limit_begin_scan(const char *limit_str);
List *io_limit_parse(const char *limit_str);
void io_limit_end_scan(IO_LIMIT_PARSER_STATE *state);

#endif
