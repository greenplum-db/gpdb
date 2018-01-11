/*-------------------------------------------------------------------------
 *
 * persistentutil.c
 *
 * Functions to support administrative tasks against gp_persistent_*_node
 * tables, gp_global_sequence and gp_relation_node. These tables do not have
 * normal MVCC semantics and changing them is usually part of a more complex
 * chain of events in the backend. So, we cannot modify them with INSERT,
 * UPDATE or DELETE as we might other catalog tables.
 *
 * So we provide these functions which update the tables and also update the in
 * memory persistent table / file rep data structures.
 *
 * Portions Copyright (c) 2010 Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/gp/persistentutil.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "funcapi.h"

#include "access/heapam.h"
#include "catalog/catalog.h"
#include "catalog/gp_persistent.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbglobalsequence.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "executor/executor.h"
#include "utils/builtins.h"



#include <dirent.h>


typedef struct
{
	Relation			 tablespaceRelation;
	HeapScanDesc		 scandesc;
	Oid					 tablespaceOid;
	Oid					 databaseOid;
	DIR					*tablespaceDir;
	DIR					*databaseDir;
	char                 tablespaceDirName[MAXPGPATH];
	char                 databaseDirName[MAXPGPATH];
} node_check_data;

static bool strToRelfilenode(char *str, Oid *relfilenode, int32 *segmentnum);
static void nodeCheckCleanup(Datum arg);

#define MAX_STRING_LEN_RELFILENODE 10 /* strlen("2147483648") */
#define NYI elog(ERROR, "not yet implemented")

Datum
gp_add_persistent_filespace_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_add_persistent_tablespace_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_add_persistent_database_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_add_persistent_relation_node_entry(PG_FUNCTION_ARGS)
{
	Datum				values[Natts_gp_persistent_relation_node];
	ItemPointerData		persistentTid;
	int64				persistentSerialNum;
	int					i;
	
	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */

	/*
	 * First parameter is the tid, remaining parameters should match the column
	 * types in gp_persistent_relation_node.
	 */
	if (PG_NARGS() != Natts_gp_persistent_relation_node + 1)
	{
		Oid		 procOid  = fcinfo->flinfo->fn_oid;
		char	*procName = format_procedure(procOid);

		elog(ERROR, "function '%s' received unexpected number of arguments",
			 procName);
	}

	/* 
	 * For the moment we don't support inserting at particular tids, 
	 * initial argument MUST be null.
	 */
	if (!PG_ARGISNULL(0))
		elog(ERROR, "direct tid assignment to %s is not yet supported",
			 "gp_persistent_relation_node");
	
	/* 
	 * validate that datatypes match expected, e.g. no one went and changed
	 * the catalog without updating this function.
	 */

	/* Build up the tuple we want to add */
	memset(&persistentTid, 0, sizeof(persistentTid));
	for (i = 0; i < Natts_gp_persistent_relation_node; i++)
	{
		if (PG_ARGISNULL(i+1))
			elog(ERROR, "null arguments not supported");
		values[i] = PG_GETARG_DATUM(i+1);
	}
	
	/*
	 * TODO: Validate the tuple
	 *   - Specified database exists
	 *   - Specified tablespace exists
	 *   - Specified relfile is in the filesystem
	 *   - etc.
	 */

	/* Add it to the table */
	PersistentFileSysObj_AddTuple(PersistentFsObjType_RelationFile,
								  values,
								  true, /* flushToXlog */
								  &persistentTid,
								  &persistentSerialNum);

	/* explain how we re-wrote that tuple */
	elog(NOTICE,
		 "inserted 1 row (TID %s, persistent_serial_num " INT64_FORMAT ")",
		 ItemPointerToString(&persistentTid),
		 persistentSerialNum);

	PG_RETURN_BOOL(true);
}

Datum
gp_add_global_sequence_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_add_relation_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_update_persistent_filespace_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_update_persistent_tablespace_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_update_persistent_database_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_update_persistent_relation_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}


/*
 * gp_update_global_sequence_entry(tid, bigint) => bool
 * 
 * Updates the given global sequence to the specified value:
 *   - Only allows increasing the sequence value
 *   - Only lets you set the tids '(0,1)' through '(0,4)'
 *     * these are the only tids currently used by the system 
 *       (see cdb/cdbglobalsequence.h)
 */
Datum
gp_update_global_sequence_entry(PG_FUNCTION_ARGS)
{
	ItemPointer			tid;
	int8				sequenceVal;
	GpGlobalSequence    sequence;

	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		elog(ERROR, "null input parameter");

	tid = (ItemPointer) PG_GETARG_POINTER(0);
	sequenceVal = PG_GETARG_INT64(1);

	/* Check tid */
	if (ItemPointerGetBlockNumber(tid) != 0)
		elog(ERROR, "unexpected block number in tid");
	sequence = (GpGlobalSequence) ItemPointerGetOffsetNumber(tid);
	switch (sequence)
	{
		case GpGlobalSequence_PersistentRelation:
		case GpGlobalSequence_PersistentDatabase:
		case GpGlobalSequence_PersistentTablespace:
		case GpGlobalSequence_PersistentFilespace:
			break;

		default:
			elog(ERROR, "unexpected offset number in tid");
	}

	/* Check sequence value */
	if (sequenceVal < GlobalSequence_Current(sequence))
		elog(ERROR, "sequence number too low");

	/* Everything looks good, update the value */
	GlobalSequence_Set(sequence, sequenceVal);
	PG_RETURN_BOOL(true);
}

Datum
gp_update_relation_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_delete_persistent_filespace_node_entry(PG_FUNCTION_ARGS)
{
	PersistentFsObjType                  fileSysObjType;
	PersistentFileSysObjData			*fileSysObjData;
	PersistentFileSysObjSharedData		*fileSysObjSharedData;
	ItemPointer							 tid;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */
	if (PG_ARGISNULL(0))
		elog(ERROR, "null input parameter");

	tid = (ItemPointer) PG_GETARG_POINTER(0);

	/* delete the tuple from the persistent table */
	fileSysObjType = PersistentFsObjType_FilespaceDir;
	PersistentFileSysObj_GetDataPtrs(fileSysObjType, 
									 &fileSysObjData, 
									 &fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;
	PersistentFileSysObj_FreeTuple(fileSysObjData,
								   fileSysObjSharedData,
								   fileSysObjType,
								   tid,
								   true  /* flushToXLog */);
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PG_RETURN_BOOL(true);
}

Datum
gp_delete_persistent_tablespace_node_entry(PG_FUNCTION_ARGS)
{
	PersistentFsObjType                  fileSysObjType;
	PersistentFileSysObjData			*fileSysObjData;
	PersistentFileSysObjSharedData		*fileSysObjSharedData;
	ItemPointer							 tid;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */
	if (PG_ARGISNULL(0))
		elog(ERROR, "null input parameter");

	tid = (ItemPointer) PG_GETARG_POINTER(0);

	/* delete the tuple from the persistent table */
	fileSysObjType = PersistentFsObjType_TablespaceDir;
	PersistentFileSysObj_GetDataPtrs(fileSysObjType, 
									 &fileSysObjData, 
									 &fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;
	PersistentFileSysObj_FreeTuple(fileSysObjData,
								   fileSysObjSharedData,
								   fileSysObjType,
								   tid,
								   true  /* flushToXLog */);
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PG_RETURN_BOOL(true);
}

Datum
gp_delete_persistent_database_node_entry(PG_FUNCTION_ARGS)
{
	PersistentFsObjType                  fileSysObjType;
	PersistentFileSysObjData			*fileSysObjData;
	PersistentFileSysObjSharedData		*fileSysObjSharedData;
	ItemPointer							 tid;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */
	if (PG_ARGISNULL(0))
		elog(ERROR, "null input parameter");

	tid = (ItemPointer) PG_GETARG_POINTER(0);

	/* delete the tuple from the persistent table */
	fileSysObjType = PersistentFsObjType_DatabaseDir;
	PersistentFileSysObj_GetDataPtrs(fileSysObjType, 
									 &fileSysObjData, 
									 &fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;
	PersistentFileSysObj_FreeTuple(fileSysObjData,
								   fileSysObjSharedData,
								   fileSysObjType,
								   tid,
								   true  /* flushToXLog */);
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PG_RETURN_BOOL(true);
}

Datum
gp_delete_persistent_relation_node_entry(PG_FUNCTION_ARGS)
{
	PersistentFsObjType                  fileSysObjType;
	PersistentFileSysObjData			*fileSysObjData;
	PersistentFileSysObjSharedData		*fileSysObjSharedData;
	ItemPointer							 tid;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");

	/* Check input arguments */
	if (PG_ARGISNULL(0))
		elog(ERROR, "null input parameter");

	tid = (ItemPointer) PG_GETARG_POINTER(0);

	/* delete the tuple from the persistent table */
	fileSysObjType = PersistentFsObjType_RelationFile;
	PersistentFileSysObj_GetDataPtrs(fileSysObjType, 
									 &fileSysObjData, 
									 &fileSysObjSharedData);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;
	PersistentFileSysObj_FreeTuple(fileSysObjData,
								   fileSysObjSharedData,
								   fileSysObjType,
								   tid,
								   true  /* flushToXLog */);
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	PG_RETURN_BOOL(true);
}

Datum
gp_delete_global_sequence_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}

Datum
gp_delete_relation_node_entry(PG_FUNCTION_ARGS)
{
	NYI;
	PG_RETURN_BOOL(true);
}




/*
 * gp_persistent_relation_node_check()
 *
 * Reads the physical filesystem for every defined filespace and returns the
 * list of relfilenodes that actually exist.  This list should match the set of
 * relfilenodes tracked in gp_persistent_relation_node.
 */
Datum
gp_persistent_relation_node_check(PG_FUNCTION_ARGS)
{
	FuncCallContext     *fcontext;
	node_check_data     *fdata;
	ReturnSetInfo       *rsinfo;
	MemoryContext        oldcontext;
	Oid                  relfilenode = InvalidOid;
	int32				 segnum		 = 0;
	HeapTuple			 tuple;
	char				*primaryPath = NULL;
	char				*mirrorPath  = NULL;

	if (SRF_IS_FIRSTCALL())
	{
		Relation    rel;
		TupleDesc	tupdesc;

		fcontext = SRF_FIRSTCALL_INIT();

		rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;

		/*
		 * The fdata cannot be allocated in the multi_call_ctx because the
		 * multi_call_context gets cleaned up by the MultiFuncCall callback
		 * function which gets called before the callback this function
		 * registers to cleanup the fdata structure.  So instead we allocate
		 * in the parent context fn_mcxt.
		 */
		oldcontext = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
		fdata = (node_check_data*) palloc0(sizeof(node_check_data));
		fcontext->user_fctx = fdata;

		/* 
		 * Register a call to cleanup when the function ends.
		 */
		RegisterExprContextCallback(rsinfo->econtext, nodeCheckCleanup,
									PointerGetDatum(fdata));

		/* 
		 * Setup the main loop over the list of tablespaces 
		 */
		fdata->tablespaceRelation = 
			heap_open(TableSpaceRelationId, AccessShareLock);
		fdata->scandesc = 
			heap_beginscan(fdata->tablespaceRelation, SnapshotNow, 0, NULL);

		/*
		 * Bless a tuple descriptor for the return type
		 */
		MemoryContextSwitchTo(fcontext->multi_call_memory_ctx);
		rel = RelationIdGetRelation(GpPersistentRelationNodeRelationId);
		tupdesc = RelationGetDescr(rel);
		fcontext->tuple_desc = BlessTupleDesc(tupdesc);
		relation_close(rel, NoLock);

		MemoryContextSwitchTo(oldcontext);
	}

	fcontext = SRF_PERCALL_SETUP();
	fdata = fcontext->user_fctx;

	/*
	 * The code here is basically a nested loop that has been unwound so that
	 * it can be wrapped up into a set-returning function.
	 *
	 * Basic structure is:
	 *  - Loop over tablespace relation
	 *    - Loop over database directories in the tablespace
	 *      - Loop over relfilenodes in the directory
	 *        - Return each tablespace, database, relfilenode, segment_number.
	 *
	 * The complicating factor is that we return from this function and
	 * reenter the loop at the innermost level, so the entire loop is turned
	 * inside-out.
	 */
	while (true)
	{

		/* Innermost loop */
		if (fdata->databaseDir)
		{
			struct dirent		*dent;
			Datum				 values[Natts_gp_persistent_relation_node];
			bool				 nulls[Natts_gp_persistent_relation_node];

			dent = ReadDir(fdata->databaseDir, fdata->databaseDirName);
			if (!dent)
			{  /* step out of innermost loop */
				FreeDir(fdata->databaseDir);
				fdata->databaseDir = NULL;
				continue;  
			}
			
			/* skip the boring stuff */
			if (strcmp(dent->d_name, ".") == 0 || 
				strcmp(dent->d_name, "..") == 0)
				continue;
			
			/* Skip things that don't look like relfilenodes */
			if (!strToRelfilenode(dent->d_name, &relfilenode, &segnum))
				continue;

			/* Return relfilenodes as we find them */
			MemSet(nulls, true, sizeof(nulls));
			nulls[Anum_gp_persistent_relation_node_tablespace_oid-1]   = false;
			nulls[Anum_gp_persistent_relation_node_database_oid-1]     = false;
			nulls[Anum_gp_persistent_relation_node_relfilenode_oid-1]  = false;
			nulls[Anum_gp_persistent_relation_node_segment_file_num-1] = false;
			values[Anum_gp_persistent_relation_node_tablespace_oid-1] =
				ObjectIdGetDatum(fdata->tablespaceOid);
			values[Anum_gp_persistent_relation_node_database_oid-1] =
				ObjectIdGetDatum(fdata->databaseOid);
			values[Anum_gp_persistent_relation_node_relfilenode_oid-1] =
				ObjectIdGetDatum(relfilenode);
			values[Anum_gp_persistent_relation_node_segment_file_num-1] =
				Int32GetDatum(segnum);

			tuple = heap_form_tuple(fcontext->tuple_desc, values, nulls);
			
			SRF_RETURN_NEXT(fcontext, HeapTupleGetDatum(tuple));
		}

		/* Loop over database directories in the tablespace */
		if (fdata->tablespaceDir)
		{
			struct dirent *dent;

			dent = ReadDir(fdata->tablespaceDir, fdata->tablespaceDirName);
			if (!dent)
			{  /* step out of database loop */
				FreeDir(fdata->tablespaceDir);
				fdata->tablespaceDir = NULL;
				continue;  
			}
			
			/* skip the borring stuff */
			if (strcmp(dent->d_name, ".") == 0 || 
				strcmp(dent->d_name, "..") == 0)
				continue;
			
			/* Skip things that don't look like database oids */
			if (strlen(dent->d_name) != strspn(dent->d_name, "0123456789"))
				continue;

			/* convert the string to an oid */
			fdata->databaseOid = DatumGetObjectId(DirectFunctionCall1(oidin,
						CStringGetDatum(dent->d_name)));

			/* form a database path using this oid */
			snprintf(fdata->databaseDirName, MAXPGPATH, "%s/%s",
					 fdata->tablespaceDirName, 
					 dent->d_name);
			
			oldcontext = 
				MemoryContextSwitchTo(fcontext->multi_call_memory_ctx);
			fdata->databaseDir = AllocateDir(fdata->databaseDirName);
			MemoryContextSwitchTo(oldcontext);

			if (fdata->databaseDir == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open directory \"%s\": %m",
								fdata->databaseDirName)));
			continue;
		}

		/* Outermost loop over tablespaces */
		tuple = heap_getnext(fdata->scandesc, ForwardScanDirection);
		if (!HeapTupleIsValid(tuple))
			SRF_RETURN_DONE(fcontext);  /* FINAL return */
				
		fdata->tablespaceOid = HeapTupleGetOid(tuple);
				
		PersistentTablespace_GetPrimaryAndMirrorFilespaces(
			fdata->tablespaceOid, &primaryPath, &mirrorPath);

		/* Find the location of this tablespace on disk */
		FormTablespacePath(fdata->tablespaceDirName, 
						   primaryPath, fdata->tablespaceOid);

		/* 
		 * Primary path is null for the pg_system filespace, additionally
		 * Mirror path is null if there are no mirrors 
		 */
		if (primaryPath)
		{
			pfree(primaryPath);
			primaryPath = NULL;
		}
		if (mirrorPath)
		{
			pfree(mirrorPath);
			mirrorPath = NULL;
		}
				
		oldcontext = 
			MemoryContextSwitchTo(fcontext->multi_call_memory_ctx);
		fdata->tablespaceDir = AllocateDir(fdata->tablespaceDirName);
		MemoryContextSwitchTo(oldcontext);

		if (fdata->tablespaceDir == NULL)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open directory \"%s\": %m",
							fdata->tablespaceDirName)));
			
		/* The global tablespace doesn't have database directories */
		if (fdata->tablespaceOid == GLOBALTABLESPACE_OID)
		{
			fdata->databaseOid = 0;
			
			/* Skip to the innermost loop */
			fdata->databaseDir = fdata->tablespaceDir;
			fdata->tablespaceDir = NULL;
			strlcpy(fdata->databaseDirName, fdata->tablespaceDirName,
					MAXPGPATH);
		}
	}

	/* Unreachable */
	SRF_RETURN_DONE(fcontext);
}


static void 
nodeCheckCleanup(Datum arg)
{
	node_check_data *fdata = (node_check_data*) DatumGetPointer(arg);
	
	if (fdata->databaseDir != NULL)
	{
		FreeDir(fdata->databaseDir);
		fdata->databaseDir = NULL;
	}
	if (fdata->tablespaceDir != NULL)
	{
		FreeDir(fdata->tablespaceDir);
		fdata->tablespaceDir = NULL;
	}
	if (fdata->scandesc != NULL)
	{
		heap_endscan(fdata->scandesc);
		fdata->scandesc = NULL;
	}
	if (fdata->tablespaceRelation != NULL)
	{
		heap_close(fdata->tablespaceRelation, AccessShareLock);
		fdata->tablespaceRelation = NULL;
	}
	pfree(fdata);
}

/*
 * strToRelfilenode() - convert a string into a relfilenode and segment number.
 *
 * Returns false if the string doesn't match "^\d+(\.\d+)?$"
 */
static bool
strToRelfilenode(char *str, Oid *relfilenode, int32 *segmentnum)
{
	char largestStringValue[MAX_STRING_LEN_RELFILENODE + 1];
	int32 relfilenodeStrLen = 0;
	char *s;

	relfilenodeStrLen = strlen(str);
	/* String must contain characters */
	if (relfilenodeStrLen == 0)
		return false;

	/* If it isn't numbers and dots then its not a relfilenode. */
	if (relfilenodeStrLen != strspn(str, "0123456789."))
		return false;
	
	/* Find the dot, and convert the preceeding part */
	s = strchr(str, '.');

	if (s != NULL)
		relfilenodeStrLen = s - str;

	memcpy(largestStringValue, str, relfilenodeStrLen);
	largestStringValue[relfilenodeStrLen] = '\0';

	/* Convert the string to a number for the relfilenode */
	*relfilenode = DatumGetObjectId(DirectFunctionCall1(oidin,
				CStringGetDatum(largestStringValue)));

	/* If we found a dot, convert the segmentnum */
	if (s == NULL)
	{
		*segmentnum = 0;
		return true;
	}

	/* Dot exists, next should be another number */
	s++;
	if (s[0] == '\0' || s[0] == '.')
		return false;
	*segmentnum = pg_atoi(s, 4, '.');

	/* We should be at the end of the string, no more dots */
	s = strchr(s, '.');
	if (s != NULL)
		return false;

	/* All done, return success */
	return true;
}
