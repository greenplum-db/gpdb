/*-------------------------------------------------------------------------
 *
 * pg_proc_callback.h
 *	  
 *   Auxillary extension to pg_proc to enable defining additional callback
 *   functions.  Currently the list of allowable callback functions is small
 *   and consists of:
 *     - DESCRIBE() - to support more complex pseudotype resolution
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_proc_callback.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_PROC_CALLBACK_H
#define PG_PROC_CALLBACK_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_proc_callback definition.  cpp turns this into
 *		typedef struct FormData_pg_proc_callback
 * ----------------
 */
#define ProcCallbackRelationId	7176

CATALOG(pg_proc_callback,7176) BKI_WITHOUT_OIDS
{
	regproc	profnoid;		/* oid of the main function */
	regproc	procallback;	/* oid of the callback function */
	char	promethod;		/* role the callback function is performing */
} FormData_pg_proc_callback;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(profnoid REFERENCES pg_proc(oid));
FOREIGN_KEY(procallback REFERENCES pg_proc(oid));

/* ----------------
 *		Form_pg_proc_callback corresponds to a pointer to a tuple with
 *		the format of pg_proc_callback relation.
 * ----------------
 */
typedef FormData_pg_proc_callback *Form_pg_proc_callback;


/* ----------------
 *		compiler constants for pg_proc_callback
 * ----------------
 */
#define Natts_pg_proc_callback				3
#define Anum_pg_proc_callback_profnoid		1
#define Anum_pg_proc_callback_procallback	2
#define Anum_pg_proc_callback_promethod		3

/* values for promethod */
#define PROMETHOD_DESCRIBE 'd'

/* Functions in pg_proc_callback.c */
extern void deleteProcCallbacks(Oid profnoid);
extern void addProcCallback(Oid profnoid, Oid procallback, char promethod);
extern Oid lookupProcCallback(Oid profnoid, char promethod);

#endif
