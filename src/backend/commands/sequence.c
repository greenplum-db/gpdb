/*-------------------------------------------------------------------------
 *
 * sequence.c
 *	  PostgreSQL sequences support code.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/sequence.c,v 1.160 2009/06/11 14:48:56 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/bufmask.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlogutils.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "miscadmin.h"
#include "storage/smgr.h"               /* RelationCloseSmgr -> smgrclose */
#include "nodes/makefuncs.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/lsyscache.h"
#include "utils/resowner.h"
#include "utils/syscache.h"

#include "catalog/oid_dispatch.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdoublylinked.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbmotion.h"
#include "cdb/ml_ipc.h"

#include "postmaster/seqserver.h"


/*
 * We don't want to log each fetching of a value from a sequence,
 * so we pre-log a few fetches in advance. In the event of
 * crash we can lose (skip over) as many values as we pre-logged.
 */
#define SEQ_LOG_VALS	32

/*
 * The "special area" of a sequence's buffer page looks like this.
 */
#define SEQ_MAGIC	  0x1717

typedef struct sequence_magic
{
	uint32		magic;
} sequence_magic;

/*
 * We store a SeqTable item for every sequence we have touched in the current
 * session.  This is needed to hold onto nextval/currval state.  (We can't
 * rely on the relcache, since it's only, well, a cache, and may decide to
 * discard entries.)
 *
 * XXX We use linear search to find pre-existing SeqTable entries.	This is
 * good when only a small number of sequences are touched in a session, but
 * would suck with many different sequences.  Perhaps use a hashtable someday.
 */
typedef struct SeqTableData
{
	struct SeqTableData *next;	/* link to next SeqTable object */
	Oid			relid;			/* pg_class OID of this sequence */
	LocalTransactionId lxid;	/* xact in which we last did a seq op */
	bool		last_valid;		/* do we have a valid "last" value? */
	int64		last;			/* value last returned by nextval */
	int64		cached;			/* last value already cached for nextval */
	/* if last != cached, we have not used up all the cached values */
	int64		increment;		/* copy of sequence's increment field */
	/* note that increment is zero until we first do read_seq_tuple() */
} SeqTableData;

typedef SeqTableData *SeqTable;

static SeqTable seqtab = NULL;	/* Head of list of SeqTable items */

/*
 * last_used_seq is updated by nextval() to point to the last used
 * sequence.
 */
static SeqTableData *last_used_seq = NULL;

static int64 nextval_internal(Oid relid);
static Relation open_share_lock(SeqTable seq);
static void init_sequence(Oid relid, SeqTable *p_elm, Relation *p_rel);
static Form_pg_sequence read_seq_tuple(SeqTable elm, Relation rel,
			   Buffer *buf, HeapTuple seqtuple);
static void init_params(List *options, bool isInit,
			Form_pg_sequence new, List **owned_by);
static void do_setval(Oid relid, int64 next, bool iscalled);
static void process_owned_by(Relation seqrel, List *owned_by);
static void mask_seq_values(Page page);

static void
cdb_sequence_nextval(SeqTable elm,
					 Relation   seqrel,
                     int64     *plast,
                     int64     *pcached,
                     int64     *pincrement,
                     bool      *seq_overflow);
static void
cdb_sequence_nextval_proxy(Relation seqrel,
                           int64   *plast,
                           int64   *pcached,
                           int64   *pincrement,
                           bool    *poverflow);

/*
 * DefineSequence
 *				Creates a new sequence relation
 */
void
DefineSequence(CreateSeqStmt *seq)
{
	FormData_pg_sequence new;
	List	   *owned_by;
	CreateStmt *stmt = makeNode(CreateStmt);
	Oid			seqoid;
	Relation	rel;
	Buffer		buf;
	Page		page;
	sequence_magic *sm;
	HeapTuple	tuple;
	TupleDesc	tupDesc;
	Datum		value[SEQ_COL_LASTCOL];
	bool		null[SEQ_COL_LASTCOL];
	int			i;
	NameData	name;
	OffsetNumber offnum;

	bool shouldDispatch =  Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode();

	/* Check and set all option values */
	init_params(seq->options, true, &new, &owned_by);

	/*
	 * Create relation (and fill value[] and null[] for the tuple)
	 */
	stmt->tableElts = NIL;
	for (i = SEQ_COL_FIRSTCOL; i <= SEQ_COL_LASTCOL; i++)
	{
		ColumnDef  *coldef = makeNode(ColumnDef);

		coldef->inhcount = 0;
		coldef->is_local = true;
		coldef->is_not_null = true;
		coldef->raw_default = NULL;
		coldef->cooked_default = NULL;
		coldef->constraints = NIL;

		null[i - 1] = false;

		switch (i)
		{
			case SEQ_COL_NAME:
				coldef->typeName = makeTypeNameFromOid(NAMEOID, -1);
				coldef->colname = "sequence_name";
				namestrcpy(&name, seq->sequence->relname);
				value[i - 1] = NameGetDatum(&name);
				break;
			case SEQ_COL_LASTVAL:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "last_value";
				value[i - 1] = Int64GetDatumFast(new.last_value);
				break;
			case SEQ_COL_STARTVAL:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "start_value";
				value[i - 1] = Int64GetDatumFast(new.start_value);
				break;
			case SEQ_COL_INCBY:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "increment_by";
				value[i - 1] = Int64GetDatumFast(new.increment_by);
				break;
			case SEQ_COL_MAXVALUE:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "max_value";
				value[i - 1] = Int64GetDatumFast(new.max_value);
				break;
			case SEQ_COL_MINVALUE:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "min_value";
				value[i - 1] = Int64GetDatumFast(new.min_value);
				break;
			case SEQ_COL_CACHE:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "cache_value";
				value[i - 1] = Int64GetDatumFast(new.cache_value);
				break;
			case SEQ_COL_LOG:
				coldef->typeName = makeTypeNameFromOid(INT8OID, -1);
				coldef->colname = "log_cnt";
				value[i - 1] = Int64GetDatum((int64) 0);
				break;
			case SEQ_COL_CYCLE:
				coldef->typeName = makeTypeNameFromOid(BOOLOID, -1);
				coldef->colname = "is_cycled";
				value[i - 1] = BoolGetDatum(new.is_cycled);
				break;
			case SEQ_COL_CALLED:
				coldef->typeName = makeTypeNameFromOid(BOOLOID, -1);
				coldef->colname = "is_called";
				value[i - 1] = BoolGetDatum(false);
				break;
		}
		stmt->tableElts = lappend(stmt->tableElts, coldef);
	}

	stmt->relation = seq->sequence;
	stmt->inhRelations = NIL;
	stmt->constraints = NIL;
	stmt->inhOids = NIL;
	stmt->parentOidCount = 0;
	stmt->options = list_make1(defWithOids(false));
	stmt->oncommit = ONCOMMIT_NOOP;
	stmt->tablespacename = NULL;
	stmt->relKind = RELKIND_SEQUENCE;
	stmt->ownerid = GetUserId();

	seqoid = DefineRelation(stmt, RELKIND_SEQUENCE, RELSTORAGE_HEAP, false);

	/*
	 * Open and lock the new sequence.  (This lock is redundant; an
	 * AccessExclusiveLock was acquired above by DefineRelation and
	 * won't be released until end of transaction.)
	 *
	 * CDB: Acquire lock on qDisp before dispatching to qExecs, so
	 * qDisp can detect and resolve any deadlocks.
	 */
	rel = heap_open(seqoid, AccessExclusiveLock);
	tupDesc = RelationGetDescr(rel);

	/* Now form sequence tuple */
	tuple = heap_form_tuple(tupDesc, value, null);

	/* Initialize first page of relation with special magic number */
	buf = ReadBuffer(rel, P_NEW);
	Assert(BufferGetBlockNumber(buf) == 0);

	page = BufferGetPage(buf);

	PageInit(page, BufferGetPageSize(buf), sizeof(sequence_magic));
	sm = (sequence_magic *) PageGetSpecialPointer(page);
	sm->magic = SEQ_MAGIC;

	/* Now insert sequence tuple */
	LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

	/*
	 * Since VACUUM does not process sequences, we have to force the tuple
	 * to have xmin = FrozenTransactionId now.	Otherwise it would become
	 * invisible to SELECTs after 2G transactions.	It is okay to do this
	 * because if the current transaction aborts, no other xact will ever
	 * examine the sequence tuple anyway.
	 *
	 */

	HeapTupleHeaderSetXmin(tuple->t_data, FrozenTransactionId);
	HeapTupleHeaderSetXminFrozen(tuple->t_data);
	HeapTupleHeaderSetCmin(tuple->t_data, FirstCommandId);
	HeapTupleHeaderSetXmax(tuple->t_data, InvalidTransactionId);
	tuple->t_data->t_infomask |= HEAP_XMAX_INVALID;
	ItemPointerSet(&tuple->t_data->t_ctid, 0, FirstOffsetNumber);

	START_CRIT_SECTION();

	MarkBufferDirty(buf);

	offnum = PageAddItem(page, (Item) tuple->t_data, tuple->t_len,
						 InvalidOffsetNumber, false, false);
	if (offnum != FirstOffsetNumber)
		elog(ERROR, "failed to add sequence tuple to page");

	/* XLOG stuff */
	if (!rel->rd_istemp)
	{
		xl_seq_rec	xlrec;
		XLogRecPtr	recptr;
		XLogRecData rdata[2];

		xlrec.node = rel->rd_node;

		rdata[0].data = (char *) &xlrec;
		rdata[0].len = sizeof(xl_seq_rec);
		rdata[0].buffer = InvalidBuffer;
		rdata[0].next = &(rdata[1]);

		rdata[1].data = (char *) tuple->t_data;
		rdata[1].len = tuple->t_len;
		rdata[1].buffer = InvalidBuffer;
		rdata[1].next = NULL;

		recptr = XLogInsert(RM_SEQ_ID, XLOG_SEQ_LOG, rdata);

		PageSetLSN(page, recptr);
	}

	END_CRIT_SECTION();

	UnlockReleaseBuffer(buf);

	/* process OWNED BY if given */
	if (owned_by)
		process_owned_by(rel, owned_by);

	heap_close(rel, NoLock);

	
	/* Dispatch to segments */
	if (shouldDispatch)
	{
		CdbDispatchUtilityStatement((Node *) seq,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}

/*
 * AlterSequence
 *
 * Modify the definition of a sequence relation
 */
void
AlterSequence(AlterSeqStmt *stmt)
{
	Oid			relid;

	/* find sequence */
	relid = RangeVarGetRelid(stmt->sequence, false);

	/* allow ALTER to sequence owner only */
	/* if you change this, see also callers of AlterSequenceInternal! */
	if (!pg_class_ownercheck(relid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   stmt->sequence->relname);

	/* do the work */
	AlterSequenceInternal(relid, stmt->options);

	if (Gp_role == GP_ROLE_DISPATCH)
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
}

/*
 * AlterSequenceInternal
 *
 * Same as AlterSequence except that the sequence is specified by OID
 * and we assume the caller already checked permissions.
 */
void
AlterSequenceInternal(Oid relid, List *options)
{
	SeqTable	elm;
	Relation	seqrel;
	Buffer		buf;
	Form_pg_sequence seq;
	FormData_pg_sequence new;
	List	   *owned_by;
	HeapTupleData seqtuple;
	int64		save_increment;
	bool		bSeqIsTemp = false;
	int			numopts	   = 0;
	char	   *alter_subtype = "";		/* metadata tracking: kind of
										   redundant to say "role" */

	/* open and AccessShareLock sequence */
	init_sequence(relid, &elm, &seqrel);

	/* lock page' buffer and read tuple into new sequence structure */

	/* hack to keep ALTER SEQUENCE OWNED BY from changing currval state */
	save_increment = elm->increment;
	
	seq = read_seq_tuple(elm, seqrel, &buf, &seqtuple);
	elm->increment = seq->increment_by;

	/* Copy old values of options into workspace */
	memcpy(&new, seq, sizeof(FormData_pg_sequence));

	/* Check and set new values */
	init_params(options, false, &new, &owned_by);

	if (owned_by)
	{
		/* Restore previous state of elm (assume nothing else changes) */
		elm->increment = save_increment;
	}
	else
	{
		/* Clear local cache so that we don't think we have cached numbers */
		/* Note that we do not change the currval() state */
		elm->cached = elm->last;
	}

	/* Now okay to update the on-disk tuple */
	START_CRIT_SECTION();

	memcpy(seq, &new, sizeof(FormData_pg_sequence));

	MarkBufferDirty(buf);

	/* XLOG stuff */

	bSeqIsTemp = seqrel->rd_istemp;

	if (!bSeqIsTemp)
	{
		xl_seq_rec	xlrec;
		XLogRecPtr	recptr;
		XLogRecData rdata[2];
		Page		page = BufferGetPage(buf);

		xlrec.node = seqrel->rd_node;

		rdata[0].data = (char *) &xlrec;
		rdata[0].len = sizeof(xl_seq_rec);
		rdata[0].buffer = InvalidBuffer;
		rdata[0].next = &(rdata[1]);

		rdata[1].data = (char *) seqtuple.t_data;
		rdata[1].len = seqtuple.t_len;
		rdata[1].buffer = InvalidBuffer;
		rdata[1].next = NULL;

		recptr = XLogInsert(RM_SEQ_ID, XLOG_SEQ_LOG, rdata);

		PageSetLSN(page, recptr);
	}

	END_CRIT_SECTION();

	UnlockReleaseBuffer(buf);

	/* process OWNED BY if given */
	if (owned_by)
		process_owned_by(seqrel, owned_by);

	relation_close(seqrel, NoLock);

	numopts = list_length(options);

	if (numopts > 1)
	{
		char allopts[NAMEDATALEN];

		sprintf(allopts, "%d OPTIONS", numopts);

		alter_subtype = pstrdup(allopts);
	}
	else if (0 == numopts)
	{
		alter_subtype = "0 OPTIONS";
	}
	else if ((Gp_role == GP_ROLE_DISPATCH) && (!bSeqIsTemp))
	{
		ListCell		*option = list_head(options);
		DefElem			*defel	= (DefElem *) lfirst(option);
		char			*tempo	= NULL;

		alter_subtype = defel->defname;
		if (0 == strcmp(alter_subtype, "owned_by"))
			alter_subtype = "OWNED BY";

		tempo = str_toupper(alter_subtype, strlen(alter_subtype));

		alter_subtype = tempo;
	}

	if (Gp_role == GP_ROLE_DISPATCH && !bSeqIsTemp)
	{
		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   relid,
						   GetUserId(),
						   "ALTER", alter_subtype);
	}
}


/*
 * Note: nextval with a text argument is no longer exported as a pg_proc
 * entry, but we keep it around to ease porting of C code that may have
 * called the function directly.
 */
Datum
nextval(PG_FUNCTION_ARGS)
{
	text	   *seqin = PG_GETARG_TEXT_P(0);
	RangeVar   *sequence;
	Oid			relid;

	sequence = makeRangeVarFromNameList(textToQualifiedNameList(seqin));
	relid = RangeVarGetRelid(sequence, false);

	PG_RETURN_INT64(nextval_internal(relid));
}

Datum
nextval_oid(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);

	PG_RETURN_INT64(nextval_internal(relid));
}

static int64
nextval_internal(Oid relid)
{
	SeqTable	elm;
	Relation	seqrel;
	bool is_overflow = false;

	/* open and AccessShareLock sequence */
	init_sequence(relid, &elm, &seqrel);

	if (elm->last != elm->cached)		/* some numbers were cached */
	{
		Assert(elm->last_valid);
		Assert(elm->increment != 0);
		elm->last += elm->increment;
		relation_close(seqrel, NoLock);
		last_used_seq = elm;
		return elm->last;
	}

	if (pg_class_aclcheck(elm->relid, GetUserId(), ACL_UPDATE) != ACLCHECK_OK)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied for sequence %s",
						RelationGetRelationName(seqrel))));

	/* Update the sequence object. */
	if (Gp_role == GP_ROLE_EXECUTE)
		cdb_sequence_nextval_proxy(seqrel,
								   &elm->last,
								   &elm->cached,
								   &elm->increment,
								   &is_overflow);
	else
		cdb_sequence_nextval(elm,
							 seqrel,
							 &elm->last,
							 &elm->cached,
							 &elm->increment,
							 &is_overflow);
	last_used_seq = elm;

	if (is_overflow)
	{
		relation_close(seqrel, NoLock);

		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("nextval: reached %s value of sequence \"%s\" (" INT64_FORMAT ")",
                        elm->increment>0 ? "maximum":"minimum",
                        RelationGetRelationName(seqrel), elm->last)));
	}
	else
		elm->last_valid = true;

	relation_close(seqrel, NoLock);
	return elm->last;
}


static void
cdb_sequence_nextval(SeqTable elm,
					 Relation   seqrel,
                     int64     *plast,
                     int64     *pcached,
                     int64     *pincrement,
                     bool      *poverflow)
{
	Buffer		buf;
	Page		page;
	HeapTupleData seqtuple;
	Form_pg_sequence seq;
	int64		incby,
				maxv,
				minv,
				cache,
				log,
				fetch,
				last;
	int64		result,
				next,
				rescnt = 0;
	bool 		have_overflow = false;
	bool		logit = false;

	/* lock page' buffer and read tuple */
	seq = read_seq_tuple(elm, seqrel, &buf, &seqtuple);
	page = BufferGetPage(buf);

	last = next = result = seq->last_value;
	incby = seq->increment_by;
	maxv = seq->max_value;
	minv = seq->min_value;
	fetch = cache = seq->cache_value;
	log = seq->log_cnt;

	if (!seq->is_called)
	{
		rescnt++;				/* return last_value if not is_called */
		fetch--;
	}

	/*
	 * Decide whether we should emit a WAL log record.	If so, force up the
	 * fetch count to grab SEQ_LOG_VALS more values than we actually need to
	 * cache.  (These will then be usable without logging.)
	 *
	 * If this is the first nextval after a checkpoint, we must force a new
	 * WAL record to be written anyway, else replay starting from the
	 * checkpoint would fail to advance the sequence past the logged values.
	 * In this case we may as well fetch extra values.
	 */
	if (log < fetch || !seq->is_called)
	{
		/* forced log to satisfy local demand for values */
		fetch = log = fetch + SEQ_LOG_VALS;
		logit = true;
	}
	else
	{
		XLogRecPtr	redoptr = GetRedoRecPtr();

		if (XLByteLE(PageGetLSN(page), redoptr))
		{
			/* last update of seq was before checkpoint */
			fetch = log = fetch + SEQ_LOG_VALS;
			logit = true;
		}
	}

	while (fetch)				/* try to fetch cache [+ log ] numbers */
	{
		/*
		 * Check MAXVALUE for ascending sequences and MINVALUE for descending
		 * sequences
		 */
		if (incby > 0)
		{
			/* ascending sequence */
			if ((maxv >= 0 && next > maxv - incby) ||
				(maxv < 0 && next + incby > maxv))
			{
				if (rescnt > 0)
					break;		/* stop fetching */
				if (!seq->is_cycled)
				{
					have_overflow = true;
				}
				else
				{
					next = minv;
				}
			}
			else
				next += incby;
		}
		else
		{
			/* descending sequence */
			if ((minv < 0 && next < minv - incby) ||
				(minv >= 0 && next + incby < minv))
			{
				if (rescnt > 0)
					break;		/* stop fetching */
				if (!seq->is_cycled)
				{
					have_overflow = true;
				}
				else
				{
					next = maxv;
				}
			}
			else
				next += incby;
		}
		fetch--;
		if (rescnt < cache)
		{
			log--;
			rescnt++;
			last = next;
			if (rescnt == 1)	/* if it's first result - */
				result = next;	/* it's what to return */
		}
	}

	log -= fetch;				/* adjust for any unfetched numbers */
	Assert(log >= 0);

    /* set results for caller */
	*poverflow = have_overflow; /* has the sequence overflown */
    *plast = result;            /* last returned number */
    *pcached = last;            /* last fetched number */
	*pincrement = incby;

	/* ready to change the on-disk (or really, in-buffer) tuple */
	START_CRIT_SECTION();

	/*
	 * We must mark the buffer dirty before doing XLogInsert(); see notes in
	 * SyncOneBuffer().  However, we don't apply the desired changes just yet.
	 * This looks like a violation of the buffer update protocol, but it is
	 * in fact safe because we hold exclusive lock on the buffer.  Any other
	 * process, including a checkpoint, that tries to examine the buffer
	 * contents will block until we release the lock, and then will see the
	 * final state that we install below.
	 */
	MarkBufferDirty(buf);

	/* XLOG stuff */
	if (logit && !seqrel->rd_istemp)
	{
		xl_seq_rec	xlrec;
		XLogRecPtr	recptr;
		XLogRecData rdata[2];

		/*
		 * We don't log the current state of the tuple, but rather the state
		 * as it would appear after "log" more fetches.  This lets us skip
		 * that many future WAL records, at the cost that we lose those
		 * sequence values if we crash.
		 */

		/* set values that will be saved in xlog */
		seq->last_value = next;
		seq->is_called = true;
		seq->log_cnt = 0;

		xlrec.node = seqrel->rd_node;
		rdata[0].data = (char *) &xlrec;
		rdata[0].len = sizeof(xl_seq_rec);
		rdata[0].buffer = InvalidBuffer;
		rdata[0].next = &(rdata[1]);

		rdata[1].data = (char *) seqtuple.t_data;
		rdata[1].len = seqtuple.t_len;
		rdata[1].buffer = InvalidBuffer;
		rdata[1].next = NULL;

		recptr = XLogInsert(RM_SEQ_ID, XLOG_SEQ_LOG, rdata);

		PageSetLSN(page, recptr);

		/* need to update where we've inserted to into shmem so that the QD can flush it
		 * when necessary
		 */
		LWLockAcquire(SeqServerControlLock, LW_EXCLUSIVE);

		if (XLByteLT(seqServerCtl->lastXlogEntry, recptr))
		{
			seqServerCtl->lastXlogEntry.xlogid = recptr.xlogid;
			seqServerCtl->lastXlogEntry.xrecoff = recptr.xrecoff;
		}

		LWLockRelease(SeqServerControlLock);
	}

	/* Now update sequence tuple to the intended final state */
	seq->last_value = last;		/* last fetched number */
	seq->is_called = true;
	seq->log_cnt = log;			/* how much is logged */

	END_CRIT_SECTION();

	UnlockReleaseBuffer(buf);
}                               /* cdb_sequence_nextval */


Datum
currval_oid(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	int64		result;
	SeqTable	elm;
	Relation	seqrel;

	/* For now, strictly forbidden on MPP. */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
				 errmsg("currval() not supported")));
	}

	/* open and AccessShareLock sequence */
	init_sequence(relid, &elm, &seqrel);

	if (pg_class_aclcheck(elm->relid, GetUserId(), ACL_SELECT) != ACLCHECK_OK &&
		pg_class_aclcheck(elm->relid, GetUserId(), ACL_USAGE) != ACLCHECK_OK)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied for sequence %s",
						RelationGetRelationName(seqrel))));

	if (!elm->last_valid)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("currval of sequence \"%s\" is not yet defined in this session",
						RelationGetRelationName(seqrel))));

	result = elm->last;

	relation_close(seqrel, NoLock);

	PG_RETURN_INT64(result);
}

Datum
lastval(PG_FUNCTION_ARGS)
{
	Relation	seqrel;
	int64		result;

	/* For now, strictly forbidden on MPP. */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
				 errmsg("lastval() not supported")));
	}

	if (last_used_seq == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("lastval is not yet defined in this session")));

	/* Someone may have dropped the sequence since the last nextval() */
	if (!SearchSysCacheExists(RELOID,
							  ObjectIdGetDatum(last_used_seq->relid),
							  0, 0, 0))
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("lastval is not yet defined in this session")));

	seqrel = open_share_lock(last_used_seq);

	/* nextval() must have already been called for this sequence */
	Assert(last_used_seq->last_valid);

	if (pg_class_aclcheck(last_used_seq->relid, GetUserId(), ACL_SELECT) != ACLCHECK_OK &&
		pg_class_aclcheck(last_used_seq->relid, GetUserId(), ACL_USAGE) != ACLCHECK_OK)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied for sequence %s",
						RelationGetRelationName(seqrel))));

	result = last_used_seq->last;
	relation_close(seqrel, NoLock);

	PG_RETURN_INT64(result);
}

/*
 * Main internal procedure that handles 2 & 3 arg forms of SETVAL.
 *
 * Note that the 3 arg version (which sets the is_called flag) is
 * only for use in pg_dump, and setting the is_called flag may not
 * work if multiple users are attached to the database and referencing
 * the sequence (unlikely if pg_dump is restoring it).
 *
 * It is necessary to have the 3 arg version so that pg_dump can
 * restore the state of a sequence exactly during data-only restores -
 * it is the only way to clear the is_called flag in an existing
 * sequence.
 */
static void
do_setval(Oid relid, int64 next, bool iscalled)
{
	SeqTable	elm;
	Relation	seqrel;
	Buffer		buf;
	HeapTupleData seqtuple;
	Form_pg_sequence seq;

	if (Gp_role == GP_ROLE_EXECUTE)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
				 errmsg("setval() not supported in this context")));
	}

	/* open and AccessShareLock sequence */
	init_sequence(relid, &elm, &seqrel);

	if (pg_class_aclcheck(elm->relid, GetUserId(), ACL_UPDATE) != ACLCHECK_OK)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied for sequence %s",
						RelationGetRelationName(seqrel))));

	/* lock page' buffer and read tuple */
	seq = read_seq_tuple(elm, seqrel, &buf, &seqtuple);
	elm->increment = seq->increment_by;

	if ((next < seq->min_value) || (next > seq->max_value))
	{
		char		bufv[100],
					bufm[100],
					bufx[100];

		snprintf(bufv, sizeof(bufv), INT64_FORMAT, next);
		snprintf(bufm, sizeof(bufm), INT64_FORMAT, seq->min_value);
		snprintf(bufx, sizeof(bufx), INT64_FORMAT, seq->max_value);
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("setval: value %s is out of bounds for sequence \"%s\" (%s..%s)",
						bufv, RelationGetRelationName(seqrel),
						bufm, bufx)));
	}

	/* Set the currval() state only if iscalled = true */
	if (iscalled)
	{
		elm->last = next;		/* last returned number */
		elm->last_valid = true;
	}

	/* In any case, forget any future cached numbers */
	elm->cached = elm->last;

	/* ready to change the on-disk (or really, in-buffer) tuple */
	START_CRIT_SECTION();

	seq->last_value = next;		/* last fetched number */
	seq->is_called = iscalled;
	seq->log_cnt = 0;

	MarkBufferDirty(buf);

	/* XLOG stuff */
	if (!seqrel->rd_istemp)
	{
		xl_seq_rec	xlrec;
		XLogRecPtr	recptr;
		XLogRecData rdata[2];
		Page		page = BufferGetPage(buf);

		xlrec.node = seqrel->rd_node;

		rdata[0].data = (char *) &xlrec;
		rdata[0].len = sizeof(xl_seq_rec);
		rdata[0].buffer = InvalidBuffer;
		rdata[0].next = &(rdata[1]);

		rdata[1].data = (char *) seqtuple.t_data;
		rdata[1].len = seqtuple.t_len;
		rdata[1].buffer = InvalidBuffer;
		rdata[1].next = NULL;

		recptr = XLogInsert(RM_SEQ_ID, XLOG_SEQ_LOG, rdata);

		PageSetLSN(page, recptr);
	}

	END_CRIT_SECTION();

	UnlockReleaseBuffer(buf);
	relation_close(seqrel, NoLock);
}

/*
 * Implement the 2 arg setval procedure.
 * See do_setval for discussion.
 */
Datum
setval_oid(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	int64		next = PG_GETARG_INT64(1);

	do_setval(relid, next, true);

	PG_RETURN_INT64(next);
}

/*
 * Implement the 3 arg setval procedure.
 * See do_setval for discussion.
 */
Datum
setval3_oid(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	int64		next = PG_GETARG_INT64(1);
	bool		iscalled = PG_GETARG_BOOL(2);

	do_setval(relid, next, iscalled);

	PG_RETURN_INT64(next);
}


/*
 * Open the sequence and acquire AccessShareLock if needed
 *
 * If we haven't touched the sequence already in this transaction,
 * we need to acquire AccessShareLock.	We arrange for the lock to
 * be owned by the top transaction, so that we don't need to do it
 * more than once per xact.
 */
static Relation
open_share_lock(SeqTable seq)
{
	LocalTransactionId thislxid = MyProc->lxid;

	/* Get the lock if not already held in this xact */
	if (seq->lxid != thislxid)
	{
		ResourceOwner currentOwner;

		currentOwner = CurrentResourceOwner;
		PG_TRY();
		{
			CurrentResourceOwner = TopTransactionResourceOwner;
			LockRelationOid(seq->relid, AccessShareLock);
		}
		PG_CATCH();
		{
			/* Ensure CurrentResourceOwner is restored on error */
			CurrentResourceOwner = currentOwner;
			PG_RE_THROW();
		}
		PG_END_TRY();
		CurrentResourceOwner = currentOwner;

		/* Flag that we have a lock in the current xact */
		seq->lxid = thislxid;
	}

	/* We now know we have AccessShareLock, and can safely open the rel */
	return relation_open(seq->relid, NoLock);
}

/*
 * Given a relation OID, open and lock the sequence.  p_elm and p_rel are
 * output parameters.
 *
 * GPDB: If p_rel is NULL, the sequence relation is not opened or locked.
 */
static void
init_sequence(Oid relid, SeqTable *p_elm, Relation *p_rel)
{
	SeqTable	elm;
	Relation	seqrel;

	/* Look to see if we already have a seqtable entry for relation */
	for (elm = seqtab; elm != NULL; elm = elm->next)
	{
		if (elm->relid == relid)
			break;
	}

	/*
	 * Allocate new seqtable entry if we didn't find one.
	 *
	 * NOTE: seqtable entries remain in the list for the life of a backend. If
	 * the sequence itself is deleted then the entry becomes wasted memory,
	 * but it's small enough that this should not matter.
	 */
	if (elm == NULL)
	{
		/*
		 * Time to make a new seqtable entry.  These entries live as long as
		 * the backend does, so we use plain malloc for them.
		 */
		elm = (SeqTable) malloc(sizeof(SeqTableData));
		if (elm == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
		elm->relid = relid;
		elm->lxid = InvalidLocalTransactionId;
		elm->last_valid = false;
		elm->last = elm->cached = elm->increment = 0;
		elm->next = seqtab;
		seqtab = elm;
	}

	/*
	 * Open the sequence relation.
	 */
	if (p_rel)
	{
		seqrel = open_share_lock(elm);

		if (seqrel->rd_rel->relkind != RELKIND_SEQUENCE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a sequence",
							RelationGetRelationName(seqrel))));

		*p_rel = seqrel;
	}
	*p_elm = elm;
}


/*
 * Given an opened sequence relation, lock the page buffer and find the tuple
 *
 * *buf receives the reference to the pinned-and-ex-locked buffer
 * *seqtuple receives the reference to the sequence tuple proper
 *		(this arg should point to a local variable of type HeapTupleData)
 *
 * Function's return value points to the data payload of the tuple
 */
static Form_pg_sequence
read_seq_tuple(SeqTable elm, Relation rel, Buffer *buf, HeapTuple seqtuple)
{
	Page		page;
	ItemId		lp;
	sequence_magic *sm;
	Form_pg_sequence seq;

	*buf = ReadBuffer(rel, 0);
	LockBuffer(*buf, BUFFER_LOCK_EXCLUSIVE);

	page = BufferGetPage(*buf);
	sm = (sequence_magic *) PageGetSpecialPointer(page);

	if (sm->magic != SEQ_MAGIC)
		elog(ERROR, "bad magic number in sequence \"%s\": %08X",
			 RelationGetRelationName(rel), sm->magic);

	lp = PageGetItemId(page, FirstOffsetNumber);
	Assert(ItemIdIsNormal(lp));

	/* Note we currently only bother to set these two fields of *seqtuple */
	seqtuple->t_data = (HeapTupleHeader) PageGetItem((Page) page, lp);
	seqtuple->t_len = ItemIdGetLength(lp);

	/*
	 * Previous releases of Postgres neglected to prevent SELECT FOR UPDATE
	 * on a sequence, which would leave a non-frozen XID in the sequence
	 * tuple's xmax, which eventually leads to clog access failures or worse.
	 * If we see this has happened, clean up after it.  We treat this like a
	 * hint bit update, ie, don't bother to WAL-log it, since we can certainly
	 * do this again if the update gets lost.
	 */
	if (HeapTupleHeaderGetXmax(seqtuple->t_data) != InvalidTransactionId)
	{
		HeapTupleHeaderSetXmax(seqtuple->t_data, InvalidTransactionId);
		seqtuple->t_data->t_infomask &= ~HEAP_XMAX_COMMITTED;
		seqtuple->t_data->t_infomask |= HEAP_XMAX_INVALID;
		MarkBufferDirtyHint(*buf, rel);
	}

	seq = (Form_pg_sequence) GETSTRUCT(seqtuple);

	/* this is a handy place to update our copy of the increment */
	elm->increment = seq->increment_by;

	return seq;
}

/*
 * init_params: process the options list of CREATE or ALTER SEQUENCE,
 * and store the values into appropriate fields of *new.  Also set
 * *owned_by to any OWNED BY option, or to NIL if there is none.
 *
 * If isInit is true, fill any unspecified options with default values;
 * otherwise, do not change existing options that aren't explicitly overridden.
 */
static void
init_params(List *options, bool isInit,
			Form_pg_sequence new, List **owned_by)
{
	DefElem    *start_value = NULL;
	DefElem    *restart_value = NULL;
	DefElem    *increment_by = NULL;
	DefElem    *max_value = NULL;
	DefElem    *min_value = NULL;
	DefElem    *cache_value = NULL;
	DefElem    *is_cycled = NULL;
	ListCell   *option;

	*owned_by = NIL;

	foreach(option, options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "increment") == 0)
		{
			if (increment_by)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			increment_by = defel;
		}
		else if (strcmp(defel->defname, "start") == 0)
		{
			if (start_value)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			start_value = defel;
		}
		else if (strcmp(defel->defname, "restart") == 0)
		{
			if (restart_value)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			restart_value = defel;
		}
		else if (strcmp(defel->defname, "maxvalue") == 0)
		{
			if (max_value)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			max_value = defel;
		}
		else if (strcmp(defel->defname, "minvalue") == 0)
		{
			if (min_value)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			min_value = defel;
		}
		else if (strcmp(defel->defname, "cache") == 0)
		{
			if (cache_value)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cache_value = defel;
		}
		else if (strcmp(defel->defname, "cycle") == 0)
		{
			if (is_cycled)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			is_cycled = defel;
		}
		else if (strcmp(defel->defname, "owned_by") == 0)
		{
			if (*owned_by)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			*owned_by = defGetQualifiedName(defel);
		}
		else
			elog(ERROR, "option \"%s\" not recognized",
				 defel->defname);
	}

	/*
	 * We must reset log_cnt when isInit or when changing any parameters
	 * that would affect future nextval allocations.
	 */
	if (isInit)
		new->log_cnt = 0;

	/* INCREMENT BY */
	if (increment_by != NULL)
	{
		new->increment_by = defGetInt64(increment_by);
		if (new->increment_by == 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("INCREMENT must not be zero")));
		new->log_cnt = 0;
	}
	else if (isInit)
		new->increment_by = 1;

	/* CYCLE */
	if (is_cycled != NULL)
	{
		new->is_cycled = intVal(is_cycled->arg);
		Assert(new->is_cycled == false || new->is_cycled == true);
		new->log_cnt = 0;
	}
	else if (isInit)
		new->is_cycled = false;

	/* MAXVALUE (null arg means NO MAXVALUE) */
	if (max_value != NULL && max_value->arg)
	{
		new->max_value = defGetInt64(max_value);
		new->log_cnt = 0;
	}
	else if (isInit || max_value != NULL)
	{
		if (new->increment_by > 0)
			new->max_value = SEQ_MAXVALUE;		/* ascending seq */
		else
			new->max_value = -1;	/* descending seq */
		new->log_cnt = 0;
	}

	/* MINVALUE (null arg means NO MINVALUE) */
	if (min_value != NULL && min_value->arg)
	{
		new->min_value = defGetInt64(min_value);
		new->log_cnt = 0;
	}
	else if (isInit || min_value != NULL)
	{
		if (new->increment_by > 0)
			new->min_value = 1; /* ascending seq */
		else
			new->min_value = SEQ_MINVALUE;		/* descending seq */
		new->log_cnt = 0;
	}

	/* crosscheck min/max */
	if (new->min_value >= new->max_value)
	{
		char		bufm[100],
					bufx[100];

		snprintf(bufm, sizeof(bufm), INT64_FORMAT, new->min_value);
		snprintf(bufx, sizeof(bufx), INT64_FORMAT, new->max_value);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("MINVALUE (%s) must be less than MAXVALUE (%s)",
						bufm, bufx)));
	}

	/* START WITH */
	if (start_value != NULL)
		new->start_value = defGetInt64(start_value);
	else if (isInit)
	{
		if (new->increment_by > 0)
			new->start_value = new->min_value;	/* ascending seq */
		else
			new->start_value = new->max_value;	/* descending seq */
	}

	/* crosscheck START */
	if (new->start_value < new->min_value)
	{
		char		bufs[100],
					bufm[100];

		snprintf(bufs, sizeof(bufs), INT64_FORMAT, new->start_value);
		snprintf(bufm, sizeof(bufm), INT64_FORMAT, new->min_value);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("START value (%s) cannot be less than MINVALUE (%s)",
						bufs, bufm)));
	}
	if (new->start_value > new->max_value)
	{
		char		bufs[100],
					bufm[100];

		snprintf(bufs, sizeof(bufs), INT64_FORMAT, new->start_value);
		snprintf(bufm, sizeof(bufm), INT64_FORMAT, new->max_value);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			  errmsg("START value (%s) cannot be greater than MAXVALUE (%s)",
					 bufs, bufm)));
	}

	/* RESTART [WITH] */
	if (restart_value != NULL)
	{
		if (restart_value->arg != NULL)
			new->last_value = defGetInt64(restart_value);
		else
			new->last_value = new->start_value;
		new->is_called = false;
		new->log_cnt = 1;
	}
	else if (isInit)
	{
		new->last_value = new->start_value;
		new->is_called = false;
		new->log_cnt = 1;
	}

	/* crosscheck RESTART (or current value, if changing MIN/MAX) */
	if (new->last_value < new->min_value)
	{
		char		bufs[100],
					bufm[100];

		snprintf(bufs, sizeof(bufs), INT64_FORMAT, new->last_value);
		snprintf(bufm, sizeof(bufm), INT64_FORMAT, new->min_value);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("RESTART value (%s) cannot be less than MINVALUE (%s)",
					  bufs, bufm)));
	}
	if (new->last_value > new->max_value)
	{
		char		bufs[100],
					bufm[100];

		snprintf(bufs, sizeof(bufs), INT64_FORMAT, new->last_value);
		snprintf(bufm, sizeof(bufm), INT64_FORMAT, new->max_value);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("RESTART value (%s) cannot be greater than MAXVALUE (%s)",
				   bufs, bufm)));
	}

	/* CACHE */
	if (cache_value != NULL)
	{
		new->cache_value = defGetInt64(cache_value);
		if (new->cache_value <= 0)
		{
			char		buf[100];

			snprintf(buf, sizeof(buf), INT64_FORMAT, new->cache_value);
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("CACHE (%s) must be greater than zero",
							buf)));
		}
		new->log_cnt = 0;
	}
	else if (isInit)
		new->cache_value = 1;
}

/*
 * Process an OWNED BY option for CREATE/ALTER SEQUENCE
 *
 * Ownership permissions on the sequence are already checked,
 * but if we are establishing a new owned-by dependency, we must
 * enforce that the referenced table has the same owner and namespace
 * as the sequence.
 */
static void
process_owned_by(Relation seqrel, List *owned_by)
{
	int			nnames;
	Relation	tablerel;
	AttrNumber	attnum;

	nnames = list_length(owned_by);
	Assert(nnames > 0);
	if (nnames == 1)
	{
		/* Must be OWNED BY NONE */
		if (strcmp(strVal(linitial(owned_by)), "none") != 0)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid OWNED BY option"),
				errhint("Specify OWNED BY table.column or OWNED BY NONE.")));
		tablerel = NULL;
		attnum = 0;
	}
	else
	{
		List	   *relname;
		char	   *attrname;
		RangeVar   *rel;

		/* Separate relname and attr name */
		relname = list_truncate(list_copy(owned_by), nnames - 1);
		attrname = strVal(lfirst(list_tail(owned_by)));

		/* Open and lock rel to ensure it won't go away meanwhile */
		rel = makeRangeVarFromNameList(relname);
		tablerel = relation_openrv(rel, AccessShareLock);

		/* Must be a regular table */
		if (tablerel->rd_rel->relkind != RELKIND_RELATION)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("referenced relation \"%s\" is not a table",
							RelationGetRelationName(tablerel))));

		/* We insist on same owner and schema */
		if (seqrel->rd_rel->relowner != tablerel->rd_rel->relowner)
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("sequence must have same owner as table it is linked to")));
		if (RelationGetNamespace(seqrel) != RelationGetNamespace(tablerel))
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("sequence must be in same schema as table it is linked to")));

		/* Now, fetch the attribute number from the system cache */
		attnum = get_attnum(RelationGetRelid(tablerel), attrname);
		if (attnum == InvalidAttrNumber)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" of relation \"%s\" does not exist",
							attrname, RelationGetRelationName(tablerel))));
	}

	/*
	 * OK, we are ready to update pg_depend.  First remove any existing AUTO
	 * dependencies for the sequence, then optionally add a new one.
	 */
	markSequenceUnowned(RelationGetRelid(seqrel));

	if (tablerel)
	{
		ObjectAddress refobject,
					depobject;

		refobject.classId = RelationRelationId;
		refobject.objectId = RelationGetRelid(tablerel);
		refobject.objectSubId = attnum;
		depobject.classId = RelationRelationId;
		depobject.objectId = RelationGetRelid(seqrel);
		depobject.objectSubId = 0;
		recordDependencyOn(&depobject, &refobject, DEPENDENCY_AUTO);
	}

	/* Done, but hold lock until commit */
	if (tablerel)
		relation_close(tablerel, NoLock);
}


void
seq_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	Buffer		buffer;
	Page		page;
	char	   *item;
	Size		itemsz;
	xl_seq_rec *xlrec = (xl_seq_rec *) XLogRecGetData(record);
	sequence_magic *sm;

	/* Backup blocks are not used in seq records */
	Assert(!(record->xl_info & XLR_BKP_BLOCK_MASK));

	if (info != XLOG_SEQ_LOG)
		elog(PANIC, "seq_redo: unknown op code %u", info);

	buffer = XLogReadBuffer(xlrec->node, 0, true);
	Assert(BufferIsValid(buffer));
	page = (Page) BufferGetPage(buffer);

	/* Always reinit the page and reinstall the magic number */
	/* See comments in DefineSequence */
	PageInit((Page) page, BufferGetPageSize(buffer), sizeof(sequence_magic));
	sm = (sequence_magic *) PageGetSpecialPointer(page);
	sm->magic = SEQ_MAGIC;

	item = (char *) xlrec + sizeof(xl_seq_rec);
	itemsz = record->xl_len - sizeof(xl_seq_rec);

	if (PageAddItem(page, (Item) item, itemsz,
					FirstOffsetNumber, false, false) == InvalidOffsetNumber)
		elog(PANIC, "seq_redo: failed to add item to page");

	PageSetLSN(page, lsn);
	MarkBufferDirty(buffer);
	UnlockReleaseBuffer(buffer);
}

void
seq_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);
	xl_seq_rec *xlrec = (xl_seq_rec *) rec;

	if (info == XLOG_SEQ_LOG)
		appendStringInfo(buf, "log: ");
	else
	{
		appendStringInfo(buf, "UNKNOWN");
		return;
	}

	appendStringInfo(buf, "rel %u/%u/%u",
			   xlrec->node.spcNode, xlrec->node.dbNode, xlrec->node.relNode);
}


/*
 * Initialize a pseudo relcache entry with just enough info to call bufmgr.
 */
static void
cdb_sequence_relation_init(Relation seqrel,
                           Oid      tablespaceid,
                           Oid      dbid,
                           Oid      relid,
                           bool     istemp)
{
    /* See RelationBuildDesc in relcache.c */
    memset(seqrel, 0, sizeof(*seqrel));

    seqrel->rd_smgr = NULL;
    seqrel->rd_refcnt = 99;

    seqrel->rd_id = relid;
    seqrel->rd_istemp = istemp;

    /* Must use shared buffer pool so seqserver & QDs can see the data. */
    seqrel->rd_isLocalBuf = false;

	seqrel->rd_rel = (Form_pg_class)palloc0(CLASS_TUPLE_SIZE);
    sprintf(seqrel->rd_rel->relname.data, "pg_class.oid=%d", relid);

    /* as in RelationInitPhysicalAddr... */
    seqrel->rd_node.spcNode = tablespaceid;
    seqrel->rd_node.dbNode = dbid;
    seqrel->rd_node.relNode = relid;
}                               /* cdb_sequence_relation_init */

/*
 * Clean up pseudo relcache entry.
 */
static void
cdb_sequence_relation_term(Relation seqrel)
{
    /* Close the file. */
    RelationCloseSmgr(seqrel);

    if (seqrel->rd_rel)
        pfree(seqrel->rd_rel);
}                               /* cdb_sequence_relation_term */



/*
 * CDB: forward a nextval request from qExec to the sequence server
 */
void
cdb_sequence_nextval_proxy(Relation	seqrel,
                           int64   *plast,
                           int64   *pcached,
                           int64   *pincrement,
                           bool    *poverflow)
{

	sendSequenceRequest(GetSeqServerFD(),
						seqrel,
    					gp_session_id,
    					plast,
    					pcached,
    					pincrement,
    					poverflow);

}                               /* cdb_sequence_server_nextval */


/*
 * CDB: nextval entry point called by sequence server
 */
void
cdb_sequence_nextval_server(Oid    tablespaceid,
                            Oid    dbid,
                            Oid    relid,
                            bool   istemp,
                            int64 *plast,
                            int64 *pcached,
                            int64 *pincrement,
                            bool  *poverflow)
{
    RelationData    fakerel;
	SeqTable	elm;
	Relation	    seqrel = &fakerel;

    *plast = 0;
    *pcached = 0;
    *pincrement = 0;

	/*
	 * In Postgres, this method is to find the SeqTable entry for the sequence.
	 * This is not required by sequence server. We only need to initialize
	 * the `elm` which is used later in `cdb_sequence_nextval()`, which
	 * is calling `read_seq_tuple()` method, and require `elm` parameter.
	 *
	 * In GPDB, a sequence server is used to generate unique values for all the sequence.
	 * It doesn't have to lock on the sequence relation, because there will be
	 * only a single instance of sequence server to handle all the requests from
	 * segments to generate the sequence values.
	 * To prevent collision of generating sequence values between 'master'
	 * (e.g.`select nextval(seq)`) and 'segments' (e.g. `insert into table with
	 * serial column`), an BUFFER_LOCK_EXCLUSIVE lock is held on the shared buffer
	 * of the sequence relation.
	 */
	init_sequence(relid, &elm, NULL);

    /* Build a pseudo relcache entry with just enough info to call bufmgr. */
    seqrel = &fakerel;
    cdb_sequence_relation_init(seqrel, tablespaceid, dbid, relid, istemp);

    /* CDB TODO: Catch errors. */

    /* Update the sequence object. */
    cdb_sequence_nextval(elm, seqrel, plast, pcached, pincrement, poverflow);

    /* Cleanup. */
    cdb_sequence_relation_term(seqrel);
}                               /* cdb_sequence_server_nextval */

/*
 * Mask last_value and log_cnt for consistency checking
 *
 * To avoid logging every fetch from a sequence, SEQ_LOG_VALS are pre-logged
 * and thus we need to mask the last_value and log_cnt during consistency
 * checks.
 */
static void
mask_seq_values(Page page)
{
	OffsetNumber 		i;
	OffsetNumber 		maxoff;
	Form_pg_sequence	seqtup;

	maxoff = PageGetMaxOffsetNumber(page);

	for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
	{
		HeapTupleData	htup;
		ItemId			iid = PageGetItemId(page, i);

		htup.t_data = (HeapTupleHeader) ((char *) page + ItemIdGetOffset(iid));
		htup.t_len = ItemIdGetLength(iid);

		seqtup = (Form_pg_sequence) GETSTRUCT(&htup);
		MemSet(&seqtup->last_value, 0, sizeof(int64));
		MemSet(&seqtup->log_cnt, 0, sizeof(int64));
	}
}

/*
 * Mask a Sequence page before performing consistency checks on it.
 */
void
seq_mask(char *pagedata, BlockNumber blkno)
{
	Page		page = (Page) pagedata;

	mask_page_lsn_and_checksum(page);

	/*
	 * last_value and log_cnt need to be masked to account for SEQ_LOG_VALS
	 * skipped loggings of fetching
	 */
	mask_seq_values(page);

	mask_unused_space(page);
}
