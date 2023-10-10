#include "postgres.h"

#include "access/genam.h"
#include "access/table.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_resgroupcapability_d.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbvars.h"
#include "commands/resgroupcmds.h"
#include "commands/tablespace.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/rel.h"
#include "utils/resgroup.h"
#include "utils/cgroup.h"
#include "utils/resource_manager.h"
#include "utils/cgroup_io_limit.h"


PG_MODULE_MAGIC;

static List* getIOLimitStats(Relation rel_resgroup_caps);

PG_FUNCTION_INFO_V1(pg_resgroup_get_iostats);
Datum
pg_resgroup_get_iostats(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		int nattr = 8;
		MemoryContext oldContext;
		TupleDesc tupdesc;

		instr_time start_time;
		instr_time end_time;
		float8 interval;

		List *stats = NIL;
		ListCell *statCell;
		List *newStats = NIL;
		ListCell *newStatCell;
		Relation rel_resgroup_caps;

		funcctx = SRF_FIRSTCALL_INIT();
		if (!IsResGroupActivated())
		{
			SRF_RETURN_DONE(funcctx);
		}

		oldContext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segindex", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "rsgname", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "groupid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "tablespace", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "rbps", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "wbps", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "riops", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "wiops", INT8OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* collect stats */
		rel_resgroup_caps = table_open(ResGroupCapabilityRelationId, AccessShareLock);

		/* pg_usleep can be interrupted, so we use timestap as the interval */
		INSTR_TIME_SET_CURRENT(start_time);
		stats = getIOLimitStats(rel_resgroup_caps);

		/* 1 second */
		pg_usleep(1000000L);

		INSTR_TIME_SET_CURRENT(end_time);
		newStats = getIOLimitStats(rel_resgroup_caps);

		interval = (INSTR_TIME_GET_MILLISEC(end_time) - INSTR_TIME_GET_MILLISEC(start_time)) / 1000.0;

		table_close(rel_resgroup_caps, AccessShareLock);

		if (list_length(stats) != list_length(newStats))
			ereport(ERROR, (errmsg("stats count differs between runs")));

		funcctx->max_calls = list_length(stats);
		funcctx->user_fctx = (void *) list_head(stats);

		/* oldStat and newStats maybe have different orders, so it need sort */
		stats = list_qsort(stats, compare_iostat);
		newStats = list_qsort(newStats, compare_iostat);

		forboth(statCell, stats, newStatCell, newStats)
		{
			IOStat *newStat = (IOStat *) lfirst(newStatCell);
			IOStat *stat = (IOStat *) lfirst(statCell);

			if (stat->groupid != newStat->groupid || stat->tablespace != newStat->tablespace)
				ereport(ERROR, (errmsg("get different result from io.stat after little interval")));

			stat->items.rios = (newStat->items.rios - stat->items.rios) / interval;
			stat->items.wios = (newStat->items.wios - stat->items.wios) / interval;

			stat->items.rbytes = (newStat->items.rbytes - stat->items.rbytes) / interval;
			stat->items.wbytes = (newStat->items.wbytes - stat->items.wbytes) / interval;
		}

		MemoryContextSwitchTo(oldContext);
	}

	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		Datum values[8];
		bool nulls[8];
		HeapTuple tuple;
		ListCell *cell = (ListCell *) funcctx->user_fctx;
		funcctx->user_fctx = (void *) cell->next;
		IOStat *stat = (IOStat *) lfirst(cell);

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = CStringGetTextDatum(GetResGroupNameForId(stat->groupid));
		values[2] = ObjectIdGetDatum(stat->groupid);

		if (stat->tablespace != InvalidOid)
			values[3] = CStringGetTextDatum(get_tablespace_name(stat->tablespace));
		else
			values[3] = CStringGetTextDatum("*");

		values[4] = Int64GetDatum((int64) stat->items.rbytes);
		values[5] = Int64GetDatum((int64) stat->items.wbytes);
		values[6] = Int64GetDatum((int64) stat->items.rios);
		values[7] = Int64GetDatum((int64) stat->items.wios);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}

static List*
getIOLimitStats(Relation rel_resgroup_caps)
{
	/* save groupid and io limits together */
	typedef struct IOLimitItem {
		Oid groupid;
		List *io_limit;
	} IOLimitItem;

	SysScanDesc	sscan;
	HeapTuple	tuple;
	List		*io_limit_items = NIL;;
	ListCell	*cell;
	List		*result = NIL;

	/* get io limit string from catalog */
	sscan = systable_beginscan(rel_resgroup_caps, InvalidOid, false,
							   NULL, 0, NULL);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		bool isNULL;
		Datum id_datum;
		Datum type_datum;
		Datum value_datum;
		Oid id;
		ResGroupLimitType type;
		char *io_limit_str;
		IOLimitItem *item;

		type_datum = heap_getattr(tuple, Anum_pg_resgroupcapability_reslimittype,
								  rel_resgroup_caps->rd_att, &isNULL);
		type = (ResGroupLimitType) DatumGetInt16(type_datum);
		if (type != RESGROUP_LIMIT_TYPE_IO_LIMIT)
			continue;

		id_datum = heap_getattr(tuple, Anum_pg_resgroupcapability_resgroupid,
								rel_resgroup_caps->rd_att, &isNULL);
		id = DatumGetObjectId(id_datum);

		value_datum = heap_getattr(tuple, Anum_pg_resgroupcapability_value,
								   rel_resgroup_caps->rd_att, &isNULL);
		io_limit_str = TextDatumGetCString(value_datum);

		if (strcmp(io_limit_str, DefaultIOLimit) == 0)
			continue;

		item = (IOLimitItem *) palloc0(sizeof(IOLimitItem));
		item->groupid = id;

		item->io_limit = io_limit_parse(io_limit_str);

		io_limit_items = lappend(io_limit_items, item);
	}
	systable_endscan(sscan);

	foreach(cell, io_limit_items)
	{
		IOLimitItem *item = (IOLimitItem *) lfirst(cell);

		List *tmp = cgroupOpsRoutine->getiostat(item->groupid, item->io_limit);

		result = list_concat(result, tmp);
	}

	list_free_deep(io_limit_items);

	return result;
}
