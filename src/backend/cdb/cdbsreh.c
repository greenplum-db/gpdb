/*--------------------------------------------------------------------------
*
* cdbsreh.c
*	  Provides routines for single row error handling for COPY and external
*	  tables.
*
* Copyright (c) 2007-2008, Greenplum inc
*
*--------------------------------------------------------------------------
*/
#include "postgres.h"
#include <unistd.h>
#include <sys/stat.h>

#include "gp-libpq-fe.h"
#include "access/transam.h"
#include "catalog/catquery.h"
#include "catalog/gp_policy.h"
#include "catalog/namespace.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_type.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "funcapi.h"
#define PQArgBlock PQArgBlock_
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "nodes/makefuncs.h"

static void OpenErrorTable(CdbSreh *cdbsreh, RangeVar *errortable);
static void InsertIntoErrorTable(CdbSreh *cdbsreh);
static void CloseErrorTable(CdbSreh *cdbsreh);
static void DropErrorTable(CdbSreh *cdbsreh);
static int  GetNextSegid(CdbSreh *cdbsreh);
static void VerifyErrorTableAttr(Form_pg_attribute *attr, 
								 int attrnum, 
								 const char *expected_attname,
								 Oid expected_atttype,
								 char *relname);
static void PreprocessByteaData(char *src);
static void ErrorLogWrite(CdbSreh *cdbsreh);

#define ErrorLogDir "errlog"
#define ErrorLogFileName(fname, dbId, relId) \
	snprintf(fname, MAXPGPATH, "errlog/%u_%u", dbId, relId)

/*
 * Function context for gp_read_error_log
 */
typedef struct ReadErrorLogContext
{
	FILE	   *fp;					/* file pointer to the error log */
	char		filename[MAXPGPATH];/* filename of fp */
	int			numTuples;			/* number of total tuples when dispatch */
	PGresult  **segResults;			/* dispatch results */
	int			numSegResults;		/* number of segResults */
	int			currentResult;		/* current index in segResults to read */
	int			currentRow;			/* current row in current result */
} ReadErrorLogContext;

typedef enum RejectLimitCode
{
	REJECT_NONE = 0,
	REJECT_FIRST_BAD_LIMIT,
	REJECT_LIMIT_REACHED,
	REJECT_UNPARSABLE_CSV,
} RejectLimitCode;

int gp_initial_bad_row_limit = 1000;

/*
 * makeCdbSreh
 *
 * Allocate and initialize a Single Row Error Handling state object.
 * Pass in the only known parameters (both we get from the SQL stmt),
 * the other variables are set later on, when they are known.
 */
CdbSreh *
makeCdbSreh(bool is_keep, bool reusing_existing_errtable,
			int rejectlimit, bool is_limit_in_rows, 
			RangeVar *errortable, char *filename, char *relname,
			bool log_to_file)
{
	CdbSreh	*h;

	h = palloc(sizeof(CdbSreh));
	
	h->errmsg = NULL;
	h->rawdata = NULL;
	h->linenumber = 0;
	h->processed = 0;
	h->relname = relname;
	h->rejectlimit = rejectlimit;
	h->is_limit_in_rows = is_limit_in_rows;
	h->rejectcount = 0;
	h->is_server_enc = false;
	h->is_keep = is_keep;
	h->should_drop = false; /* we'll decide later */
	h->reusing_errtbl = reusing_existing_errtable;
	h->cdbcopy = NULL;
	h->errtbl = NULL;
	h->lastsegid = 0;
	h->consec_csv_err = 0;
	AssertImply(log_to_file, errortable == NULL);
	h->log_to_file = log_to_file;

	snprintf(h->filename, sizeof(h->filename),
			 "%s", filename ? filename : "<stdin>");

	/* error table was specified open it (and create it first if necessary) */
	if(errortable)
		OpenErrorTable(h, errortable);
	
	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype input routines, and should be faster than retail pfree's
	 * anyway.
	 */
	h->badrowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "SrehMemCtxt",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);
	
	return h;
}

void
destroyCdbSreh(CdbSreh *cdbsreh)
{
	
	/* delete the bad row context */
    MemoryContextDelete(cdbsreh->badrowcontext);
	
	/* close error table */
	if (cdbsreh->errtbl)
		CloseErrorTable(cdbsreh);
	
	/* drop error table if need to */
	if (cdbsreh->should_drop && Gp_role == GP_ROLE_DISPATCH)
		DropErrorTable(cdbsreh);
	
	pfree(cdbsreh);
}

/*
 * HandleSingleRowError
 *
 * The single row error handler. Gets called when a data error happened
 * in SREH mode. Reponsible for making a decision of what to do at that time.
 *
 * Some of the main actions are:
 *  - Keep track of reject limit. if reached make sure notify caller.
 *  - If error table used, call the error logger to log this error.
 *  - If QD COPY send the bad row to the QE COPY to deal with.
 *
 */
void HandleSingleRowError(CdbSreh *cdbsreh)
{
	
	/* increment total number of errors for this segment */ 
	cdbsreh->rejectcount++;
	
	/* 
	 * if reached the segment reject limit don't do anything.
	 * (this will get checked and handled later on by the caller).
	 */
	if(IsRejectLimitReached(cdbsreh))
		return;

	/*
	 * If not specified table or file, do nothing.  Otherwise,
	 * record the error:
	 *   QD - send the bad data row to a random QE (via roundrobin).
	 *   QE - log the error in the error table or file.
	 */
	if (cdbsreh->errtbl || cdbsreh->log_to_file)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			cdbCopySendData(cdbsreh->cdbcopy,
							GetNextSegid(cdbsreh),
							cdbsreh->rawdata,
							strlen(cdbsreh->rawdata));
			
		}
		else
		{
			if (cdbsreh->errtbl)
			{
				/* Insert into error table */
				Insist(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
				InsertIntoErrorTable(cdbsreh);
			}
			else
			{
				Assert(cdbsreh->log_to_file);
				ErrorLogWrite(cdbsreh);
			}
		}
		
	}
	
	return; /* OK */
}

/*
 * OpenErrorTable
 *
 * Open the error table for this operation, and perform all necessary checks.
 */
void OpenErrorTable(CdbSreh *cdbsreh, RangeVar *errortable)
{
	AclResult		aclresult;
	AclMode			required_access = ACL_INSERT;
	Oid				relOid;


	/* Open and lock the error relation, using the appropriate lock type. */
	cdbsreh->errtbl = heap_openrv(errortable, RowExclusiveLock);
	
	relOid = RelationGetRelid(cdbsreh->errtbl);
	
	/* Check relation permissions. */
	aclresult = pg_class_aclcheck(relOid,
								  GetUserId(),
								  required_access);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_CLASS,
					   RelationGetRelationName(cdbsreh->errtbl));
	
	/* check read-only transaction */
	if (XactReadOnly &&
		!isTempNamespace(RelationGetNamespace(cdbsreh->errtbl)))
		ereport(ERROR,
				(errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION),
				 errmsg("transaction is read-only")));
	
	/* make sure this is a regular relation */
	if (cdbsreh->errtbl->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" exists in the database but is a non table relation",
						RelationGetRelationName(cdbsreh->errtbl))));
}

/*
 * CloseErrorTable
 *
 * Done using the error table. Close it.
 */
void CloseErrorTable(CdbSreh *cdbsreh)
{
	/* close the error relation */
	if (cdbsreh->errtbl)
		heap_close(cdbsreh->errtbl, NoLock);
}

/*
 * DropErrorTable
 *
 * Drop the error table from the database. This function will be called from
 * destroyCdbSreh when an autogenerated error table was not used in the COPY
 * operation granted KEEP wasn't specified.
 *
 */
static
void DropErrorTable(CdbSreh *cdbsreh)
{
	StringInfoData dropstmt;
	RangeVar *errtbl_rv;
	
	Insist(Gp_role == GP_ROLE_DISPATCH);

	ereport(NOTICE,
			(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
			 errmsg("Dropping the auto-generated unused error table"),
			 errhint("Use KEEP in LOG INTO clause to force keeping the error table alive")));								
	
	initStringInfo(&dropstmt); 
	
	appendStringInfo(&dropstmt, "DROP TABLE %s.%s",
					 quote_identifier(get_namespace_name(RelationGetNamespace(cdbsreh->errtbl))),
					 quote_identifier(RelationGetRelationName(cdbsreh->errtbl)));
	
	errtbl_rv = makeRangeVar(get_namespace_name(RelationGetNamespace(cdbsreh->errtbl)),
							 RelationGetRelationName(cdbsreh->errtbl), -1);

	/* DROP the relation on the QD */
	RemoveRelation(errtbl_rv,DROP_RESTRICT, NULL);
				   
	/* dispatch the DROP to the QEs */
	CdbDoCommand(dropstmt.data, false, /*no txn */ false);

	pfree(dropstmt.data);
}

/*
 * Returns the fixed schema for error table/error log tuple.
 */
static TupleDesc
GetErrorTupleDesc(void)
{
	static TupleDesc tupdesc = NULL, tmp;
	MemoryContext oldcontext;
	int natts = NUM_ERRORTABLE_ATTR;
	FormData_pg_attribute attrs[NUM_ERRORTABLE_ATTR] = {
		{0,{"cmdtime"},1184,-1,8,1,0,-1,-1,true,'p','d',false,false,false,true,0},
		{0,{"relname"},25,-1,-1,2,0,-1,-1,false,'x','i',false,false,false,true,0},
		{0,{"filename"},25,-1,-1,3,0,-1,-1,false,'x','i',false,false,false,true,0},
		{0,{"linenum"},23,-1,4,4,0,-1,-1,true,'p','i',false,false,false,true,0},
		{0,{"bytenum"},23,-1,4,5,0,-1,-1,true,'p','i',false,false,false,true,0},
		{0,{"errmsg"},25,-1,-1,6,0,-1,-1,false,'x','i',false,false,false,true,0},
		{0,{"rawdata"},25,-1,-1,7,0,-1,-1,false,'x','i',false,false,false,true,0},
		{0,{"rawbytes"},17,-1,-1,8,0,-1,-1,false,'x','i',false,false,false,true,0}
	};

	/* If we have created it, use it. */
	if (tupdesc != NULL)
		return tupdesc;

	/*
	 * Keep the tupdesc for long in the cache context.  It should never
	 * be scribbled.
	 */
	oldcontext = MemoryContextSwitchTo(CacheMemoryContext);
	tmp = CreateTemplateTupleDesc(natts, false);
	tmp->tdrefcount = 0;
	tmp->tdtypeid = RECORDOID;
	tmp->tdtypmod = -1;
	for (int i = 0; i < natts; i++)
	{
		memcpy(tmp->attrs[i], &attrs[i], ATTRIBUTE_FIXED_PART_SIZE);
		tmp->attrs[i]->attcacheoff = -1;
	}
	tmp->attrs[0]->attcacheoff = 0;

	tupdesc = tmp;

	MemoryContextSwitchTo(oldcontext);

	return tupdesc;
}

static HeapTuple
FormErrorTuple(CdbSreh *cdbsreh)
{
	bool		nulls[NUM_ERRORTABLE_ATTR];
	Datum		values[NUM_ERRORTABLE_ATTR];
	MemoryContext oldcontext;
					
	oldcontext = MemoryContextSwitchTo(cdbsreh->badrowcontext);
	
	/* Initialize all values for row to NULL */
	MemSet(values, 0, NUM_ERRORTABLE_ATTR * sizeof(Datum));
	MemSet(nulls, true, NUM_ERRORTABLE_ATTR * sizeof(bool));
	
	/* command start time */
	values[errtable_cmdtime - 1] = TimestampTzGetDatum(GetCurrentStatementStartTimestamp());
	nulls[errtable_cmdtime - 1] = false;
		
	/* line number */
	if (cdbsreh->linenumber > 0)
	{
		values[errtable_linenum - 1] = Int64GetDatum(cdbsreh->linenumber);
		nulls[errtable_linenum - 1] = false;
	}

	if(cdbsreh->is_server_enc)
	{
		/* raw data */
		values[errtable_rawdata - 1] = DirectFunctionCall1(textin, CStringGetDatum(cdbsreh->rawdata));
		nulls[errtable_rawdata - 1] = false;
	}
	else
	{
		/* raw bytes */
		PreprocessByteaData(cdbsreh->rawdata);
		values[errtable_rawbytes - 1] = DirectFunctionCall1(byteain, CStringGetDatum(cdbsreh->rawdata));
		nulls[errtable_rawbytes - 1] = false;
	}

	/* file name */
	values[errtable_filename - 1] = DirectFunctionCall1(textin, CStringGetDatum(cdbsreh->filename));
	nulls[errtable_filename - 1] = false;

	/* relation name */
	values[errtable_relname - 1] = DirectFunctionCall1(textin, CStringGetDatum(cdbsreh->relname));
	nulls[errtable_relname - 1] = false;
	
	/* error message */
	values[errtable_errmsg - 1] = DirectFunctionCall1(textin, CStringGetDatum(cdbsreh->errmsg));
	nulls[errtable_errmsg - 1] = false;
	
	
	MemoryContextSwitchTo(oldcontext);
	
	/*
	 * And now we can form the input tuple.
	 */
	if (RelationIsValid(cdbsreh->errtbl))
		return heap_form_tuple(RelationGetDescr(cdbsreh->errtbl), values, nulls);
	else
		return heap_form_tuple(GetErrorTupleDesc(), values, nulls);
}

/*
 * InsertIntoErrorTable
 *
 * Insert the information in cdbsreh into the error table we are using.
 * The destination is a regular heap table in a writer gang, and tuplestore
 * if it's a reader gang.  The tuplestore data will be redirected to
 * the writer gang in the same session later.
 * By design the error table rows are inserted in a frozen fashion.
 */
void
InsertIntoErrorTable(CdbSreh *cdbsreh)
{
	HeapTuple	tuple;

	tuple = FormErrorTuple(cdbsreh);

	/* store and freeze the tuple */
	frozen_heap_insert(cdbsreh->errtbl, tuple);

	heap_freetuple(tuple);
}


/* 
 * ReportSrehResults
 *
 * When necessary emit a NOTICE that describes the end result of the
 * SREH operations. Information includes the total number of rejected
 * rows, and whether rows were ignored or logged into an error table.
 */
void ReportSrehResults(CdbSreh *cdbsreh, int total_rejected)
{
	if(total_rejected > 0)
	{
		if(cdbsreh && cdbsreh->errtbl)
			ereport(NOTICE, 
					(errmsg("Found %d data formatting errors (%d or more "
							"input rows). Errors logged into error table \"%s\"", 
							total_rejected, total_rejected,
							RelationGetRelationName(cdbsreh->errtbl))));
		else
			ereport(NOTICE, 
					(errmsg("Found %d data formatting errors (%d or more "
							"input rows). Rejected related input data.", 
							total_rejected, total_rejected)));
	}
}


/*
 * SendNumRowsRejected
 *
 * Using this function the QE sends back to the client QD the number 
 * of rows that were rejected in this last data load in SREH mode.
 */
void SendNumRowsRejected(int numrejected)
{
	StringInfoData buf;
	
	if (Gp_role != GP_ROLE_EXECUTE)
		elog(FATAL, "SendNumRowsRejected: called outside of execute context.");

	pq_beginmessage(&buf, 'j'); /* 'j' is the msg code for rejected records */
	pq_sendint(&buf, numrejected, 4);
	pq_endmessage(&buf);	
}

/*
 * ValidateErrorTableMetaData
 *
 * This function gets called if a user wants an already existing table to be
 * used as an error table for some COPY or external table operation with SREH.
 * In here we verify that the metadata of the user selected table matches the
 * predefined requirement for an error table.
 */
void
ValidateErrorTableMetaData(Relation rel)
{
	TupleDesc   tupDesc = RelationGetDescr(rel);
	Form_pg_attribute *attr = tupDesc->attrs;
	TupleConstr *constr = tupDesc->constr;
	char		*relname = RelationGetRelationName(rel);
	int			attr_count = tupDesc->natts;

	/*
	 * Verify it is an ordinary table.
	 */
	if (rel->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("error table \"%s\" is not a table", relname)));

	/*
	 * Verify it is a heap table.
	 */
	if (!RelationIsHeap(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("error table \"%s\" is not a heap table", relname),
				 errhint("Use a relation with heap storage type for error table")));

	/*
	 * Verify number of attributes match
	 */
	if (attr_count != NUM_ERRORTABLE_ATTR)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" already exists and is not of a valid "
						"error table format (expected %d attributes, found %d)",
						relname, NUM_ERRORTABLE_ATTR, attr_count)));

	/*
	 * Verify this table is randomly-distributed.
	 */
	if (!GpPolicyIsRandomly(rel->rd_cdbpolicy))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("error table \"%s\" is not randomly distributed",
						relname),
				 errhint("Use a randomly-distributed realtion for error table")));

	/*
	 * Verify this table is not a partitioned or child partition table.
	 */
	if (rel_is_partitioned(RelationGetRelid(rel)) ||
		rel_is_child_partition(RelationGetRelid(rel)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("error table \"%s\" is a partitioned table",
						relname),
				 errdetail("Either root or child partition are not allowed as an error table")));

	/*
	 * Verify this table has no constraints.
	 *
	 * This errors out with DEFAULTs, CHECKs or NOT NULLs.
	 *
	 * Unique constraint is not allowed in randomly-distributed table
	 * so we don't check it here.
	 *
	 * We never insert NULL values in some attributes, but allowing
	 * NOT NULL for error table columns doesn't give much convenience
	 * to the user anyway.  Likewise, DEFAULT values are not of interest
	 * mostly, but it does not give much value to user, either.
	 */
	if (constr)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" already exists and is not of a valid "
						"error table format. If appears to have constraints "
						"defined.", relname)));

	/*
	 * run through each attribute at a time and verify it's what we expect
	 */
	VerifyErrorTableAttr(attr, errtable_cmdtime, "cmdtime", TIMESTAMPTZOID, relname);
	VerifyErrorTableAttr(attr, errtable_relname, "relname", TEXTOID, relname);
	VerifyErrorTableAttr(attr, errtable_filename, "filename", TEXTOID, relname);
	VerifyErrorTableAttr(attr, errtable_linenum, "linenum", INT4OID, relname);
	VerifyErrorTableAttr(attr, errtable_bytenum, "bytenum", INT4OID, relname);
	VerifyErrorTableAttr(attr, errtable_errmsg, "errmsg", TEXTOID, relname);
	VerifyErrorTableAttr(attr, errtable_rawdata, "rawdata", TEXTOID, relname);
	VerifyErrorTableAttr(attr, errtable_rawbytes, "rawbytes", BYTEAOID, relname);
}

/*
 * VerifyErrorTableAttr
 *
 * Called by ValidateErrorTableMetaData() on each table attribute to verify
 * that it has the right predefined name, type and other attr characteristics.
 */
static void
VerifyErrorTableAttr(Form_pg_attribute *attr,
					 int attrnum,
					 const char *expected_attname,
					 Oid expected_atttype,
					 char *relname)
{
	bool		cur_attisdropped = attr[attrnum - 1]->attisdropped;
	Name		cur_attname = &(attr[attrnum - 1]->attname);
	Oid			cur_atttype = attr[attrnum - 1]->atttypid;
	int4		cur_attndims = attr[attrnum - 1]->attndims;

	if(cur_attisdropped)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" includes dropped attributes and is "
						"therefore not of a valid error table format",
						relname)));

	if (namestrcmp(cur_attname, expected_attname) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" is an invalid error table. Expected "
						"attribute \"%s\" found \"%s\"",
						relname, expected_attname, NameStr(*cur_attname))));

	if(cur_atttype != expected_atttype)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" is an invalid error table. Wrong data "
						"type for attribute \"%s\"",
						relname, NameStr(*cur_attname))));

	if(cur_attndims > 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Relation \"%s\" is an invalid error table. Attribute "
						"\"%s\" has more than zero dimensions (array).",
						relname, NameStr(*cur_attname))));
}

/*
 * IsErrorTable
 *
 * Returns true if relid is used as an error table, which has dependent object
 * that is an external table.  Though it's not great we didn't have a clear
 * definition of Error Table, it satisfies the current requirements.
 */
bool
IsErrorTable(Relation rel)
{
	cqContext	   *pcqCtx, *pcqCtxExt, ctxExt;
	HeapTuple		tup;
	Relation		extrel;
	bool			result = false;

	/* fast path to quick check */
	if (!RelationIsHeap(rel))
		return false;

	/*
	 * Otherwise, go deeper and play with pg_depend...
	 */
	pcqCtx = caql_beginscan(NULL,
							cql("SELECT * FROM pg_depend "
								" WHERE refclassid = :1 "
								" AND refobjid = :2 "
								" AND refobjsubid = :3 ",
								ObjectIdGetDatum(RelationRelationId),
								ObjectIdGetDatum(RelationGetRelid(rel)),
								Int32GetDatum(0)));

	extrel = relation_open(ExtTableRelationId, AccessShareLock);
	pcqCtxExt = caql_addrel(cqclr(&ctxExt), extrel);

	while (HeapTupleIsValid(tup = caql_getnext(pcqCtx)))
	{
		Form_pg_depend dep = (Form_pg_depend) GETSTRUCT(tup);
		Oid				fmterrtbl;

		fmterrtbl = caql_getoid(pcqCtxExt,
								cql("SELECT fmterrtbl FROM pg_exttable "
									" WHERE reloid = :1",
									ObjectIdGetDatum(dep->objid)));
		if (RelationGetRelid(rel) == fmterrtbl)
		{
			result = true;
			break;
		}
	}

	relation_close(extrel, AccessShareLock);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * SetErrorTableVerdict
 *
 * Do we kill (DROP) the error table or let it live? This function is called 
 * at the end of execution of a COPY operation involving an error table. The
 * rule is - always keep it alive if KEEP was specified in the COPY command
 * otherwise DROP it only if no rows were rejected *and* if it was auto
 * generated at the start of this COPY command (did not exist before).
 */
void
SetErrorTableVerdict(CdbSreh *cdbsreh, int total_rejected)
{
	/* never let a QE decide whether to drop the table or not */
	Insist(Gp_role == GP_ROLE_DISPATCH);
	
	/* this is for COPY use only, don't call this function for external tables */
	Insist(cdbsreh->cdbcopy);
		   
	/* we can't get here if we don't have an open error table */
	Insist(cdbsreh->errtbl);
	
	if(!cdbsreh->is_keep && !cdbsreh->reusing_errtbl && total_rejected == 0)
	{
		cdbsreh->should_drop = true; /* mark this table to be dropped */
	}
}

/* Identify the reject limit type */
static RejectLimitCode
GetRejectLimitCode(CdbSreh *cdbsreh)
{
	RejectLimitCode code = REJECT_NONE;

	/* special case: check for the case that we are rejecting every single row */
	if (ExceedSegmentRejectHardLimit(cdbsreh))
		return REJECT_FIRST_BAD_LIMIT;

	/* special case: check for un-parsable csv format errors */
	if(CSV_IS_UNPARSABLE(cdbsreh))
		return REJECT_UNPARSABLE_CSV;

	/* now check if actual reject limit is reached */
	if(cdbsreh->is_limit_in_rows)
	{
		/* limit is in ROWS */
		if (cdbsreh->rejectcount >= cdbsreh->rejectlimit)
			code = REJECT_LIMIT_REACHED;
	}
	else
	{
		/* limit is in PERCENT */
		
		/* calculate the percent only if threshold is satisfied */
		if(cdbsreh->processed > gp_reject_percent_threshold)
		{
			if( (cdbsreh->rejectcount * 100) / cdbsreh->processed >= cdbsreh->rejectlimit)
				code = REJECT_LIMIT_REACHED;
		}
	}

	return code;
}

/*
 * Reports error if we reached segment reject limit. If non-NULL cdbCopy is passed,
 * it will call cdbCopyEnd to stop QE work before erroring out.
 * */
void
ErrorIfRejectLimitReached(CdbSreh *cdbsreh, CdbCopy *cdbCopy)
{
	RejectLimitCode		code;

	code = GetRejectLimitCode(cdbsreh);

	if (code == REJECT_NONE)
		return;

	/*
	 * Stop QE copy when we error out.
	 */
	if (cdbCopy)
		cdbCopyEnd(cdbCopy);

	switch (code)
	{
		case REJECT_FIRST_BAD_LIMIT:
			/* the special "first X rows are bad" case */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_GP_REJECT_LIMIT_REACHED),
					 errmsg("All %d first rows in this segment were rejected. "
							"Aborting operation regardless of REJECT LIMIT value. "
							"Last error was: %s",
							gp_initial_bad_row_limit, cdbsreh->errmsg)));
			break;
		case REJECT_UNPARSABLE_CSV:
			/* the special "csv un-parsable" case */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_GP_REJECT_LIMIT_REACHED),
					 errmsg("Input includes invalid CSV data that corrupts the "
							"ability to parse data rows. This usually means "
							"several unescaped embedded QUOTE characters. "
							"Data is not parsable. Last error was: %s",
							cdbsreh->errmsg)));
			break;
		case REJECT_LIMIT_REACHED:
			/* the normal case */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_GP_REJECT_LIMIT_REACHED),
					 errmsg("Segment reject limit reached. Aborting operation. "
							"Last error was: %s",
							cdbsreh->errmsg)));
			break;
		default:
			elog(ERROR, "unknown reject code %d", code);
	}
}

/*
 * Return true if the first bad rows exceed the hard limit.  We assume the
 * input is not well configured or similar case.  Stop the work regardless of
 * SEGMENT REJECT LIMIT value before exhausting disk space.  Setting the GUC
 * value to 0 indicates no hard limit.
 */
bool
ExceedSegmentRejectHardLimit(CdbSreh *cdbsreh)
{
	/* Keep going if the hard limit is "unlimited" */
	if (gp_initial_bad_row_limit == 0)
		return false;

	/* Stop if all the first bad rows reach to the hard limit. */
	if (cdbsreh->processed == gp_initial_bad_row_limit &&
		cdbsreh->rejectcount >= gp_initial_bad_row_limit)
		return true;

	return false;
}

/*
 * IsRejectLimitReached
 *
 * Returns true if seg reject limit reached, false otherwise.
 */
bool
IsRejectLimitReached(CdbSreh *cdbsreh)
{
	return GetRejectLimitCode(cdbsreh) != REJECT_NONE;
}

/*
 * emitSameTxnWarning()
 *
 * Warn the user that it's better to have the error table created in a
 * transaction that is older than the one that is using it. This mostly
 * can happen in COPY but also in some not so common external table use
 * cases (BEGIN, CREATE EXTERNAL TABLE with errtable, SELECT..., ...).
 */
void emitSameTxnWarning(void)
{	
	ereport(WARNING, 
			(errcode(ERRCODE_T_R_GP_ERROR_TABLE_MAY_DROP),
			 errmsg("The error table was created in the same "
					"transaction as this operation. It will get "
					"dropped if transaction rolls back even if bad "
					"rows are present"),
			 errhint("To avoid this create the error table ahead "
					"of time using: CREATE TABLE <name> (cmdtime "
					"timestamp with time zone, relname text, "
					"filename text, linenum integer, bytenum "
					"integer, errmsg text, rawdata text, rawbytes "
					"bytea)")));
}
/*
 * GetNextSegid
 *
 * Return the next sequential segment id of available segids (roundrobin).
 */
static
int GetNextSegid(CdbSreh *cdbsreh)
{
	int total_segs = cdbsreh->cdbcopy->total_segs;
	
	if(cdbsreh->lastsegid == total_segs)
		cdbsreh->lastsegid = 0; /* start over from first segid */
	
	return (cdbsreh->lastsegid++ % total_segs);
}


/*
 * This function is called when we are preparing to insert a bad row that
 * includes an encoding error into the bytea field of the error table 
 * (rawbytes). In rare occasions this bad row may also have an invalid bytea
 * sequence - a backslash not followed by a valid octal sequence - in which
 * case inserting into the error table will fail. In here we make a pass to
 * detect if there's a risk of failing. If there isn't we just return. If there
 * is we remove the backslash and replace it with a x20 char. Yes, we are
 * actually modifying the user data, but this is a much better opion than
 * failing the entire load. It's also a bad row - a row that will require user
 * intervention anyway in order to reload.
 *
 * reference: MPP-2107
 *
 * NOTE: code is copied from esc_dec_len() in encode.c and slightly modified.
 */
static
void PreprocessByteaData(char *src)
{
	const char *end = src + strlen(src);
	
	while (src < end)
	{
		if (src[0] != '\\')
			src++;
		else if (src + 3 < end &&
				 (src[1] >= '0' && src[1] <= '3') &&
				 (src[2] >= '0' && src[2] <= '7') &&
				 (src[3] >= '0' && src[3] <= '7'))
		{
			/*
			 * backslash + valid octal
			 */
			src += 4;
		}
		else if (src + 1 < end &&
				 (src[1] == '\\'))
		{
			/*
			 * two backslashes = backslash
			 */
			src += 2;
		}
		else
		{
			/*
			 * one backslash, not followed by ### valid octal. remove the
			 * backslash and put a x20 in its place.
			 */
			src[0] = ' ';
			src++;
		}		
	}
	
}

/*
 * IsRejectLimitValid
 * 
 * verify that the the reject limit specified by the user is within the
 * allowed values for ROWS or PERCENT.
 */
void VerifyRejectLimit(char rejectlimittype, int rejectlimit)
{
	if(rejectlimittype == 'r')
	{
		/* ROWS */
		if(rejectlimit < 2)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("Segment reject limit in ROWS "
							"must be 2 or larger (got %d)", rejectlimit)));
	}
	else
	{
		/* PERCENT */
		Assert(rejectlimittype == 'p');
		if (rejectlimit < 1 || rejectlimit > 100)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("Segment reject limit in PERCENT "
							"must be between 1 and 100 (got %d)", rejectlimit)));
	}

}

/*
 * Write into the error log file.  This opens the file every time,
 * so that we can keep it simple to deal with concurrent write.
 */
static void
ErrorLogWrite(CdbSreh *cdbsreh)
{
	HeapTuple	tuple;
	char		filename[MAXPGPATH];
	FILE	   *fp;
	pg_crc32	crc;

	Assert(OidIsValid(cdbsreh->relid));
	ErrorLogFileName(filename, MyDatabaseId, cdbsreh->relid);
	tuple = FormErrorTuple(cdbsreh);

	INIT_CRC32C(crc);
	COMP_CRC32C(crc, tuple->t_data, tuple->t_len);
	FIN_CRC32C(crc);

	LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
	fp = AllocateFile(filename, "a");
	if (!fp)
	{
		mkdir(ErrorLogDir, S_IRWXU);

		fp = AllocateFile(filename, "a");
	}
	if (!fp)
		ereport(ERROR,
				(errmsg("could not open \"%s\": %m", filename)));

	/*
	 * format:
	 *     0-4: length
	 *     5-8: crc
	 *     9-n: tuple data
	 */
	if (fwrite(&tuple->t_len, 1, sizeof(tuple->t_len), fp) != sizeof(tuple->t_len))
		elog(ERROR, "could not write tuple length: %m");
	if (fwrite(&crc, 1, sizeof(pg_crc32), fp) != sizeof(pg_crc32))
		elog(ERROR, "could not write checksum: %m");
	if (fwrite(tuple->t_data, 1, tuple->t_len, fp) != tuple->t_len)
		elog(ERROR, "could not write tuple data: %m");

	FreeFile(fp);
	LWLockRelease(ErrorLogLock);

	heap_freetuple(tuple);
}

/*
 * Read one tuple and checksum value from the error log file pointed by fp.
 * This returns NULL whenever we see unexpected read or EOF.
 */
static HeapTuple
ErrorLogRead(FILE *fp, pg_crc32 *crc)
{
	uint32		t_len;
	HeapTuple	tuple = NULL;

	LWLockAcquire(ErrorLogLock, LW_SHARED);

	do
	{
		if (fread(&t_len, 1, sizeof(uint32), fp) != sizeof(uint32))
			break;

		/*
		 * The tuple is "in-memory" format of HeapTuple.  Allocate
		 * the whole chunk consectively.
		 */
		tuple = palloc(HEAPTUPLESIZE + t_len);
		tuple->t_len = t_len;
		ItemPointerSetInvalid(&tuple->t_self);
		tuple->t_data = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);

		if (fread(crc, 1, sizeof(pg_crc32), fp) != sizeof(pg_crc32))
		{
			tuple = NULL;
			break;
		}

		if (fread(tuple->t_data, 1, tuple->t_len, fp) != tuple->t_len)
		{
			pfree(tuple);
			tuple = NULL;
			break;
		}
	} while(0);

	LWLockRelease(ErrorLogLock);

	return tuple;
}

/*
 * Utility to convert PGresult cell in text to Datum
 */
static Datum
ResultToDatum(PGresult *result, int row, AttrNumber attnum, PGFunction func, bool *isnull)
{
	if (PQgetisnull(result, row, attnum))
	{
		*isnull = true;
		return (Datum) 0;
	}
	else
	{
		*isnull = false;
		return DirectFunctionCall3(func,
				CStringGetDatum(PQgetvalue(result, row, attnum)),
				ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
	}
}

/*
 * gp_read_error_log
 *
 * Returns set of error log tuples.
 */
Datum
gp_read_error_log(PG_FUNCTION_ARGS)
{
	FuncCallContext	   *funcctx;
	ReadErrorLogContext *context;
	HeapTuple			tuple;
	Datum				result;

	/*
	 * First call setup
	 */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;
		FILE	   *fp;
		text	   *relname;

		funcctx = SRF_FIRSTCALL_INIT();

		relname = PG_GETARG_TEXT_P(0);
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		context = palloc0(sizeof(ReadErrorLogContext));
		funcctx->user_fctx = (void *) context;

		funcctx->tuple_desc = BlessTupleDesc(GetErrorTupleDesc());

		/*
		 * Though this function is usually executed on segment, we dispatch
		 * the execution if it happens to be on QD, and combine the results
		 * into one set.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			int		resultCount = 0;
			PGresult **results = NULL;
			StringInfoData sql;
			StringInfoData errbuf;
			int		i;

			initStringInfo(&sql);
			initStringInfo(&errbuf);

			/*
			 * construct SQL
			 */
			appendStringInfo(&sql,
					"SELECT * FROM pg_catalog.gp_read_error_log(%s) ",
							 quote_literal_internal(text_to_cstring(relname)));

			results = cdbdisp_dispatchRMCommand(sql.data, true, &errbuf,
												&resultCount);

			if (errbuf.len > 0)
				elog(ERROR, "%s", errbuf.data);
			Assert(resultCount > 0);

			for (i = 0; i < resultCount; i++)
			{
				if (PQresultStatus(results[i]) != PGRES_TUPLES_OK)
					elog(ERROR, "unexpected result from segment: %d",
								PQresultStatus(results[i]));
				context->numTuples += PQntuples(results[i]);
			}

			pfree(errbuf.data);
			pfree(sql.data);

			context->segResults = results;
			context->numSegResults = resultCount;
		}
		else
		{
			/*
			 * In QE, read the error log.
			 */
			RangeVar	   *relrv;
			Oid				relid;

			relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
			relid = RangeVarGetRelid(relrv, true);

			/*
			 * If the relation has gone, silently return no tuples.
			 */
			if (OidIsValid(relid))
			{
				AclResult aclresult;

				/*
				 * Requires SELECT priv to read error log.
				 */
				aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_SELECT);
				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, ACL_KIND_CLASS, relrv->relname);

				ErrorLogFileName(context->filename, MyDatabaseId, relid);
				fp = AllocateFile(context->filename, "r");
				context->fp = fp;
			}
		}

		MemoryContextSwitchTo(oldcontext);

		if (Gp_role != GP_ROLE_DISPATCH && !context->fp)
		{
			pfree(context);
			SRF_RETURN_DONE(funcctx);
		}
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (ReadErrorLogContext *) funcctx->user_fctx;

	/*
	 * Read error log, probably on segments.  We don't check Gp_role, however,
	 * in case master also wants to read the file.
	 */
	if (context->fp)
	{
		pg_crc32	crc, written_crc;
		tuple = ErrorLogRead(context->fp, &written_crc);

		/*
		 * CRC check.
		 */
		if (HeapTupleIsValid(tuple))
		{
			INIT_CRC32C(crc);
			COMP_CRC32C(crc, tuple->t_data, tuple->t_len);
			FIN_CRC32C(crc);

			// TODO: I'm pretty sure this should _not_ be using legacy crc32,
			// but crc32c since they are different. I found this while looking
			// through a diff but don't have means to test right now.
			if (!EQ_LEGACY_CRC32(crc, written_crc))
			{
				elog(LOG, "incorrect checksum in error log %s",
						  context->filename);
				tuple = NULL;
			}
		}

		/*
		 * If we found a valid tuple, return it.  Otherwise, fall through
		 * in the DONE routine.
		 */
		if (HeapTupleIsValid(tuple))
		{
			/*
			 * We need to set typmod for the executor to understand
			 * its type we just blessed.
			 */
			HeapTupleHeaderSetTypMod(tuple->t_data,
									 funcctx->tuple_desc->tdtypmod);

			result = HeapTupleGetDatum(tuple);
			SRF_RETURN_NEXT(funcctx, result);
		}
	}

	/*
	 * If we got results from dispatch, return all the tuples.
	 */
	while (context->currentResult < context->numSegResults)
	{
		Datum		values[NUM_ERRORTABLE_ATTR];
		bool		isnull[NUM_ERRORTABLE_ATTR];
		PGresult   *segres = context->segResults[context->currentResult];
		int			row = context->currentRow;

		if (row >= PQntuples(segres))
		{
			context->currentRow = 0;
			context->currentResult++;
			continue;
		}
		context->currentRow++;

		MemSet(isnull, false, sizeof(isnull));

		values[0] = ResultToDatum(segres, row, 0, timestamptz_in, &isnull[0]);
		values[1] = ResultToDatum(segres, row, 1, textin, &isnull[1]);
		values[2] = ResultToDatum(segres, row, 2, textin, &isnull[2]);
		values[3] = ResultToDatum(segres, row, 3, int4in, &isnull[3]);
		values[4] = ResultToDatum(segres, row, 4, int4in, &isnull[4]);
		values[5] = ResultToDatum(segres, row, 5, textin, &isnull[5]);
		values[6] = ResultToDatum(segres, row, 6, textin, &isnull[6]);
		values[7] = ResultToDatum(segres, row, 7, byteain, &isnull[7]);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, isnull);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}

	if (context->segResults != NULL)
	{
		int		i;

		for (i = 0; i < context->numSegResults; i++)
			PQclear(context->segResults[i]);

		/* XXX: better to copy to palloc'ed area */
		free(context->segResults[i]);
	}

	/*
	 * Close the file, if we have opened it.
	 */
	if (context->fp != NULL)
	{
		FreeFile(context->fp);
		context->fp = NULL;
	}

	SRF_RETURN_DONE(funcctx);
}

/*
 * Delete the error log of the relation and return true if any.
 * If relationId is InvalidOid, scan the directory to look for
 * all the files prefixed with the databaseId, and delete them.
 */
bool
ErrorLogDelete(Oid databaseId, Oid relationId)
{
	char		filename[MAXPGPATH];
	bool		result = true;

	if (!OidIsValid(relationId))
	{
		DIR	   *dir;
		struct dirent *de;
		char   *dirpath = ErrorLogDir;
		char	prefix[MAXPGPATH];

		if (OidIsValid(databaseId))
			snprintf(prefix, sizeof(prefix), "%u_", databaseId);
		dir = AllocateDir(dirpath);

		/*
		 * If we cannot open the directory, most likely it does not exist.
		 * Do nothing.
		 */
		if (dir == NULL)
			return false;

		while ((de = ReadDir(dir, dirpath)) != NULL)
		{
			if (strcmp(de->d_name, ".") == 0 ||
				strcmp(de->d_name, "..") == 0 ||
				strcmp(de->d_name, "/") == 0)
				continue;

			/*
			 * If database id is not given, delete all files.
			 */
			if (!OidIsValid(databaseId))
			{
				LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
				sprintf(filename, "%s/%s", dirpath, de->d_name);
				unlink(filename);
				LWLockRelease(ErrorLogLock);
				continue;
			}

			/*
			 * Filter by the database id prefix.
			 */
			if (strncmp(de->d_name, prefix, strlen(prefix)) == 0)
			{
				int		res;
				Oid		dummyDbId, relid;

				res = sscanf(de->d_name, "%u_%u", &dummyDbId, &relid);
				Assert(dummyDbId == databaseId);
				/*
				 * Recursively delete the file.
				 */
				if (res == 2)
				{
					ErrorLogDelete(databaseId, relid);
				}
			}
		}

		FreeDir(dir);
		return true;
	}
	LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
	ErrorLogFileName(filename, databaseId, relationId);
	if (unlink(filename) < 0)
		result = false;
	LWLockRelease(ErrorLogLock);

	return result;
}

/*
 * Delete error log of the specified relation.  This returns true from master
 * iif all segments and master find the relation.
 */
Datum
gp_truncate_error_log(PG_FUNCTION_ARGS)
{
	text	   *relname;
	char	   *relname_str;
	RangeVar	   *relrv;
	Oid				relid;
	bool		allResults = true;

	relname = PG_GETARG_TEXT_P(0);

	/*
	 * Dispatch the work to segments.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		int			i, resultCount = 0;
		StringInfoData	sql, errbuf;
		PGresult  **results;

		initStringInfo(&sql);
		initStringInfo(&errbuf);

		appendStringInfo(&sql,
						 "SELECT pg_catalog.gp_truncate_error_log('%s')",
						 text_to_cstring(relname));

		results = cdbdisp_dispatchRMCommand(sql.data, true, &errbuf,
											&resultCount);

		if (errbuf.len > 0)
			elog(ERROR, "%s", errbuf.data);
		Assert(resultCount > 0);

		for (i = 0; i < resultCount; i++)
		{
			Datum		value;
			bool		isnull;

			if (PQresultStatus(results[i]) != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errmsg("unexpected result from segment: %d",
								PQresultStatus(results[i]))));

			value = ResultToDatum(results[i], 0, 0, boolin, &isnull);
			allResults &= (!isnull && DatumGetBool(value));
			PQclear(results[i]);
		}

		pfree(errbuf.data);
		pfree(sql.data);
	}

	relname_str = text_to_cstring(relname);
	if (strcmp(relname_str, "*.*") == 0)
	{
		/*
		 * Only superuser is allowed to delete log files across database.
		 */
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					(errmsg("must be superuser to delete all error log files"))));

		ErrorLogDelete(InvalidOid, InvalidOid);
	}
	else if (strcmp(relname_str, "*") == 0)
	{
		/*
		 * Database owner can delete error log files.
		 */
		if (!pg_database_ownercheck(MyDatabaseId, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_DATABASE,
						   get_database_name(MyDatabaseId));

		ErrorLogDelete(MyDatabaseId, InvalidOid);
	}
	else
	{
		AclResult	aclresult;

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
		relid = RangeVarGetRelid(relrv, true);

		/* Return false if the relation does not exist. */
		if (!OidIsValid(relid))
			PG_RETURN_BOOL(false);

		/*
		 * Allow only the table owner to truncate error log.
		 */
		aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_TRUNCATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_CLASS, relrv->relname);

		/* We don't care if this fails or not. */
		ErrorLogDelete(MyDatabaseId, relid);
	}

	/* Return true iif all segments return true. */
	PG_RETURN_BOOL(allResults);
}
