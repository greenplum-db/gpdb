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

static int getIOLimitStats(Relation rel_resgroup_caps, IOStat **result);

static int
getIOLimitStats(Relation rel_resgroup_caps, IOStat **result)
{
	/* save groupid and io limits together */
	typedef struct IOLimitItem {
		Oid groupid;
		List *io_limit;
	} IOLimitItem;

	SysScanDesc	sscan;
	HeapTuple	tuple;
	int			count = 0;
	int			res_count = 0;
	List		*io_limit_items = NIL;;
	ListCell	*cell;

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
		count += list_length(item->io_limit);

		io_limit_items = lappend(io_limit_items, item);
	}
	systable_endscan(sscan);

	*result = (IOStat *) palloc0(sizeof(IOStat) * count);

	foreach(cell, io_limit_items)
	{
		IOLimitItem *item = (IOLimitItem *) lfirst(cell);

		res_count += cgroupOpsRoutine->getiostat(item->groupid, item->io_limit, (*result) + res_count);
	}

	Assert(count == res_count);

	list_free_deep(io_limit_items);

	return res_count;
}

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

		IOStat *stats = NULL;
	  	IOStat *newStats = NULL;
	  	int     numStats;
	  	int     newNumStats;
	  	int		i;
	  	Relation rel_resgroup_caps;

	  	/* collect stats */
	  	rel_resgroup_caps = table_open(ResGroupCapabilityRelationId, AccessShareLock);
	  	numStats = getIOLimitStats(rel_resgroup_caps, &stats);
	  	pg_usleep(1000000L);
	  	newNumStats = getIOLimitStats(rel_resgroup_caps, &newStats);
	  	table_close(rel_resgroup_caps, AccessShareLock);

	  	if (numStats != newNumStats)
	  		ereport(ERROR, (errmsg("stats count differs between runs")));

	  	funcctx->max_calls = numStats;
	  	funcctx->user_fctx = (void *)stats;

	  	/* oldStat and newStats maybe have different orders, so it need sort */
	  	qsort(stats, numStats, sizeof(IOStat), compare_iostat);
	  	qsort(newStats, newNumStats, sizeof(IOStat), compare_iostat);

	  	for (i = 0; i < numStats; ++i)
	  	{
	  		IOStat *newStat = &newStats[i];
	  		IOStat *stat = &stats[i];

	  		stat->items.rios = newStat->items.rios - stat->items.rios;
	  		stat->items.wios = newStat->items.wios - stat->items.wios;

	  		/* convert bytes to Megabytes */
	  		stat->items.rbytes = (newStat->items.rbytes - stat->items.rbytes);
	  		stat->items.wbytes = (newStat->items.wbytes - stat->items.wbytes);
	  	}


		MemoryContextSwitchTo(oldContext);
	}

	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		Datum values[8];
		bool nulls[8];
		HeapTuple tuple;
		IOStat stat = ((IOStat *)funcctx->user_fctx)[funcctx->call_cntr];

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = CStringGetTextDatum(GetResGroupNameForId(stat.groupid));
		values[2] = ObjectIdGetDatum(stat.groupid);

		if (stat.tablespace != InvalidOid)
			values[3] = CStringGetTextDatum(get_tablespace_name(stat.tablespace));
		else
			values[3] = CStringGetTextDatum(psprintf("%c", '*'));

		values[4] = Int64GetDatum((int64) stat.items.rbytes);
		values[5] = Int64GetDatum((int64) stat.items.wbytes);
		values[6] = Int64GetDatum((int64) stat.items.rios);
		values[7] = Int64GetDatum((int64) stat.items.wios);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}
