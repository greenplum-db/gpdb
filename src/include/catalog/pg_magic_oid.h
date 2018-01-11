/*-------------------------------------------------------------------------
 *
 * pg_magic_oid.h
 *	  Magic oids
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_magic_oid.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_MAGIC_OID_H
#define PG_MAGIC_OID_H

/* ----------
 *		Object ID (OID) zero is InvalidOid.
 *
 *		OIDs 1-9999 are reserved for manual assignment (see the files
 *		in src/include/catalog/).
 *
 *		OIDS 10000-16383 are reserved for assignment during initdb
 *		using the OID generator.  (We start the generator at 10000.)
 *
 *		OIDs beginning at 16384 are assigned from the OID generator
 *		during normal multiuser operation.	(We force the generator up to
 *		16384 as soon as we are in normal operation.)
 *
 * The choices of 10000 and 16384 are completely arbitrary, and can be moved
 * if we run low on OIDs in either category.  Changing the macros below
 * should be sufficient to do this.
 *
 * NOTE: if the OID generator wraps around, we skip over OIDs 0-16383
 * and resume with 16384.  This minimizes the odds of OID conflict, by not
 * reassigning OIDs that might have been assigned during initdb.
 * ----------
 */
#define FirstBootstrapObjectId	10000
#define FirstNormalObjectId	16384

/*
 * For the time being, we split the OID range so that newly added objects
 * won't conflict between GPDB and GPSQL.  If two merge into one in some day,
 * this boundary will disappear.
 */
#define LowestGPSQLBootstrapObjectId 7500

/*
 * Reserve a block of OIDs for use during binary upgrade. We create functions
 * needed during the upgrade here, but they are dropped after the ugprade, so
 * should not be present in other contexts. They need a reserved block of OIDs
 * to avoid colliding with user objects that are created later in the upgrade
 * process.
 */
#define FirstBinaryUpgradeReservedObjectId 9000
#define LastBinaryUpgradeReservedObjectId 9100

#endif
