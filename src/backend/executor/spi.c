/*-------------------------------------------------------------------------
 *
 * spi.c
 *				Server Programming Interface
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/spi.c,v 1.169 2007/01/09 22:00:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/printtup.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/catquery.h"
#include "catalog/heap.h"
#include "commands/trigger.h"
#include "executor/spi_priv.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/typcache.h"
#include "utils/resscheduler.h"

#include "gp-libpq-fe.h"
#include "libpq/libpq-be.h"
#include "gp-libpq-int.h"
#include "nodes/makefuncs.h"
#include "nodes/parsenodes.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbdtxcontextinfo.h"
#include "cdb/cdbdisp.h"
#include "miscadmin.h"
#include "commands/dbcommands.h"	/* get_database_name() */
#include "postmaster/postmaster.h"		/* PostPortNumber */
#include "postmaster/backoff.h"
#include "postmaster/autostats.h" /* auto_stats() */
#include "nodes/print.h"
#include "catalog/namespace.h"
#include "catalog/pg_namespace.h"
#include "executor/functions.h"
#include "cdb/memquota.h"
#include "executor/nodeFunctionscan.h"

extern char *savedSeqServerHost;
extern int savedSeqServerPort;

/*
 * Update the legacy 32-bit processed counter, but handle overflow.
 */
#define SET_SPI_PROCESSED	\
		if (SPI_processed64 > UINT_MAX) \
			SPI_processed = UINT_MAX; \
		else \
			SPI_processed = (uint32)SPI_processed64


uint64		SPI_processed64 = 0;
uint32		SPI_processed = 0;
Oid			SPI_lastoid = InvalidOid;
SPITupleTable *SPI_tuptable = NULL;
int			SPI_result;

static _SPI_connection *_SPI_stack = NULL;
static _SPI_connection *_SPI_current = NULL;
static int	_SPI_stack_depth = 0;		/* allocated size of _SPI_stack */
static int	_SPI_connected = -1;
static int	_SPI_curid = -1;

static Portal SPI_cursor_open_internal(const char *name, SPIPlanPtr plan,
						 Datum *Values, const char *Nulls,
						 bool read_only, int pflags);

static void _SPI_prepare_plan(const char *src, SPIPlanPtr plan,
				  ParamListInfo boundParams);

static int _SPI_execute_plan(SPIPlanPtr plan, ParamListInfo paramLI,
				  Snapshot snapshot, Snapshot crosscheck_snapshot,
				  bool read_only, bool fire_triggers, long tcount);

static ParamListInfo _SPI_convert_params(int nargs, Oid *argtypes,
					Datum *Values, const char *Nulls,
					int pflags);

static void _SPI_assign_query_mem(QueryDesc * queryDesc);

static int	_SPI_pquery(QueryDesc * queryDesc, bool fire_triggers, long tcount);

static void _SPI_error_callback(void *arg);

static void _SPI_cursor_operation(Portal portal, bool forward, long count,
					  DestReceiver *dest);

static SPIPlanPtr _SPI_copy_plan(SPIPlanPtr plan, int location);

static int	_SPI_begin_call(bool execmem);
static int	_SPI_end_call(bool procmem);
static MemoryContext _SPI_execmem(void);
static MemoryContext _SPI_procmem(void);
static bool _SPI_checktuples(void);

bool		gp_use_snapshop_during_callback = true;


/* =================== interface functions =================== */

int
SPI_connect(void)
{
	int			newdepth;

	/*
	 * When procedure called by Executor _SPI_curid expected to be equal to
	 * _SPI_connected
	 */
	if (_SPI_curid != _SPI_connected)
		return SPI_ERROR_CONNECT;

	if (_SPI_stack == NULL)
	{
		if (_SPI_connected != -1 || _SPI_stack_depth != 0)
			insist_log(false, "SPI stack corrupted");
		newdepth = 16;
		_SPI_stack = (_SPI_connection *)
			MemoryContextAlloc(TopTransactionContext,
							   newdepth * sizeof(_SPI_connection));
		_SPI_stack_depth = newdepth;
	}
	else
	{
		if (_SPI_stack_depth <= 0 || _SPI_stack_depth <= _SPI_connected)
			insist_log(false, "SPI stack corrupted");
		if (_SPI_stack_depth == _SPI_connected + 1)
		{
			newdepth = _SPI_stack_depth * 2;
			_SPI_stack = (_SPI_connection *)
				repalloc(_SPI_stack,
						 newdepth * sizeof(_SPI_connection));
			_SPI_stack_depth = newdepth;
		}
	}

	/*
	 * We're entering procedure where _SPI_curid == _SPI_connected - 1
	 */
	_SPI_connected++;
	Assert(_SPI_connected >= 0 && _SPI_connected < _SPI_stack_depth);

	_SPI_current = &(_SPI_stack[_SPI_connected]);
	_SPI_current->processed = 0;
	_SPI_current->lastoid = InvalidOid;
	_SPI_current->tuptable = NULL;
	_SPI_current->procCxt = NULL;		/* in case we fail to create 'em */
	_SPI_current->execCxt = NULL;
	_SPI_current->connectSubid = GetCurrentSubTransactionId();

	/*
	 * Create memory contexts for this procedure
	 *
	 * XXX it would be better to use PortalContext as the parent context, but
	 * we may not be inside a portal (consider deferred-trigger execution).
	 * Perhaps CurTransactionContext would do?	For now it doesn't matter
	 * because we clean up explicitly in AtEOSubXact_SPI().
	 */
	_SPI_current->procCxt = AllocSetContextCreate(TopTransactionContext,
												  "SPI Proc",
												  ALLOCSET_DEFAULT_MINSIZE,
												  ALLOCSET_DEFAULT_INITSIZE,
												  ALLOCSET_DEFAULT_MAXSIZE);
	_SPI_current->execCxt = AllocSetContextCreate(TopTransactionContext,
												  "SPI Exec",
												  ALLOCSET_DEFAULT_MINSIZE,
												  ALLOCSET_DEFAULT_INITSIZE,
												  ALLOCSET_DEFAULT_MAXSIZE);
	/* ... and switch to procedure's context */
	_SPI_current->savedcxt = MemoryContextSwitchTo(_SPI_current->procCxt);

	return SPI_OK_CONNECT;
}


/*
 * Note that we cannot free any connection back to the QD at SPI_finish time.
 * Our transaction may not be complete yet, so we don't yet know if the work
 * done on the QD should be committed or rolled back.
 */
int
SPI_finish(void)
{
	int			res;

	res = _SPI_begin_call(false);		/* live in procedure memory */
	if (res < 0)
		return res;

	/* Restore memory context as it was before procedure call */
	MemoryContextSwitchTo(_SPI_current->savedcxt);

	/* Release memory used in procedure call */
	MemoryContextDelete(_SPI_current->execCxt);
	_SPI_current->execCxt = NULL;
	MemoryContextDelete(_SPI_current->procCxt);
	_SPI_current->procCxt = NULL;

	/*
	 * Reset result variables, especially SPI_tuptable which is probably
	 * pointing at a just-deleted tuptable
	 */
	SPI_processed64 = 0;
	SPI_processed = 0;
	SPI_lastoid = InvalidOid;
	SPI_tuptable = NULL;

	/*
	 * After _SPI_begin_call _SPI_connected == _SPI_curid. Now we are closing
	 * connection to SPI and returning to upper Executor and so _SPI_connected
	 * must be equal to _SPI_curid.
	 */
	_SPI_connected--;
	_SPI_curid--;
	if (_SPI_connected == -1)
		_SPI_current = NULL;
	else
		_SPI_current = &(_SPI_stack[_SPI_connected]);

	return SPI_OK_FINISH;
}

/*
 * Clean up SPI state at transaction commit or abort.
 */
void
AtEOXact_SPI(bool isCommit)
{
	/*
	 * Note that memory contexts belonging to SPI stack entries will be freed
	 * automatically, so we can ignore them here.  We just need to restore our
	 * static variables to initial state.
	 */
	if (isCommit && _SPI_connected != -1)
		ereport(WARNING,
				(errcode(ERRCODE_WARNING),
				 errmsg("transaction left non-empty SPI stack"),
				 errhint("Check for missing \"SPI_finish\" calls.")));

	_SPI_current = _SPI_stack = NULL;
	_SPI_stack_depth = 0;
	_SPI_connected = _SPI_curid = -1;
	SPI_processed64 = 0;
	SPI_processed = 0;
	SPI_lastoid = InvalidOid;
	SPI_tuptable = NULL;
}

/*
 * Clean up SPI state at subtransaction commit or abort.
 *
 * During commit, there shouldn't be any unclosed entries remaining from
 * the current subtransaction; we emit a warning if any are found.
 */
void
AtEOSubXact_SPI(bool isCommit, SubTransactionId mySubid)
{
	bool		found = false;

	while (_SPI_connected >= 0)
	{
		_SPI_connection *connection = &(_SPI_stack[_SPI_connected]);

		if (connection->connectSubid != mySubid)
			break;				/* couldn't be any underneath it either */

		found = true;

		/*
		 * Release procedure memory explicitly (see note in SPI_connect)
		 */
		if (connection->execCxt)
		{
			MemoryContextDelete(connection->execCxt);
			connection->execCxt = NULL;
		}
		if (connection->procCxt)
		{
			MemoryContextDelete(connection->procCxt);
			connection->procCxt = NULL;
		}

		/*
		 * Pop the stack entry and reset global variables.	Unlike
		 * SPI_finish(), we don't risk switching to memory contexts that might
		 * be already gone.
		 */
		_SPI_connected--;
		_SPI_curid = _SPI_connected;
		if (_SPI_connected == -1)
			_SPI_current = NULL;
		else
			_SPI_current = &(_SPI_stack[_SPI_connected]);
		SPI_processed64 = 0;
		SPI_processed = 0;
		SPI_lastoid = InvalidOid;
		SPI_tuptable = NULL;
	}

	if (found && isCommit)
		ereport(WARNING,
				(errcode(ERRCODE_WARNING),
				 errmsg("subtransaction left non-empty SPI stack"),
				 errhint("Check for missing \"SPI_finish\" calls.")));

	/*
	 * If we are aborting a subtransaction and there is an open SPI context
	 * surrounding the subxact, clean up to prevent memory leakage.
	 */
	if (_SPI_current && !isCommit)
	{
		/* free Executor memory the same as _SPI_end_call would do */
		MemoryContextResetAndDeleteChildren(_SPI_current->execCxt);
		/* throw away any partially created tuple-table */
		SPI_freetuptable(_SPI_current->tuptable);
		_SPI_current->tuptable = NULL;
	}
}


/* Pushes SPI stack to allow recursive SPI calls */
void
SPI_push(void)
{
	_SPI_curid++;
}

/* Pops SPI stack to allow recursive SPI calls */
void
SPI_pop(void)
{
	_SPI_curid--;
}

/* Conditional push: push only if we're inside a SPI procedure */
bool
SPI_push_conditional(void)
{
	bool		pushed = (_SPI_curid != _SPI_connected);

	if (pushed)
	{
		_SPI_curid++;
		/* We should now be in a state where SPI_connect would succeed */
		Assert(_SPI_curid == _SPI_connected);
	}
	return pushed;
}

/* Conditional pop: pop only if SPI_push_conditional pushed */
void
SPI_pop_conditional(bool pushed)
{
	/* We should be in a state where SPI_connect would succeed */
	Assert(_SPI_curid == _SPI_connected);
	if (pushed)
		_SPI_curid--;
}

/* Restore state of SPI stack after aborting a subtransaction */
void
SPI_restore_connection(void)
{
	Assert(_SPI_connected >= 0);
	_SPI_curid = _SPI_connected - 1;
}

/* Parse, plan, and execute a query string */
int
SPI_execute(const char *src, bool read_only, long tcount)
{
	_SPI_plan	plan;
	int			res;

	if (src == NULL || tcount < 0)
		return SPI_ERROR_ARGUMENT;

	res = _SPI_begin_call(true);
	if (res < 0)
		return res;

	memset(&plan, 0, sizeof(_SPI_plan));
	plan.magic = _SPI_PLAN_MAGIC;
	plan.cursor_options = 0;
	plan.plancxt = NULL;		/* doesn't have own context */
	plan.query = src;
	plan.nargs = 0;
	plan.argtypes = NULL;
	plan.use_count = 0;

	_SPI_prepare_plan(src, &plan, NULL);

	res = _SPI_execute_plan(&plan, NULL,
							InvalidSnapshot, InvalidSnapshot,
							read_only, true, tcount);

	_SPI_end_call(true);
	return res;
}

/* Obsolete version of SPI_execute */
int
SPI_exec(const char *src, long tcount)
{
	return SPI_execute(src, false, tcount);
}

/* Execute a previously prepared plan */
int
SPI_execute_plan(SPIPlanPtr plan, Datum *Values, const char *Nulls,
				 bool read_only, long tcount)
{
	int			res;

	if (plan == NULL || tcount < 0)
		return SPI_ERROR_ARGUMENT;

	if (plan->nargs > 0 && Values == NULL)
		return SPI_ERROR_PARAM;

	res = _SPI_begin_call(true);
	if (res < 0)
		return res;

	res = _SPI_execute_plan(plan,
							_SPI_convert_params(plan->nargs, plan->argtypes,
												Values, Nulls,
												0),
							InvalidSnapshot, InvalidSnapshot,
							read_only, true, tcount);

	_SPI_end_call(true);
	return res;
}

/* Obsolete version of SPI_execute_plan */
int
SPI_execp(SPIPlanPtr plan, Datum *Values, const char *Nulls, long tcount)
{
	return SPI_execute_plan(plan, Values, Nulls, false, tcount);
}

/*
 * SPI_execute_snapshot -- identical to SPI_execute_plan, except that we allow
 * the caller to specify exactly which snapshots to use, which will be
 * registered here.  Also, the caller may specify that AFTER triggers should be
 * queued as part of the outer query rather than being fired immediately at the
 * end of the command.
 *
 * This is currently not documented in spi.sgml because it is only intended
 * for use by RI triggers.
 *
 * Passing snapshot == InvalidSnapshot will select the normal behavior of
 * fetching a new snapshot for each query.
 */
int
SPI_execute_snapshot(SPIPlanPtr plan,
					 Datum *Values, const char *Nulls,
					 Snapshot snapshot, Snapshot crosscheck_snapshot,
					 bool read_only, bool fire_triggers, long tcount)
{
	int			res;

	if (plan == NULL || tcount < 0)
		return SPI_ERROR_ARGUMENT;

	if (plan->nargs > 0 && Values == NULL)
		return SPI_ERROR_PARAM;

	res = _SPI_begin_call(true);
	if (res < 0)
		return res;

	res = _SPI_execute_plan(plan,
							_SPI_convert_params(plan->nargs, plan->argtypes,
												Values, Nulls,
												0),
							snapshot, crosscheck_snapshot,
							read_only, fire_triggers, tcount);

	_SPI_end_call(true);
	return res;
}

/*
 * SPI_execute_with_args -- plan and execute a query with supplied arguments
 *
 * This is functionally comparable to SPI_prepare followed by
 * SPI_execute_plan, except that since we know the plan will be used only
 * once, we can tell the planner to rely on the parameter values as constants.
 * This eliminates potential performance disadvantages compared to
 * inserting the parameter values directly into the query text.
 */
int
SPI_execute_with_args(const char *src,
					  int nargs, Oid *argtypes,
					  Datum *Values, const char *Nulls,
					  bool read_only, long tcount)
{
	int			res;
	_SPI_plan	plan;
	ParamListInfo paramLI;

	if (src == NULL || nargs < 0 || tcount < 0)
		return SPI_ERROR_ARGUMENT;

	if (nargs > 0 && (argtypes == NULL || Values == NULL))
		return SPI_ERROR_PARAM;

	res = _SPI_begin_call(true);
	if (res < 0)
		return res;

	memset(&plan, 0, sizeof(_SPI_plan));
	plan.magic = _SPI_PLAN_MAGIC;
	plan.cursor_options = 0;
	plan.nargs = nargs;
	plan.argtypes = argtypes;

	/*
	 * Add this to be compatible with current version of GPDB
	 *
	 * TODO: Remove it after the related codes are backported
	 *		 from upstream, e.g. plan.query is to be assigned
	 *		 in _SPI_prepare_plan
	 */
	plan.query = src;
	plan.use_count = 0;
	plan.plancxt = NULL;

	paramLI = _SPI_convert_params(nargs, argtypes,
								  Values, Nulls,
								  PARAM_FLAG_CONST);

	_SPI_prepare_plan(src, &plan, paramLI);

	/* We don't need to copy the plan since it will be thrown away anyway */

	res = _SPI_execute_plan(&plan, paramLI,
							InvalidSnapshot, InvalidSnapshot,
							read_only, true, tcount);

	_SPI_end_call(true);
	return res;
}

SPIPlanPtr
SPI_prepare(const char *src, int nargs, Oid *argtypes)
{
	_SPI_plan	plan;
	_SPI_plan  *result;

	if (src == NULL || nargs < 0 || (nargs > 0 && argtypes == NULL))
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return NULL;
	}

	SPI_result = _SPI_begin_call(true);
	if (SPI_result < 0)
		return NULL;

	memset(&plan, 0, sizeof(_SPI_plan));
	plan.magic = _SPI_PLAN_MAGIC;
	//plan.cursor_options = cursorOptions;
	plan.plancxt = NULL;		/* doesn't have own context */
	plan.query = src;
	plan.nargs = nargs;
	plan.argtypes = argtypes;
	plan.use_count = 0;

	_SPI_prepare_plan(src, &plan, NULL);

	/* copy plan to procedure context */
	result = _SPI_copy_plan(&plan, _SPI_CPLAN_PROCXT);

	_SPI_end_call(true);

	return result;
}

SPIPlanPtr
SPI_saveplan(SPIPlanPtr plan)
{
	SPIPlanPtr	newplan;

	if (plan == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return NULL;
	}

	SPI_result = _SPI_begin_call(false);		/* don't change context */
	if (SPI_result < 0)
		return NULL;

	newplan = _SPI_copy_plan(plan, _SPI_CPLAN_TOPCXT);

	_SPI_curid--;
	SPI_result = 0;

	return newplan;
}

int
SPI_freeplan(SPIPlanPtr plan)
{
	if (plan == NULL)
		return SPI_ERROR_ARGUMENT;

	MemoryContextDelete(plan->plancxt);
	return 0;
}

HeapTuple
SPI_copytuple(HeapTuple tuple)
{
	MemoryContext oldcxt = NULL;
	HeapTuple	ctuple;

	if (tuple == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return NULL;
	}

	if (_SPI_curid + 1 == _SPI_connected)		/* connected */
	{
		if (_SPI_current != &(_SPI_stack[_SPI_curid + 1]))
			insist_log(false, "SPI stack corrupted");
		oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);
	}

	ctuple = heap_copytuple(tuple);

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	return ctuple;
}

HeapTupleHeader
SPI_returntuple(HeapTuple tuple, TupleDesc tupdesc)
{
	MemoryContext oldcxt = NULL;
	HeapTupleHeader dtup;

	if (tuple == NULL || tupdesc == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return NULL;
	}

	/* For RECORD results, make sure a typmod has been assigned */
	if (tupdesc->tdtypeid == RECORDOID &&
		tupdesc->tdtypmod < 0)
		assign_record_type_typmod(tupdesc);

	if (_SPI_curid + 1 == _SPI_connected)		/* connected */
	{
		if (_SPI_current != &(_SPI_stack[_SPI_curid + 1]))
			insist_log(false, "SPI stack corrupted");
		oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);
	}

	dtup = (HeapTupleHeader) palloc(tuple->t_len);
	memcpy((char *) dtup, (char *) tuple->t_data, tuple->t_len);

	HeapTupleHeaderSetDatumLength(dtup, tuple->t_len);
	HeapTupleHeaderSetTypeId(dtup, tupdesc->tdtypeid);
	HeapTupleHeaderSetTypMod(dtup, tupdesc->tdtypmod);

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	return dtup;
}

HeapTuple
SPI_modifytuple(Relation rel, HeapTuple tuple, int natts, int *attnum,
				Datum *Values, const char *Nulls)
{
	MemoryContext oldcxt = NULL;
	HeapTuple	mtuple;
	int			numberOfAttributes;
	Datum	   *v;
	bool	   *n;
	int			i;

	if (rel == NULL || tuple == NULL || natts < 0 || attnum == NULL || Values == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return NULL;
	}

	if (_SPI_curid + 1 == _SPI_connected)		/* connected */
	{
		if (_SPI_current != &(_SPI_stack[_SPI_curid + 1]))
			insist_log(false, "SPI stack corrupted");
		oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);
	}
	SPI_result = 0;
	numberOfAttributes = rel->rd_att->natts;
	v = (Datum *) palloc(numberOfAttributes * sizeof(Datum));
	n = (bool *) palloc(numberOfAttributes * sizeof(bool));

	/* fetch old values and nulls */
	heap_deform_tuple(tuple, rel->rd_att, v, n);

	/* replace values and nulls */
	for (i = 0; i < natts; i++)
	{
		if (attnum[i] <= 0 || attnum[i] > numberOfAttributes)
			break;
		v[attnum[i] - 1] = Values[i];
		n[attnum[i] - 1] = (Nulls && Nulls[i] == 'n');
	}

	if (i == natts)				/* no errors in *attnum */
	{
		mtuple = heap_form_tuple(rel->rd_att, v, n);

		/*
		 * copy the identification info of the old tuple: t_ctid, t_self, and
		 * OID (if any)
		 */
		mtuple->t_data->t_ctid = tuple->t_data->t_ctid;
		mtuple->t_self = tuple->t_self;
		if (rel->rd_att->tdhasoid)
			HeapTupleSetOid(mtuple, HeapTupleGetOid(tuple));
	}
	else
	{
		mtuple = NULL;
		SPI_result = SPI_ERROR_NOATTRIBUTE;
	}

	pfree(v);
	pfree(n);

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	return mtuple;
}

int
SPI_fnumber(TupleDesc tupdesc, const char *fname)
{
	int			res;
	Form_pg_attribute sysatt;

	for (res = 0; res < tupdesc->natts; res++)
	{
		if (namestrcmp(&tupdesc->attrs[res]->attname, fname) == 0)
			return res + 1;
	}

	sysatt = SystemAttributeByName(fname, true /* "oid" will be accepted */ );
	if (sysatt != NULL)
		return sysatt->attnum;

	/* SPI_ERROR_NOATTRIBUTE is different from all sys column numbers */
	return SPI_ERROR_NOATTRIBUTE;
}

char *
SPI_fname(TupleDesc tupdesc, int fnumber)
{
	Form_pg_attribute att;

	SPI_result = 0;

	if (fnumber > tupdesc->natts || fnumber == 0 ||
		fnumber <= FirstLowInvalidHeapAttributeNumber)
	{
		SPI_result = SPI_ERROR_NOATTRIBUTE;
		return NULL;
	}

	if (fnumber > 0)
		att = tupdesc->attrs[fnumber - 1];
	else
		att = SystemAttributeDefinition(fnumber, true);

	return pstrdup(NameStr(att->attname));
}

char *
SPI_getvalue(HeapTuple tuple, TupleDesc tupdesc, int fnumber)
{
	char	   *result;
	Datum		origval,
				val;
	bool		isnull;
	Oid			typoid,
				foutoid;
	bool		typisvarlena;

	SPI_result = 0;

	if (fnumber > HeapTupleHeaderGetNatts(tuple->t_data) || fnumber == 0 ||
		fnumber <= FirstLowInvalidHeapAttributeNumber)
	{
		SPI_result = SPI_ERROR_NOATTRIBUTE;
		return NULL;
	}

	origval = heap_getattr(tuple, fnumber, tupdesc, &isnull);
	if (isnull)
		return NULL;

	if (fnumber > 0)
		typoid = tupdesc->attrs[fnumber - 1]->atttypid;
	else
		typoid = (SystemAttributeDefinition(fnumber, true))->atttypid;

	getTypeOutputInfo(typoid, &foutoid, &typisvarlena);

	/*
	 * If we have a toasted datum, forcibly detoast it here to avoid memory
	 * leakage inside the type's output routine.
	 */
	if (typisvarlena)
		val = PointerGetDatum(PG_DETOAST_DATUM(origval));
	else
		val = origval;

	result = OidOutputFunctionCall(foutoid, val);

	/* Clean up detoasted copy, if any */
	if (val != origval)
		pfree(DatumGetPointer(val));

	return result;
}

Datum
SPI_getbinval(HeapTuple tuple, TupleDesc tupdesc, int fnumber, bool *isnull)
{
	SPI_result = 0;

	if (fnumber > HeapTupleHeaderGetNatts(tuple->t_data) || fnumber == 0 ||
		fnumber <= FirstLowInvalidHeapAttributeNumber)
	{
		SPI_result = SPI_ERROR_NOATTRIBUTE;
		*isnull = true;
		return (Datum) 0;
	}

	return heap_getattr(tuple, fnumber, tupdesc, isnull);
}

char *
SPI_gettype(TupleDesc tupdesc, int fnumber)
{
	Oid			typoid;
	int			fetchCount;
	char	   *result;

	SPI_result = 0;

	if (fnumber > tupdesc->natts || fnumber == 0 ||
		fnumber <= FirstLowInvalidHeapAttributeNumber)
	{
		SPI_result = SPI_ERROR_NOATTRIBUTE;
		return NULL;
	}

	if (fnumber > 0)
		typoid = tupdesc->attrs[fnumber - 1]->atttypid;
	else
		typoid = (SystemAttributeDefinition(fnumber, true))->atttypid;

	result = caql_getcstring_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT typname FROM pg_type "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(typoid)));

	if (!fetchCount)
	{
		SPI_result = SPI_ERROR_TYPUNKNOWN;
		return NULL;
	}

	return result;
}

Oid
SPI_gettypeid(TupleDesc tupdesc, int fnumber)
{
	SPI_result = 0;

	if (fnumber > tupdesc->natts || fnumber == 0 ||
		fnumber <= FirstLowInvalidHeapAttributeNumber)
	{
		SPI_result = SPI_ERROR_NOATTRIBUTE;
		return InvalidOid;
	}

	if (fnumber > 0)
		return tupdesc->attrs[fnumber - 1]->atttypid;
	else
		return (SystemAttributeDefinition(fnumber, true))->atttypid;
}

char *
SPI_getrelname(Relation rel)
{
	return pstrdup(RelationGetRelationName(rel));
}

char *
SPI_getnspname(Relation rel)
{
	return get_namespace_name(RelationGetNamespace(rel));
}

void *
SPI_palloc(Size size)
{
	MemoryContext oldcxt = NULL;
	void	   *pointer;

	if (_SPI_curid + 1 == _SPI_connected)		/* connected */
	{
		if (_SPI_current != &(_SPI_stack[_SPI_curid + 1]))
			insist_log(false, "SPI stack corrupted");
		oldcxt = MemoryContextSwitchTo(_SPI_current->savedcxt);
	}

	pointer = palloc(size);

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	return pointer;
}

void *
SPI_repalloc(void *pointer, Size size)
{
	/* No longer need to worry which context chunk was in... */
	return repalloc(pointer, size);
}

void
SPI_pfree(void *pointer)
{
	/* No longer need to worry which context chunk was in... */
	pfree(pointer);
}

void
SPI_freetuple(HeapTuple tuple)
{
	/* No longer need to worry which context tuple was in... */
	heap_freetuple(tuple);
}

void
SPI_freetuptable(SPITupleTable *tuptable)
{
	if (tuptable != NULL)
		MemoryContextDelete(tuptable->tuptabcxt);
}


/*
 * SPI_cursor_open()
 *
 *	Open a prepared SPI plan as a portal
 */
Portal
SPI_cursor_open(const char *name, SPIPlanPtr plan,
				Datum *Values, const char *Nulls,
				bool read_only)
{
	return SPI_cursor_open_internal(name, plan, Values, Nulls,
									read_only, 0);
}


/*
 * SPI_cursor_open_with_args()
 *
 * Parse and plan a query and open it as a portal.  Like SPI_execute_with_args,
 * we can tell the planner to rely on the parameter values as constants,
 * because the plan will only be used once.
 */
Portal
SPI_cursor_open_with_args(const char *name,
						  const char *src,
						  int nargs, Oid *argtypes,
						  Datum *Values, const char *Nulls,
						  bool read_only, int cursorOptions)
{
	Portal		result;
	_SPI_plan	plan;
	ParamListInfo paramLI;

	if (src == NULL || nargs < 0)
		elog(ERROR, "SPI_cursor_open_with_args called with invalid arguments");

	if (nargs > 0 && (argtypes == NULL || Values == NULL))
		elog(ERROR, "SPI_cursor_open_with_args called with missing parameters");

	SPI_result = _SPI_begin_call(true);
	if (SPI_result < 0)
		elog(ERROR, "SPI_cursor_open_with_args called while not connected");

	memset(&plan, 0, sizeof(_SPI_plan));
	plan.magic = _SPI_PLAN_MAGIC;
	plan.cursor_options = cursorOptions;
	plan.nargs = nargs;
	plan.argtypes = argtypes;

	/*
	 * Add this to be compatible with current version of GPDB
	 *
	 * TODO: Remove it after the related codes are backported
	 *		 from upstream, e.g. plan.query is to be assigned
	 *		 in _SPI_prepare_plan
	 */
	plan.query = src; 
	plan.use_count = 0;
	plan.plancxt = NULL;

	paramLI = _SPI_convert_params(nargs, argtypes,
								  Values, Nulls,
								  PARAM_FLAG_CONST);

	_SPI_prepare_plan(src, &plan, paramLI);

	/* We needn't copy the plan; SPI_cursor_open_internal will do so */

	/* Adjust stack so that SPI_cursor_open_internal doesn't complain */
	_SPI_curid--;

	/* SPI_cursor_open_internal expects to be called in procedure memory context */
	_SPI_procmem();

	result = SPI_cursor_open_internal(name, &plan, Values, Nulls,
									  read_only, PARAM_FLAG_CONST);

	/* And clean up */
	_SPI_curid++;
	_SPI_end_call(true);

	return result;
}

static Portal
SPI_cursor_open_internal(const char *name, SPIPlanPtr plan,
						 Datum *Values, const char *Nulls,
						 bool read_only, int pflags)
{

	_SPI_plan  *spiplan = (_SPI_plan *) plan;
	List	   *qtlist;
	List	   *ptlist;
	ParamListInfo paramLI;
	Snapshot	snapshot;
	MemoryContext oldcontext;
	Portal		portal;
	int			k;

	elog(DEBUG1, "SPI_cursor_open local: %s", spiplan->query);


	/*
	 * Check that the plan is something the Portal code will special-case as
	 * returning one tupleset.
	 */
	if (!SPI_is_cursor_plan(spiplan))
	{
		/* try to give a good error message */
		Query	   *queryTree;

		if (list_length(spiplan->qtlist) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
					 errmsg("cannot open multi-query plan as cursor")));
		queryTree = (Query *) PortalListGetPrimaryStmt((List *) linitial(spiplan->qtlist));
		if (queryTree == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
					 errmsg("cannot open empty query as cursor")));
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
		/* translator: %s is name of a SQL command, eg INSERT */
				 errmsg("cannot open %s query as cursor",
						CreateCommandTag((Node*)queryTree))));
	}

	Assert(list_length(spiplan->qtlist) == 1);
	qtlist = (List *) linitial(spiplan->qtlist);
	ptlist = spiplan->ptlist;
	if (list_length(qtlist) != list_length(ptlist))
		insist_log(false, "corrupted SPI plan lists");

	/*
	 * If told to be read-only, we'd better check for read-only queries.
	 */
	if (read_only)
	{
		ListCell   *lc;

		foreach(lc, qtlist)
		{
			Query  *qry = (Query *) lfirst(lc);

			if (!QueryIsReadOnly(qry))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 /* translator: %s is a SQL statement name */
						 errmsg("%s is not allowed in a non-volatile function",
								CreateCommandTag((Node*)qry))));
		}
	}

	/* Reset SPI result (note we deliberately don't touch lastoid) */
	SPI_processed64 = 0;
	SPI_processed = 0;
	SPI_tuptable = NULL;
	_SPI_current->processed = 0;
	_SPI_current->tuptable = NULL;

	/* Create the portal */
	if (name == NULL || name[0] == '\0')
	{
		/* Use a random nonconflicting name */
		portal = CreateNewPortal();
	}
	else
	{
		/* In this path, error if portal of same name already exists */
		portal = CreatePortal(name, false, false);
	}

	/* Switch to portal's memory and copy the parsetrees and plans to there */
	oldcontext = MemoryContextSwitchTo(PortalGetHeapMemory(portal));
	qtlist = copyObject(qtlist);
	ptlist = copyObject(ptlist);

	/* If the plan has parameters, set them up */
	if (spiplan->nargs > 0)
	{
		/* sizeof(ParamListInfoData) includes the first array element */
		paramLI = (ParamListInfo) palloc(sizeof(ParamListInfoData) +
							  (spiplan->nargs - 1) *sizeof(ParamExternData));
		paramLI->numParams = spiplan->nargs;

		for (k = 0; k < spiplan->nargs; k++)
		{
			ParamExternData *prm = &paramLI->params[k];

			prm->ptype = spiplan->argtypes[k];
			prm->pflags = pflags;
			prm->isnull = (Nulls && Nulls[k] == 'n');
			if (prm->isnull)
			{
				/* nulls just copy */
				prm->value = Values[k];
			}
			else
			{
				/* pass-by-ref values must be copied into portal context */
				int16		paramTypLen;
				bool		paramTypByVal;

				get_typlenbyval(prm->ptype, &paramTypLen, &paramTypByVal);
				prm->value = datumCopy(Values[k],
									   paramTypByVal, paramTypLen);
			}
		}
	}
	else
		paramLI = NULL;

	/* Copy the plan's query string into the portal */
	char *query_string = pstrdup(spiplan->query);

	/*
	 * Set up the portal.
	 */
	PortalDefineQuery(portal,
					  NULL,		/* no statement name */
					  query_string,
					  T_SelectStmt,
					  CreateCommandTag(PortalListGetPrimaryStmt(qtlist)),
					  ptlist,
					  PortalGetHeapMemory(portal));

	MemoryContextSwitchTo(oldcontext);

	/*
	 * Set up options for portal.
	 */
	portal->cursorOptions &= ~(CURSOR_OPT_SCROLL | CURSOR_OPT_NO_SCROLL);
	{
		int option = CURSOR_OPT_NO_SCROLL;
		
		if ( list_length(ptlist) == 1 )
		{
			Node *node = (Node *)linitial(ptlist);

			/*
			 * For utility statements such as EXPLAIN, we may not have
			 * a valid PlannedStmt pointer. Therefore, make sure that
			 * the cast to PlannedStmt* is valid (MPP-22435).
			 */
			if (nodeTag(node) == T_PlannedStmt)
			{
				PlannedStmt *stmt = (PlannedStmt *)node;
				if ( stmt && stmt->planTree &&
					ExecSupportsBackwardScan(stmt->planTree) )
				{
					option = CURSOR_OPT_SCROLL;
				}
			}
		}
		portal->cursorOptions |= option;
	}

	/*
	 * Greenplum Database needs this
	 */
	portal->is_extended_query = true;

	/*
	 * Set up the snapshot to use.	(PortalStart will do CopySnapshot, so we
	 * skip that here.)
	 */
	if (read_only)
		snapshot = ActiveSnapshot;
	else
	{
		CommandCounterIncrement();
		snapshot = GetTransactionSnapshot();
	}

	/*
	 * Start portal execution.
	 */
	PortalStart(portal, paramLI, snapshot,
				savedSeqServerHost, savedSeqServerPort);

	Assert(portal->strategy != PORTAL_MULTI_QUERY);

	/* Return the created portal */
	return portal;
}

/*
 * SPI_cursor_find()
 *
 *	Find the portal of an existing open cursor
 */
Portal
SPI_cursor_find(const char *name)
{
	return GetPortalByName(name);
}


/*
 * SPI_cursor_fetch()
 *
 *	Fetch rows in a cursor
 */
void
SPI_cursor_fetch(Portal portal, bool forward, long count)
{
	_SPI_cursor_operation(portal, forward, count,
						  CreateDestReceiver(DestSPI, NULL));
	/* we know that the DestSPI receiver doesn't need a destroy call */
}


/*
 * SPI_cursor_move()
 *
 *	Move in a cursor
 */
void
SPI_cursor_move(Portal portal, bool forward, long count)
{
	_SPI_cursor_operation(portal, forward, count, None_Receiver);
}


/*
 * SPI_cursor_close()
 *
 *	Close a cursor
 */
void
SPI_cursor_close(Portal portal)
{
	insist_log(PortalIsValid(portal), "invalid portal in SPI cursor operation");

	PortalDrop(portal, false);
}

/*
 * Returns the Oid representing the type id for argument at argIndex. First
 * parameter is at index zero.
 */
Oid
SPI_getargtypeid(SPIPlanPtr plan, int argIndex)
{
	if (plan == NULL || argIndex < 0 || argIndex >= ((_SPI_plan *) plan)->nargs)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return InvalidOid;
	}
	return plan->argtypes[argIndex];
}

/*
 * Returns the number of arguments for the prepared plan.
 */
int
SPI_getargcount(SPIPlanPtr plan)
{
	if (plan == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return -1;
	}
	return plan->nargs;
}

/*
 * Returns true if the plan contains exactly one command
 * and that command returns tuples to the caller (eg, SELECT or
 * INSERT ... RETURNING, but not SELECT ... INTO). In essence,
 * the result indicates if the command can be used with SPI_cursor_open
 *
 * Parameters
 *	  plan: A plan previously prepared using SPI_prepare
 */
bool
SPI_is_cursor_plan(SPIPlanPtr plan)
{
	_SPI_plan  *spiplan = (_SPI_plan *) plan;

	if (spiplan == NULL)
	{
		SPI_result = SPI_ERROR_ARGUMENT;
		return false;
	}

	if (list_length(spiplan->qtlist) != 1)
		return false;			/* not exactly 1 pre-rewrite command */

	switch (ChoosePortalStrategy((List *) linitial(spiplan->qtlist)))
	{
		case PORTAL_ONE_SELECT:
		case PORTAL_ONE_RETURNING:
		case PORTAL_UTIL_SELECT:
			/* OK */
			return true;

		case PORTAL_MULTI_QUERY:
			/* will not return tuples */
			break;
	}
	return false;
}

/*
 * SPI_result_code_string --- convert any SPI return code to a string
 *
 * This is often useful in error messages.	Most callers will probably
 * only pass negative (error-case) codes, but for generality we recognize
 * the success codes too.
 */
const char *
SPI_result_code_string(int code)
{
	static char buf[64];

	switch (code)
	{
		case SPI_ERROR_CONNECT:
			return "SPI_ERROR_CONNECT";
		case SPI_ERROR_COPY:
			return "SPI_ERROR_COPY";
		case SPI_ERROR_OPUNKNOWN:
			return "SPI_ERROR_OPUNKNOWN";
		case SPI_ERROR_UNCONNECTED:
			return "SPI_ERROR_UNCONNECTED";
		case SPI_ERROR_CURSOR:
			return "SPI_ERROR_CURSOR";
		case SPI_ERROR_ARGUMENT:
			return "SPI_ERROR_ARGUMENT";
		case SPI_ERROR_PARAM:
			return "SPI_ERROR_PARAM";
		case SPI_ERROR_TRANSACTION:
			return "SPI_ERROR_TRANSACTION";
		case SPI_ERROR_NOATTRIBUTE:
			return "SPI_ERROR_NOATTRIBUTE";
		case SPI_ERROR_NOOUTFUNC:
			return "SPI_ERROR_NOOUTFUNC";
		case SPI_ERROR_TYPUNKNOWN:
			return "SPI_ERROR_TYPUNKNOWN";
		case SPI_OK_CONNECT:
			return "SPI_OK_CONNECT";
		case SPI_OK_FINISH:
			return "SPI_OK_FINISH";
		case SPI_OK_FETCH:
			return "SPI_OK_FETCH";
		case SPI_OK_UTILITY:
			return "SPI_OK_UTILITY";
		case SPI_OK_SELECT:
			return "SPI_OK_SELECT";
		case SPI_OK_SELINTO:
			return "SPI_OK_SELINTO";
		case SPI_OK_INSERT:
			return "SPI_OK_INSERT";
		case SPI_OK_DELETE:
			return "SPI_OK_DELETE";
		case SPI_OK_UPDATE:
			return "SPI_OK_UPDATE";
		case SPI_OK_CURSOR:
			return "SPI_OK_CURSOR";
		case SPI_OK_INSERT_RETURNING:
			return "SPI_OK_INSERT_RETURNING";
		case SPI_OK_DELETE_RETURNING:
			return "SPI_OK_DELETE_RETURNING";
		case SPI_OK_UPDATE_RETURNING:
			return "SPI_OK_UPDATE_RETURNING";
	}
	/* Unrecognized code ... return something useful ... */
	sprintf(buf, "Unrecognized SPI code %d", code);
	return buf;
}

/* =================== private functions =================== */

/*
 * spi_dest_startup
 *		Initialize to receive tuples from Executor into SPITupleTable
 *		of current SPI procedure
 */
void
spi_dest_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
	SPITupleTable *tuptable;
	MemoryContext oldcxt;
	MemoryContext tuptabcxt;

	/*
	 * When called by Executor _SPI_curid expected to be equal to
	 * _SPI_connected
	 */
	if (_SPI_curid != _SPI_connected || _SPI_connected < 0)
		insist_log(false, "improper call to spi_dest_startup");
	if (_SPI_current != &(_SPI_stack[_SPI_curid]))
		insist_log(false, "SPI stack corrupted");

	if (_SPI_current->tuptable != NULL)
		insist_log(false, "improper call to spi_dest_startup");

	oldcxt = _SPI_procmem();	/* switch to procedure memory context */

	tuptabcxt = AllocSetContextCreate(CurrentMemoryContext,
									  "SPI TupTable",
									  ALLOCSET_DEFAULT_MINSIZE,
									  ALLOCSET_DEFAULT_INITSIZE,
									  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(tuptabcxt);

	_SPI_current->tuptable = tuptable = (SPITupleTable *)
		palloc(sizeof(SPITupleTable));
	tuptable->tuptabcxt = tuptabcxt;
	tuptable->alloced = tuptable->free = 128;
	tuptable->vals = (HeapTuple *) palloc(tuptable->alloced * sizeof(HeapTuple));
	tuptable->tupdesc = CreateTupleDescCopy(typeinfo);

	MemoryContextSwitchTo(oldcxt);
}

/*
 * spi_printtup
 *		store tuple retrieved by Executor into SPITupleTable
 *		of current SPI procedure
 */
void
spi_printtup(TupleTableSlot *slot, DestReceiver *self)
{
	SPITupleTable *tuptable;
	MemoryContext oldcxt;

	/*
	 * When called by Executor _SPI_curid expected to be equal to
	 * _SPI_connected
	 */
	if (_SPI_curid != _SPI_connected || _SPI_connected < 0)
		insist_log(false, "improper call to spi_printtup");
	if (_SPI_current != &(_SPI_stack[_SPI_curid]))
		insist_log(false, "SPI stack corrupted");

	tuptable = _SPI_current->tuptable;
	if (tuptable == NULL)
		insist_log(false, "improper call to spi_printtup");

	oldcxt = MemoryContextSwitchTo(tuptable->tuptabcxt);

	if (tuptable->free == 0)
	{
		tuptable->free = 256;
		tuptable->alloced += tuptable->free;
		tuptable->vals = (HeapTuple *) repalloc(tuptable->vals,
									  tuptable->alloced * sizeof(HeapTuple));
	}

	/*
	 * XXX TODO: This is extremely stupid.	Most likely we only need a
	 * memtuple. However, TONS of places, assumes heaptuple.
	 *
	 * Suggested fix: In SPITupleTable, change TupleDesc tupdesc to a slot, and
	 * access everything through slot_XXX intreface.
	 */
	tuptable->vals[tuptable->alloced - tuptable->free] = ExecCopySlotHeapTuple(slot);
	(tuptable->free)--;

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Static functions
 */

/*
 * Parse and plan a querystring.
 *
 * At entry, plan->argtypes, plan->nargs, and plan->cursor_options must be
 * valid.  If boundParams isn't NULL then it represents parameter values
 * that are made available to the planner (as either estimates or hard values
 * depending on their PARAM_FLAG_CONST marking).  The boundParams had better
 * match the param types embedded in the plan!
 *
 * Results are stored into *plan (specifically, plan->plancache_list).
 * Note however that the result trees are all in CurrentMemoryContext
 * and need to be copied somewhere to survive.
 */
static void
_SPI_prepare_plan(const char *src, SPIPlanPtr plan, ParamListInfo boundParams)
{
	List	   *raw_parsetree_list;
	List	   *query_list_list;
	List	   *plan_list;
	ListCell   *list_item;
	ErrorContextCallback spierrcontext;
	Oid		   *argtypes = plan->argtypes;
	int			nargs = plan->nargs;

	/*
	 * Increment CommandCounter to see changes made by now.  We must do this
	 * to be sure of seeing any schema changes made by a just-preceding SPI
	 * command.  (But we don't bother advancing the snapshot, since the
	 * planner generally operates under SnapshotNow rules anyway.)
	 */
	CommandCounterIncrement();

	/*
	 * Setup error traceback support for ereport()
	 */
	spierrcontext.callback = _SPI_error_callback;
	spierrcontext.arg = (void *) src;
	spierrcontext.previous = error_context_stack;
	error_context_stack = &spierrcontext;

	/*
	 * Parse the request string into a list of raw parse trees.
	 */
	raw_parsetree_list = pg_parse_query(src);

	/*
	 * Do parse analysis and rule rewrite for each raw parsetree.
	 *
	 * We save the querytrees from each raw parsetree as a separate sublist.
	 * This allows _SPI_execute_plan() to know where the boundaries between
	 * original queries fall.
	 *
	 * TO DO Find a cleaner way to find query boundaries.  We retained this
	 *       approach when implementing the ground work for PlannedStmt in
	 *       order minimize changes.
	 */
	query_list_list = NIL; /* a list of list of rewritten Query nodes. */
	plan_list = NIL; /* a list of PlannedStmt nodes. */

	foreach(list_item, raw_parsetree_list)
	{
		Node	   *parsetree = (Node *) lfirst(list_item);
		List	   *query_list;

		query_list = pg_analyze_and_rewrite(parsetree, src, argtypes, nargs);

		ListCell *lc = NULL;
		foreach (lc, query_list)
		{
			Query *query = (Query *) lfirst(lc);
			
			if (Gp_role == GP_ROLE_EXECUTE)
			{
				/* This method will error out if the query cannot be safely executed on segment */
				querytree_safe_for_segment(query);
			}
		}
		
		query_list_list = lappend(query_list_list, query_list);

		plan_list = list_concat(plan_list,
								pg_plan_queries(query_list, NULL, false));
	}

	plan->qtlist = query_list_list;
	plan->ptlist = plan_list;

	elog_node_display(DEBUG5, "_SPI_prepare_plan queryTree", plan->qtlist, true);

	/*
	 * Pop the error context stack
	 */
	error_context_stack = spierrcontext.previous;
}

/*
 * Execute the given plan with the given parameter values
 *
 * snapshot: query snapshot to use, or InvalidSnapshot for the normal
 *		behavior of taking a new snapshot for each query.
 * crosscheck_snapshot: for RI use, all others pass InvalidSnapshot
 * read_only: TRUE for read-only execution (no CommandCounterIncrement)
 * fire_triggers: TRUE to fire AFTER triggers at end of query (normal case);
 *		FALSE means any AFTER triggers are postponed to end of outer query
 * tcount: execution tuple-count limit, or 0 for none
 */
static int
_SPI_execute_plan(_SPI_plan * plan, ParamListInfo paramLI,
				  Snapshot snapshot, Snapshot crosscheck_snapshot,
				  bool read_only, bool fire_triggers, long tcount)
{
	volatile int my_res = 0;
	volatile uint64 my_processed = 0;
	volatile Oid my_lastoid = InvalidOid;
	SPITupleTable *volatile my_tuptable = NULL;
	volatile int res = 0;
	Snapshot	saveActiveSnapshot;
	const char *saved_query_string;
	char *current_query_string = NULL;

	/* Be sure to restore ActiveSnapshot on error exit */
	saveActiveSnapshot = ActiveSnapshot;

	/*
	 * In the normal case, where we are on the QD, we can do the normal
	 * PostgreSQL thing and run the command ourselves
	 */

	elog(DEBUG1, "_SPI_execute_plan local: %s", plan->query);

	/*
	 * elog_node_display(DEBUG5,"_SPI_execute_plan
	 * queryTree",plan->qtlist,true);
	 */

	saved_query_string = debug_query_string;

	/* 
	 * plan->query is not memory context safe when FATAL/PANIC level error occurs,
	 * to avoid debug_query_string referring to a invalid address, need to copy
	 * the query string from TopMemoryContext. 
	 */
	current_query_string = MemoryContextStrdup(TopMemoryContext, plan->query);

	debug_query_string = current_query_string;

	PG_TRY();
	{
		List	   *query_list_list = plan->qtlist;
		ListCell   *plan_list_item = list_head(plan->ptlist);
		ListCell   *query_list_list_item;
		ErrorContextCallback spierrcontext;

		/*
		 * Setup error traceback support for ereport()
		 */
		spierrcontext.callback = _SPI_error_callback;
		spierrcontext.arg = (void *) plan->query;
		spierrcontext.previous = error_context_stack;
		error_context_stack = &spierrcontext;

		/* indicate plan is being used */
		plan->use_count++;

		foreach(query_list_list_item, query_list_list)
		{
			List	   *query_list = lfirst(query_list_list_item);
			ListCell   *query_list_item;

			foreach(query_list_item, query_list)
			{
				Query	   *queryTree = (Query *) lfirst(query_list_item);
				PlannedStmt *stmt;
				QueryDesc  *qdesc;
				DestReceiver *dest;

				stmt = (PlannedStmt*)lfirst(plan_list_item);
				plan_list_item = lnext(plan_list_item);

				/*
				 * Get copy of the queryTree and the plan since this may be modified further down.
				 */
				queryTree = copyObject(queryTree);
				stmt = copyObject(stmt);

				_SPI_current->processed = 0;
				_SPI_current->lastoid = InvalidOid;
				_SPI_current->tuptable = NULL;

				if (queryTree->commandType == CMD_UTILITY)
				{
					if (IsA(queryTree->utilityStmt, CopyStmt))
					{
						CopyStmt   *stmt = (CopyStmt *) queryTree->utilityStmt;

						if (stmt->filename == NULL)
						{
							my_res = SPI_ERROR_COPY;
							goto fail;
						}
					}
					else if (IsA(queryTree->utilityStmt, DeclareCursorStmt) ||
							 IsA(queryTree->utilityStmt, ClosePortalStmt) ||
							 IsA(queryTree->utilityStmt, FetchStmt))
					{
						my_res = SPI_ERROR_CURSOR;
						goto fail;
					}
					else if (IsA(queryTree->utilityStmt, TransactionStmt))
					{
						my_res = SPI_ERROR_TRANSACTION;
						goto fail;
					}
				}

				if (read_only && !QueryIsReadOnly(queryTree))
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					/* translator: %s is a SQL statement name */
					   errmsg("%s is not allowed in a non-volatile function",
							  CreateCommandTag((Node*)queryTree))));

				/*
				 * If not read-only mode, advance the command counter before
				 * each command.
				 */
				if (!read_only)
					CommandCounterIncrement();

				dest = CreateDestReceiver(queryTree->canSetTag ? DestSPI : DestNone,
										  NULL);

				if (snapshot == InvalidSnapshot)
				{
					/*
					 * Default read_only behavior is to use the entry-time
					 * ActiveSnapshot; if read-write, grab a full new snap.
					 */
					if (read_only)
						ActiveSnapshot = CopySnapshot(saveActiveSnapshot);
					else
						ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());
				}
				else
				{
					/*
					 * We interpret read_only with a specified snapshot to be
					 * exactly that snapshot, but read-write means use the
					 * snap with advancing of command ID.
					 */
					ActiveSnapshot = CopySnapshot(snapshot);
					if (!read_only)
						ActiveSnapshot->curcid = GetCurrentCommandId();
				}

				if (queryTree->commandType == CMD_UTILITY)
				{
					ProcessUtility(queryTree->utilityStmt,
								   plan->query,
								   paramLI,
								   false, /* not top level */
								   dest, 
								   NULL);
					/* Update "processed" if stmt returned tuples */
					if (_SPI_current->tuptable)
						_SPI_current->processed = _SPI_current->tuptable->alloced - _SPI_current->tuptable->free;
					res = SPI_OK_UTILITY;
				}
				else
				{
					Assert(stmt); /* s.b. NULL only for utility command */

					qdesc = CreateQueryDesc(stmt, plan->query,
											ActiveSnapshot,
											crosscheck_snapshot,
											dest,
											paramLI, false);

                    if (gp_enable_gpperfmon 
                    		&& Gp_role == GP_ROLE_DISPATCH 
                    		&& log_min_messages < DEBUG4)
                    {
                    	/* For log level of DEBUG4, gpmon is sent information about SPI internal queries as well */
                    	Assert(plan->query);
            			gpmon_qlog_query_text(qdesc->gpmon_pkt,
            					plan->query,
            					application_name,
            					NULL /* resqueue name*/,
            					NULL /* priority */);
                    }
                    else
                    {
                    	/* Otherwise, we do not record information about internal queries */
                    	qdesc->gpmon_pkt = NULL;
                    }

					res = _SPI_pquery(qdesc, fire_triggers,
									  queryTree->canSetTag ? tcount : 0);
					FreeQueryDesc(qdesc);
				}
				FreeSnapshot(ActiveSnapshot);
				ActiveSnapshot = NULL;

				/*
				 * The last canSetTag query sets the status values returned to
				 * the caller.	Be careful to free any tuptables not returned,
				 * to avoid intratransaction memory leak.
				 */
				if (queryTree->canSetTag)
				{
					my_processed = _SPI_current->processed;
					my_lastoid = _SPI_current->lastoid;
					SPI_freetuptable(my_tuptable);
					my_tuptable = _SPI_current->tuptable;
					my_res = res;
				}
				else
				{
					SPI_freetuptable(_SPI_current->tuptable);
					_SPI_current->tuptable = NULL;
				}
				/* we know that the receiver doesn't need a destroy call */
				if (res < 0)
				{
					my_res = res;
					goto fail;
				}
			}
		}

fail:

		/*
		 * Pop the error context stack
		 */
		error_context_stack = spierrcontext.previous;
	}
	PG_CATCH();
	{
		pfree(current_query_string);
		debug_query_string = saved_query_string;

		/* Restore global vars and propagate error */
		ActiveSnapshot = saveActiveSnapshot;

		/* decrement plan use_count */
		plan->use_count--;
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(current_query_string);
	debug_query_string = saved_query_string;

	ActiveSnapshot = saveActiveSnapshot;

	/* Save results for caller */
	SPI_processed64 = my_processed;
	SET_SPI_PROCESSED;

	SPI_lastoid = my_lastoid;
	SPI_tuptable = my_tuptable;

	/* tuptable now is caller's responsibility, not SPI's */
	_SPI_current->tuptable = NULL;

	/* plan execution is done */
	plan->use_count--;

	/*
	 * If none of the queries had canSetTag, we return the last query's result
	 * code, but not its auxiliary results (for backwards compatibility).
	 */
	if (my_res == 0)
		my_res = res;

	return my_res;
}

/*
 * Convert query parameters to form wanted by planner and executor
 */
static ParamListInfo
_SPI_convert_params(int nargs, Oid *argtypes,
					Datum *Values, const char *Nulls,
					int pflags)
{
	ParamListInfo paramLI;

	if (nargs > 0)
	{
		int			i;

		/* sizeof(ParamListInfoData) includes the first array element */
		paramLI = (ParamListInfo) palloc(sizeof(ParamListInfoData) +
									   (nargs - 1) *sizeof(ParamExternData));
		paramLI->numParams = nargs;

		for (i = 0; i < nargs; i++)
		{
			ParamExternData *prm = &paramLI->params[i];

			prm->value = Values[i];
			prm->isnull = (Nulls && Nulls[i] == 'n');
			prm->pflags = pflags;
			prm->ptype = argtypes[i];
		}
	}
	else
		paramLI = NULL;
	return paramLI;
}


/*
 * Assign memory for a query before executing through SPI.
 * There are two possibilities:
 *   1. We're not in a function scan. We calculate the
 * 	    query's limit using the queue.
 *   2. We're inside a function scan. We use the memory
 *      allocated to the function scan operator.
 *
 */
static void
_SPI_assign_query_mem(QueryDesc * queryDesc)
{
	if (Gp_role == GP_ROLE_DISPATCH &&
			ResourceScheduler &&
			!superuser() &&
			ActivePortal &&
			(gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE))
		{
			if (!SPI_IsMemoryReserved())
			{
				queryDesc->plannedstmt->query_mem =
					ResourceQueueGetQueryMemoryLimit(queryDesc->plannedstmt,
													 ActivePortal->queueId);
			}
			else
			{
				queryDesc->plannedstmt->query_mem =
					SPI_GetMemoryReservation();
			}
			Assert(queryDesc->plannedstmt->query_mem > 0);
		}
}

static int
_SPI_pquery(QueryDesc * queryDesc, bool fire_triggers, long tcount)
{
	int			operation = queryDesc->operation;
	int			res;

	switch (operation)
	{
		case CMD_SELECT:
			if (queryDesc->plannedstmt->intoClause)		/* select into table? */
				res = SPI_OK_SELINTO;
			else if (queryDesc->dest->mydest != DestSPI)
			{
				/* Don't return SPI_OK_SELECT if we're discarding result */
				res = SPI_OK_UTILITY;
			}
			else
				res = SPI_OK_SELECT;

			/* 
			 * Checking if we need to put this through resource queue.
			 * Same as in pquery.c, except we check ActivePortal->holdingResLock.
			 * If the Active portal already hold a lock on the queue, we cannot
			 * acquire it again.
			 */
			if (Gp_role == GP_ROLE_DISPATCH && ResourceScheduler &&
					!superuser()
			   )
			{
				/*
				 * This is SELECT, so we should have planTree anyway.
				 */
				Assert(queryDesc->plannedstmt->planTree);

				/* 
				 * MPP-6421 - An active portal may not yet be defined if we're
				 * constant folding a stable or volatile function marked as
				 * immutable -- a hack some customers use for partition pruning.
				 *
				 * MPP-16571 - Don't warn about such an event because there are
				 * legitimate parts of the code where we evaluate stable and
				 * volatile functions without an active portal -- describe
				 * functions for table functions, for example.
				 */
				if (ActivePortal)
				{
					if (!ActivePortal->holdingResLock)
					{
						/** TODO: siva - can we ever reach this point? */
						ActivePortal->status = PORTAL_QUEUE;
					
						_SPI_assign_query_mem(queryDesc);

						ActivePortal->holdingResLock =
							ResLockPortal(ActivePortal, queryDesc);
						ActivePortal->status = PORTAL_ACTIVE;
					} 
					else
					{
						/**
						 * If we're in this case, then we already have a
						 * resource queue lock which means we're calling an SPI
						 * query after we've gone through the resource queue. We
						 * need to determine how much memory this query
						 * deserves. There are two possibilities:
						 * 1) We're not in a function scan. We calculate the
						 * query's limit using the queue. We don't go through
						 * the queue, though.
						 * 2) We're inside a function scan. We use the memory
						 * allocated to the function scan operator.
						 */
						Assert(ActivePortal);
						Assert(ActivePortal->holdingResLock);

						_SPI_assign_query_mem(queryDesc);
					}
				}
			}

			break;
		/* TODO Find a better way to indicate "returning".  When PlannedStmt
		 * support is finished, the queryTree field will be gone.
		 */
		case CMD_INSERT:
			_SPI_assign_query_mem(queryDesc);

			if (queryDesc->plannedstmt->returningLists)
				res = SPI_OK_INSERT_RETURNING;
			else
				res = SPI_OK_INSERT;
			break;
		case CMD_DELETE:
			_SPI_assign_query_mem(queryDesc);

			if (queryDesc->plannedstmt->returningLists)
				res = SPI_OK_DELETE_RETURNING;
			else
				res = SPI_OK_DELETE;
			break;
		case CMD_UPDATE:
			_SPI_assign_query_mem(queryDesc);

			if (queryDesc->plannedstmt->returningLists)
				res = SPI_OK_UPDATE_RETURNING;
			else
				res = SPI_OK_UPDATE;
			break;
		default:
			return SPI_ERROR_OPUNKNOWN;
	}

#ifdef SPI_EXECUTOR_STATS
	if (ShowExecutorStats)
		ResetUsage();
#endif

	if (gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE
			&& superuser())
	{
		if (!SPI_IsMemoryReserved())
		{
			queryDesc->plannedstmt->query_mem = ResourceQueueGetSuperuserQueryMemoryLimit();                		
		}
		else
		{
			queryDesc->plannedstmt->query_mem = SPI_GetMemoryReservation();
		}
		Assert(queryDesc->plannedstmt->query_mem > 0);
	}

	if (!cdbpathlocus_querysegmentcatalogs && fire_triggers)
		AfterTriggerBeginQuery();

	bool orig_gp_enable_gpperfmon = gp_enable_gpperfmon;

	PG_TRY();
	{
		/*
		 * Temporarily disable gpperfmon since we don't send information for internal queries in
		 * most cases, except when the debugging level is set to DEBUG4 or DEBUG5.
		 */
		if (log_min_messages > DEBUG4)
		{
			gp_enable_gpperfmon = false;
		}

		ExecutorStart(queryDesc, 0);

		ExecutorRun(queryDesc, ForwardScanDirection, tcount);
		
		_SPI_current->processed = queryDesc->estate->es_processed;
		_SPI_current->lastoid = queryDesc->estate->es_lastoid;
		
		if ((res == SPI_OK_SELECT || queryDesc->plannedstmt->returningLists) &&
			queryDesc->dest->mydest == DestSPI)
		{
			if (_SPI_checktuples())
				insist_log(false, "consistency check on SPI tuple count failed");
		}
		
		if (!cdbpathlocus_querysegmentcatalogs)
			/* Take care of any queued AFTER triggers */
			if (fire_triggers)
				AfterTriggerEndQuery(queryDesc->estate);

		ExecutorEnd(queryDesc);

		gp_enable_gpperfmon = orig_gp_enable_gpperfmon;

		/* MPP-14001: Running auto_stats */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			Oid	relationOid = InvalidOid; 	/* relation that is modified */
			AutoStatsCmdType cmdType = AUTOSTATS_CMDTYPE_SENTINEL; 	/* command type */
			autostats_get_cmdtype(queryDesc->plannedstmt, &cmdType, &relationOid);
			auto_stats(cmdType, relationOid, queryDesc->es_processed, true /* inFunction */);
		}
	}
	PG_CATCH();
	{
		gp_enable_gpperfmon = orig_gp_enable_gpperfmon;
		PG_RE_THROW();
	}
	PG_END_TRY();

	_SPI_current->processed = queryDesc->es_processed;	/* Mpp: Dispatched
														 * queries fill in this
														 * at Executor End */
	_SPI_current->lastoid = queryDesc->es_lastoid;

#ifdef SPI_EXECUTOR_STATS
	if (ShowExecutorStats)
		ShowUsage("SPI EXECUTOR STATS");
#endif

	return res;
}

/*
 * _SPI_error_callback
 *
 * Add context information when a query invoked via SPI fails
 */
static void
_SPI_error_callback(void *arg)
{
	const char *query = (const char *) arg;
	int			syntaxerrposition;

	/*
	 * If there is a syntax error position, convert to internal syntax error;
	 * otherwise treat the query as an item of context stack
	 */
	syntaxerrposition = geterrposition();
	if (syntaxerrposition > 0)
	{
		errposition(0);
		internalerrposition(syntaxerrposition);
		internalerrquery(query);
	}
	else
		errcontext("SQL statement \"%s\"", query);
}

/*
 * _SPI_cursor_operation()
 *
 *	Do a FETCH or MOVE in a cursor
 */
static void
_SPI_cursor_operation(Portal portal, bool forward, long count,
					  DestReceiver *dest)
{
	int64		nfetched;

	elog(DEBUG1, "SPI_cursor_operation");

	/* Check that the portal is valid */
	if (!PortalIsValid(portal))
		insist_log(false, "invalid portal in SPI cursor operation");

	/* Push the SPI stack */
	if (_SPI_begin_call(true) < 0)
		insist_log(false, "SPI cursor operation called while not connected");

	/* Reset the SPI result (note we deliberately don't touch lastoid) */
	SPI_processed64 = 0;
	SPI_processed = 0;
	SPI_tuptable = NULL;
	_SPI_current->processed = 0;
	_SPI_current->tuptable = NULL;

	/* Run the cursor */
	nfetched = PortalRunFetch(portal,
							  forward ? FETCH_FORWARD : FETCH_BACKWARD,
							  count,
							  dest);

	/*
	 * Think not to combine this store with the preceding function call. If
	 * the portal contains calls to functions that use SPI, then SPI_stack is
	 * likely to move around while the portal runs.  When control returns,
	 * _SPI_current will point to the correct stack entry... but the pointer
	 * may be different than it was beforehand. So we must be sure to re-fetch
	 * the pointer after the function call completes.
	 */
	_SPI_current->processed = nfetched;

	if (dest->mydest == DestSPI && _SPI_checktuples())
		insist_log(false, "consistency check on SPI tuple count failed");

	/* Put the result into place for access by caller */
	SPI_processed64 = _SPI_current->processed;
	SET_SPI_PROCESSED;

	SPI_tuptable = _SPI_current->tuptable;

	/* tuptable now is caller's responsibility, not SPI's */
	_SPI_current->tuptable = NULL;

	/* Pop the SPI stack */
	_SPI_end_call(true);
}


static MemoryContext
_SPI_execmem(void)
{
	return MemoryContextSwitchTo(_SPI_current->execCxt);
}

static MemoryContext
_SPI_procmem(void)
{
	return MemoryContextSwitchTo(_SPI_current->procCxt);
}

/*
 * _SPI_begin_call: begin a SPI operation within a connected procedure
 */
static int
_SPI_begin_call(bool execmem)
{
	if (_SPI_curid + 1 != _SPI_connected)
		return SPI_ERROR_UNCONNECTED;
	_SPI_curid++;
	insist_log(_SPI_current == &(_SPI_stack[_SPI_curid]), "SPI stack corrupted");

	if (execmem)				/* switch to the Executor memory context */
		_SPI_execmem();

	return 0;
}

/*
 * _SPI_end_call: end a SPI operation within a connected procedure
 *
 * Note: this currently has no failure return cases, so callers don't check
 */
static int
_SPI_end_call(bool procmem)
{
	/*
	 * We're returning to procedure where _SPI_curid == _SPI_connected - 1
	 */
	_SPI_curid--;

	if (procmem)				/* switch to the procedure memory context */
	{
		_SPI_procmem();
		/* and free Executor memory */
		MemoryContextResetAndDeleteChildren(_SPI_current->execCxt);
	}

	return 0;
}

static bool
_SPI_checktuples(void)
{
	uint32		processed = _SPI_current->processed;
	SPITupleTable *tuptable = _SPI_current->tuptable;
	bool		failed = false;

	if (tuptable == NULL)		/* spi_dest_startup was not called */
		failed = true;
	else if (processed != (tuptable->alloced - tuptable->free))
		failed = true;

	return failed;
}

static SPIPlanPtr
_SPI_copy_plan(SPIPlanPtr plan, int location)
{
	SPIPlanPtr  newplan;
	MemoryContext oldcxt;
	MemoryContext plancxt;
	MemoryContext parentcxt;

	elog(DEBUG1, "_SPI_copy_plan");

	/* Determine correct parent for the plan's memory context */
	if (location == _SPI_CPLAN_PROCXT)
		parentcxt = _SPI_current->procCxt;
	else if (location == _SPI_CPLAN_TOPCXT)
		parentcxt = TopMemoryContext;
	else
		/* (this case not currently used) */
		parentcxt = CurrentMemoryContext;

	/*
	 * Create a memory context for the plan.  We don't expect the plan to be
	 * very large, so use smaller-than-default alloc parameters.
	 */
	plancxt = AllocSetContextCreate(parentcxt,
									"SPI Plan",
									ALLOCSET_SMALL_MINSIZE,
									ALLOCSET_SMALL_INITSIZE,
									ALLOCSET_SMALL_MAXSIZE);
	oldcxt = MemoryContextSwitchTo(plancxt);

	/* Copy the SPI plan into its own context */
	newplan = (SPIPlanPtr) palloc(sizeof(_SPI_plan));
	newplan->plancxt = plancxt;
	newplan->query = pstrdup(plan->query);
	newplan->qtlist = (List *) copyObject(plan->qtlist);
	/* We don't copy the list directly, like this,
	 *     newplan->ptlist = (List *) copyObject(plan->ptlist);
	 * because we want to propagate the memory context into to PlannedStmt 
	 * nodes for dispatch.  
	 *
	 * TO DO Simplify this when dispatch no longer modifies the Plan.
	 */
	{
		ListCell *lc;
		newplan->ptlist = NIL;
		foreach (lc, plan->ptlist)
		{
			Node *node = copyObject(lfirst(lc));
			if (IsA(node, PlannedStmt))
			{
				PlannedStmt *ps = (PlannedStmt*) node;
				ps->qdContext = plancxt;
			}
			newplan->ptlist = lappend(newplan->ptlist, node);
		}
	}
	
	newplan->nargs = plan->nargs;
	if (plan->nargs > 0)
	{
		newplan->argtypes = (Oid *) palloc(plan->nargs * sizeof(Oid));
		memcpy(newplan->argtypes, plan->argtypes, plan->nargs * sizeof(Oid));
	}
	else
		newplan->argtypes = NULL;

	newplan->use_count = plan->use_count;

	MemoryContextSwitchTo(oldcxt);

	return newplan;
}

/**
 * Memory reserved for SPI cals
 */
static uint64 SPIMemReserved = 0;

/**
 * Initialize the SPI memory reservation stack. See SPI_ReserveMemory() for detailed comments on how this stack
 * is used.
 */
void SPI_InitMemoryReservation(void)
{
	Assert(gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE);
	SPIMemReserved = (uint64) statement_mem * 1024L;;
}

/**
 * Push memory reserved for next SPI call. It is possible for an operator to (after several levels of nesting),
 * result in execution of SQL statements via SPI e.g. a pl/pgsql function that issues queries. These queries must be sandboxed into
 * the memory limits of the operator. This stack represents the nesting of these operators and each
 * operator will push its own limit.
 */
void SPI_ReserveMemory(uint64 mem_reserved)
{
	Assert(gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE);
	if (mem_reserved > 0
			&& (SPIMemReserved == 0 || mem_reserved < SPIMemReserved))
	{
		SPIMemReserved = mem_reserved;
	}

	if (gp_log_resqueue_memory)
	{
		elog(gp_resqueue_memory_log_level, "SPI memory reservation %d", (int) SPIMemReserved);
	}
}

/**
 * What was the amount of memory reserved for the last operator? See SPI_ReserveMemory()
 * for details.
 */
uint64 SPI_GetMemoryReservation(void)
{
	Assert(gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE);
	return SPIMemReserved;
}

/**
 * Is memory reserved stack empty?
 */
bool SPI_IsMemoryReserved(void)
{
	Assert(gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE);
	return (SPIMemReserved == 0);
}

/**
  * Are we in SPI context 
  */
bool
SPI_context(void)
{ 
	return (_SPI_connected != -1); 
}
