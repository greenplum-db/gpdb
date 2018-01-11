/*
 * xlogdump_rmgr.c
 *
 * a collection of functions which print xlog records generated
 * by each resource manager.
 */
#include "xlogdump_rmgr.h"
#include "xlogdump.h"

#include <time.h>

#include "postgres.h"
#include "access/clog.h"
#include "access/gist_private.h"
#include "access/htup.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/xact.h"
#include "catalog/pg_control.h"
#include "commands/dbcommands.h"
#include "cdb/cdbappendonlyam.h"
#include "access/xlogmm.h"

#if PG_VERSION_NUM >=90000
#include "utils/relmapper.h"
#endif

#include "xlogdump_oid2name.h"
#include "xlogdump_statement.h"

/*
 * See access/tramsam/rmgr.c for more details.
 */
const char * const RM_names[RM_MAX_ID+1] = {
	"XLOG",						/* 0 */
	"Transaction",					/* 1 */
	"Storage",					/* 2 */
	"CLOG",						/* 3 */
	"Database",					/* 4 */
	"Tablespace",					/* 5 */
	"MultiXact",					/* 6 */
#if PG_VERSION_NUM >=90000
	"RelMap",					/* 7 */
	"Standby",					/* 8 */
#else
	"Reserved 7",					/* 7 */
	"Reserved 8",					/* 8 */
#endif
	"Heap2",					/* 9 */
	"Heap",						/* 10 */
	"Btree",					/* 11 */
	"Hash",						/* 12 */
	"Gin",						/* 13 */
	"Gist",						/* 14 */
	"Sequence",					/* 15 */
#if PG_VERSION_NUM >=90200
	"SPGist"					/* 16 */
#else
	"Reserved 16",
#endif	/* PG_VERSION_NUM >=90200 */
	"DistributedLog",			/* 17 */
	"MasterMirrorLog",			/* 18 */

#ifdef USE_SEGWALREP
	"Appendonly"				/* 19 */
#endif		/* USE_SEGWALREP */
};

/* copy from utils/timestamp.h */
#define SECS_PER_DAY	86400
#ifdef HAVE_INT64_TIMESTAMP
#define USECS_PER_DAY	INT64CONST(86400000000)
#endif

/* copy from utils/datetime.h */
#define UNIX_EPOCH_JDATE		2440588 /* == date2j(1970, 1, 1) */
#define POSTGRES_EPOCH_JDATE	2451545 /* == date2j(2000, 1, 1) */

static bool dump_enabled = true;

struct xlogdump_rmgr_stats_t {
	int xlog_checkpoint;
	int xlog_switch;
	int xlog_backup_end;
	int xact_commit;
	int xact_abort;
	int heap_insert;
	int heap_delete;
	int heap_update;
	int heap_hot_update;
	int heap_move;
	int heap_newpage;
	int heap_lock;
	int heap_inplace;
	int heap_init_page;
};

static struct xlogdump_rmgr_stats_t rmgr_stats;

static pg_time_t _timestamptz_to_time_t(TimestampTz);
static char *str_time(time_t);
static bool dump_xlog_btree_insert_meta(XLogRecord *);
/* GIST stuffs */
static void decodePageUpdateRecord(PageUpdateRecord *, XLogRecord *);
static void decodePageSplitRecord(PageSplitRecord *, XLogRecord *);

void
print_xlog_rmgr_stats(int rmid)
{
	switch (rmid)
	{
	case RM_XLOG_ID:
		printf("                 checkpoint: %d, switch: %d, backup end: %d\n",
		       rmgr_stats.xlog_checkpoint,
		       rmgr_stats.xlog_switch,
		       rmgr_stats.xlog_backup_end);
		break;

	case RM_XACT_ID:
		printf("                 commit: %d, abort: %d\n",
		       rmgr_stats.xact_commit,
		       rmgr_stats.xact_abort);
		break;

	case RM_HEAP_ID:
		printf("                 ins: %d, upd/hot_upd: %d/%d, del: %d\n",
		       rmgr_stats.heap_insert,
		       rmgr_stats.heap_update,
		       rmgr_stats.heap_hot_update,
		       rmgr_stats.heap_delete);
		break;
	}
}

/* copy from utils/adt/timestamp.c, and renamed because of the name conflict. */
static pg_time_t
_timestamptz_to_time_t(TimestampTz t)
{
  pg_time_t       result;

#ifdef HAVE_INT64_TIMESTAMP
  result = (pg_time_t) (t / USECS_PER_SEC +
			((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY));
#else
  result = (pg_time_t) (t +
			((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY));
#endif

  return result;
}

static char *
str_time(time_t tnow)
{
	static char buf[32];

	strftime(buf, sizeof(buf),
			 "%Y-%m-%d %H:%M:%S %Z",
			 localtime(&tnow));

	return buf;
}

void
enable_rmgr_dump(bool flag)
{
	dump_enabled = flag;
}

/*
 * a common part called by each `print_rmgr_*()' to print a xlog record header
 * with the detail.
 */
static void
print_rmgr_record(XLogRecPtr cur, XLogRecord *rec, const char *detail)
{
	if (!dump_enabled)
		return;

	PRINT_XLOGRECORD_HEADER(cur, rec);
	printf("%s\n", detail);
}

void
print_rmgr_xlog(XLogRecPtr cur, XLogRecord *record, uint8 info, bool hideTimestamps)
{
	char buf[1024];

	switch (info)
	{
	case XLOG_CHECKPOINT_SHUTDOWN:
	case XLOG_CHECKPOINT_ONLINE:
	{
		CheckPoint	*checkpoint = (CheckPoint*) XLogRecGetData(record);
		if(!hideTimestamps)
			snprintf(buf, sizeof(buf), "checkpoint: redo %u/%08X; tli %u; nextxid %u;"
			       "  nextoid %u; nextrelfilenode %u; nextmulti %u; nextoffset %u; %s at %s",
			       checkpoint->redo.xlogid, checkpoint->redo.xrecoff,
			       checkpoint->ThisTimeLineID, checkpoint->nextXid,
			       checkpoint->nextOid, checkpoint->nextRelfilenode,
			       checkpoint->nextMulti,
			       checkpoint->nextMultiOffset,
			       (info == XLOG_CHECKPOINT_SHUTDOWN) ?
			       "shutdown" : "online",
			       str_time(checkpoint->time));
		else
			snprintf(buf, sizeof(buf), "checkpoint: redo %u/%08X; tli %u; nextxid %u;"
			       "  nextoid %u; nextrelfilenode %u; nextmulti %u; nextoffset %u; %s",
			       checkpoint->redo.xlogid, checkpoint->redo.xrecoff,
			       checkpoint->ThisTimeLineID, checkpoint->nextXid,
			       checkpoint->nextOid, checkpoint->nextRelfilenode,
			       checkpoint->nextMulti,
			       checkpoint->nextMultiOffset,
			       (info == XLOG_CHECKPOINT_SHUTDOWN) ?
			       "shutdown" : "online");
		
		rmgr_stats.xlog_checkpoint++;
		break;
	}

#if PG_VERSION_NUM >= 80300
	case XLOG_NOOP:
	{
		snprintf(buf, sizeof(buf), "noop:");
		break;
	}
#endif

	case XLOG_NEXTOID:
	{
		Oid		nextOid;
		
		memcpy(&nextOid, XLogRecGetData(record), sizeof(Oid));
		snprintf(buf, sizeof(buf), "nextOid: %u", nextOid);
		break;
	}

	case XLOG_NEXTRELFILENODE:
	{
		Oid		nextRelfilenode;

		memcpy(&nextRelfilenode, XLogRecGetData(record), sizeof(Oid));
		snprintf(buf, sizeof(buf), "nextRelfilenode: %u", nextRelfilenode);
		break;
	}

	case XLOG_SWITCH:
	{
		snprintf(buf, sizeof(buf), "switch:");
		rmgr_stats.xlog_switch++;
		break;
	}

	case XLOG_BACKUP_END:
	{
		XLogRecPtr startpoint;

		memcpy(&startpoint, XLogRecGetData(record), sizeof(XLogRecPtr));
		snprintf(buf, sizeof(buf), "backup end: started at %X/%X.",
			 startpoint.xlogid, startpoint.xrecoff);

		rmgr_stats.xlog_backup_end++;
		break;
	}

#if PG_VERSION_NUM >= 90000
	case XLOG_PARAMETER_CHANGE:
	{
		snprintf(buf, sizeof(buf), "parameter change:");
		break;
	}
#endif

#if PG_VERSION_NUM >= 90100
	case XLOG_RESTORE_POINT:
	{
		snprintf(buf, sizeof(buf), "restore point:");
		break;
	}
#endif

#if PG_VERSION_NUM >= 90200
	case XLOG_FPW_CHANGE:
	{
		snprintf(buf, sizeof(buf), "full_page_write changed:");
		break;
	}
#endif

	default:
		snprintf(buf, sizeof(buf), "unknown XLOG operation - 0x%02x.", info);
		break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_xact(XLogRecPtr cur, XLogRecord *record, uint8 info, bool hideTimestamps)
{
	char buf[1024];

	memset(buf, 0, sizeof(buf));

	switch (info)
	{
	case XLOG_XACT_COMMIT:
		{
		xl_xact_commit	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
#if PG_VERSION_NUM >= 90000
		snprintf(buf, sizeof(buf), "d/s:%d/%d commit at %s",
			 xlrec.dbId, xlrec.tsId,
			 str_time(_timestamptz_to_time_t(xlrec.xact_time)));
#elif PG_VERSION_NUM >= 80300
		snprintf(buf, sizeof(buf), "commit at %s",
			 str_time(_timestamptz_to_time_t(xlrec.xact_time)));
#else
		snprintf(buf, sizeof(buf), "commit at %s",
			 str_time(_timestamptz_to_time_t(xlrec.xtime)));
#endif
		}
		rmgr_stats.xact_commit++;
		break;

	case XLOG_XACT_PREPARE:
		snprintf(buf, sizeof(buf), "prepare");
		break;

	case XLOG_XACT_ABORT:
		{
		xl_xact_abort	xlrec;
		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
		snprintf(buf, sizeof(buf), "abort at %s",
#if PG_VERSION_NUM >= 80300
			 str_time(_timestamptz_to_time_t(xlrec.xact_time)));
#else
			 str_time(_timestamptz_to_time_t(xlrec.xtime)));
#endif
		}
		rmgr_stats.xact_abort++;
		break;

	case XLOG_XACT_COMMIT_PREPARED:
		{
		xl_xact_commit_prepared	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
#if PG_VERSION_NUM >= 90000
		snprintf(buf, sizeof(buf), "commit prepared xid:%d, dxid:%d, dtimeStamp:%d dbid:%d, spcid:%d, commit at %s",
				 xlrec.xid,
				 xlrec.distribXid,
				 xlrec.distribTimeStamp,
				 xlrec.crec.dbId, xlrec.crec.tsId,
				 str_time(_timestamptz_to_time_t(xlrec.crec.xact_time)));
#elif PG_VERSION_NUM >= 80300
		snprintf(buf, sizeof(buf), "commit prepared xid:%d, dxid:%d, dtimeStamp:%d commit at %s",
				 xlrec.xid,
				 xlrec.distribXid,
				 xlrec.distribTimeStamp,
				 str_time(_timestamptz_to_time_t(xlrec.crec.xact_time)));
#else
		snprintf(buf, sizeof(buf), "commit prepared xid:%d, dxid:%d, dtimeStamp:%d commit at %s",
				 xlrec.xid,
				 xlrec.distribXid,
				 xlrec.distribTimeStamp,
				 str_time(_timestamptz_to_time_t(xlrec.crec.xtime)));
#endif
		}
		break;

	case XLOG_XACT_ABORT_PREPARED:
		{
		xl_xact_commit_prepared	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
#if PG_VERSION_NUM >= 90000
		snprintf(buf, sizeof(buf), "abort prepared xid:%d, dbid:%d, spcid:%d, commit at %s",
			 xlrec.xid,
			 xlrec.crec.dbId, xlrec.crec.tsId,
			 str_time(_timestamptz_to_time_t(xlrec.crec.xact_time)));
#elif PG_VERSION_NUM >= 80300
		snprintf(buf, sizeof(buf), "abort prepared xid:%d, commit at %s",
			 xlrec.xid,
			 str_time(_timestamptz_to_time_t(xlrec.crec.xact_time)));
#else
		snprintf(buf, sizeof(buf), "abort prepared xid:%d, commit at %s",
			 xlrec.xid,
			 str_time(_timestamptz_to_time_t(xlrec.crec.xtime)));
#endif
		}
		break;

#if PG_VERSION_NUM >= 90000
	case XLOG_XACT_ASSIGNMENT:
		{
		xl_xact_assignment	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
		snprintf(buf, sizeof(buf), "assignment xtop:%d, nsubxacts:%d",
			 xlrec.xtop,
			 xlrec.nsubxacts);
		}
		break;
#endif

#if PG_VERSION_NUM >= 90200
	case XLOG_XACT_COMMIT_COMPACT:
		{
		xl_xact_commit_compact	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
#ifdef HAVE_INT64_TIMESTAMP
		snprintf(buf, sizeof(buf), "commit_compact xact_time:" INT64_FORMAT ", nsubxacts:%d",
			 xlrec.xact_time,
			 xlrec.nsubxacts);
#else
		snprintf(buf, sizeof(buf), "commit_compact xact_time:%f, nsubxacts:%d",
			 xlrec.xact_time,
			 xlrec.nsubxacts);
#endif
		}
		break;
#endif

	default:
		snprintf(buf, sizeof(buf), "unknown XACT operation - %d.", info);
		break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_smgr(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];
	char buf[1024];

	switch (info)
	{
	case XLOG_SMGR_CREATE:
		{
		xl_smgr_create	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
		getSpaceName(xlrec.rnode.spcNode, spaceName, sizeof(spaceName));
		getDbName(xlrec.rnode.dbNode, dbName, sizeof(dbName));
		getRelName(xlrec.rnode.relNode, relName, sizeof(relName));
		snprintf(buf, sizeof(buf), "create rel: s/d/r:%s/%s/%s", 
			spaceName, dbName, relName);
		}
		break;

	case XLOG_SMGR_TRUNCATE:
		{
		xl_smgr_truncate	xlrec;

		memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
		getSpaceName(xlrec.rnode.spcNode, spaceName, sizeof(spaceName));
		getDbName(xlrec.rnode.dbNode, dbName, sizeof(dbName));
		getRelName(xlrec.rnode.relNode, relName, sizeof(relName));
		snprintf(buf, sizeof(buf), "truncate rel: s/d/r:%s/%s/%s at block %u",
			 spaceName, dbName, relName, xlrec.blkno);
		}
		break;

	default:
		snprintf(buf, sizeof(buf), "unknown SMGR operation - %d.", info);
		break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_clog(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char buf[1024];

	int		pageno;
	memcpy(&pageno, XLogRecGetData(record), sizeof(int));

	switch (info)
	{
	case CLOG_ZEROPAGE:
		snprintf(buf, sizeof(buf), "zero clog page 0x%04x", pageno);
		break;

	case CLOG_TRUNCATE:
		snprintf(buf, sizeof(buf), "truncate clog page 0x%04x", pageno);
		break;

	default:
		snprintf(buf, sizeof(buf), "unknown CLOG operation - %d.", info);
		break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_dbase(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char buf[1024];

	/* FIXME: need to be implemented. */
	switch (info)
	{
	case XLOG_DBASE_CREATE:
	  {
	    xl_dbase_create_rec *xlrec = (xl_dbase_create_rec *)XLogRecGetData(record);
	    snprintf(buf, sizeof(buf), "dbase_create: db_id:%d, tablespace_id:%d, src_db_id:%d, src_tablespace_id:%d",
		     xlrec->db_id,
		     xlrec->tablespace_id,
		     xlrec->src_db_id,
		     xlrec->src_tablespace_id);
	  }
	  break;

#if 0 /* Doesn't exist in GPDB */
	case XLOG_DBASE_DROP:
	  {
	    xl_dbase_drop_rec *xlrec = (xl_dbase_drop_rec *)XLogRecGetData(record);
	    snprintf(buf, sizeof(buf), "dbase_drop: db_id:%d, tablespace_id:%d",
		     xlrec->db_id,
		     xlrec->tablespace_id);
	  }
	  break;
#endif

	default:
		snprintf(buf, sizeof(buf), "unknown DBASE operation - %d.", info);
		break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_tblspc(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	/* FIXME: need to be implemented. */
	print_rmgr_record(cur, record, "tblspc");
}

void
print_rmgr_multixact(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char buf[1024];

	switch (info & XLOG_HEAP_OPMASK)
	{
		case XLOG_MULTIXACT_ZERO_OFF_PAGE:
		{
			int		pageno;

			memcpy(&pageno, XLogRecGetData(record), sizeof(int));
			snprintf(buf, sizeof(buf), "zero offset page 0x%04x", pageno);
			break;
		}
		case XLOG_MULTIXACT_ZERO_MEM_PAGE:
		{
			int		pageno;

			memcpy(&pageno, XLogRecGetData(record), sizeof(int));
			snprintf(buf, sizeof(buf), "zero members page 0x%04x", pageno);
			break;
		}
		case XLOG_MULTIXACT_CREATE_ID:
		{
			xl_multixact_create xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			snprintf(buf, sizeof(buf), "multixact create: %u off %u nxids %u",
				   xlrec.mid,
				   xlrec.moff,
				   xlrec.nxids);
			break;
		}
		default:
			snprintf(buf, sizeof(buf), "unknown MULTIXACT operation - %d.", info & XLOG_HEAP_OPMASK);
			break;
	}

	print_rmgr_record(cur, record, buf);
}

#if PG_VERSION_NUM >= 90000

void
print_rmgr_relmap(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char buf[1024];

	switch (info)
	{
		case XLOG_RELMAP_UPDATE:
		{
			xl_relmap_update *xlrec = (xl_relmap_update *)XLogRecGetData(record);
			snprintf(buf, sizeof(buf), "update: dbid:%d, tsid:%d, nbytes:%d",
				 xlrec->dbid,
				 xlrec->tsid,
				 xlrec->nbytes);
			break;
		}

		default:
			snprintf(buf, sizeof(buf), "unknown RELMAP operation - %d.", info);
			break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_standby(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	/* FIXME: need to be implemented. */
	print_rmgr_record(cur, record, "standby");
}

#endif 

void
print_rmgr_heap2(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];

	char buf[1024];

	switch (info)
	{
		case XLOG_HEAP2_FREEZE:
		{
			xl_heap_freeze xlrec;
			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			snprintf(buf, sizeof(buf), "freeze: ts %d db %d rel %d block %d cutoff_xid %d",
				xlrec.heapnode.node.spcNode,
				xlrec.heapnode.node.dbNode,
				xlrec.heapnode.node.relNode,
				xlrec.block, xlrec.cutoff_xid
			);
		}
		break;

#if PG_VERSION_NUM >= 80300
		case XLOG_HEAP2_CLEAN:
#if PG_VERSION_NUM < 90000
		case XLOG_HEAP2_CLEAN_MOVE:
#endif
		{
			xl_heap_clean xlrec;
			int total_off;
			int nunused = 0;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));

#if PG_VERSION_NUM >= 90000
			getSpaceName(xlrec.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.node.relNode, relName, sizeof(relName));
#else
			getSpaceName(xlrec.heapnode.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.heapnode.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.heapnode.node.relNode, relName, sizeof(relName));
#endif

			total_off = (record->xl_len - SizeOfHeapClean) / sizeof(OffsetNumber);

			if (total_off > xlrec.nredirected + xlrec.ndead)
				nunused = total_off - (xlrec.nredirected + xlrec.ndead);

#if PG_VERSION_NUM >= 90000
			snprintf(buf, sizeof(buf), "clean%s: s/d/r:%s/%s/%s block:%u redirected/dead/unused:%d/%d/%d removed xid:%d",
			       "",
			       spaceName, dbName, relName,
#else
			snprintf(buf, sizeof(buf), "clean%s: s/d/r:%s/%s/%s block:%u redirected/dead/unused:%d/%d/%d",
			       info == XLOG_HEAP2_CLEAN_MOVE ? "_move" : "",
					 spaceName, dbName, relName,
#endif
			       xlrec.block,
			       xlrec.nredirected, xlrec.ndead, nunused
#if PG_VERSION_NUM >= 90000
			       , xlrec.latestRemovedXid
#endif
			       );
			break;
		}
		break;

#if PG_VERSION_NUM >= 90000
		case XLOG_HEAP2_CLEANUP_INFO:
		{
			xl_heap_cleanup_info xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "cleanup_info: s/d/r:%s/%s/%s removed xid:%d",
				 spaceName, dbName, relName,
				 xlrec.latestRemovedXid);
		}
		break;
#endif

#if PG_VERSION_NUM >= 90200
		case XLOG_HEAP2_VISIBLE:
		{
			snprintf(buf, sizeof(buf), "heap2_visible:");
		}
		break;

		case XLOG_HEAP2_MULTI_INSERT:
		{
			snprintf(buf, sizeof(buf), "heap2_multi_insert:");
		}
		break;
#endif

#endif

		default:
			snprintf(buf, sizeof(buf), "unknown HEAP2 operation - %d.", info);
			break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_heap(XLogRecPtr cur, XLogRecord *record, uint8 info, bool statements)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];
	char buf[1024];

	switch (info & XLOG_HEAP_OPMASK)
	{
		case XLOG_HEAP_INSERT:
		{
			xl_heap_insert xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));

			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			if(statements)
				printInsert((xl_heap_insert *) XLogRecGetData(record), record->xl_len - SizeOfHeapInsert - SizeOfHeapHeader, relName);

			snprintf(buf, sizeof(buf), "insert%s: s/d/r:%s/%s/%s blk/off:%u/%u",
				   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
				   spaceName, dbName, relName,
				   ItemPointerGetBlockNumber(&xlrec.target.tid),
				   ItemPointerGetOffsetNumber(&xlrec.target.tid));
			/* If backup block doesn't exist, dump rmgr data. */
			if (!(record->xl_info & XLR_BKP_BLOCK_MASK))
			{
				char buf2[1024];

				xl_heap_header *header = (xl_heap_header *)
					(XLogRecGetData(record) + SizeOfHeapInsert);
#if PG_VERSION_NUM >= 80300
				snprintf(buf2, sizeof(buf2), " header: t_infomask2 %d t_infomask %d t_hoff %d",
					header->t_infomask2,
					header->t_infomask,
					header->t_hoff);
#else
				snprintf(buf2, sizeof(buf2), " header: t_infomask %d t_hoff %d",
					header->t_infomask,
					header->t_hoff);
#endif

				strlcat(buf, buf2, sizeof(buf));
			}
			else
				strlcat(buf, " header: none", sizeof(buf));

			rmgr_stats.heap_insert++;
			break;
		}
		case XLOG_HEAP_DELETE:
		{
			xl_heap_delete xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));
					
			if(statements)
				printf("DELETE FROM %s WHERE ...", relName);
					
			snprintf(buf, sizeof(buf), "delete%s: s/d/r:%s/%s/%s block %u off %u",
				   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
				   spaceName, dbName, relName,
				   ItemPointerGetBlockNumber(&xlrec.target.tid),
				   ItemPointerGetOffsetNumber(&xlrec.target.tid));

			rmgr_stats.heap_delete++;
			break;
		}
		case XLOG_HEAP_UPDATE:
#if PG_VERSION_NUM >= 80300
		case XLOG_HEAP_HOT_UPDATE:
#endif
		{
			xl_heap_update xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			if(statements)
				printUpdate((xl_heap_update *) XLogRecGetData(record), record->xl_len - SizeOfHeapUpdate - SizeOfHeapHeader, relName);

			snprintf(buf, sizeof(buf), "%supdate%s: s/d/r:%s/%s/%s block %u off %u to block %u off %u",
#if PG_VERSION_NUM >= 80300
				   (info & XLOG_HEAP_HOT_UPDATE) ? "hot_" : "",
#else
				   "",
#endif
				   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
				   spaceName, dbName, relName,
				   ItemPointerGetBlockNumber(&xlrec.target.tid),
				   ItemPointerGetOffsetNumber(&xlrec.target.tid),
				   ItemPointerGetBlockNumber(&xlrec.newtid),
				   ItemPointerGetOffsetNumber(&xlrec.newtid));

			if ((info & XLOG_HEAP_OPMASK) == XLOG_HEAP_UPDATE)
				rmgr_stats.heap_update++;
			else
				rmgr_stats.heap_hot_update++;

			break;
		}
#if PG_VERSION_NUM < 90000
		case XLOG_HEAP_MOVE:
		{
			xl_heap_update xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));
			snprintf(buf, sizeof(buf), "move%s: s/d/r:%s/%s/%s block %u off %u to block %u off %u",
				   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
				   spaceName, dbName, relName,
				   ItemPointerGetBlockNumber(&xlrec.target.tid),
				   ItemPointerGetOffsetNumber(&xlrec.target.tid),
				   ItemPointerGetBlockNumber(&xlrec.newtid),
				   ItemPointerGetOffsetNumber(&xlrec.newtid));

			rmgr_stats.heap_move++;
			break;
		}
#endif
		case XLOG_HEAP_NEWPAGE:
		{
			xl_heap_newpage xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.heapnode.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.heapnode.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.heapnode.node.relNode, relName, sizeof(relName));
			snprintf(buf, sizeof(buf), "newpage: s/d/r:%s/%s/%s block %u", 
					spaceName, dbName, relName,
				   xlrec.blkno);

			rmgr_stats.heap_newpage++;
			break;
		}
		case XLOG_HEAP_LOCK:
		{
			xl_heap_lock xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));
			snprintf(buf, sizeof(buf), "lock %s: s/d/r:%s/%s/%s block %u off %u",
				   xlrec.shared_lock ? "shared" : "exclusive",
				   spaceName, dbName, relName,
				   ItemPointerGetBlockNumber(&xlrec.target.tid),
				   ItemPointerGetOffsetNumber(&xlrec.target.tid));

			rmgr_stats.heap_lock++;
			break;
		}

		case XLOG_HEAP_INPLACE:
		{
			xl_heap_inplace xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));
			snprintf(buf, sizeof(buf), "inplace: s/d/r:%s/%s/%s block %u off %u", 
					spaceName, dbName, relName,
				   	ItemPointerGetBlockNumber(&xlrec.target.tid),
				   	ItemPointerGetOffsetNumber(&xlrec.target.tid));

			rmgr_stats.heap_inplace++;
			break;
		}

		case XLOG_HEAP_INIT_PAGE:
		{
			snprintf(buf, sizeof(buf), "init page");
			rmgr_stats.heap_init_page++;
			break;
		}

		default:
			snprintf(buf, sizeof(buf), "unknown HEAP operation - %d.", (info & XLOG_HEAP_OPMASK));
			break;
	}

	print_rmgr_record(cur, record, buf);
}

static bool
dump_xlog_btree_insert_meta(XLogRecord *record)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];

	xl_btree_insert *xlrec = (xl_btree_insert *) XLogRecGetData(record);
	char *datapos;
	int datalen;
	xl_btree_metadata md;
	BlockNumber downlink;

	/* copied from btree_xlog_insert(nbtxlog.c:191) */
	datapos = (char *) xlrec + SizeOfBtreeInsert;
	datalen = record->xl_len - SizeOfBtreeInsert;

	if ( getSpaceName(xlrec->target.node.spcNode, spaceName, sizeof(spaceName))==NULL ||
	     getDbName(xlrec->target.node.dbNode, dbName, sizeof(dbName))==NULL ||
	     getRelName(xlrec->target.node.relNode, relName, sizeof(relName))==NULL )
		return false;

	/* downlink */
	memcpy(&downlink, datapos, sizeof(BlockNumber));
	datapos += sizeof(BlockNumber);
	datalen -= sizeof(BlockNumber);

	/* xl_insert_meta */
	memcpy(&md, datapos, sizeof(xl_btree_metadata));
	datapos += sizeof(xl_btree_metadata);
	datalen -= sizeof(xl_btree_metadata);

	printf("insert_meta: index %s/%s/%s tid %u/%u downlink %u froot %u/%u\n", 
		spaceName, dbName, relName,
		BlockIdGetBlockNumber(&xlrec->target.tid.ip_blkid),
		xlrec->target.tid.ip_posid,
		downlink,
		md.fastroot, md.fastlevel
	);

	return true;
}

void
print_rmgr_btree(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];
	char buf[1024];

	switch (info)
	{
		case XLOG_BTREE_INSERT_LEAF:
		{
			xl_btree_insert xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "insert_leaf: s/d/r:%s/%s/%s tid %u/%u",
					spaceName, dbName, relName,
				   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
				   	xlrec.target.tid.ip_posid);
			break;
		}
		case XLOG_BTREE_INSERT_UPPER:
		{
			xl_btree_insert xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "insert_upper: s/d/r:%s/%s/%s tid %u/%u",
					spaceName, dbName, relName,
				   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
				   	xlrec.target.tid.ip_posid);
			break;
		}
		case XLOG_BTREE_INSERT_META:
			dump_xlog_btree_insert_meta(record);
			/* FIXME: need to check the result code. */
			break;
		case XLOG_BTREE_SPLIT_L:
		case XLOG_BTREE_SPLIT_L_ROOT:
		{
			char *datapos = XLogRecGetData(record);
			xl_btree_split xlrec;
			OffsetNumber newitemoff;
			char buf2[1024];

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			datapos += SizeOfBtreeSplit;
#if PG_VERSION_NUM >= 80300
			getSpaceName(xlrec.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.node.relNode, relName, sizeof(relName));
#endif

#if PG_VERSION_NUM >= 80300
			snprintf(buf, sizeof(buf), "split_l%s: s/d/r:%s/%s/%s"
				 " lsib %u rsib %u rnext %u level %u firstright %u",
				info == XLOG_BTREE_SPLIT_L_ROOT ? "_root" : "",
				spaceName, dbName, relName,
				xlrec.leftsib, xlrec.rightsib,
				xlrec.rnext, xlrec.level, xlrec.firstright);
#else
			snprintf(buf, sizeof(buf), "split_l%s: rightblk %u"
				 " lblk %u rblk %u level %u",
				info == XLOG_BTREE_SPLIT_L_ROOT ? "_root" : "",
				xlrec.rightblk,
				xlrec.leftblk, xlrec.rightblk,
				xlrec.level);
#endif

			/* downlinks */
			if (xlrec.level > 0)
			{
				BlockIdData downlink;
				memcpy(&downlink, XLogRecGetData(record) + SizeOfBtreeSplit, sizeof(downlink));
				datapos += sizeof(downlink);
				printf("downlink: %u\n",
					BlockIdGetBlockNumber(&downlink));
			}
			/* newitemoff */
			memcpy(&newitemoff, datapos, sizeof(OffsetNumber));
			datapos += sizeof(OffsetNumber);

			snprintf(buf2, sizeof(buf2), " newitemoff: %u", newitemoff);
			strlcat(buf, buf2, sizeof(buf));

			/* newitem (only when bkpblock1 is not recorded) */
			if (!(record->xl_info & XLR_BKP_BLOCK_1))
			{
				IndexTuple newitem = (IndexTuple)datapos;
				snprintf(buf2, sizeof(buf2), " newitem: { block %u pos 0x%x }",
					BlockIdGetBlockNumber(&newitem->t_tid.ip_blkid),
					newitem->t_tid.ip_posid);
				strlcat(buf, buf2, sizeof(buf));
			}
			/* items in right page should be here */
			break;
		}
		case XLOG_BTREE_SPLIT_R:
		case XLOG_BTREE_SPLIT_R_ROOT:
		{
			xl_btree_split xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
#if PG_VERSION_NUM >= 80300
			getSpaceName(xlrec.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.node.relNode, relName, sizeof(relName));
#endif

#if PG_VERSION_NUM >= 80300
			snprintf(buf, sizeof(buf), "split_r%s: s/d/r:%s/%s/%s"
				 " lsib %u rsib %u rnext %u level %u firstright %u",
				info == XLOG_BTREE_SPLIT_L_ROOT ? "_root" : "",
				spaceName, dbName, relName,
				xlrec.leftsib, xlrec.rightsib,
				xlrec.rnext, xlrec.level, xlrec.firstright);
#else
			snprintf(buf, sizeof(buf), "split_r%s: leftblk %u\n", 
					info == XLOG_BTREE_SPLIT_R_ROOT ? "_root" : "",
					xlrec.leftblk);
#endif
			break;
		}
		case XLOG_BTREE_DELETE:
		{
			xl_btree_delete xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.btreenode.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.btreenode.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.btreenode.node.relNode, relName, sizeof(relName));
			snprintf(buf, sizeof(buf), "delete: s/d/r:%s/%s/%s block %u", 
					spaceName, dbName,	relName,
				   	xlrec.block);
			break;
		}
		case XLOG_BTREE_DELETE_PAGE:
		{
			xl_btree_delete_page xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "delete_page: s/d/r:%s/%s/%s tid %u/%u deadblk %u",
					spaceName, dbName, relName,
				   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
				   	xlrec.target.tid.ip_posid,
				   	xlrec.deadblk);
			break;
		}
		case XLOG_BTREE_DELETE_PAGE_META:
		{
			xl_btree_delete_page xlrec;
			xl_btree_metadata md;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			memcpy(&md, XLogRecGetData(record) + sizeof(xlrec),
				sizeof(xl_btree_metadata));

			snprintf(buf, sizeof(buf), "delete_page_meta: s/d/r:%s/%s/%s tid %u/%u deadblk %u root %u/%u froot %u/%u", 
					spaceName, dbName, relName,
				   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
				   	xlrec.target.tid.ip_posid,
				   	xlrec.deadblk,
					md.root, md.level, md.fastroot, md.fastlevel);
			break;
		}
		case XLOG_BTREE_NEWROOT:
		{
			xl_btree_newroot xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.btreenode.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.btreenode.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.btreenode.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "newroot: s/d/r:%s/%s/%s rootblk %u level %u", 
					spaceName, dbName, relName,
				   	xlrec.rootblk, xlrec.level);
			break;
		}
		case XLOG_BTREE_DELETE_PAGE_HALF:
		{
			xl_btree_delete_page xlrec;

			memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
			getSpaceName(xlrec.target.node.spcNode, spaceName, sizeof(spaceName));
			getDbName(xlrec.target.node.dbNode, dbName, sizeof(dbName));
			getRelName(xlrec.target.node.relNode, relName, sizeof(relName));

			snprintf(buf, sizeof(buf), "delete_page_half: s/d/r:%s/%s/%s tid %u/%u deadblk %u",
					spaceName, dbName, relName,
				   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
				   	xlrec.target.tid.ip_posid,
				   	xlrec.deadblk);
			break;
		}

#if PG_VERSION_NUM >= 90000
		case XLOG_BTREE_VACUUM:
		{
			snprintf(buf, sizeof(buf), "vacuum: ");
			break;
		}

		case XLOG_BTREE_REUSE_PAGE:
		{
			snprintf(buf, sizeof(buf), "reuse_page: ");
			break;
		}
#endif

		default:
			snprintf(buf, sizeof(buf), "unkonwn BTREE operation - %d.", info);
			break;
	}

	print_rmgr_record(cur, record, buf);
}

void
print_rmgr_hash(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	/* FIXME: need to be implemented. */
	print_rmgr_record(cur, record, "hash");
}

/* copied from backend/access/gist/gistxlog.c */
static void
decodePageUpdateRecord(PageUpdateRecord *decoded, XLogRecord *record)
{
	char	   *begin = XLogRecGetData(record),
			   *ptr;
	int			i = 0,
				addpath = 0;

	decoded->data = (gistxlogPageUpdate *) begin;

	if (decoded->data->ntodelete)
	{
		decoded->todelete = (OffsetNumber *) (begin + sizeof(gistxlogPageUpdate) + addpath);
		addpath = MAXALIGN(sizeof(OffsetNumber) * decoded->data->ntodelete);
	}
	else
		decoded->todelete = NULL;

	decoded->len = 0;
	ptr = begin + sizeof(gistxlogPageUpdate) + addpath;
	while (ptr - begin < record->xl_len)
	{
		decoded->len++;
		ptr += IndexTupleSize((IndexTuple) ptr);
	}

	decoded->itup = (IndexTuple *) malloc(sizeof(IndexTuple) * decoded->len);

	ptr = begin + sizeof(gistxlogPageUpdate) + addpath;
	while (ptr - begin < record->xl_len)
	{
		decoded->itup[i] = (IndexTuple) ptr;
		ptr += IndexTupleSize(decoded->itup[i]);
		i++;
	}
}

/* copied from backend/access/gist/gistxlog.c */
static void
decodePageSplitRecord(PageSplitRecord *decoded, XLogRecord *record)
{
	char	   *begin = XLogRecGetData(record),
			   *ptr;
	int			j,
				i = 0;

	decoded->data = (gistxlogPageSplit *) begin;
	decoded->page = (NewPage *) malloc(sizeof(NewPage) * decoded->data->npage);

	ptr = begin + sizeof(gistxlogPageSplit);
	for (i = 0; i < decoded->data->npage; i++)
	{
		Assert(ptr - begin < record->xl_len);
		decoded->page[i].header = (gistxlogPage *) ptr;
		ptr += sizeof(gistxlogPage);

		decoded->page[i].itup = (IndexTuple *)
			malloc(sizeof(IndexTuple) * decoded->page[i].header->num);
		j = 0;
		while (j < decoded->page[i].header->num)
		{
			Assert(ptr - begin < record->xl_len);
			decoded->page[i].itup[j] = (IndexTuple) ptr;
			ptr += IndexTupleSize((IndexTuple) ptr);
			j++;
		}
	}
}

void
print_rmgr_gin(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	/* FIXME: need to be implemented. */
	print_rmgr_record(cur, record, "gin");
}

void
print_rmgr_gist(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	print_rmgr_record(cur, record, "git");

	switch (info)
	{
		case XLOG_GIST_PAGE_UPDATE:
#if PG_VERSION_NUM < 90100
		case XLOG_GIST_NEW_ROOT:
#endif
			{
				int i;
				PageUpdateRecord rec;
				decodePageUpdateRecord(&rec, record);

#if PG_VERSION_NUM >= 90100
				printf("%s: rel=(%u/%u/%u) blk=%u leftchild=%d add=%d ntodelete=%d\n",
					info == XLOG_GIST_PAGE_UPDATE ? "page_update" : "new_root",
					rec.data->node.spcNode, rec.data->node.dbNode,
					rec.data->node.relNode,
					rec.data->blkno,
					rec.data->leftchild,
					rec.len,
					rec.data->ntodelete
				);
#else
				printf("%s: rel=(%u/%u/%u) blk=%u key=(%d,%d) add=%d ntodelete=%d\n",
					info == XLOG_GIST_PAGE_UPDATE ? "page_update" : "new_root",
					rec.data->node.spcNode, rec.data->node.dbNode,
					rec.data->node.relNode,
					rec.data->blkno,
					ItemPointerGetBlockNumber(&rec.data->key),
					rec.data->key.ip_posid,
					rec.len,
					rec.data->ntodelete
				);
#endif
				for (i = 0; i < rec.len; i++)
				{
					printf("  itup[%d] points (%d, %d)\n",
						i,
						ItemPointerGetBlockNumber(&rec.itup[i]->t_tid),
						rec.itup[i]->t_tid.ip_posid
					);
				}
				for (i = 0; i < rec.data->ntodelete; i++)
				{
					printf("  todelete[%d] offset %d\n", i, rec.todelete[i]);
				}
				free(rec.itup);
			}
			break;
		case XLOG_GIST_PAGE_SPLIT:
			{
				int i;
				PageSplitRecord rec;

				decodePageSplitRecord(&rec, record);
#if PG_VERSION_NUM >= 90100
				printf("page_split: orig %u leftchild %d\n",
					rec.data->origblkno,
					rec.data->leftchild
				);
#else
				printf("page_split: orig %u key (%d,%d)\n",
					rec.data->origblkno,
					ItemPointerGetBlockNumber(&rec.data->key),
					rec.data->key.ip_posid
				);
#endif
				for (i = 0; i < rec.data->npage; i++)
				{
					printf("  page[%d] block %u tuples %d\n",
						i,
						rec.page[i].header->blkno,
						rec.page[i].header->num
					);
#if 0
					for (int j = 0; j < rec.page[i].header->num; j++)
					{
						NewPage *newpage = rec.page + i;
						printf("   itup[%d] points (%d,%d)\n",
							j,
							BlockIdGetBlockNumber(&newpage->itup[j]->t_tid.ip_blkid),
							newpage->itup[j]->t_tid.ip_posid
						);
					}
#endif
					free(rec.page[i].itup);
				}
			}
			break;
#if PG_VERSION_NUM < 90100
		case XLOG_GIST_INSERT_COMPLETE:
			{
				printf("insert_complete: \n");
			}
			break;
#endif
		case XLOG_GIST_CREATE_INDEX:
			printf("create_index: \n");
			break;
		case XLOG_GIST_PAGE_DELETE:
			printf("page_delete: \n");
			break;
	}
}

void
print_rmgr_seq(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	/* FIXME: need to be implemented. */
	print_rmgr_record(cur, record, "seq");
}

void
print_rmgr_mmxlog(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	xl_mm_fs_obj *xlrec = (xl_mm_fs_obj *)XLogRecGetData(record);
	char 		  buf[1024];
	char		  operation[256];
	char 		  objectDetails[256];

	switch (info)
	{
	case MMXLOG_CREATE_DIR:
		strlcpy(operation, "create dir", sizeof(operation));
		break;

	case MMXLOG_CREATE_FILE:
		strlcpy(operation, "create file", sizeof(operation));
		break;

	case MMXLOG_REMOVE_DIR:
		strlcpy(operation, "remove dir", sizeof(operation));
		break;

	case MMXLOG_REMOVE_APPENDONLY_FILE:
		strlcpy(operation, "remove appendonly file", sizeof(operation));
		break;

	case MMXLOG_REMOVE_HEAP_FILE:
		strlcpy(operation, "remove heap file", sizeof(operation));
		break;

	default:
		snprintf(operation, sizeof(operation),
				 "unknown MMX operation - 0x%x", info);
		break;
	}

	switch (xlrec->objtype)
	{
	case MM_OBJ_FILESPACE:
		snprintf(objectDetails, sizeof(objectDetails), "filespace %u",
				 xlrec->filespace);
		break;

	case MM_OBJ_TABLESPACE:
		snprintf(objectDetails, sizeof(objectDetails), "tablespace %u",
				 xlrec->tablespace);
		break;

	case MM_OBJ_DATABASE:
		snprintf(objectDetails, sizeof(objectDetails),
				 "tablespace %d, database %u",
				 xlrec->tablespace, xlrec->database);
		break;

	case MM_OBJ_RELFILENODE:
		snprintf(objectDetails, sizeof(objectDetails),
				 "relation  %u/%u/%u, segment file #%u", xlrec->tablespace,
				 xlrec->database, xlrec->relfilenode, xlrec->segnum);
		break;

	default:
		snprintf(objectDetails, sizeof(objectDetails), "unknown object type - %d",
				 xlrec->objtype);
		break;
	}

	snprintf(buf, sizeof(buf),"%s for %s, master dbid: %d, mirror dbid: %d, "
			 "master path: %s, mirror path: %s", operation, objectDetails,
			 xlrec->u.dbid.master, xlrec->u.dbid.mirror, xlrec->master_path,
			 xlrec->mirror_path);

	print_rmgr_record(cur, record, buf);
}

#ifdef USE_SEGWALREP
void
print_rmgr_ao(XLogRecPtr cur, XLogRecord *record, uint8 info)
{
	char spaceName[NAMEDATALEN];
	char dbName[NAMEDATALEN];
	char relName[NAMEDATALEN];
	char buf[1024];

	xl_ao_target target;

	memcpy(&target, XLogRecGetData(record), sizeof(target));

	getSpaceName(target.node.spcNode, spaceName, sizeof(spaceName));
	getDbName(target.node.dbNode, dbName, sizeof(dbName));
	getRelName(target.node.relNode, relName, sizeof(relName));

	switch (info)
	{
		case XLOG_APPENDONLY_INSERT:
			{
				uint64       len;

				len = record->xl_len - SizeOfAOInsert;

				snprintf(
					buf, sizeof(buf),
					"insert: s/d/r:%s/%s/%s segfile/off:%u/" INT64_FORMAT ", len:%lu",
					spaceName, dbName, relName,
					target.segment_filenum, target.offset, len);
			}
			break;
		case XLOG_APPENDONLY_TRUNCATE:
			{
				snprintf(
					buf, sizeof(buf),
					"truncate: s/d/r:%s/%s/%s segfile/off:%u/" INT64_FORMAT,
					spaceName, dbName, relName,
					target.segment_filenum, target.offset);
			}
			break;
		default:
			snprintf(buf, sizeof(buf), "unknown");
	}

	print_rmgr_record(cur, record, buf);
}
#endif		/* USE_SEGWALREP */
