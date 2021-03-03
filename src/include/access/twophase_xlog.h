/*-------------------------------------------------------------------------
 *
 * twophase_xlog.h
 *	  Two-phase-commit state file.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/twophase_xlog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TWOPHASE_XLOG_H
#define TWOPHASE_XLOG_H

/* GPDB-specific: TwoPhaseFileHeader is defined in twophase.c in Postgres */
/*
 * Header for a 2PC state file
 */
typedef struct TwoPhaseFileHeader
{
	uint32		magic;			/* format identifier */
	uint32		total_len;		/* actual file length */
	TransactionId xid;			/* original transaction XID */
	Oid			database;		/* OID of database it was in */
	TimestampTz prepared_at;	/* time of preparation */
	Oid			owner;			/* user running the transaction */
	int32		nsubxacts;		/* number of following subxact XIDs */
	int32		ncommitrels;	/* number of delete-on-commit rels */
	int32		nabortrels;		/* number of delete-on-abort rels */
	int32		ncommitdbs;		/* number of delete-on-commit dbs */
	int32		nabortdbs;		/* number of delete-on-abort dbs */
	int32		ninvalmsgs;		/* number of cache invalidation messages */
	bool		initfileinval;	/* does relcache init file need invalidation? */
	Oid			tablespace_oid_to_delete_on_abort;
	Oid			tablespace_oid_to_delete_on_commit;
	uint16		gidlen;			/* length of the GID - GID follows the header */
	XLogRecPtr	origin_lsn;		/* lsn of this record at origin node */
	TimestampTz origin_timestamp;	/* time of prepare at origin node */
} TwoPhaseFileHeader;

#endif							/* TWOPHASE_H */
