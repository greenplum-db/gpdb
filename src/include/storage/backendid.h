/*-------------------------------------------------------------------------
 *
 * backendid.h
 *	  POSTGRES backend id communication definitions
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/backendid.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BACKENDID_H
#define BACKENDID_H

/* ----------------
 *		-cim 8/17/90
 * ----------------
 */
typedef int BackendId;			/* unique currently active backend identifier */

#define InvalidBackendId		(-1)

/*
 * TempRelBackendId is used in GPDB in place of a real backend ID in some
 * places where we deal with a temporary tables.
 */
#define TempRelBackendId		(-2)

extern PGDLLIMPORT BackendId MyBackendId;		/* backend id of this backend */

#endif   /* BACKENDID_H */
