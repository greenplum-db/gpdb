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
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "catalog/pg_proc_callback_d.h"

/* ----------------
 *		pg_proc_callback definition.  cpp turns this into
 *		typedef struct FormData_pg_proc_callback
 * ----------------
 */
CATALOG(pg_proc_callback,7176,ProcCallbackRelationId)
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

/* values for promethod */
#define PROMETHOD_DESCRIBE 'd'

/* Functions in pg_proc_callback.c */
extern void deleteProcCallbacks(Oid profnoid);
extern void addProcCallback(Oid profnoid, Oid procallback, char promethod);
extern Oid lookupProcCallback(Oid profnoid, char promethod);

#endif
