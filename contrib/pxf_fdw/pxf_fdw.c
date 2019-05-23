/*
 * pxf_fdw.c
 *		  Foreign-data wrapper for PXF (Platform Extension Framework)
 *
 * IDENTIFICATION
 *		  contrib/pxf_fdw/pxf_fdw.c
 */

#include "postgres.h"

#include "foreign/fdwapi.h"

PG_MODULE_MAGIC;

extern Datum pxf_fdw_handler(PG_FUNCTION_ARGS);

/*
 * SQL functions
 */
PG_FUNCTION_INFO_V1(pxf_fdw_handler);

/*
 * Foreign-data wrapper handler functions:
 * returns a struct with pointers to the
 * pxf_fdw callback routines.
 */
Datum
pxf_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdw_routine = makeNode(FdwRoutine);

	PG_RETURN_POINTER(fdw_routine);
}
