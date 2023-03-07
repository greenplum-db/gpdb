//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorRelcacheToDXL.cpp
//
//	@doc:
//		Class translating relcache entries into DXL objects
//
//	@test:
//
//
//---------------------------------------------------------------------------

extern "C" {
#include "postgres.h"

#include "access/external.h"
#include "access/heapam.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_statistic_ext.h"
#include "cdb/cdbhash.h"
#include "partitioning/partdesc.h"
#include "utils/array.h"
#include "utils/datum.h"
#include "utils/elog.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
}

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/error/CException.h"
#include "gpos/io/COstreamString.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/gpdbwrappers.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/exception.h"
#include "naucrates/md/CDXLColStats.h"
#include "naucrates/md/CDXLExtStats.h"
#include "naucrates/md/CDXLExtStatsInfo.h"
#include "naucrates/md/CDXLRelStats.h"
#include "naucrates/md/CMDArrayCoerceCastGPDB.h"
#include "naucrates/md/CMDCastGPDB.h"
#include "naucrates/md/CMDIdCast.h"
#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/CMDIdScCmp.h"
#include "naucrates/md/CMDIndexGPDB.h"
#include "naucrates/md/CMDPartConstraintGPDB.h"
#include "naucrates/md/CMDScCmpGPDB.h"
#include "naucrates/md/CMDTypeBoolGPDB.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"
#include "naucrates/md/CMDTypeInt2GPDB.h"
#include "naucrates/md/CMDTypeInt4GPDB.h"
#include "naucrates/md/CMDTypeInt8GPDB.h"
#include "naucrates/md/CMDTypeOidGPDB.h"

using namespace gpdxl;
using namespace gpopt;


static const ULONG cmp_type_mappings[][2] = {
	{IMDType::EcmptEq, CmptEq},	  {IMDType::EcmptNEq, CmptNEq},
	{IMDType::EcmptL, CmptLT},	  {IMDType::EcmptG, CmptGT},
	{IMDType::EcmptGEq, CmptGEq}, {IMDType::EcmptLEq, CmptLEq}};

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveObject
//
//	@doc:
//		Retrieve a metadata object from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveObject(CMemoryPool *mp,
										 CMDAccessor *md_accessor, IMDId *mdid,
										 IMDCacheObject::Emdtype mdtype)
{
	IMDCacheObject *md_obj = nullptr;
	GPOS_ASSERT(nullptr != md_accessor);

#ifdef FAULT_INJECTOR
	gpdb::InjectFaultInOptTasks("opt_relcache_translator_catalog_access");
#endif	// FAULT_INJECTOR

	switch (mdid->MdidType())
	{
		case IMDId::EmdidGeneral:
			md_obj = RetrieveObjectGPDB(mp, mdid, mdtype);
			break;

		case IMDId::EmdidRelStats:
			md_obj = RetrieveRelStats(mp, mdid);
			break;

		case IMDId::EmdidColStats:
			md_obj = RetrieveColStats(mp, md_accessor, mdid);
			break;

		case IMDId::EmdidCastFunc:
			md_obj = RetrieveCast(mp, mdid);
			break;

		case IMDId::EmdidScCmp:
			md_obj = RetrieveScCmp(mp, mdid);
			break;

		case IMDId::EmdidRel:
			md_obj = RetrieveRel(mp, md_accessor, mdid);
			break;

		case IMDId::EmdidInd:
			md_obj = RetrieveIndex(mp, md_accessor, mdid);
			break;

		case IMDId::EmdidCheckConstraint:
			md_obj = RetrieveCheckConstraints(mp, md_accessor, mdid);
			break;

		case IMDId::EmdidExtStats:
			md_obj = RetrieveExtStats(mp, mdid);
			break;

		case IMDId::EmdidExtStatsInfo:
			md_obj = RetrieveExtStatsInfo(mp, mdid);
			break;

		default:
			break;
	}

	if (nullptr == md_obj)
	{
		// no match found
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	return md_obj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveMDObjGPDB
//
//	@doc:
//		Retrieve a GPDB metadata object from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveObjectGPDB(CMemoryPool *mp, IMDId *mdid,
											 IMDCacheObject::Emdtype mdtype)
{
	GPOS_ASSERT(mdid->MdidType() == CMDIdGPDB::EmdidGeneral);

	OID oid = CMDIdGPDB::CastMdid(mdid)->Oid();

	GPOS_RTL_ASSERT(0 != oid);

	switch (mdtype)
	{
		case IMDCacheObject::EmdtType:
			return RetrieveType(mp, mdid);

		case IMDCacheObject::EmdtOp:
			return RetrieveScOp(mp, mdid);

		case IMDCacheObject::EmdtAgg:
			return RetrieveAgg(mp, mdid);

		case IMDCacheObject::EmdtFunc:
			return RetrieveFunc(mp, mdid);

		case IMDCacheObject::EmdtSentinel:
			// for window function lookup
			if (gpdb::AggregateExists(oid))
			{
				return RetrieveAgg(mp, mdid);
			}
			else if (gpdb::FunctionExists(oid))
			{
				return RetrieveFunc(mp, mdid);
			}
			// no match found
			return nullptr;

		default:
			GPOS_RTL_ASSERT_MSG(false, "Unexpected MD type.");
			return nullptr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetRelName
//
//	@doc:
//		Return a relation name
//
//---------------------------------------------------------------------------
CMDName *
CTranslatorRelcacheToDXL::GetRelName(CMemoryPool *mp, Relation rel)
{
	GPOS_ASSERT(nullptr != rel);
	CHAR *relname = NameStr(rel->rd_rel->relname);
	CWStringDynamic *relname_str =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, relname);
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, relname_str);
	GPOS_DELETE(relname_str);
	return mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelIndexInfo
//
//	@doc:
//		Return the indexes defined on the given relation
//
//---------------------------------------------------------------------------
CMDIndexInfoArray *
CTranslatorRelcacheToDXL::RetrieveRelIndexInfo(CMemoryPool *mp, Relation rel)
{
	GPOS_ASSERT(nullptr != rel);
	CMDIndexInfoArray *md_index_info_array = GPOS_NEW(mp) CMDIndexInfoArray(mp);

	// not a partitioned table: obtain indexes directly from the catalog
	List *index_oids = gpdb::GetRelationIndexes(rel);

	ListCell *lc = nullptr;

	ForEach(lc, index_oids)
	{
		OID index_oid = lfirst_oid(lc);

		// only add supported indexes
		gpdb::RelationWrapper index_rel = gpdb::GetRelation(index_oid);

		if (!index_rel)
		{
			WCHAR wstr[1024];
			CWStringStatic str(wstr, 1024);
			COstreamString oss(&str);
			oss << (ULONG) index_oid;
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
					   str.GetBuffer());
		}

		GPOS_ASSERT(nullptr != index_rel->rd_indextuple);

		if (IsIndexSupported(index_rel.get()))
		{
			CMDIdGPDB *mdid_index =
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidInd, index_oid);
			// for a regular table, foreign table or leaf partition, an index is always complete
			CMDIndexInfo *md_index_info =
				GPOS_NEW(mp) CMDIndexInfo(mdid_index, false /* is_partial */);
			md_index_info_array->Append(md_index_info);
		}
	}

	return md_index_info_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelCheckConstraints
//
//	@doc:
//		Return the check constraints defined on the relation with the given oid
//
//---------------------------------------------------------------------------
IMdIdArray *
CTranslatorRelcacheToDXL::RetrieveRelCheckConstraints(CMemoryPool *mp, OID oid)
{
	IMdIdArray *check_constraint_mdids = GPOS_NEW(mp) IMdIdArray(mp);
	List *check_constraints = gpdb::GetCheckConstraintOids(oid);

	ListCell *lc = nullptr;
	ForEach(lc, check_constraints)
	{
		OID check_constraint_oid = lfirst_oid(lc);
		GPOS_ASSERT(0 != check_constraint_oid);
		CMDIdGPDB *mdid_check_constraint = GPOS_NEW(mp)
			CMDIdGPDB(IMDId::EmdidCheckConstraint, check_constraint_oid);
		check_constraint_mdids->Append(mdid_check_constraint);
	}

	return check_constraint_mdids;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::CheckUnsupportedRelation
//
//	@doc:
//		Check and fall back to planner for unsupported relations
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::CheckUnsupportedRelation(Relation rel)
{
	if (!rel->rd_partdesc && gpdb::HasSubclassSlow(rel->rd_id))
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("Inherited tables"));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveExtStats
//
//	@doc:
//		Retrieve extended statistics from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveExtStats(CMemoryPool *mp, IMDId *mdid)
{
	OID stat_oid = CMDIdGPDB::CastMdid(mdid)->Oid();
	List *kinds = gpdb::GetExtStatsKinds(stat_oid);

	CMDDependencyArray *deps = GPOS_NEW(mp) CMDDependencyArray(mp);
	if (list_member_int(kinds, STATS_EXT_DEPENDENCIES))
	{
		MVDependencies *dependencies = gpdb::GetMVDependencies(stat_oid);

		for (ULONG i = 0; i < dependencies->ndeps; i++)
		{
			MVDependency *dep = dependencies->deps[i];

			// Note: MVDependency->attributes's last index is the dependent "to"
			//       column.
			IntPtrArray *from_attnos = GPOS_NEW(mp) IntPtrArray(mp);
			for (INT j = 0; j < dep->nattributes - 1; j++)
			{
				from_attnos->Append(GPOS_NEW(mp) INT(dep->attributes[j]));
			}
			deps->Append(GPOS_NEW(mp) CMDDependency(
				mp, dep->degree, from_attnos,
				dep->attributes[dep->nattributes - 1]));
		}
	}

	CMDNDistinctArray *md_ndistincts = GPOS_NEW(mp) CMDNDistinctArray(mp);
	if (list_member_int(kinds, STATS_EXT_NDISTINCT))
	{
		MVNDistinct *ndistinct = gpdb::GetMVNDistinct(stat_oid);

		for (ULONG i = 0; i < ndistinct->nitems; i++)
		{
			MVNDistinctItem item = ndistinct->items[i];

			CBitSet *attnos = GPOS_NEW(mp) CBitSet(mp);

			int attno = -1;
			while ((attno = bms_next_member(item.attrs, attno)) >= 0)
			{
				attnos->ExchangeSet(attno);
			}
			md_ndistincts->Append(GPOS_NEW(mp)
									  CMDNDistinct(mp, item.ndistinct, attnos));
		}
	}

	const CWStringConst *statname =
		GPOS_NEW(mp) CWStringConst(CDXLUtils::CreateDynamicStringFromCharArray(
									   mp, gpdb::GetExtStatsName(stat_oid))
									   ->GetBuffer());
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, statname);

	return GPOS_NEW(mp) CDXLExtStats(mp, mdid, mdname, deps, md_ndistincts);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveExtStats
//
//	@doc:
//		Retrieve extended statistics metadata from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveExtStatsInfo(CMemoryPool *mp, IMDId *mdid)
{
	OID rel_oid = CMDIdGPDB::CastMdid(mdid)->Oid();

	CMDExtStatsInfoArray *extstats_info_array =
		GPOS_NEW(mp) CMDExtStatsInfoArray(mp);

	gpdb::RelationWrapper rel = gpdb::GetRelation(rel_oid);
	List *extstats = gpdb::GetExtStats(rel.get());

	ListCell *lc = nullptr;
	ForEach(lc, extstats)
	{
		StatisticExtInfo *info = (StatisticExtInfo *) lfirst(lc);

		CBitSet *keys = GPOS_NEW(mp) CBitSet(mp);

		int attno = -1;
		while ((attno = bms_next_member(info->keys, attno)) >= 0)
		{
			keys->ExchangeSet(attno);
		}

		CMDExtStatsInfo::Estattype statkind = CMDExtStatsInfo::EstatSentinel;
		switch (info->kind)
		{
			case STATS_EXT_DEPENDENCIES:
			{
				statkind = CMDExtStatsInfo::EstatDependencies;
				break;
			}
			case STATS_EXT_NDISTINCT:
			{
				statkind = CMDExtStatsInfo::EstatNDistinct;
				break;
			}
			case STATS_EXT_MCV:
			{
				statkind = CMDExtStatsInfo::EstatMCV;
				break;
			}
			default:
			{
				GPOS_ASSERT(false && "Unknown extended stat type");
			}
		}

		const CWStringConst *statname = GPOS_NEW(mp)
			CWStringConst(CDXLUtils::CreateDynamicStringFromCharArray(
							  mp, gpdb::GetExtStatsName(info->statOid))
							  ->GetBuffer());
		CMDName *mdname = GPOS_NEW(mp) CMDName(mp, statname);

		extstats_info_array->Append(GPOS_NEW(mp) CMDExtStatsInfo(
			mp, info->statOid, mdname, statkind, keys));
	}

	return GPOS_NEW(mp) CDXLExtStatsInfo(mp, mdid, GetRelName(mp, rel.get()),
										 extstats_info_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRel
//
//	@doc:
//		Retrieve a relation from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDRelation *
CTranslatorRelcacheToDXL::RetrieveRel(CMemoryPool *mp, CMDAccessor *md_accessor,
									  IMDId *mdid)
{
	OID oid = CMDIdGPDB::CastMdid(mdid)->Oid();
	GPOS_ASSERT(InvalidOid != oid);

	gpdb::RelationWrapper rel = gpdb::GetRelation(oid);

	if (!rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CheckUnsupportedRelation(rel.get());

	if (nullptr != rel->rd_cdbpolicy &&
		POLICYTYPE_ENTRY != rel->rd_cdbpolicy->ptype &&
		gpdb::GetGPSegmentCount() != rel->rd_cdbpolicy->numsegments)
	{
		// GPORCA does not support partially distributed tables yet
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiDXLInvalidAttributeValue,
				   GPOS_WSZ_LIT("Partially Distributed Data"));
	}

	CMDName *mdname = nullptr;
	IMDRelation::Erelstoragetype rel_storage_type =
		IMDRelation::ErelstorageSentinel;
	CMDColumnArray *mdcol_array = nullptr;
	IMDRelation::Ereldistrpolicy dist = IMDRelation::EreldistrSentinel;
	ULongPtrArray *distr_cols = nullptr;
	IMdIdArray *distr_op_families = nullptr;
	CMDIndexInfoArray *md_index_info_array = nullptr;
	ULongPtrArray *part_keys = nullptr;
	CharPtrArray *part_types = nullptr;
	BOOL convert_hash_to_random = false;
	ULongPtr2dArray *keyset_array = nullptr;
	IMdIdArray *check_constraint_mdids = nullptr;
	BOOL is_temporary = false;
	BOOL is_partitioned = false;
	IMDRelation *md_rel = nullptr;
	IMdIdArray *partition_oids = nullptr;
	IMDId *foreign_server_mdid = nullptr;

	// get rel name
	mdname = GetRelName(mp, rel.get());

	// get storage type
	rel_storage_type = RetrieveRelStorageType(rel.get());

	// get relation columns
	mdcol_array = RetrieveRelColumns(mp, md_accessor, rel.get());
	const ULONG max_cols =
		GPDXL_SYSTEM_COLUMNS + (ULONG) rel->rd_att->natts + 1;
	ULONG *attno_mapping = ConstructAttnoMapping(mp, mdcol_array, max_cols);

	// get distribution policy
	GpPolicy *gp_policy = gpdb::GetDistributionPolicy(rel.get());
	dist = GetRelDistribution(gp_policy);

	// get distribution columns
	if (IMDRelation::EreldistrHash == dist)
	{
		distr_cols =
			RetrieveRelDistributionCols(mp, gp_policy, mdcol_array, max_cols);
		distr_op_families = RetrieveRelDistributionOpFamilies(mp, gp_policy);
	}

	convert_hash_to_random = gpdb::IsChildPartDistributionMismatched(rel.get());

	// collect relation indexes
	md_index_info_array = RetrieveRelIndexInfo(mp, rel.get());

	is_partitioned = (nullptr != rel->rd_partdesc);

	// get number of leaf partitions
	if (rel->rd_partdesc)
	{
		RetrievePartKeysAndTypes(mp, rel.get(), oid, &part_keys, &part_types);

		partition_oids = GPOS_NEW(mp) IMdIdArray(mp);
		for (int i = 0; i < rel->rd_partdesc->nparts; ++i)
		{
			Oid part_oid = rel->rd_partdesc->oids[i];
			partition_oids->Append(GPOS_NEW(mp)
									   CMDIdGPDB(IMDId::EmdidRel, part_oid));
			gpdb::RelationWrapper rel_part = gpdb::GetRelation(part_oid);
			if (rel_part->rd_partdesc)
			{
				// Multi-level partitioned tables are unsupported - fall back
				GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
						   GPOS_WSZ_LIT("Multi-level partitioned tables"));
			}
		}
	}

	// get key sets
	BOOL should_add_default_keys = RelHasSystemColumns(rel->rd_rel->relkind);
	keyset_array = RetrieveRelKeysets(mp, oid, should_add_default_keys,
									  is_partitioned, attno_mapping);

	// collect all check constraints
	check_constraint_mdids = RetrieveRelCheckConstraints(mp, oid);

	is_temporary = (rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP);

	GPOS_DELETE_ARRAY(attno_mapping);

	GPOS_ASSERT(IMDRelation::ErelstorageSentinel != rel_storage_type);
	GPOS_ASSERT(IMDRelation::EreldistrSentinel != dist);

	mdid->AddRef();

	CDXLNode *mdpart_constraint = nullptr;

	// retrieve the part constraints if relation is partitioned
	// FIMXE: Do this only if Relation::rd_rel::relispartition is true
	mdpart_constraint =
		RetrievePartConstraintForRel(mp, md_accessor, rel.get(), mdcol_array);


	// root partitions don't have a foreign server
	if (IMDRelation::ErelstorageForeign == rel_storage_type && !is_partitioned)
	{
		foreign_server_mdid = GPOS_NEW(mp)
			CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetForeignServerId(oid));
	}

	md_rel = GPOS_NEW(mp) CMDRelationGPDB(
		mp, mdid, mdname, is_temporary, rel_storage_type, dist, mdcol_array,
		distr_cols, distr_op_families, part_keys, part_types, partition_oids,
		convert_hash_to_random, keyset_array, md_index_info_array,
		check_constraint_mdids, mdpart_constraint, foreign_server_mdid);

	return md_rel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelColumns
//
//	@doc:
//		Get relation columns
//
//---------------------------------------------------------------------------
CMDColumnArray *
CTranslatorRelcacheToDXL::RetrieveRelColumns(CMemoryPool *mp,
											 CMDAccessor *md_accessor,
											 Relation rel)
{
	CMDColumnArray *mdcol_array = GPOS_NEW(mp) CMDColumnArray(mp);

	for (ULONG ul = 0; ul < (ULONG) rel->rd_att->natts; ul++)
	{
		Form_pg_attribute att = &rel->rd_att->attrs[ul];
		CMDName *md_colname =
			CDXLUtils::CreateMDNameFromCharArray(mp, NameStr(att->attname));

		// translate the default column value
		CDXLNode *dxl_default_col_val = nullptr;

		if (!att->attisdropped && !rel->rd_att->attrs[ul].attgenerated)
		{
			dxl_default_col_val = GetDefaultColumnValue(
				mp, md_accessor, rel->rd_att, att->attnum);
		}

		ULONG col_len = gpos::ulong_max;
		CMDIdGPDB *mdid_col =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, att->atttypid);
		HeapTuple stats_tup = gpdb::GetAttStats(rel->rd_id, ul + 1);

		// Column width priority:
		// 1. If there is average width kept in the stats for that column, pick that value.
		// 2. If not, if it is a fixed length text type, pick the size of it. E.g if it is
		//    varchar(10), assign 10 as the column length.
		// 3. Else if it not dropped and a fixed length type such as int4, assign the fixed
		//    length.
		// 4. Otherwise, assign it to default column width which is 8.
		if (HeapTupleIsValid(stats_tup))
		{
			Form_pg_statistic form_pg_stats =
				(Form_pg_statistic) GETSTRUCT(stats_tup);

			// column width
			col_len = form_pg_stats->stawidth;
			gpdb::FreeHeapTuple(stats_tup);
		}
		else if ((mdid_col->Equals(&CMDIdGPDB::m_mdid_bpchar) ||
				  mdid_col->Equals(&CMDIdGPDB::m_mdid_varchar)) &&
				 (VARHDRSZ < att->atttypmod))
		{
			col_len = (ULONG) att->atttypmod - VARHDRSZ;
		}
		else
		{
			DOUBLE width = CStatistics::DefaultColumnWidth.Get();
			col_len = (ULONG) width;

			if (!att->attisdropped)
			{
				IMDType *md_type =
					CTranslatorRelcacheToDXL::RetrieveType(mp, mdid_col);
				if (md_type->IsFixedLength())
				{
					col_len = md_type->Length();
				}
				md_type->Release();
			}
		}

		CMDColumn *md_col = GPOS_NEW(mp)
			CMDColumn(md_colname, att->attnum, mdid_col, att->atttypmod,
					  !att->attnotnull, att->attisdropped,
					  dxl_default_col_val /* default value */, col_len);

		mdcol_array->Append(md_col);
	}

	// add system columns
	if (RelHasSystemColumns(rel->rd_rel->relkind))
	{
		AddSystemColumns(mp, mdcol_array, rel);
	}

	return mdcol_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetDefaultColumnValue
//
//	@doc:
//		Return the dxl representation of column's default value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorRelcacheToDXL::GetDefaultColumnValue(CMemoryPool *mp,
												CMDAccessor *md_accessor,
												TupleDesc rd_att,
												AttrNumber attno)
{
	GPOS_ASSERT(attno > 0);

	Node *node = nullptr;

	// Scan to see if relation has a default for this column
	if (nullptr != rd_att->constr && 0 < rd_att->constr->num_defval)
	{
		AttrDefault *defval = rd_att->constr->defval;
		INT num_def = rd_att->constr->num_defval;

		GPOS_ASSERT(nullptr != defval);
		for (ULONG ul = 0; ul < (ULONG) num_def; ul++)
		{
			if (attno == defval[ul].adnum)
			{
				// found it, convert string representation to node tree.
				node = gpdb::StringToNode(defval[ul].adbin);
				break;
			}
		}
	}

	if (nullptr == node)
	{
		// get the default value for the type
		Form_pg_attribute att_tup = &rd_att->attrs[attno - 1];
		node = gpdb::GetTypeDefault(att_tup->atttypid);
	}

	if (nullptr == node)
	{
		return nullptr;
	}

	// translate the default value expression
	return CTranslatorScalarToDXL::TranslateStandaloneExprToDXL(
		mp, md_accessor,
		nullptr, /* var_colid_mapping --- subquery or external variable are not supported in default expression */
		(Expr *) node);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetRelDistribution
//
//	@doc:
//		Return the distribution policy of the relation
//
//---------------------------------------------------------------------------
IMDRelation::Ereldistrpolicy
CTranslatorRelcacheToDXL::GetRelDistribution(GpPolicy *gp_policy)
{
	if (nullptr == gp_policy)
	{
		return IMDRelation::EreldistrMasterOnly;
	}

	if (POLICYTYPE_REPLICATED == gp_policy->ptype)
	{
		return IMDRelation::EreldistrReplicated;
	}

	if (POLICYTYPE_PARTITIONED == gp_policy->ptype)
	{
		if (0 == gp_policy->nattrs)
		{
			return IMDRelation::EreldistrRandom;
		}

		return IMDRelation::EreldistrHash;
	}

	if (POLICYTYPE_ENTRY == gp_policy->ptype)
	{
		return IMDRelation::EreldistrMasterOnly;
	}

	GPOS_RAISE(gpdxl::ExmaMD, ExmiDXLUnrecognizedType,
			   GPOS_WSZ_LIT("unrecognized distribution policy"));
	return IMDRelation::EreldistrSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelDistributionCols
//
//	@doc:
//		Get distribution columns
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorRelcacheToDXL::RetrieveRelDistributionCols(
	CMemoryPool *mp, GpPolicy *gp_policy, CMDColumnArray *mdcol_array,
	ULONG size)
{
	ULONG *attno_mapping = GPOS_NEW_ARRAY(mp, ULONG, size);

	for (ULONG ul = 0; ul < mdcol_array->Size(); ul++)
	{
		const IMDColumn *md_col = (*mdcol_array)[ul];
		INT attno = md_col->AttrNum();

		ULONG idx = (ULONG)(GPDXL_SYSTEM_COLUMNS + attno);
		attno_mapping[idx] = ul;
	}

	ULongPtrArray *distr_cols = GPOS_NEW(mp) ULongPtrArray(mp);

	for (ULONG ul = 0; ul < (ULONG) gp_policy->nattrs; ul++)
	{
		AttrNumber attno = gp_policy->attrs[ul];

		distr_cols->Append(
			GPOS_NEW(mp) ULONG(GetAttributePosition(attno, attno_mapping)));
	}

	GPOS_DELETE_ARRAY(attno_mapping);
	return distr_cols;
}

IMdIdArray *
CTranslatorRelcacheToDXL::RetrieveRelDistributionOpFamilies(CMemoryPool *mp,
															GpPolicy *gp_policy)
{
	IMdIdArray *distr_op_classes = GPOS_NEW(mp) IMdIdArray(mp);

	Oid *opclasses = gp_policy->opclasses;
	for (ULONG ul = 0; ul < (ULONG) gp_policy->nattrs; ul++)
	{
		Oid opfamily = gpdb::GetOpclassFamily(opclasses[ul]);
		distr_op_classes->Append(GPOS_NEW(mp)
									 CMDIdGPDB(IMDId::EmdidGeneral, opfamily));
	}

	return distr_op_classes;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::AddSystemColumns
//
//	@doc:
//		Adding system columns (oid, tid, xmin, etc) in table descriptors
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::AddSystemColumns(CMemoryPool *mp,
										   CMDColumnArray *mdcol_array,
										   Relation rel)
{
	for (INT i = SelfItemPointerAttributeNumber;
		 i > FirstLowInvalidHeapAttributeNumber; i--)
	{
		AttrNumber attno = AttrNumber(i);
		GPOS_ASSERT(0 != attno);

		const FormData_pg_attribute *att_tup = SystemAttributeDefinition(attno);

		// get system name for that attribute
		const CWStringConst *sys_colname = GPOS_NEW(mp)
			CWStringConst(CDXLUtils::CreateDynamicStringFromCharArray(
							  mp, NameStr(att_tup->attname))
							  ->GetBuffer());
		GPOS_ASSERT(nullptr != sys_colname);

		// copy string into column name
		CMDName *md_colname = GPOS_NEW(mp) CMDName(mp, sys_colname);

		CMDColumn *md_col = GPOS_NEW(mp) CMDColumn(
			md_colname, attno,
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, att_tup->atttypid),
			default_type_modifier,
			false,	  // is_nullable
			false,	  // is_dropped
			nullptr,  // default value
			att_tup->attlen);

		mdcol_array->Append(md_col);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveIndex
//
//	@doc:
//		Retrieve an index from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDIndex *
CTranslatorRelcacheToDXL::RetrieveIndex(CMemoryPool *mp,
										CMDAccessor *md_accessor,
										IMDId *mdid_index)
{
	OID index_oid = CMDIdGPDB::CastMdid(mdid_index)->Oid();
	GPOS_ASSERT(0 != index_oid);
	gpdb::RelationWrapper index_rel = gpdb::GetRelation(index_oid);

	if (!index_rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid_index->GetBuffer());
	}

	const IMDRelation *md_rel = nullptr;
	Form_pg_index form_pg_index = nullptr;
	CMDName *mdname = nullptr;
	IMDIndex::EmdindexType index_type = IMDIndex::EmdindSentinel;
	IMDId *mdid_item_type = nullptr;
	bool index_clustered = false;
	bool index_partitioned = false;
	ULongPtrArray *index_key_cols_array = nullptr;
	ULONG *attno_mapping = nullptr;

	if (!IsIndexSupported(index_rel.get()))
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("Index type"));
	}

	form_pg_index = index_rel->rd_index;
	GPOS_ASSERT(nullptr != form_pg_index);
	index_clustered = form_pg_index->indisclustered;

	OID rel_oid = form_pg_index->indrelid;

	CMDIdGPDB *mdid_rel = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidRel, rel_oid);

	md_rel = md_accessor->RetrieveRel(mdid_rel);
	mdid_item_type = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_ANY);
	switch (index_rel->rd_rel->relam)
	{
		case BTREE_AM_OID:
			index_type = IMDIndex::EmdindBtree;
			break;
		case HASH_AM_OID:
			index_type = IMDIndex::EmdindHash;
			break;
		case BITMAP_AM_OID:
			index_type = IMDIndex::EmdindBitmap;
			break;
		case BRIN_AM_OID:
			index_type = IMDIndex::EmdindBrin;
			break;
		case GIN_AM_OID:
			index_type = IMDIndex::EmdindGin;
			break;
		case GIST_AM_OID:
			index_type = IMDIndex::EmdindGist;
			break;
		default:
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
					   GPOS_WSZ_LIT("Index access method"));
	}

	// get the index name
	CHAR *index_name = NameStr(index_rel->rd_rel->relname);
	CWStringDynamic *str_name =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, index_name);
	mdname = GPOS_NEW(mp) CMDName(mp, str_name);
	GPOS_DELETE(str_name);

	Oid table_oid = CMDIdGPDB::CastMdid(md_rel->MDId())->Oid();
	ULONG size = GPDXL_SYSTEM_COLUMNS +
				 (ULONG) gpdb::GetRelation(table_oid)->rd_att->natts + 1;

	attno_mapping = PopulateAttnoPositionMap(mp, md_rel, size);

	// extract the position of the key columns
	index_key_cols_array = GPOS_NEW(mp) ULongPtrArray(mp);

	for (int i = 0; i < form_pg_index->indnatts; i++)
	{
		INT attno = form_pg_index->indkey.values[i];
		GPOS_ASSERT(0 != attno && "Index expressions not supported");

		index_key_cols_array->Append(
			GPOS_NEW(mp) ULONG(GetAttributePosition(attno, attno_mapping)));
	}
	mdid_rel->Release();

	ULongPtrArray *included_cols = ComputeIncludedCols(mp, md_rel);
	mdid_index->AddRef();
	IMdIdArray *op_families_mdids = RetrieveIndexOpFamilies(mp, mdid_index);

	// get child indexes
	IMdIdArray *child_index_oids = nullptr;
	if (index_rel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
	{
		index_partitioned = true;
		child_index_oids = RetrieveIndexPartitions(mp, index_oid);
	}
	else
	{
		child_index_oids = GPOS_NEW(mp) IMdIdArray(mp);
	}

	CMDIndexGPDB *index = GPOS_NEW(mp)
		CMDIndexGPDB(mp, mdid_index, mdname, index_clustered, index_partitioned,
					 index_type, mdid_item_type, index_key_cols_array,
					 included_cols, op_families_mdids, child_index_oids);

	GPOS_DELETE_ARRAY(attno_mapping);
	return index;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::ComputeIncludedCols
//
//	@doc:
//		Compute the included columns in an index
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorRelcacheToDXL::ComputeIncludedCols(CMemoryPool *mp,
											  const IMDRelation *md_rel)
{
	// TODO: 3/19/2012; currently we assume that all the columns
	// in the table are available from the index.

	ULongPtrArray *included_cols = GPOS_NEW(mp) ULongPtrArray(mp);
	const ULONG num_included_cols = md_rel->ColumnCount();
	for (ULONG ul = 0; ul < num_included_cols; ul++)
	{
		if (!md_rel->GetMdCol(ul)->IsDropped())
		{
			included_cols->Append(GPOS_NEW(mp) ULONG(ul));
		}
	}

	return included_cols;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetAttributePosition
//
//	@doc:
//		Return the position of a given attribute
//
//---------------------------------------------------------------------------
ULONG
CTranslatorRelcacheToDXL::GetAttributePosition(
	INT attno, const ULONG *GetAttributePosition)
{
	ULONG idx = (ULONG)(GPDXL_SYSTEM_COLUMNS + attno);
	ULONG pos = GetAttributePosition[idx];
	GPOS_ASSERT(gpos::ulong_max != pos);

	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PopulateAttnoPositionMap
//
//	@doc:
//		Populate the attribute to position mapping
//
//---------------------------------------------------------------------------
ULONG *
CTranslatorRelcacheToDXL::PopulateAttnoPositionMap(CMemoryPool *mp,
												   const IMDRelation *md_rel,
												   ULONG size)
{
	GPOS_ASSERT(nullptr != md_rel);
	const ULONG num_included_cols = md_rel->ColumnCount();

	GPOS_ASSERT(num_included_cols <= size);
	ULONG *attno_mapping = GPOS_NEW_ARRAY(mp, ULONG, size);

	for (ULONG ul = 0; ul < size; ul++)
	{
		attno_mapping[ul] = gpos::ulong_max;
	}

	for (ULONG ul = 0; ul < num_included_cols; ul++)
	{
		const IMDColumn *md_col = md_rel->GetMdCol(ul);

		INT attno = md_col->AttrNum();

		ULONG idx = (ULONG)(GPDXL_SYSTEM_COLUMNS + attno);
		GPOS_ASSERT(size > idx);
		attno_mapping[idx] = ul;
	}

	return attno_mapping;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveType
//
//	@doc:
//		Retrieve a type from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDType *
CTranslatorRelcacheToDXL::RetrieveType(CMemoryPool *mp, IMDId *mdid)
{
	OID oid_type = CMDIdGPDB::CastMdid(mdid)->Oid();
	GPOS_ASSERT(InvalidOid != oid_type);

	// check for supported base types
	switch (oid_type)
	{
		case GPDB_INT2_OID:
			return GPOS_NEW(mp) CMDTypeInt2GPDB(mp);

		case GPDB_INT4_OID:
			return GPOS_NEW(mp) CMDTypeInt4GPDB(mp);

		case GPDB_INT8_OID:
			return GPOS_NEW(mp) CMDTypeInt8GPDB(mp);

		case GPDB_BOOL:
			return GPOS_NEW(mp) CMDTypeBoolGPDB(mp);

		case GPDB_OID_OID:
			return GPOS_NEW(mp) CMDTypeOidGPDB(mp);
	}

	// continue to construct a generic type
	INT iFlags = TYPECACHE_EQ_OPR | TYPECACHE_LT_OPR | TYPECACHE_GT_OPR |
				 TYPECACHE_CMP_PROC | TYPECACHE_EQ_OPR_FINFO |
				 TYPECACHE_CMP_PROC_FINFO | TYPECACHE_TUPDESC;

	TypeCacheEntry *ptce = gpdb::LookupTypeCache(oid_type, iFlags);

	// get type name
	CMDName *mdname = GetTypeName(mp, mdid);

	BOOL is_fixed_length = false;
	ULONG length = 0;

	if (0 < ptce->typlen)
	{
		is_fixed_length = true;
		length = ptce->typlen;
	}

	BOOL is_passed_by_value = ptce->typbyval;

	// collect ids of different comparison operators for types
	CMDIdGPDB *mdid_op_eq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, ptce->eq_opr);
	CMDIdGPDB *mdid_op_neq = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetInverseOp(ptce->eq_opr));
	CMDIdGPDB *mdid_op_lt =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, ptce->lt_opr);
	CMDIdGPDB *mdid_op_leq = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetInverseOp(ptce->gt_opr));
	CMDIdGPDB *mdid_op_gt =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, ptce->gt_opr);
	CMDIdGPDB *mdid_op_geq = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetInverseOp(ptce->lt_opr));
	CMDIdGPDB *mdid_op_cmp =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, ptce->cmp_proc);
	BOOL is_hashable = gpdb::IsOpHashJoinable(ptce->eq_opr, oid_type);
	BOOL is_merge_joinable = gpdb::IsOpMergeJoinable(ptce->eq_opr, oid_type);
	BOOL is_composite_type = gpdb::IsCompositeType(oid_type);
	BOOL is_text_related_type = gpdb::IsTextRelatedType(oid_type);

	// get standard aggregates
	CMDIdGPDB *mdid_min = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetAggregate("min", oid_type));
	CMDIdGPDB *mdid_max = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetAggregate("max", oid_type));
	CMDIdGPDB *mdid_avg = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetAggregate("avg", oid_type));
	CMDIdGPDB *mdid_sum = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetAggregate("sum", oid_type));

	// count aggregate is the same for all types
	CMDIdGPDB *mdid_count =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, COUNT_ANY_OID);

	// check if type is composite
	CMDIdGPDB *mdid_type_relid = nullptr;
	if (is_composite_type)
	{
		mdid_type_relid = GPOS_NEW(mp)
			CMDIdGPDB(IMDId::EmdidRel, gpdb::GetTypeRelid(oid_type));
	}

	// get array type mdid
	CMDIdGPDB *mdid_type_array = GPOS_NEW(mp)
		CMDIdGPDB(IMDId::EmdidGeneral, gpdb::GetArrayType(oid_type));

	OID distr_opfamily = gpdb::GetDefaultDistributionOpfamilyForType(oid_type);

	BOOL is_redistributable = false;
	CMDIdGPDB *mdid_distr_opfamily = nullptr;
	if (distr_opfamily != InvalidOid)
	{
		mdid_distr_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, distr_opfamily);
		is_redistributable = true;
	}

	CMDIdGPDB *mdid_legacy_distr_opfamily = nullptr;
	OID legacy_opclass = gpdb::GetLegacyCdbHashOpclassForBaseType(oid_type);
	if (legacy_opclass != InvalidOid)
	{
		OID legacy_opfamily = gpdb::GetOpclassFamily(legacy_opclass);
		mdid_legacy_distr_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, legacy_opfamily);
	}

	mdid->AddRef();
	return GPOS_NEW(mp) CMDTypeGenericGPDB(
		mp, mdid, mdname, is_redistributable, is_fixed_length, length,
		is_passed_by_value, mdid_distr_opfamily, mdid_legacy_distr_opfamily,
		mdid_op_eq, mdid_op_neq, mdid_op_lt, mdid_op_leq, mdid_op_gt,
		mdid_op_geq, mdid_op_cmp, mdid_min, mdid_max, mdid_avg, mdid_sum,
		mdid_count, is_hashable, is_merge_joinable, is_composite_type,
		is_text_related_type, mdid_type_relid, mdid_type_array, ptce->typlen);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveScOp
//
//	@doc:
//		Retrieve a scalar operator from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDScalarOpGPDB *
CTranslatorRelcacheToDXL::RetrieveScOp(CMemoryPool *mp, IMDId *mdid)
{
	OID op_oid = CMDIdGPDB::CastMdid(mdid)->Oid();

	GPOS_ASSERT(InvalidOid != op_oid);

	// get operator name
	CHAR *name = gpdb::GetOpName(op_oid);

	if (nullptr == name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CMDName *mdname = CDXLUtils::CreateMDNameFromCharArray(mp, name);

	OID left_oid = InvalidOid;
	OID right_oid = InvalidOid;

	// get operator argument types
	gpdb::GetOpInputTypes(op_oid, &left_oid, &right_oid);

	CMDIdGPDB *mdid_type_left = nullptr;
	CMDIdGPDB *mdid_type_right = nullptr;

	if (InvalidOid != left_oid)
	{
		mdid_type_left = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, left_oid);
	}

	if (InvalidOid != right_oid)
	{
		mdid_type_right =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, right_oid);
	}

	// get comparison type
	CmpType cmpt = (CmpType) gpdb::GetComparisonType(op_oid);
	IMDType::ECmpType cmp_type = ParseCmpType(cmpt);

	// get func oid
	OID func_oid = gpdb::GetOpFunc(op_oid);
	GPOS_ASSERT(InvalidOid != func_oid);

	CMDIdGPDB *mdid_func =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, func_oid);

	// get result type
	OID result_oid = gpdb::GetFuncRetType(func_oid);

	GPOS_ASSERT(InvalidOid != result_oid);

	CMDIdGPDB *result_type_mdid =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, result_oid);

	// get commutator and inverse
	CMDIdGPDB *mdid_commute_opr = nullptr;

	OID commute_oid = gpdb::GetCommutatorOp(op_oid);

	if (InvalidOid != commute_oid)
	{
		mdid_commute_opr =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, commute_oid);
	}

	CMDIdGPDB *m_mdid_inverse_opr = nullptr;

	OID inverse_oid = gpdb::GetInverseOp(op_oid);

	if (InvalidOid != inverse_oid)
	{
		m_mdid_inverse_opr =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, inverse_oid);
	}

	BOOL returns_null_on_null_input = gpdb::IsOpStrict(op_oid);
	BOOL is_ndv_preserving = gpdb::IsOpNDVPreserving(op_oid);

	CMDIdGPDB *mdid_hash_opfamily = nullptr;
	OID distr_opfamily = gpdb::GetCompatibleHashOpFamily(op_oid);
	if (InvalidOid != distr_opfamily)
	{
		mdid_hash_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, distr_opfamily);
	}

	CMDIdGPDB *mdid_legacy_hash_opfamily = nullptr;
	OID legacy_distr_opfamily = gpdb::GetCompatibleLegacyHashOpFamily(op_oid);
	if (InvalidOid != legacy_distr_opfamily)
	{
		mdid_legacy_hash_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, legacy_distr_opfamily);
	}

	mdid->AddRef();
	CMDScalarOpGPDB *md_scalar_op = GPOS_NEW(mp) CMDScalarOpGPDB(
		mp, mdid, mdname, mdid_type_left, mdid_type_right, result_type_mdid,
		mdid_func, mdid_commute_opr, m_mdid_inverse_opr, cmp_type,
		returns_null_on_null_input, RetrieveScOpOpFamilies(mp, mdid),
		mdid_hash_opfamily, mdid_legacy_hash_opfamily, is_ndv_preserving);
	return md_scalar_op;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::LookupFuncProps
//
//	@doc:
//		Lookup function properties
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::LookupFuncProps(
	OID func_oid,
	IMDFunction::EFuncStbl *stability,	// output: function stability
	IMDFunction::EFuncDataAcc *access,	// output: function datya access
	BOOL *is_strict,					// output: is function strict?
	BOOL *is_ndv_preserving,			// output: preserves NDVs of inputs
	BOOL *returns_set,					// output: does function return set?
	BOOL *
		is_allowed_for_PS  // output: is this a lossy (non-implicit) cast which is allowed for Partition selection
)
{
	GPOS_ASSERT(nullptr != stability);
	GPOS_ASSERT(nullptr != access);
	GPOS_ASSERT(nullptr != is_strict);
	GPOS_ASSERT(nullptr != is_ndv_preserving);
	GPOS_ASSERT(nullptr != returns_set);

	*stability = GetFuncStability(gpdb::FuncStability(func_oid));
	*access = GetEFuncDataAccess(gpdb::FuncDataAccess(func_oid));

	if (gpdb::FuncExecLocation(func_oid) != PROEXECLOCATION_ANY)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
				   GPOS_WSZ_LIT("unsupported exec location"));
	}

	*returns_set = gpdb::GetFuncRetset(func_oid);
	*is_strict = gpdb::FuncStrict(func_oid);
	*is_ndv_preserving = gpdb::IsFuncNDVPreserving(func_oid);
	*is_allowed_for_PS = gpdb::IsFuncAllowedForPartitionSelection(func_oid);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveFunc
//
//	@doc:
//		Retrieve a function from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDFunctionGPDB *
CTranslatorRelcacheToDXL::RetrieveFunc(CMemoryPool *mp, IMDId *mdid)
{
	OID func_oid = CMDIdGPDB::CastMdid(mdid)->Oid();

	GPOS_ASSERT(InvalidOid != func_oid);

	// get func name
	CHAR *name = gpdb::GetFuncName(func_oid);

	if (nullptr == name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CWStringDynamic *func_name_str =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, name);
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, func_name_str);

	// CMDName ctor created a copy of the string
	GPOS_DELETE(func_name_str);

	// get result type
	OID result_oid = gpdb::GetFuncRetType(func_oid);

	GPOS_ASSERT(InvalidOid != result_oid);

	CMDIdGPDB *result_type_mdid =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, result_oid);

	// get output argument types if any
	List *out_arg_types_list = gpdb::GetFuncOutputArgTypes(func_oid);

	IMdIdArray *arg_type_mdids = nullptr;
	if (nullptr != out_arg_types_list)
	{
		ListCell *lc = nullptr;
		arg_type_mdids = GPOS_NEW(mp) IMdIdArray(mp);

		ForEach(lc, out_arg_types_list)
		{
			OID oidArgType = lfirst_oid(lc);
			GPOS_ASSERT(InvalidOid != oidArgType);
			CMDIdGPDB *pmdidArgType =
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, oidArgType);
			arg_type_mdids->Append(pmdidArgType);
		}

		gpdb::GPDBFree(out_arg_types_list);
	}

	IMDFunction::EFuncStbl stability = IMDFunction::EfsImmutable;
	IMDFunction::EFuncDataAcc access = IMDFunction::EfdaNoSQL;
	BOOL is_strict = true;
	BOOL returns_set = true;
	BOOL is_ndv_preserving = true;
	BOOL is_allowed_for_PS = false;
	LookupFuncProps(func_oid, &stability, &access, &is_strict,
					&is_ndv_preserving, &returns_set, &is_allowed_for_PS);

	mdid->AddRef();
	CMDFunctionGPDB *md_func = GPOS_NEW(mp) CMDFunctionGPDB(
		mp, mdid, mdname, result_type_mdid, arg_type_mdids, returns_set,
		stability, access, is_strict, is_ndv_preserving, is_allowed_for_PS);

	return md_func;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveAgg
//
//	@doc:
//		Retrieve an aggregate from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDAggregateGPDB *
CTranslatorRelcacheToDXL::RetrieveAgg(CMemoryPool *mp, IMDId *mdid)
{
	OID agg_oid = CMDIdGPDB::CastMdid(mdid)->Oid();

	GPOS_ASSERT(InvalidOid != agg_oid);

	// get agg name
	CHAR *name = gpdb::GetFuncName(agg_oid);

	if (nullptr == name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CWStringDynamic *agg_name_str =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, name);
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, agg_name_str);

	// CMDName ctor created a copy of the string
	GPOS_DELETE(agg_name_str);

	// get result type
	OID result_oid = gpdb::GetFuncRetType(agg_oid);

	GPOS_ASSERT(InvalidOid != result_oid);

	CMDIdGPDB *result_type_mdid =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, result_oid);
	IMDId *intermediate_result_type_mdid =
		RetrieveAggIntermediateResultType(mp, mdid);

	mdid->AddRef();

	BOOL is_ordered = gpdb::IsOrderedAgg(agg_oid);
	BOOL is_repsafe = gpdb::IsRepSafeAgg(agg_oid);

	// GPDB does not support splitting of ordered aggs and aggs without a
	// combine function
	BOOL is_splittable = !is_ordered && gpdb::IsAggPartialCapable(agg_oid);

	// cannot use hash agg for ordered aggs or aggs without a combine func
	// due to the fact that hashAgg may spill
	BOOL is_hash_agg_capable =
		!is_ordered && gpdb::IsAggPartialCapable(agg_oid);

	CMDAggregateGPDB *pmdagg = GPOS_NEW(mp) CMDAggregateGPDB(
		mp, mdid, mdname, result_type_mdid, intermediate_result_type_mdid,
		is_ordered, is_splittable, is_hash_agg_capable, is_repsafe);
	return pmdagg;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveCheckConstraints
//
//	@doc:
//		Retrieve a check constraint from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDCheckConstraintGPDB *
CTranslatorRelcacheToDXL::RetrieveCheckConstraints(CMemoryPool *mp,
												   CMDAccessor *md_accessor,
												   IMDId *mdid)
{
	OID check_constraint_oid = CMDIdGPDB::CastMdid(mdid)->Oid();
	GPOS_ASSERT(InvalidOid != check_constraint_oid);

	// get name of the check constraint
	CHAR *name = gpdb::GetCheckConstraintName(check_constraint_oid);
	if (nullptr == name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}
	CWStringDynamic *check_constr_name =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, name);
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, check_constr_name);
	GPOS_DELETE(check_constr_name);

	// get relation oid associated with the check constraint
	OID rel_oid = gpdb::GetCheckConstraintRelid(check_constraint_oid);
	GPOS_ASSERT(InvalidOid != rel_oid);
	CMDIdGPDB *mdid_rel = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidRel, rel_oid);

	// translate the check constraint expression
	Node *node = gpdb::PnodeCheckConstraint(check_constraint_oid);
	GPOS_ASSERT(nullptr != node);

	// generate a mock mapping between var to column information
	CMappingVarColId *var_colid_mapping = GPOS_NEW(mp) CMappingVarColId(mp);
	CDXLColDescrArray *dxl_col_descr_array = GPOS_NEW(mp) CDXLColDescrArray(mp);
	const IMDRelation *md_rel = md_accessor->RetrieveRel(mdid_rel);
	const ULONG length = md_rel->ColumnCount();
	for (ULONG ul = 0; ul < length; ul++)
	{
		const IMDColumn *md_col = md_rel->GetMdCol(ul);

		if (md_col->IsDropped())
		{
			continue;
		}

		CMDName *md_colname =
			GPOS_NEW(mp) CMDName(mp, md_col->Mdname().GetMDName());
		CMDIdGPDB *mdid_col_type = CMDIdGPDB::CastMdid(md_col->MdidType());
		mdid_col_type->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *dxl_col_descr = GPOS_NEW(mp) CDXLColDescr(
			md_colname, ul + 1 /*colid*/, md_col->AttrNum(), mdid_col_type,
			md_col->TypeModifier(), false /* fColDropped */
		);
		dxl_col_descr_array->Append(dxl_col_descr);
	}
	var_colid_mapping->LoadColumns(0 /*query_level */, 1 /* rteIndex */,
								   dxl_col_descr_array);

	// translate the check constraint expression
	CDXLNode *scalar_dxlnode =
		CTranslatorScalarToDXL::TranslateStandaloneExprToDXL(
			mp, md_accessor, var_colid_mapping, (Expr *) node);

	// cleanup
	dxl_col_descr_array->Release();
	GPOS_DELETE(var_colid_mapping);

	mdid->AddRef();

	return GPOS_NEW(mp)
		CMDCheckConstraintGPDB(mp, mdid, mdname, mdid_rel, scalar_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetTypeName
//
//	@doc:
//		Retrieve a type's name from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDName *
CTranslatorRelcacheToDXL::GetTypeName(CMemoryPool *mp, IMDId *mdid)
{
	OID oid_type = CMDIdGPDB::CastMdid(mdid)->Oid();

	GPOS_ASSERT(InvalidOid != oid_type);

	CHAR *typename_str = gpdb::GetTypeName(oid_type);
	GPOS_ASSERT(nullptr != typename_str);

	CWStringDynamic *str_name =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, typename_str);
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, str_name);

	// cleanup
	GPOS_DELETE(str_name);
	return mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetFuncStability
//
//	@doc:
//		Get function stability property from the GPDB character representation
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncStbl
CTranslatorRelcacheToDXL::GetFuncStability(CHAR c)
{
	CMDFunctionGPDB::EFuncStbl efuncstbl = CMDFunctionGPDB::EfsSentinel;

	switch (c)
	{
		case 's':
			efuncstbl = CMDFunctionGPDB::EfsStable;
			break;
		case 'i':
			efuncstbl = CMDFunctionGPDB::EfsImmutable;
			break;
		case 'v':
			efuncstbl = CMDFunctionGPDB::EfsVolatile;
			break;
		default:
			GPOS_ASSERT(!"Invalid stability property");
	}

	return efuncstbl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetEFuncDataAccess
//
//	@doc:
//		Get function data access property from the GPDB character representation
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncDataAcc
CTranslatorRelcacheToDXL::GetEFuncDataAccess(CHAR c)
{
	CMDFunctionGPDB::EFuncDataAcc access = CMDFunctionGPDB::EfdaSentinel;

	switch (c)
	{
		case 'n':
			access = CMDFunctionGPDB::EfdaNoSQL;
			break;
		case 'c':
			access = CMDFunctionGPDB::EfdaContainsSQL;
			break;
		case 'r':
			access = CMDFunctionGPDB::EfdaReadsSQLData;
			break;
		case 'm':
			access = CMDFunctionGPDB::EfdaModifiesSQLData;
			break;
		case 's':
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
					   GPOS_WSZ_LIT("unknown data access"));
		default:
			GPOS_ASSERT(!"Invalid data access property");
	}

	return access;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveAggIntermediateResultType
//
//	@doc:
//		Retrieve the type id of an aggregate's intermediate results
//
//---------------------------------------------------------------------------
IMDId *
CTranslatorRelcacheToDXL::RetrieveAggIntermediateResultType(CMemoryPool *mp,
															IMDId *mdid)
{
	OID agg_oid = CMDIdGPDB::CastMdid(mdid)->Oid();
	OID intermediate_type_oid;

	GPOS_ASSERT(InvalidOid != agg_oid);
	intermediate_type_oid = gpdb::GetAggIntermediateResultType(agg_oid);

	/*
	 * If the transition type is 'internal', we will use the
	 * serial/deserial type to convert it to a bytea, for transfer
	 * between the segments. Therefore return 'bytea' as the
	 * intermediate type, so that any Motion nodes in between use the
	 * right datatype.
	 */
	if (intermediate_type_oid == INTERNALOID)
	{
		intermediate_type_oid = BYTEAOID;
	}

	return GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, intermediate_type_oid);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelStats
//
//	@doc:
//		Retrieve relation statistics from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveRelStats(CMemoryPool *mp, IMDId *mdid)
{
	CMDIdRelStats *m_rel_stats_mdid = CMDIdRelStats::CastMdid(mdid);
	IMDId *mdid_rel = m_rel_stats_mdid->GetRelMdId();
	OID rel_oid = CMDIdGPDB::CastMdid(mdid_rel)->Oid();

	gpdb::RelationWrapper rel = gpdb::GetRelation(rel_oid);
	if (!rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	double num_rows = 0.0;
	CMDName *mdname = nullptr;

	// get rel name
	CHAR *relname = NameStr(rel->rd_rel->relname);
	CWStringDynamic *relname_str =
		CDXLUtils::CreateDynamicStringFromCharArray(mp, relname);
	mdname = GPOS_NEW(mp) CMDName(mp, relname_str);
	// CMDName ctor created a copy of the string
	GPOS_DELETE(relname_str);

	num_rows = gpdb::CdbEstimatePartitionedNumTuples(rel.get());

	m_rel_stats_mdid->AddRef();

	/*
	 * relation_empty should be set to true only if the total row
	 * count of the partition table is 0.
	 */
	BOOL relation_empty = false;
	if (num_rows == 0.0)
	{
		relation_empty = true;
	}

	ULONG relpages = rel->rd_rel->relpages;
	ULONG relallvisible = rel->rd_rel->relallvisible;

	CDXLRelStats *dxl_rel_stats = GPOS_NEW(mp)
		CDXLRelStats(mp, m_rel_stats_mdid, mdname, CDouble(num_rows),
					 relation_empty, relpages, relallvisible);

	return dxl_rel_stats;
}

// Retrieve column statistics from relcache
// If all statistics are missing, create dummy statistics
// Also, if the statistics are broken, create dummy statistics
// However, if any statistics are present and not broken,
// create column statistics using these statistics
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveColStats(CMemoryPool *mp,
										   CMDAccessor *md_accessor,
										   IMDId *mdid)
{
	CMDIdColStats *mdid_col_stats = CMDIdColStats::CastMdid(mdid);
	IMDId *mdid_rel = mdid_col_stats->GetRelMdId();
	ULONG pos = mdid_col_stats->Position();
	OID rel_oid = CMDIdGPDB::CastMdid(mdid_rel)->Oid();

	const IMDRelation *md_rel = md_accessor->RetrieveRel(mdid_rel);
	const IMDColumn *md_col = md_rel->GetMdCol(pos);
	AttrNumber attno = (AttrNumber) md_col->AttrNum();

	// number of rows from pg_class
	double num_rows;

	num_rows =
		gpdb::CdbEstimatePartitionedNumTuples(gpdb::GetRelation(rel_oid).get());

	// extract column name and type
	CMDName *md_colname =
		GPOS_NEW(mp) CMDName(mp, md_col->Mdname().GetMDName());
	OID att_type = CMDIdGPDB::CastMdid(md_col->MdidType())->Oid();

	CDXLBucketArray *dxl_stats_bucket_array = GPOS_NEW(mp) CDXLBucketArray(mp);

	// extract out histogram and mcv information from pg_statistic
	HeapTuple stats_tup = gpdb::GetAttStats(rel_oid, attno);

	// if there is no colstats
	if (!HeapTupleIsValid(stats_tup))
	{
		dxl_stats_bucket_array->Release();
		mdid_col_stats->AddRef();

		CDouble width = CStatistics::DefaultColumnWidth;

		if (!md_col->IsDropped())
		{
			CMDIdGPDB *mdid_atttype =
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, att_type);
			IMDType *md_type = RetrieveType(mp, mdid_atttype);
			width = CStatisticsUtils::DefaultColumnWidth(md_type);
			md_type->Release();
			mdid_atttype->Release();
		}

		return CDXLColStats::CreateDXLDummyColStats(mp, mdid_col_stats,
													md_colname, width);
	}

	Form_pg_statistic form_pg_stats = (Form_pg_statistic) GETSTRUCT(stats_tup);

	// null frequency and NDV
	CDouble null_freq(0.0);
	if (CStatistics::Epsilon < form_pg_stats->stanullfrac)
	{
		null_freq = form_pg_stats->stanullfrac;
	}

	// column width
	CDouble width = CDouble(form_pg_stats->stawidth);

	// calculate total number of distinct values
	CDouble num_distinct(1.0);
	if (form_pg_stats->stadistinct < 0)
	{
		GPOS_ASSERT(form_pg_stats->stadistinct > -1.01);
		num_distinct =
			num_rows * (1 - null_freq) * CDouble(-form_pg_stats->stadistinct);
	}
	else
	{
		num_distinct = CDouble(form_pg_stats->stadistinct);
	}
	num_distinct = num_distinct.Ceil();

	BOOL is_dummy_stats = false;
	// most common values and their frequencies extracted from the pg_statistic
	// tuple for a given column
	AttStatsSlot mcv_slot;

	(void) gpdb::GetAttrStatsSlot(&mcv_slot, stats_tup, STATISTIC_KIND_MCV,
								  InvalidOid,
								  ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
	if (InvalidOid != mcv_slot.valuetype && mcv_slot.valuetype != att_type)
	{
		char msgbuf[NAMEDATALEN * 2 + 100];
		snprintf(
			msgbuf, sizeof(msgbuf),
			"Type mismatch between attribute %ls of table %ls having type %d and statistic having type %d, please ANALYZE the table again",
			md_col->Mdname().GetMDName()->GetBuffer(),
			md_rel->Mdname().GetMDName()->GetBuffer(), att_type,
			mcv_slot.valuetype);
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION, NOTICE, msgbuf, nullptr);

		gpdb::FreeAttrStatsSlot(&mcv_slot);
		is_dummy_stats = true;
	}

	else if (mcv_slot.nvalues != mcv_slot.nnumbers)
	{
		char msgbuf[NAMEDATALEN * 2 + 100];
		snprintf(
			msgbuf, sizeof(msgbuf),
			"The number of most common values and frequencies do not match on column %ls of table %ls.",
			md_col->Mdname().GetMDName()->GetBuffer(),
			md_rel->Mdname().GetMDName()->GetBuffer());
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION, NOTICE, msgbuf, nullptr);

		// if the number of MCVs(nvalues) and number of MCFs(nnumbers) do not match, we discard the MCVs and MCFs
		gpdb::FreeAttrStatsSlot(&mcv_slot);
		is_dummy_stats = true;
	}
	else
	{
		// fix mcv and null frequencies (sometimes they can add up to more than 1.0)
		NormalizeFrequencies(mcv_slot.numbers, (ULONG) mcv_slot.nvalues,
							 &null_freq);

		// total MCV frequency
		CDouble sum_mcv_freq = 0.0;
		for (int i = 0; i < mcv_slot.nvalues; i++)
		{
			sum_mcv_freq = sum_mcv_freq + CDouble(mcv_slot.numbers[i]);
		}
	}

	// histogram values extracted from the pg_statistic tuple for a given column
	AttStatsSlot hist_slot;

	// get histogram datums from pg_statistic entry
	(void) gpdb::GetAttrStatsSlot(&hist_slot, stats_tup,
								  STATISTIC_KIND_HISTOGRAM, InvalidOid,
								  ATTSTATSSLOT_VALUES);

	if (InvalidOid != hist_slot.valuetype && hist_slot.valuetype != att_type)
	{
		char msgbuf[NAMEDATALEN * 2 + 100];
		snprintf(
			msgbuf, sizeof(msgbuf),
			"Type mismatch between attribute %ls of table %ls having type %d and statistic having type %d, please ANALYZE the table again",
			md_col->Mdname().GetMDName()->GetBuffer(),
			md_rel->Mdname().GetMDName()->GetBuffer(), att_type,
			hist_slot.valuetype);
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION, NOTICE, msgbuf, nullptr);

		gpdb::FreeAttrStatsSlot(&hist_slot);
		is_dummy_stats = true;
	}

	if (is_dummy_stats)
	{
		dxl_stats_bucket_array->Release();
		mdid_col_stats->AddRef();

		CDouble col_width = CStatistics::DefaultColumnWidth;
		gpdb::FreeHeapTuple(stats_tup);
		return CDXLColStats::CreateDXLDummyColStats(mp, mdid_col_stats,
													md_colname, col_width);
	}

	CDouble num_ndv_buckets(0.0);
	CDouble num_freq_buckets(0.0);
	CDouble distinct_remaining(0.0);
	CDouble freq_remaining(0.0);

	// transform all the bits and pieces from pg_statistic
	// to a single bucket structure
	CDXLBucketArray *dxl_stats_bucket_array_transformed =
		TransformStatsToDXLBucketArray(
			mp, att_type, num_distinct, null_freq, mcv_slot.values,
			mcv_slot.numbers, ULONG(mcv_slot.nvalues), hist_slot.values,
			ULONG(hist_slot.nvalues));

	GPOS_ASSERT(nullptr != dxl_stats_bucket_array_transformed);

	const ULONG num_buckets = dxl_stats_bucket_array_transformed->Size();
	for (ULONG ul = 0; ul < num_buckets; ul++)
	{
		CDXLBucket *dxl_bucket = (*dxl_stats_bucket_array_transformed)[ul];
		num_ndv_buckets = num_ndv_buckets + dxl_bucket->GetNumDistinct();
		num_freq_buckets = num_freq_buckets + dxl_bucket->GetFrequency();
	}

	CUtils::AddRefAppend(dxl_stats_bucket_array,
						 dxl_stats_bucket_array_transformed);
	dxl_stats_bucket_array_transformed->Release();

	// there will be remaining tuples if the merged histogram and the NULLS do not cover
	// the total number of distinct values
	if ((1 - CStatistics::Epsilon > num_freq_buckets + null_freq) &&
		(0 < num_distinct - num_ndv_buckets))
	{
		distinct_remaining =
			std::max(CDouble(0.0), (num_distinct - num_ndv_buckets));
		freq_remaining =
			std::max(CDouble(0.0), (1 - num_freq_buckets - null_freq));
	}

	// free up allocated datum and float4 arrays
	gpdb::FreeAttrStatsSlot(&mcv_slot);
	gpdb::FreeAttrStatsSlot(&hist_slot);

	gpdb::FreeHeapTuple(stats_tup);

	// create col stats object
	mdid_col_stats->AddRef();
	CDXLColStats *dxl_col_stats = GPOS_NEW(mp) CDXLColStats(
		mp, mdid_col_stats, md_colname, width, null_freq, distinct_remaining,
		freq_remaining, dxl_stats_bucket_array, false /* is_col_stats_missing */
	);

	return dxl_col_stats;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveCast
//
//	@doc:
//		Retrieve a cast function from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveCast(CMemoryPool *mp, IMDId *mdid)
{
	CMDIdCast *mdid_cast = CMDIdCast::CastMdid(mdid);
	IMDId *mdid_src = mdid_cast->MdidSrc();
	IMDId *mdid_dest = mdid_cast->MdidDest();

	OID src_oid = CMDIdGPDB::CastMdid(mdid_src)->Oid();
	OID dest_oid = CMDIdGPDB::CastMdid(mdid_dest)->Oid();
	CoercionPathType pathtype;

	OID cast_fn_oid = 0;
	BOOL is_binary_coercible = false;

	BOOL cast_exists = gpdb::GetCastFunc(
		src_oid, dest_oid, &is_binary_coercible, &cast_fn_oid, &pathtype);

	if (!cast_exists)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CHAR *func_name = nullptr;
	if (InvalidOid != cast_fn_oid)
	{
		func_name = gpdb::GetFuncName(cast_fn_oid);
	}
	else
	{
		// no explicit cast function: use the destination type name as the cast name
		func_name = gpdb::GetTypeName(dest_oid);
	}

	if (nullptr == func_name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	mdid->AddRef();
	mdid_src->AddRef();
	mdid_dest->AddRef();

	CMDName *mdname = CDXLUtils::CreateMDNameFromCharArray(mp, func_name);

	switch (pathtype)
	{
		case COERCION_PATH_ARRAYCOERCE:
		{
			return GPOS_NEW(mp) CMDArrayCoerceCastGPDB(
				mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, cast_fn_oid),
				IMDCast::EmdtArrayCoerce, default_type_modifier, false,
				EdxlcfImplicitCast, -1);
		}
		break;
		case COERCION_PATH_FUNC:
			return GPOS_NEW(mp) CMDCastGPDB(
				mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, cast_fn_oid),
				IMDCast::EmdtFunc);
			break;
		case COERCION_PATH_RELABELTYPE:
			// binary-compatible cast, no function
			GPOS_ASSERT(cast_fn_oid == 0);
			return GPOS_NEW(mp) CMDCastGPDB(
				mp, mdid, mdname, mdid_src, mdid_dest,
				true /*is_binary_coercible*/,
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, cast_fn_oid));
			break;
		case COERCION_PATH_COERCEVIAIO:
			// uses IO functions from types, no function in the cast
			GPOS_ASSERT(cast_fn_oid == 0);
			return GPOS_NEW(mp) CMDCastGPDB(
				mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, cast_fn_oid),
				IMDCast::EmdtCoerceViaIO);
		default:
			break;
	}

	// fall back for none path types
	return GPOS_NEW(mp)
		CMDCastGPDB(mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
					GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, cast_fn_oid));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveScCmp
//
//	@doc:
//		Retrieve a scalar comparison from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::RetrieveScCmp(CMemoryPool *mp, IMDId *mdid)
{
	CMDIdScCmp *mdid_scalar_cmp = CMDIdScCmp::CastMdid(mdid);
	IMDId *mdid_left = mdid_scalar_cmp->GetLeftMdid();
	IMDId *mdid_right = mdid_scalar_cmp->GetRightMdid();

	IMDType::ECmpType cmp_type = mdid_scalar_cmp->ParseCmpType();

	OID left_oid = CMDIdGPDB::CastMdid(mdid_left)->Oid();
	OID right_oid = CMDIdGPDB::CastMdid(mdid_right)->Oid();
	CmpType cmpt = (CmpType) GetComparisonType(cmp_type);

	OID scalar_cmp_oid = gpdb::GetComparisonOperator(left_oid, right_oid, cmpt);

	if (InvalidOid == scalar_cmp_oid)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	CHAR *name = gpdb::GetOpName(scalar_cmp_oid);

	if (nullptr == name)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
				   mdid->GetBuffer());
	}

	mdid->AddRef();
	mdid_left->AddRef();
	mdid_right->AddRef();

	CMDName *mdname = CDXLUtils::CreateMDNameFromCharArray(mp, name);

	return GPOS_NEW(mp) CMDScCmpGPDB(
		mp, mdid, mdname, mdid_left, mdid_right, cmp_type,
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, scalar_cmp_oid));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::TransformStatsToDXLBucketArray
//
//	@doc:
//		transform stats from pg_stats form to optimizer's preferred form
//
//---------------------------------------------------------------------------
CDXLBucketArray *
CTranslatorRelcacheToDXL::TransformStatsToDXLBucketArray(
	CMemoryPool *mp, OID att_type, CDouble num_distinct, CDouble null_freq,
	const Datum *mcv_values, const float4 *mcv_frequencies,
	ULONG num_mcv_values, const Datum *hist_values, ULONG num_hist_values)
{
	CMDIdGPDB *mdid_atttype =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, att_type);
	IMDType *md_type = RetrieveType(mp, mdid_atttype);

	// translate MCVs to Orca histogram. Create an empty histogram if there are no MCVs.
	CHistogram *gpdb_mcv_hist = TransformMcvToOrcaHistogram(
		mp, md_type, mcv_values, mcv_frequencies, num_mcv_values);

	GPOS_ASSERT(gpdb_mcv_hist->IsValid());

	CDouble mcv_freq = gpdb_mcv_hist->GetFrequency();
	BOOL has_mcv = 0 < num_mcv_values && CStatistics::Epsilon < mcv_freq;

	CDouble hist_freq = 0.0;
	if (1 < num_hist_values)
	{
		hist_freq = CDouble(1.0) - null_freq - mcv_freq;
	}

	BOOL is_text_type = mdid_atttype->Equals(&CMDIdGPDB::m_mdid_varchar) ||
						mdid_atttype->Equals(&CMDIdGPDB::m_mdid_bpchar) ||
						mdid_atttype->Equals(&CMDIdGPDB::m_mdid_text);
	BOOL has_hist = !is_text_type && 1 < num_hist_values &&
					CStatistics::Epsilon < hist_freq;

	CHistogram *histogram = nullptr;

	// if histogram has any significant information, then extract it
	if (has_hist)
	{
		// histogram from gpdb histogram
		histogram = TransformHistToOrcaHistogram(
			mp, md_type, hist_values, num_hist_values, num_distinct, hist_freq);
		if (0 == histogram->GetNumBuckets())
		{
			has_hist = false;
		}
	}

	CDXLBucketArray *dxl_stats_bucket_array = nullptr;

	if (has_hist && !has_mcv)
	{
		// if histogram exists and dominates, use histogram only
		dxl_stats_bucket_array =
			TransformHistogramToDXLBucketArray(mp, md_type, histogram);
	}
	else if (!has_hist && has_mcv)
	{
		// if MCVs exist and dominate, use MCVs only
		dxl_stats_bucket_array =
			TransformHistogramToDXLBucketArray(mp, md_type, gpdb_mcv_hist);
	}
	else if (has_hist && has_mcv)
	{
		// both histogram and MCVs exist and have significant info, merge MCV and histogram buckets
		CHistogram *merged_hist =
			CStatisticsUtils::MergeMCVHist(mp, gpdb_mcv_hist, histogram);
		dxl_stats_bucket_array =
			TransformHistogramToDXLBucketArray(mp, md_type, merged_hist);
		GPOS_DELETE(merged_hist);
	}
	else
	{
		// no MCVs nor histogram
		GPOS_ASSERT(!has_hist && !has_mcv);
		dxl_stats_bucket_array = GPOS_NEW(mp) CDXLBucketArray(mp);
	}

	// cleanup
	mdid_atttype->Release();
	md_type->Release();
	GPOS_DELETE(gpdb_mcv_hist);

	if (nullptr != histogram)
	{
		GPOS_DELETE(histogram);
	}

	return dxl_stats_bucket_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::TransformMcvToOrcaHistogram
//
//	@doc:
//		Transform gpdb's mcv info to optimizer histogram
//
//---------------------------------------------------------------------------
CHistogram *
CTranslatorRelcacheToDXL::TransformMcvToOrcaHistogram(
	CMemoryPool *mp, const IMDType *md_type, const Datum *mcv_values,
	const float4 *mcv_frequencies, ULONG num_mcv_values)
{
	IDatumArray *datums = GPOS_NEW(mp) IDatumArray(mp);
	CDoubleArray *freqs = GPOS_NEW(mp) CDoubleArray(mp);

	for (ULONG ul = 0; ul < num_mcv_values; ul++)
	{
		Datum datumMCV = mcv_values[ul];
		IDatum *datum = CTranslatorScalarToDXL::CreateIDatumFromGpdbDatum(
			mp, md_type, false /* is_null */, datumMCV);
		datums->Append(datum);
		freqs->Append(GPOS_NEW(mp) CDouble(mcv_frequencies[ul]));

		if (!datum->StatsAreComparable(datum))
		{
			// if less than operation is not supported on this datum, then no point
			// building a histogram. return an empty histogram
			datums->Release();
			freqs->Release();
			return GPOS_NEW(mp) CHistogram(mp);
		}
	}

	CHistogram *hist = CStatisticsUtils::TransformMCVToHist(
		mp, md_type, datums, freqs, num_mcv_values);

	datums->Release();
	freqs->Release();
	return hist;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::TransformHistToOrcaHistogram
//
//	@doc:
//		Transform GPDB's hist info to optimizer's histogram
//
//---------------------------------------------------------------------------
CHistogram *
CTranslatorRelcacheToDXL::TransformHistToOrcaHistogram(
	CMemoryPool *mp, const IMDType *md_type, const Datum *hist_values,
	ULONG num_hist_values, CDouble num_distinct, CDouble hist_freq)
{
	GPOS_ASSERT(1 < num_hist_values);

	const ULONG num_buckets = num_hist_values - 1;
	CDouble distinct_per_bucket = num_distinct / CDouble(num_buckets);
	CDouble freq_per_bucket = hist_freq / CDouble(num_buckets);

	BOOL last_bucket_was_singleton = false;
	// create buckets
	CBucketArray *buckets = GPOS_NEW(mp) CBucketArray(mp);
	for (ULONG ul = 0; ul < num_buckets; ul++)
	{
		IDatum *min_datum = CTranslatorScalarToDXL::CreateIDatumFromGpdbDatum(
			mp, md_type, false /* is_null */, hist_values[ul]);
		IDatum *max_datum = CTranslatorScalarToDXL::CreateIDatumFromGpdbDatum(
			mp, md_type, false /* is_null */, hist_values[ul + 1]);
		BOOL is_lower_closed, is_upper_closed;

		if (min_datum->StatsAreEqual(max_datum))
		{
			// Singleton bucket !!!!!!!!!!!!!
			is_lower_closed = true;
			is_upper_closed = true;
			last_bucket_was_singleton = true;
		}
		else if (last_bucket_was_singleton)
		{
			// Last bucket was a singleton, so lower must be open now.
			is_lower_closed = false;
			is_upper_closed = false;
			last_bucket_was_singleton = false;
		}
		else
		{
			// Normal bucket
			// GPDB histograms assumes lower bound to be closed and upper bound to be open
			is_lower_closed = true;
			is_upper_closed = false;
		}

		if (ul == num_buckets - 1)
		{
			// last bucket upper bound is also closed
			is_upper_closed = true;
		}

		CBucket *bucket = GPOS_NEW(mp)
			CBucket(GPOS_NEW(mp) CPoint(min_datum),
					GPOS_NEW(mp) CPoint(max_datum), is_lower_closed,
					is_upper_closed, freq_per_bucket, distinct_per_bucket);
		buckets->Append(bucket);

		if (!min_datum->StatsAreComparable(max_datum) ||
			!min_datum->StatsAreLessThan(max_datum))
		{
			// if less than operation is not supported on this datum,
			// or the translated histogram does not conform to GPDB sort order (e.g. text column in Linux platform),
			// then no point building a histogram. return an empty histogram

			// TODO: 03/01/2014 translate histogram into Orca even if sort
			// order is different in GPDB, and use const expression eval to compare
			// datums in Orca (MPP-22780)
			buckets->Release();
			return GPOS_NEW(mp) CHistogram(mp);
		}
	}

	CHistogram *hist = GPOS_NEW(mp) CHistogram(mp, buckets);
	return hist;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::TransformHistogramToDXLBucketArray
//
//	@doc:
//		Histogram to array of dxl buckets
//
//---------------------------------------------------------------------------
CDXLBucketArray *
CTranslatorRelcacheToDXL::TransformHistogramToDXLBucketArray(
	CMemoryPool *mp, const IMDType *md_type, const CHistogram *hist)
{
	CDXLBucketArray *dxl_stats_bucket_array = GPOS_NEW(mp) CDXLBucketArray(mp);
	const CBucketArray *buckets = hist->GetBuckets();
	ULONG num_buckets = buckets->Size();
	for (ULONG ul = 0; ul < num_buckets; ul++)
	{
		CBucket *bucket = (*buckets)[ul];
		IDatum *datum_lower = bucket->GetLowerBound()->GetDatum();
		CDXLDatum *dxl_lower = md_type->GetDatumVal(mp, datum_lower);
		IDatum *datum_upper = bucket->GetUpperBound()->GetDatum();
		CDXLDatum *dxl_upper = md_type->GetDatumVal(mp, datum_upper);
		CDXLBucket *dxl_bucket = GPOS_NEW(mp)
			CDXLBucket(dxl_lower, dxl_upper, bucket->IsLowerClosed(),
					   bucket->IsUpperClosed(), bucket->GetFrequency(),
					   bucket->GetNumDistinct());
		dxl_stats_bucket_array->Append(dxl_bucket);
	}
	return dxl_stats_bucket_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelStorageType
//
//	@doc:
//		Get relation storage type
//
//---------------------------------------------------------------------------
IMDRelation::Erelstoragetype
CTranslatorRelcacheToDXL::RetrieveRelStorageType(Relation rel)
{
	IMDRelation::Erelstoragetype rel_storage_type =
		IMDRelation::ErelstorageSentinel;

	// handle partition root first, note the partition type returned here
	// is not necessarily the same as the one root partition carries
	if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
	{
		return RetrieveStorageTypeForPartitionedTable(rel);
	}

	switch (rel->rd_rel->relam)
	{
		case HEAP_TABLE_AM_OID:
			rel_storage_type = IMDRelation::ErelstorageHeap;
			break;
		case AO_COLUMN_TABLE_AM_OID:
			rel_storage_type = IMDRelation::ErelstorageAppendOnlyCols;
			break;
		case AO_ROW_TABLE_AM_OID:
			rel_storage_type = IMDRelation::ErelstorageAppendOnlyRows;
			break;
		case 0:

			if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
			{
				rel_storage_type = IMDRelation::ErelstorageCompositeType;
			}
			else if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
			{
				if (!optimizer_enable_foreign_table)
				{
					GPOS_RAISE(
						gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
						GPOS_WSZ_LIT(
							"Use optimizer_enable_foreign_table to enable Orca with foreign tables"));
				}
				rel_storage_type = IMDRelation::ErelstorageForeign;
			}
			else
			{
				GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
						   GPOS_WSZ_LIT("Unsupported table AM"));
			}
			break;
		default:
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
					   GPOS_WSZ_LIT("Unsupported table AM"));
	}

	return rel_storage_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrievePartKeysAndTypes
//
//	@doc:
//		Get partition keys and types for relation or NULL if relation not partitioned.
//		Caller responsible for closing the relation if an exception is raised
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::RetrievePartKeysAndTypes(CMemoryPool *mp,
												   Relation rel, OID oid,
												   ULongPtrArray **part_keys,
												   CharPtrArray **part_types)
{
	GPOS_ASSERT(nullptr != rel);

	// FIXME: isn't it faster to check rel.rd_partkey?
	if (!rel->rd_partdesc)
	{
		// not a partitioned table
		*part_keys = nullptr;
		*part_types = nullptr;
		return;
	}

	*part_keys = GPOS_NEW(mp) ULongPtrArray(mp);
	*part_types = GPOS_NEW(mp) CharPtrArray(mp);

	PartitionKeyData *partkey = rel->rd_partkey;

	if (1 < partkey->partnatts)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("Composite part key"));
	}

	AttrNumber attno = partkey->partattrs[0];
	CHAR part_type = (CHAR) partkey->strategy;
	if (attno == 0)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("partitioning by expression"));
	}

	if (PARTITION_STRATEGY_HASH == part_type)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("hash partitioning"));
	}

	(*part_keys)->Append(GPOS_NEW(mp) ULONG(attno - 1));
	(*part_types)->Append(GPOS_NEW(mp) CHAR(part_type));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::ConstructAttnoMapping
//
//	@doc:
//		Construct a mapping for GPDB attnos to positions in the columns array
//
//---------------------------------------------------------------------------
ULONG *
CTranslatorRelcacheToDXL::ConstructAttnoMapping(CMemoryPool *mp,
												CMDColumnArray *mdcol_array,
												ULONG max_cols)
{
	GPOS_ASSERT(nullptr != mdcol_array);
	GPOS_ASSERT(0 < mdcol_array->Size());
	GPOS_ASSERT(max_cols > mdcol_array->Size());

	// build a mapping for attnos->positions
	const ULONG num_of_cols = mdcol_array->Size();
	ULONG *attno_mapping = GPOS_NEW_ARRAY(mp, ULONG, max_cols);

	// initialize all positions to gpos::ulong_max
	for (ULONG ul = 0; ul < max_cols; ul++)
	{
		attno_mapping[ul] = gpos::ulong_max;
	}

	for (ULONG ul = 0; ul < num_of_cols; ul++)
	{
		const IMDColumn *md_col = (*mdcol_array)[ul];
		INT attno = md_col->AttrNum();

		ULONG idx = (ULONG)(GPDXL_SYSTEM_COLUMNS + attno);
		attno_mapping[idx] = ul;
	}

	return attno_mapping;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveRelKeysets
//
//	@doc:
//		Get key sets for relation
//
//---------------------------------------------------------------------------
ULongPtr2dArray *
CTranslatorRelcacheToDXL::RetrieveRelKeysets(CMemoryPool *mp, OID oid,
											 BOOL should_add_default_keys,
											 BOOL is_partitioned,
											 ULONG *attno_mapping)
{
	ULongPtr2dArray *key_sets = GPOS_NEW(mp) ULongPtr2dArray(mp);

	List *rel_keys = gpdb::GetRelationKeys(oid);

	ListCell *lc_key = nullptr;
	ForEach(lc_key, rel_keys)
	{
		List *key_elem_list = (List *) lfirst(lc_key);

		ULongPtrArray *key_set = GPOS_NEW(mp) ULongPtrArray(mp);

		ListCell *lc_key_elem = nullptr;
		ForEach(lc_key_elem, key_elem_list)
		{
			INT key_idx = lfirst_int(lc_key_elem);
			ULONG pos = GetAttributePosition(key_idx, attno_mapping);
			key_set->Append(GPOS_NEW(mp) ULONG(pos));
		}
		GPOS_ASSERT(0 < key_set->Size());

		key_sets->Append(key_set);
	}

	// add {segid, ctid} as a key

	if (should_add_default_keys)
	{
		ULongPtrArray *key_set = GPOS_NEW(mp) ULongPtrArray(mp);
		if (is_partitioned)
		{
			// TableOid is part of default key for partitioned tables
			ULONG table_oid_pos =
				GetAttributePosition(TableOidAttributeNumber, attno_mapping);
			key_set->Append(GPOS_NEW(mp) ULONG(table_oid_pos));
		}
		ULONG seg_id_pos =
			GetAttributePosition(GpSegmentIdAttributeNumber, attno_mapping);
		ULONG ctid_pos =
			GetAttributePosition(SelfItemPointerAttributeNumber, attno_mapping);
		key_set->Append(GPOS_NEW(mp) ULONG(seg_id_pos));
		key_set->Append(GPOS_NEW(mp) ULONG(ctid_pos));

		key_sets->Append(key_set);
	}

	return key_sets;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::NormalizeFrequencies
//
//	@doc:
//		Sometimes a set of frequencies can add up to more than 1.0.
//		Fix these cases
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::NormalizeFrequencies(float4 *freqs, ULONG length,
											   CDouble *null_freq)
{
	if (length == 0 && (*null_freq) < 1.0)
	{
		return;
	}

	CDouble total = *null_freq;
	for (ULONG ul = 0; ul < length; ul++)
	{
		total = total + CDouble(freqs[ul]);
	}

	if (total > CDouble(1.0))
	{
		float4 denom = (float4)(total + CStatistics::Epsilon).Get();

		// divide all values by the total
		for (ULONG ul = 0; ul < length; ul++)
		{
			freqs[ul] = freqs[ul] / denom;
		}
		*null_freq = *null_freq / denom;
	}

#ifdef GPOS_DEBUG
	// recheck
	CDouble recheck_total = *null_freq;
	for (ULONG ul = 0; ul < length; ul++)
	{
		recheck_total = recheck_total + CDouble(freqs[ul]);
	}
	GPOS_ASSERT(recheck_total <= CDouble(1.0));
#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::IsIndexSupported
//
//	@doc:
//		Check if index type is supported
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::IsIndexSupported(Relation index_rel)
{
	HeapTupleData *tup = index_rel->rd_indextuple;

	// covering index -- it has INCLUDE (...) columns
	if (index_rel->rd_index->indnatts > index_rel->rd_index->indnkeyatts)
	{
		return false;
	}

	// index expressions and index constraints not supported
	return gpdb::HeapAttIsNull(tup, Anum_pg_index_indexprs) &&
		   gpdb::HeapAttIsNull(tup, Anum_pg_index_indpred) &&
		   index_rel->rd_index->indisvalid &&
		   (BTREE_AM_OID == index_rel->rd_rel->relam ||
			HASH_AM_OID == index_rel->rd_rel->relam ||
			BITMAP_AM_OID == index_rel->rd_rel->relam ||
			GIST_AM_OID == index_rel->rd_rel->relam ||
			GIN_AM_OID == index_rel->rd_rel->relam ||
			BRIN_AM_OID == index_rel->rd_rel->relam);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrievePartConstraintForIndex
//
//	@doc:
//		Retrieve part constraint for index
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB *
CTranslatorRelcacheToDXL::RetrievePartConstraintForIndex(
	CMemoryPool *mp, CMDAccessor *md_accessor, const IMDRelation *md_rel,
	Node *part_constraint, ULongPtrArray *level_with_default_part_array,
	BOOL is_unbounded)
{
	CDXLColDescrArray *dxl_col_descr_array = GPOS_NEW(mp) CDXLColDescrArray(mp);
	const ULONG num_columns = md_rel->ColumnCount();

	for (ULONG ul = 0; ul < num_columns; ul++)
	{
		const IMDColumn *md_col = md_rel->GetMdCol(ul);
		CMDName *md_colname =
			GPOS_NEW(mp) CMDName(mp, md_col->Mdname().GetMDName());
		CMDIdGPDB *mdid_col_type = CMDIdGPDB::CastMdid(md_col->MdidType());
		mdid_col_type->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *dxl_col_descr = GPOS_NEW(mp) CDXLColDescr(
			md_colname,
			ul + 1,	 // colid
			md_col->AttrNum(), mdid_col_type, md_col->TypeModifier(),
			false  // fColDropped
		);
		dxl_col_descr_array->Append(dxl_col_descr);
	}

	CMDPartConstraintGPDB *mdpart_constraint = RetrievePartConstraintFromNode(
		mp, md_accessor, dxl_col_descr_array, part_constraint,
		level_with_default_part_array, is_unbounded);

	dxl_col_descr_array->Release();

	return mdpart_constraint;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrievePartConstraintForRel
//
//	@doc:
//		Retrieve part constraint for relation
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorRelcacheToDXL::RetrievePartConstraintForRel(
	CMemoryPool *mp, CMDAccessor *md_accessor, Relation rel,
	CMDColumnArray *mdcol_array)
{
	// get the part constraints
	Node *node = gpdb::GetRelationPartConstraints(rel);

	if (nullptr == node)
	{
		return nullptr;
	}

	// create var-colid mapping for translating part constraints
	CAutoRef<CDXLColDescrArray> dxl_col_descr_array(GPOS_NEW(mp)
														CDXLColDescrArray(mp));
	const ULONG num_columns = mdcol_array->Size();
	for (ULONG ul = 0, idx = 0; ul < num_columns; ul++)
	{
		const IMDColumn *md_col = (*mdcol_array)[ul];

		if (md_col->IsDropped())
		{
			continue;
		}

		CMDName *md_colname =
			GPOS_NEW(mp) CMDName(mp, md_col->Mdname().GetMDName());
		CMDIdGPDB *mdid_col_type = CMDIdGPDB::CastMdid(md_col->MdidType());
		mdid_col_type->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *dxl_col_descr = GPOS_NEW(mp) CDXLColDescr(
			md_colname,
			idx + 1,  // colid
			md_col->AttrNum(), mdid_col_type, md_col->TypeModifier(),
			false  // fColDropped
		);
		dxl_col_descr_array->Append(dxl_col_descr);
		++idx;
	}

	CMappingVarColId var_colid_mapping(mp);
	var_colid_mapping.LoadColumns(0 /*query_level */, 1 /* rteIndex */,
								  dxl_col_descr_array.Value());
	CDXLNode *scalar_dxlnode =
		CTranslatorScalarToDXL::TranslateStandaloneExprToDXL(
			mp, md_accessor, &var_colid_mapping, (Expr *) node);

	return scalar_dxlnode;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrievePartConstraintFromNode
//
//	@doc:
//		Retrieve part constraint from GPDB node
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB *
CTranslatorRelcacheToDXL::RetrievePartConstraintFromNode(
	CMemoryPool *mp, CMDAccessor *md_accessor,
	CDXLColDescrArray *dxl_col_descr_array, Node *part_constraints,
	ULongPtrArray *level_with_default_part_array, BOOL is_unbounded)
{
	if (nullptr == part_constraints)
	{
		return nullptr;
	}

	// generate a mock mapping between var to column information
	CMappingVarColId *var_colid_mapping = GPOS_NEW(mp) CMappingVarColId(mp);

	var_colid_mapping->LoadColumns(0 /*query_level */, 1 /* rteIndex */,
								   dxl_col_descr_array);

	// translate the check constraint expression
	CDXLNode *scalar_dxlnode =
		CTranslatorScalarToDXL::TranslateStandaloneExprToDXL(
			mp, md_accessor, var_colid_mapping, (Expr *) part_constraints);

	// cleanup
	GPOS_DELETE(var_colid_mapping);

	level_with_default_part_array->AddRef();
	return GPOS_NEW(mp) CMDPartConstraintGPDB(mp, level_with_default_part_array,
											  is_unbounded, scalar_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RelHasSystemColumns
//
//	@doc:
//		Does given relation type have system columns.
//		Currently regular relations, sequences, toast values relations,
//		AO segment relations and foreign tables have system columns
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::RelHasSystemColumns(char rel_kind)
{
	return RELKIND_RELATION == rel_kind || RELKIND_SEQUENCE == rel_kind ||
		   RELKIND_AOSEGMENTS == rel_kind || RELKIND_TOASTVALUE == rel_kind ||
		   RELKIND_FOREIGN_TABLE == rel_kind ||
		   RELKIND_PARTITIONED_TABLE == rel_kind;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::ParseCmpType
//
//	@doc:
//		Translate GPDB comparison types into optimizer comparison types
//
//---------------------------------------------------------------------------
IMDType::ECmpType
CTranslatorRelcacheToDXL::ParseCmpType(ULONG cmpt)
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(cmp_type_mappings); ul++)
	{
		const ULONG *mapping = cmp_type_mappings[ul];
		if (mapping[1] == cmpt)
		{
			return (IMDType::ECmpType) mapping[0];
		}
	}

	return IMDType::EcmptOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetComparisonType
//
//	@doc:
//		Translate optimizer comparison types into GPDB comparison types
//
//---------------------------------------------------------------------------
ULONG
CTranslatorRelcacheToDXL::GetComparisonType(IMDType::ECmpType cmp_type)
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(cmp_type_mappings); ul++)
	{
		const ULONG *mapping = cmp_type_mappings[ul];
		if (mapping[0] == cmp_type)
		{
			return (ULONG) mapping[1];
		}
	}

	return CmptOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveIndexOpFamilies
//
//	@doc:
//		Retrieve the opfamilies for the keys of the given index
//
//---------------------------------------------------------------------------
IMdIdArray *
CTranslatorRelcacheToDXL::RetrieveIndexOpFamilies(CMemoryPool *mp,
												  IMDId *mdid_index)
{
	List *op_families =
		gpdb::GetIndexOpFamilies(CMDIdGPDB::CastMdid(mdid_index)->Oid());
	IMdIdArray *input_col_mdids = GPOS_NEW(mp) IMdIdArray(mp);

	ListCell *lc = nullptr;

	ForEach(lc, op_families)
	{
		OID op_family_oid = lfirst_oid(lc);
		input_col_mdids->Append(
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, op_family_oid));
	}

	return input_col_mdids;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::RetrieveScOpOpFamilies
//
//	@doc:
//		Retrieve the families for the keys of the given scalar operator
//
//---------------------------------------------------------------------------
IMdIdArray *
CTranslatorRelcacheToDXL::RetrieveScOpOpFamilies(CMemoryPool *mp,
												 IMDId *mdid_scalar_op)
{
	List *op_families =
		gpdb::GetOpFamiliesForScOp(CMDIdGPDB::CastMdid(mdid_scalar_op)->Oid());
	IMdIdArray *input_col_mdids = GPOS_NEW(mp) IMdIdArray(mp);

	ListCell *lc = nullptr;

	ForEach(lc, op_families)
	{
		OID op_family_oid = lfirst_oid(lc);
		input_col_mdids->Append(
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, op_family_oid));
	}

	return input_col_mdids;
}

IMdIdArray *
CTranslatorRelcacheToDXL::RetrieveIndexPartitions(CMemoryPool *mp, OID rel_oid)
{
	IMdIdArray *partition_oids = GPOS_NEW(mp) IMdIdArray(mp);

	List *partition_oid_list = gpdb::GetRelChildIndexes(rel_oid);
	ListCell *lc;
	foreach (lc, partition_oid_list)
	{
		OID oid = lfirst_oid(lc);
		partition_oids->Append(GPOS_NEW(mp)
								   CMDIdGPDB(IMDId::EmdidGeneral, oid));
	}

	return partition_oids;
}

IMDRelation::Erelstoragetype
CTranslatorRelcacheToDXL::RetrieveStorageTypeForPartitionedTable(Relation rel)
{
	IMDRelation::Erelstoragetype rel_storage_type =
		IMDRelation::ErelstorageSentinel;
	if (rel->rd_partdesc->nparts == 0)
	{
		return IMDRelation::ErelstorageHeap;
	}

	BOOL all_foreign = true;
	for (int i = 0; i < rel->rd_partdesc->nparts; ++i)
	{
		Oid oid = rel->rd_partdesc->oids[i];
		gpdb::RelationWrapper child_rel = gpdb::GetRelation(oid);
		IMDRelation::Erelstoragetype child_storage =
			RetrieveRelStorageType(child_rel.get());
		if (child_storage == IMDRelation::ErelstorageForeign)
		{
			// for partitioned tables with foreign partitions, we want to ignore the foreign partitions
			// for determining the storage-type (unless all of the partitions are foreign) as we'll be separating them out to different scans later in CXformExpandDynamicGetWithForeignPartitions
			if (!optimizer_enable_foreign_table)
			{
				GPOS_RAISE(
					gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
					GPOS_WSZ_LIT(
						"Use optimizer_enable_foreign_table to enable Orca with foreign partitions"));
			}
			continue;
		}
		all_foreign = false;
		if (rel_storage_type == IMDRelation::ErelstorageSentinel)
		{
			rel_storage_type = child_storage;
		}

		// mark any partitioned table with supported partitions of mixed storage types,
		// this is more conservative for certain skans (eg: we can't do an index scan if any
		// partition is ao, we must only do a sequential or bitmap scan)
		if (rel_storage_type != child_storage)
		{
			rel_storage_type = IMDRelation::ErelstorageMixedPartitioned;
		}
	}
	if (all_foreign)
	{
		rel_storage_type = IMDRelation::ErelstorageForeign;
	}
	return rel_storage_type;
}


// EOF
