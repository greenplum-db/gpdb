/*-------------------------------------------------------------------------
 *
 * gp_global_sequence.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/gp_global_sequence.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"
#include "storage/fd.h"

#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_global_sequence.h"
#include "cdb/cdbsharedoidsearch.h"
#include "storage/itemptr.h"
#include "storage/shmem.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "utils/guc.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "utils/builtins.h"

void GpGlobalSequence_GetValues(
	Datum						*values,

	int64						*sequenceNum)
{
	*sequenceNum = DatumGetInt64(values[Anum_gp_global_sequence_sequence_num - 1]);

}

void GpGlobalSequence_SetDatumValues(
	Datum					*values,

	int64					sequenceNum)
{
	values[Anum_gp_global_sequence_sequence_num - 1] = 
									Int64GetDatum(sequenceNum);
}
