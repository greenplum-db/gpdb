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

#include "postgres.h"
#include "utils/array.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "utils/datum.h"
#include "utils/elog.h"
#include "utils/guc.h"
#include "cdb/cdbhash.h"
#include "access/heapam.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_proc.h"

#include "cdb/cdbpartition.h"
#include "catalog/namespace.h"
#include "catalog/pg_statistic.h"

#include "naucrates/md/CMDIdCast.h"
#include "naucrates/md/CMDIdScCmp.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/md/CMDCastGPDB.h"
#include "naucrates/md/CMDArrayCoerceCastGPDB.h"
#include "naucrates/md/CMDScCmpGPDB.h"

#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "gpos/base.h"
#include "gpos/error/CException.h"

#include "naucrates/exception.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/md/CMDTypeBoolGPDB.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"
#include "naucrates/md/CMDTypeInt2GPDB.h"
#include "naucrates/md/CMDTypeInt4GPDB.h"
#include "naucrates/md/CMDTypeInt8GPDB.h"
#include "naucrates/md/CMDTypeOidGPDB.h"
#include "naucrates/md/CMDIndexGPDB.h"
#include "naucrates/md/CMDPartConstraintGPDB.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/CDXLRelStats.h"
#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/CDXLColStats.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpopt;


static 
const ULONG rgulCmpTypeMappings[][2] = 
{
	{IMDType::EcmptEq, CmptEq},
	{IMDType::EcmptNEq, CmptNEq},
	{IMDType::EcmptL, CmptLT},
	{IMDType::EcmptG, CmptGT},
	{IMDType::EcmptGEq, CmptGEq},
	{IMDType::EcmptLEq, CmptLEq}
};

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pimdobj
//
//	@doc:
//		Retrieve a metadata object from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::Pimdobj
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
{
	IMDCacheObject *pmdcacheobj = NULL;
	GPOS_ASSERT(NULL != pmda);

#ifdef FAULT_INJECTOR
	gpdb::OptTasksFaultInjector(OptRelcacheTranslatorCatalogAccess);
#endif // FAULT_INJECTOR

	switch(pmdid->Emdidt())
	{
		case IMDId::EmdidGPDB:
			pmdcacheobj = PimdobjGPDB(pmp, pmda, pmdid);
			break;
		
		case IMDId::EmdidRelStats:
			pmdcacheobj = PimdobjRelStats(pmp, pmdid);
			break;
		
		case IMDId::EmdidColStats:
			pmdcacheobj = PimdobjColStats(pmp, pmda, pmdid);
			break;
		
		case IMDId::EmdidCastFunc:
			pmdcacheobj = PimdobjCast(pmp, pmdid);
			break;
		
		case IMDId::EmdidScCmp:
			pmdcacheobj = PmdobjScCmp(pmp, pmdid);
			break;
			
		default:
			break;
	}

	if (NULL == pmdcacheobj)
	{
		// no match found
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	return pmdcacheobj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PimdobjGPDB
//
//	@doc:
//		Retrieve a GPDB metadata object from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::PimdobjGPDB
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
{
	GPOS_ASSERT(pmdid->Emdidt() == CMDIdGPDB::EmdidGPDB);

	OID oid = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	// GPDB_84_MERGE_FIXME: The OIDS of a few built-in window
	// functions have been hard-coded in ORCA. But the OIDs
	// were changed when we merged the upstream window function
	// implementation, to match the upstream OIDs. Map the old
	// OIDs to the upstream ones.
	if (oid == 7000)	// ROW_NUMBER()
		oid = 3100;
	if (oid == 7002)	// DENSE_RANK()
		oid = 3102;
	if (oid == 7003)	// PERCENT_RANK()
		oid = 3103;
	if (oid == 7004)	// CUME_DIST()
		oid = 3104;
	if (oid == 7005)	// NTILE(int4)
		oid = 3105;

	GPOS_ASSERT(0 != oid);

	// find out what type of object this oid stands for

	if (gpdb::FIndexExists(oid))
	{
		return Pmdindex(pmp, pmda, pmdid);
	}

	if (gpdb::FTypeExists(oid))
	{
		return Pmdtype(pmp, pmdid);
	}

	if (gpdb::FRelationExists(oid))
	{
		return Pmdrel(pmp, pmda, pmdid);
	}

	if (gpdb::FOperatorExists(oid))
	{
		return Pmdscop(pmp, pmdid);
	}

	if (gpdb::FAggregateExists(oid))
	{
		return Pmdagg(pmp, pmdid);
	}

	if (gpdb::FFunctionExists(oid))
	{
		return Pmdfunc(pmp, pmdid);
	}

	if (gpdb::FTriggerExists(oid))
	{
		return Pmdtrigger(pmp, pmdid);
	}

	if (gpdb::FCheckConstraintExists(oid))
	{
		return Pmdcheckconstraint(pmp, pmda, pmdid);
	}

	// no match found
	return NULL;

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdnameRel
//
//	@doc:
//		Return a relation name
//
//---------------------------------------------------------------------------
CMDName *
CTranslatorRelcacheToDXL::PmdnameRel
	(
	IMemoryPool *pmp,
	Relation rel
	)
{
	GPOS_ASSERT(NULL != rel);
	CHAR *szRelName = NameStr(rel->rd_rel->relname);
	CWStringDynamic *pstrRelName = CDXLUtils::PstrFromSz(pmp, szRelName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrRelName);
	GPOS_DELETE(pstrRelName);
	return pmdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpmdRelIndexInfo
//
//	@doc:
//		Return the indexes defined on the given relation
//
//---------------------------------------------------------------------------
DrgPmdIndexInfo *
CTranslatorRelcacheToDXL::PdrgpmdRelIndexInfo
	(
	IMemoryPool *pmp,
	Relation rel
	)
{
	GPOS_ASSERT(NULL != rel);

	if (gpdb::FRelPartIsNone(rel->rd_id) || gpdb::FLeafPartition(rel->rd_id))
	{
		return PdrgpmdRelIndexInfoNonPartTable(pmp, rel);
	}
	else if (gpdb::FRelPartIsRoot(rel->rd_id))
	{
		return PdrgpmdRelIndexInfoPartTable(pmp, rel);
	}
	else  
	{
		// interior partition: do not consider indexes
		DrgPmdIndexInfo *pdrgpmdIndexInfo = GPOS_NEW(pmp) DrgPmdIndexInfo(pmp);
		return pdrgpmdIndexInfo;
	}
}

// return index info list of indexes defined on a partitioned table
DrgPmdIndexInfo *
CTranslatorRelcacheToDXL::PdrgpmdRelIndexInfoPartTable
	(
	IMemoryPool *pmp,
	Relation relRoot
	)
{
	DrgPmdIndexInfo *pdrgpmdIndexInfo = GPOS_NEW(pmp) DrgPmdIndexInfo(pmp);

	// root of partitioned table: aggregate index information across different parts
	List *plLogicalIndexInfo = PlIndexInfoPartTable(relRoot);

	ListCell *plc = NULL;

	ForEach (plc, plLogicalIndexInfo)
	{
		LogicalIndexInfo *logicalIndexInfo = (LogicalIndexInfo *) lfirst(plc);
		OID oidIndex = logicalIndexInfo->logicalIndexOid;

		// only add supported indexes
		Relation relIndex = gpdb::RelGetRelation(oidIndex);

		if (NULL == relIndex)
		{
			WCHAR wsz[1024];
			CWStringStatic str(wsz, 1024);
			COstreamString oss(&str);
			oss << (ULONG) oidIndex;
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, str.Wsz());
		}

		GPOS_ASSERT(NULL != relIndex->rd_indextuple);

		GPOS_TRY
		{
			if (FIndexSupported(relIndex))
			{
				CMDIdGPDB *pmdidIndex = GPOS_NEW(pmp) CMDIdGPDB(oidIndex);
				BOOL fPartial = (NULL != logicalIndexInfo->partCons) || (NIL != logicalIndexInfo->defaultLevels);
				CMDIndexInfo *pmdIndexInfo = GPOS_NEW(pmp) CMDIndexInfo(pmdidIndex, fPartial);
				pdrgpmdIndexInfo->Append(pmdIndexInfo);
			}

			gpdb::CloseRelation(relIndex);
		}
		GPOS_CATCH_EX(ex)
		{
			gpdb::CloseRelation(relIndex);
			GPOS_RETHROW(ex);
		}
		GPOS_CATCH_END;
	}
	return pdrgpmdIndexInfo;
}

// return index info list of indexes defined on regular, external tables or leaf partitions
DrgPmdIndexInfo *
CTranslatorRelcacheToDXL::PdrgpmdRelIndexInfoNonPartTable
	(
	IMemoryPool *pmp,
	Relation rel
	)
{
	DrgPmdIndexInfo *pdrgpmdIndexInfo = GPOS_NEW(pmp) DrgPmdIndexInfo(pmp);

	// not a partitioned table: obtain indexes directly from the catalog
	List *plIndexOids = gpdb::PlRelationIndexes(rel);

	ListCell *plc = NULL;

	ForEach (plc, plIndexOids)
	{
		OID oidIndex = lfirst_oid(plc);

		// only add supported indexes
		Relation relIndex = gpdb::RelGetRelation(oidIndex);

		if (NULL == relIndex)
		{
			WCHAR wsz[1024];
			CWStringStatic str(wsz, 1024);
			COstreamString oss(&str);
			oss << (ULONG) oidIndex;
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, str.Wsz());
		}

		GPOS_ASSERT(NULL != relIndex->rd_indextuple);

		GPOS_TRY
		{
			if (FIndexSupported(relIndex))
			{
				CMDIdGPDB *pmdidIndex = GPOS_NEW(pmp) CMDIdGPDB(oidIndex);
				// for a regular table, external table or leaf partition, an index is always complete
				CMDIndexInfo *pmdIndexInfo = GPOS_NEW(pmp) CMDIndexInfo(pmdidIndex, false /* fPartial */);
				pdrgpmdIndexInfo->Append(pmdIndexInfo);
			}

			gpdb::CloseRelation(relIndex);
		}
		GPOS_CATCH_EX(ex)
		{
			gpdb::CloseRelation(relIndex);
			GPOS_RETHROW(ex);
		}
		GPOS_CATCH_END;
	}

	return pdrgpmdIndexInfo;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PlIndexInfoPartTable
//
//	@doc:
//		Return the index info list of on a partitioned table
//
//---------------------------------------------------------------------------
List *
CTranslatorRelcacheToDXL::PlIndexInfoPartTable
	(
	Relation rel
	)
{
	List *plgidxinfo = NIL;
	
	LogicalIndexes *plgidx = gpdb::Plgidx(rel->rd_id);

	if (NULL == plgidx)
	{
		return NIL;
	}
	GPOS_ASSERT(NULL != plgidx);
	GPOS_ASSERT(0 <= plgidx->numLogicalIndexes);
	
	const ULONG ulIndexes = (ULONG) plgidx->numLogicalIndexes;
	for (ULONG ul = 0; ul < ulIndexes; ul++)
	{
		LogicalIndexInfo *pidxinfo = (plgidx->logicalIndexInfo)[ul];
		plgidxinfo = gpdb::PlAppendElement(plgidxinfo, pidxinfo);
	}
	
	gpdb::GPDBFree(plgidx);
	
	return plgidxinfo;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpmdidTriggers
//
//	@doc:
//		Return the triggers defined on the given relation
//
//---------------------------------------------------------------------------
DrgPmdid *
CTranslatorRelcacheToDXL::PdrgpmdidTriggers
	(
	IMemoryPool *pmp,
	Relation rel
	)
{
	GPOS_ASSERT(NULL != rel);
	if (rel->rd_rel->relhastriggers && NULL == rel->trigdesc)
	{
		gpdb::BuildRelationTriggers(rel);
		if (NULL == rel->trigdesc)
		{
			rel->rd_rel->relhastriggers = false;
		}
	}

	DrgPmdid *pdrgpmdidTriggers = GPOS_NEW(pmp) DrgPmdid(pmp);
	if (rel->rd_rel->relhastriggers)
	{
		const ULONG ulTriggers = rel->trigdesc->numtriggers;

		for (ULONG ul = 0; ul < ulTriggers; ul++)
		{
			Trigger trigger = rel->trigdesc->triggers[ul];
			OID oidTrigger = trigger.tgoid;
			CMDIdGPDB *pmdidTrigger = GPOS_NEW(pmp) CMDIdGPDB(oidTrigger);
			pdrgpmdidTriggers->Append(pmdidTrigger);
		}
	}

	return pdrgpmdidTriggers;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpmdidCheckConstraints
//
//	@doc:
//		Return the check constraints defined on the relation with the given oid
//
//---------------------------------------------------------------------------
DrgPmdid *
CTranslatorRelcacheToDXL::PdrgpmdidCheckConstraints
	(
	IMemoryPool *pmp,
	OID oid
	)
{
	DrgPmdid *pdrgpmdidCheckConstraints = GPOS_NEW(pmp) DrgPmdid(pmp);
	List *plOidCheckConstraints = gpdb::PlCheckConstraint(oid);

	ListCell *plcOid = NULL;
	ForEach (plcOid, plOidCheckConstraints)
	{
		OID oidCheckConstraint = lfirst_oid(plcOid);
		GPOS_ASSERT(0 != oidCheckConstraint);
		CMDIdGPDB *pmdidCheckConstraint = GPOS_NEW(pmp) CMDIdGPDB(oidCheckConstraint);
		pdrgpmdidCheckConstraints->Append(pmdidCheckConstraint);
	}

	return pdrgpmdidCheckConstraints;
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
CTranslatorRelcacheToDXL::CheckUnsupportedRelation
	(
	OID oidRel
	)
{
	if (gpdb::FRelPartIsInterior(oidRel))
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Query on intermediate partition"));
	}

	List *plPartKeys = gpdb::PlPartitionAttrs(oidRel);
	ULONG ulLevels = gpdb::UlListLength(plPartKeys);

	if (0 == ulLevels && gpdb::FHasSubclassSlow(oidRel))
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Inherited tables"));
	}

	if (1 < ulLevels)
	{
		if (!optimizer_multilevel_partitioning)
		{
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Multi-level partitioned tables"));
		}

		if (!gpdb::FMultilevelPartitionUniform(oidRel))
		{
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Multi-level partitioned tables with non-uniform partitioning structure"));
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdrel
//
//	@doc:
//		Retrieve a relation from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDRelation *
CTranslatorRelcacheToDXL::Pmdrel
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
{
	OID oid = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();
	GPOS_ASSERT(InvalidOid != oid);

	CheckUnsupportedRelation(oid);

	Relation rel = gpdb::RelGetRelation(oid);

	if (NULL == rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	CMDName *pmdname = NULL;
	IMDRelation::Erelstoragetype erelstorage = IMDRelation::ErelstorageSentinel;
	DrgPmdcol *pdrgpmdcol = NULL;
	IMDRelation::Ereldistrpolicy ereldistribution = IMDRelation::EreldistrSentinel;
	DrgPul *pdrpulDistrCols = NULL;
	DrgPmdIndexInfo *pdrgpmdIndexInfo = NULL;
	DrgPmdid *pdrgpmdidTriggers = NULL;
	DrgPul *pdrgpulPartKeys = NULL;
	DrgPsz *pdrgpszPartTypes = NULL;
	ULONG ulLeafPartitions = 0;
	BOOL fConvertHashToRandom = false;
	DrgPdrgPul *pdrgpdrgpulKeys = NULL;
	DrgPmdid *pdrgpmdidCheckConstraints = NULL;
	BOOL fTemporary = false;
	BOOL fHasOids = false;
	BOOL fPartitioned = false;
	IMDRelation *pmdrel = NULL;


	GPOS_TRY
	{
		// get rel name
		pmdname = PmdnameRel(pmp, rel);

		// get storage type
		erelstorage = Erelstorage(rel->rd_rel->relstorage);

		// get relation columns
		pdrgpmdcol = Pdrgpmdcol(pmp, pmda, rel, erelstorage);
		const ULONG ulMaxCols = GPDXL_SYSTEM_COLUMNS + (ULONG) rel->rd_att->natts + 1;
		ULONG *pulAttnoMapping = PulAttnoMapping(pmp, pdrgpmdcol, ulMaxCols);

		// get distribution policy
		GpPolicy *pgppolicy = gpdb::Pdistrpolicy(rel);
		ereldistribution = Ereldistribution(pgppolicy);

		// get distribution columns
		if (IMDRelation::EreldistrHash == ereldistribution)
		{
			pdrpulDistrCols = PdrpulDistrCols(pmp, pgppolicy, pdrgpmdcol, ulMaxCols);
		}

		fConvertHashToRandom = gpdb::FChildPartDistributionMismatch(rel);

		// collect relation indexes
		pdrgpmdIndexInfo = PdrgpmdRelIndexInfo(pmp, rel);

		// collect relation triggers
		pdrgpmdidTriggers = PdrgpmdidTriggers(pmp, rel);

		// get partition keys
		if (IMDRelation::ErelstorageExternal != erelstorage)
		{
			GetPartKeysAndTypes(pmp, rel, oid, &pdrgpulPartKeys, &pdrgpszPartTypes);
		}
		fPartitioned = (NULL != pdrgpulPartKeys && 0 < pdrgpulPartKeys->UlLength());

		if (fPartitioned && IMDRelation::ErelstorageAppendOnlyParquet != erelstorage && IMDRelation::ErelstorageExternal != erelstorage)
		{
			// mark relation as Parquet if one of its children is parquet
			if (gpdb::FHasParquetChildren(oid))
			{
				erelstorage = IMDRelation::ErelstorageAppendOnlyParquet;
			}
		}

		// get number of leaf partitions
		if (gpdb::FRelPartIsRoot(oid))
		{
			ulLeafPartitions = gpdb::UlLeafPartitions(oid);
		}

		// get key sets
		BOOL fAddDefaultKeys = FHasSystemColumns(rel->rd_rel->relkind);
		pdrgpdrgpulKeys = PdrgpdrgpulKeys(pmp, oid, fAddDefaultKeys, fPartitioned, pulAttnoMapping);

		// collect all check constraints
		pdrgpmdidCheckConstraints = PdrgpmdidCheckConstraints(pmp, oid);

		fTemporary = rel->rd_istemp;
		fHasOids = rel->rd_rel->relhasoids;
	
		GPOS_DELETE_ARRAY(pulAttnoMapping);
		gpdb::CloseRelation(rel);
	}
	GPOS_CATCH_EX(ex)
	{
		gpdb::CloseRelation(rel);
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	GPOS_ASSERT(IMDRelation::ErelstorageSentinel != erelstorage);
	GPOS_ASSERT(IMDRelation::EreldistrSentinel != ereldistribution);

	pmdid->AddRef();

	if (IMDRelation::ErelstorageExternal == erelstorage)
	{
		ExtTableEntry *extentry = gpdb::Pexttable(oid);

		// get format error table id
		IMDId *pmdidFmtErrTbl = NULL;
		if (InvalidOid != extentry->fmterrtbl)
		{
			pmdidFmtErrTbl = GPOS_NEW(pmp) CMDIdGPDB(extentry->fmterrtbl);
		}

		pmdrel = GPOS_NEW(pmp) CMDRelationExternalGPDB
							(
							pmp,
							pmdid,
							pmdname,
							ereldistribution,
							pdrgpmdcol,
							pdrpulDistrCols,
							fConvertHashToRandom,
							pdrgpdrgpulKeys,
							pdrgpmdIndexInfo,
							pdrgpmdidTriggers,
							pdrgpmdidCheckConstraints,
							extentry->rejectlimit,
							('r' == extentry->rejectlimittype),
							pmdidFmtErrTbl
							);
	}
	else
	{
		CMDPartConstraintGPDB *pmdpartcnstr = NULL;

		// retrieve the part constraints if relation is partitioned
		if (fPartitioned)
			pmdpartcnstr = PmdpartcnstrRelation(pmp, pmda, oid, pdrgpmdcol, pdrgpmdIndexInfo->UlLength() > 0 /*fhasIndex*/);

		pmdrel = GPOS_NEW(pmp) CMDRelationGPDB
							(
							pmp,
							pmdid,
							pmdname,
							fTemporary,
							erelstorage,
							ereldistribution,
							pdrgpmdcol,
							pdrpulDistrCols,
							pdrgpulPartKeys,
							pdrgpszPartTypes,
							ulLeafPartitions,
							fConvertHashToRandom,
							pdrgpdrgpulKeys,
							pdrgpmdIndexInfo,
							pdrgpmdidTriggers,
							pdrgpmdidCheckConstraints,
							pmdpartcnstr,
							fHasOids
							);
	}

	return pmdrel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pdrgpmdcol
//
//	@doc:
//		Get relation columns
//
//---------------------------------------------------------------------------
DrgPmdcol *
CTranslatorRelcacheToDXL::Pdrgpmdcol
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	Relation rel,
	IMDRelation::Erelstoragetype erelstorage
	)
{
	DrgPmdcol *pdrgpmdcol = GPOS_NEW(pmp) DrgPmdcol(pmp);

	for (ULONG ul = 0;  ul < (ULONG) rel->rd_att->natts; ul++)
	{
		Form_pg_attribute att = rel->rd_att->attrs[ul];
		CMDName *pmdnameCol = CDXLUtils::PmdnameFromSz(pmp, NameStr(att->attname));
	
		// translate the default column value
		CDXLNode *pdxlnDefault = NULL;
		
		if (!att->attisdropped)
		{
			pdxlnDefault = PdxlnDefaultColumnValue(pmp, pmda, rel->rd_att, att->attnum);
		}

		ULONG ulColLen = ULONG_MAX;
		CMDIdGPDB *pmdidCol = GPOS_NEW(pmp) CMDIdGPDB(att->atttypid);
		HeapTuple heaptupleStats = gpdb::HtAttrStats(rel->rd_id, ul+1);

		// Column width priority:
		// 1. If there is average width kept in the stats for that column, pick that value.
		// 2. If not, if it is a fixed length text type, pick the size of it. E.g if it is
		//    varchar(10), assign 10 as the column length.
		// 3. Else if it not dropped and a fixed length type such as int4, assign the fixed
		//    length.
		// 4. Otherwise, assign it to default column width which is 8.
		if(HeapTupleIsValid(heaptupleStats))
		{
			Form_pg_statistic fpsStats = (Form_pg_statistic) GETSTRUCT(heaptupleStats);

			// column width
			ulColLen = fpsStats->stawidth;
			gpdb::FreeHeapTuple(heaptupleStats);
		}
		else if ((pmdidCol->FEquals(&CMDIdGPDB::m_mdidBPChar) || pmdidCol->FEquals(&CMDIdGPDB::m_mdidVarChar)) && (VARHDRSZ < att->atttypmod))
		{
			ulColLen = (ULONG) att->atttypmod - VARHDRSZ;
		}
		else
		{
			DOUBLE dWidth = CStatistics::DDefaultColumnWidth.DVal();
			ulColLen = (ULONG) dWidth;

			if (!att->attisdropped)
			{
				IMDType *pmdtype = CTranslatorRelcacheToDXL::Pmdtype(pmp, pmdidCol);
				if(pmdtype->FFixedLength())
				{
					ulColLen = pmdtype->UlLength();
				}
				pmdtype->Release();
			}
		}

		CMDColumn *pmdcol = GPOS_NEW(pmp) CMDColumn
										(
										pmdnameCol,
										att->attnum,
										pmdidCol,
										!att->attnotnull,
										att->attisdropped,
										pdxlnDefault /* default value */,
										ulColLen
										);

		pdrgpmdcol->Append(pmdcol);
	}

	// add system columns
	if (FHasSystemColumns(rel->rd_rel->relkind))
	{
		BOOL fAOTable = IMDRelation::ErelstorageAppendOnlyRows == erelstorage ||
				IMDRelation::ErelstorageAppendOnlyCols == erelstorage;
		AddSystemColumns(pmp, pdrgpmdcol, rel, fAOTable);
	}

	return pdrgpmdcol;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdxlnDefaultColumnValue
//
//	@doc:
//		Return the dxl representation of column's default value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorRelcacheToDXL::PdxlnDefaultColumnValue
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	TupleDesc rd_att,
	AttrNumber attno
	)
{
	GPOS_ASSERT(attno > 0);

	Node *pnode = NULL;

	// Scan to see if relation has a default for this column
	if (NULL != rd_att->constr && 0 < rd_att->constr->num_defval)
	{
		AttrDefault *defval = rd_att->constr->defval;
		INT	iNumDef = rd_att->constr->num_defval;

		GPOS_ASSERT(NULL != defval);
		for (ULONG ulCounter = 0; ulCounter < (ULONG) iNumDef; ulCounter++)
		{
			if (attno == defval[ulCounter].adnum)
			{
				// found it, convert string representation to node tree.
				pnode = gpdb::Pnode(defval[ulCounter].adbin);
				break;
			}
		}
	}

	if (NULL == pnode)
	{
		// get the default value for the type
		Form_pg_attribute att_tup = rd_att->attrs[attno - 1];
		Oid	oidAtttype = att_tup->atttypid;
		pnode = gpdb::PnodeTypeDefault(oidAtttype);
	}

	if (NULL == pnode)
	{
		return NULL;
	}

	// translate the default value expression
	CTranslatorScalarToDXL sctranslator
							(
							pmp,
							pmda,
							NULL, /* pulidgtorCol */
							NULL, /* pulidgtorCTE */
							0, /* ulQueryLevel */
							true, /* m_fQuery */
							NULL, /* phmulCTEEntries */
							NULL /* pdrgpdxlnCTE */
							);

	return sctranslator.PdxlnScOpFromExpr
							(
							(Expr *) pnode,
							NULL /* pmapvarcolid --- subquery or external variable are not supported in default expression */
							);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Ereldistribution
//
//	@doc:
//		Return the distribution policy of the relation
//
//---------------------------------------------------------------------------
IMDRelation::Ereldistrpolicy
CTranslatorRelcacheToDXL::Ereldistribution
	(
	GpPolicy *pgppolicy
	)
{
	if (NULL == pgppolicy)
	{
		return IMDRelation::EreldistrMasterOnly;
	}

	if (POLICYTYPE_PARTITIONED == pgppolicy->ptype)
	{
		if (0 == pgppolicy->nattrs)
		{
			return IMDRelation::EreldistrRandom;
		}

		return IMDRelation::EreldistrHash;
	}

	if (POLICYTYPE_ENTRY == pgppolicy->ptype)
	{
		return IMDRelation::EreldistrMasterOnly;
	}

	GPOS_ASSERT(!"Unrecognized distribution policy");
	return IMDRelation::EreldistrSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrpulDistrCols
//
//	@doc:
//		Get distribution columns
//
//---------------------------------------------------------------------------
DrgPul *
CTranslatorRelcacheToDXL::PdrpulDistrCols
	(
	IMemoryPool *pmp,
	GpPolicy *pgppolicy,
	DrgPmdcol *pdrgpmdcol,
	ULONG ulSize
	)
{
	ULONG *pul = GPOS_NEW_ARRAY(pmp , ULONG, ulSize);

	for (ULONG ul = 0;  ul < pdrgpmdcol->UlLength(); ul++)
	{
		const IMDColumn *pmdcol = (*pdrgpmdcol)[ul];
		INT iAttno = pmdcol->IAttno();

		ULONG ulIndex = (ULONG) (GPDXL_SYSTEM_COLUMNS + iAttno);
		pul[ulIndex] = ul;
	}

	DrgPul *pdrpulDistrCols = GPOS_NEW(pmp) DrgPul(pmp);

	for (ULONG ul = 0; ul < (ULONG) pgppolicy->nattrs; ul++)
	{
		AttrNumber attno = pgppolicy->attrs[ul];
		pdrpulDistrCols->Append(GPOS_NEW(pmp) ULONG(UlPosition(attno, pul)));
	}

	GPOS_DELETE_ARRAY(pul);
	return pdrpulDistrCols;
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
CTranslatorRelcacheToDXL::AddSystemColumns
	(
	IMemoryPool *pmp,
	DrgPmdcol *pdrgpmdcol,
	Relation rel,
	BOOL fAOTable
	)
{
	BOOL fHasOid = rel->rd_att->tdhasoid;
	fAOTable = fAOTable || gpdb::FAppendOnlyPartitionTable(rel->rd_id);

	for (INT i= SelfItemPointerAttributeNumber; i > FirstLowInvalidHeapAttributeNumber; i--)
	{
		AttrNumber attno = AttrNumber(i);
		GPOS_ASSERT(0 != attno);

		if (ObjectIdAttributeNumber == i && !fHasOid)
		{
			continue;
		}

		if (FTransactionVisibilityAttribute(i) && fAOTable)
		{
			// skip transaction attrbutes like xmin, xmax, cmin, cmax for AO tables
			continue;
		}

		// get system name for that attribute
		const CWStringConst *pstrSysColName = CTranslatorUtils::PstrSystemColName(attno);
		GPOS_ASSERT(NULL != pstrSysColName);

		// copy string into column name
		CMDName *pmdnameCol = GPOS_NEW(pmp) CMDName(pmp, pstrSysColName);

		CMDColumn *pmdcol = GPOS_NEW(pmp) CMDColumn
										(
										pmdnameCol, 
										attno, 
										CTranslatorUtils::PmdidSystemColType(pmp, attno), 
										false,	// fNullable
										false,	// fDropped
										NULL,	// default value
										CTranslatorUtils::UlSystemColLength(attno)
										);

		pdrgpmdcol->Append(pmdcol);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::FTransactionVisibilityAttribute
//
//	@doc:
//		Check if attribute number is one of the system attributes related to 
//		transaction visibility such as xmin, xmax, cmin, cmax
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::FTransactionVisibilityAttribute
	(
	INT iAttNo
	)
{
	return iAttNo == MinTransactionIdAttributeNumber || iAttNo == MaxTransactionIdAttributeNumber || 
			iAttNo == MinCommandIdAttributeNumber || iAttNo == MaxCommandIdAttributeNumber;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdindex
//
//	@doc:
//		Retrieve an index from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDIndex *
CTranslatorRelcacheToDXL::Pmdindex
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdidIndex
	)
{
	OID oidIndex = CMDIdGPDB::PmdidConvert(pmdidIndex)->OidObjectId();
	GPOS_ASSERT(0 != oidIndex);
	Relation relIndex = gpdb::RelGetRelation(oidIndex);

	if (NULL == relIndex)
	{
		 GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdidIndex->Wsz());
	}

	const IMDRelation *pmdrel = NULL;
	Form_pg_index pgIndex = NULL;
	CMDName *pmdname = NULL;
	IMDIndex::EmdindexType emdindt = IMDIndex::EmdindSentinel;
	IMDId *pmdidItemType = NULL;

	GPOS_TRY
	{
		if (!FIndexSupported(relIndex))
		{
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Index type"));
		}

		pgIndex = relIndex->rd_index;
		GPOS_ASSERT (NULL != pgIndex);

		OID oidRel = pgIndex->indrelid;

		if (gpdb::FLeafPartition(oidRel))
		{
			oidRel = gpdb::OidRootPartition(oidRel);
		}

		CMDIdGPDB *pmdidRel = GPOS_NEW(pmp) CMDIdGPDB(oidRel);

		pmdrel = pmda->Pmdrel(pmdidRel);
	
		if (pmdrel->FPartitioned())
		{
			LogicalIndexes *plgidx = gpdb::Plgidx(oidRel);
			GPOS_ASSERT(NULL != plgidx);

			IMDIndex *pmdindex = PmdindexPartTable(pmp, pmda, pmdidIndex, pmdrel, plgidx);

			// cleanup
			gpdb::GPDBFree(plgidx);

			if (NULL != pmdindex)
			{
				pmdidRel->Release();
				gpdb::CloseRelation(relIndex);
				return pmdindex;
			}
		}
	
		emdindt = IMDIndex::EmdindBtree;
		IMDRelation::Erelstoragetype erelstorage = pmdrel->Erelstorage();
		if (BITMAP_AM_OID == relIndex->rd_rel->relam || IMDRelation::ErelstorageAppendOnlyRows == erelstorage || IMDRelation::ErelstorageAppendOnlyCols == erelstorage)
		{
			emdindt = IMDIndex::EmdindBitmap;
			pmdidItemType = GPOS_NEW(pmp) CMDIdGPDB(GPDB_ANY);
		}
		
		// get the index name
		CHAR *szIndexName = NameStr(relIndex->rd_rel->relname);
		CWStringDynamic *pstrName = CDXLUtils::PstrFromSz(pmp, szIndexName);
		pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrName);
		GPOS_DELETE(pstrName);
		pmdidRel->Release();
		gpdb::CloseRelation(relIndex);
	}
	GPOS_CATCH_EX(ex)
	{
		gpdb::CloseRelation(relIndex);
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	
	Relation relTable = gpdb::RelGetRelation(CMDIdGPDB::PmdidConvert(pmdrel->Pmdid())->OidObjectId());
	ULONG ulRgSize = GPDXL_SYSTEM_COLUMNS + (ULONG) relTable->rd_att->natts + 1;
	gpdb::CloseRelation(relTable); // close relation as early as possible

	ULONG *pul = PulAttnoPositionMap(pmp, pmdrel, ulRgSize);

	DrgPul *pdrgpulIncludeCols = PdrgpulIndexIncludedColumns(pmp, pmdrel);

	// extract the position of the key columns
	DrgPul *pdrgpulKeyCols = GPOS_NEW(pmp) DrgPul(pmp);
	ULONG ulKeys = pgIndex->indnatts;
	for (ULONG ul = 0; ul < ulKeys; ul++)
	{
		INT iAttno = pgIndex->indkey.values[ul];
		GPOS_ASSERT(0 != iAttno && "Index expressions not supported");

		pdrgpulKeyCols->Append(GPOS_NEW(pmp) ULONG(UlPosition(iAttno, pul)));
	}

	pmdidIndex->AddRef();	
	DrgPmdid *pdrgpmdidOpFamilies = PdrgpmdidIndexOpFamilies(pmp, pmdidIndex);
	
	CMDIndexGPDB *pmdindex = GPOS_NEW(pmp) CMDIndexGPDB
										(
										pmp,
										pmdidIndex,
										pmdname,
										pgIndex->indisclustered,
										emdindt,
										pmdidItemType,
										pdrgpulKeyCols,
										pdrgpulIncludeCols,
										pdrgpmdidOpFamilies,
										NULL // pmdpartcnstr
										);

	GPOS_DELETE_ARRAY(pul);

	return pmdindex;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdindexPartTable
//
//	@doc:
//		Retrieve an index over a partitioned table from the relcache given its 
//		mdid
//
//---------------------------------------------------------------------------
IMDIndex *
CTranslatorRelcacheToDXL::PmdindexPartTable
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdidIndex,
	const IMDRelation *pmdrel,
	LogicalIndexes *plind
	)
{
	GPOS_ASSERT(NULL != plind);
	GPOS_ASSERT(0 < plind->numLogicalIndexes);
	
	OID oid = CMDIdGPDB::PmdidConvert(pmdidIndex)->OidObjectId();
	
	LogicalIndexInfo *pidxinfo = PidxinfoLookup(plind, oid);
	if (NULL == pidxinfo)
	{
		 return NULL;
	}
	
	return PmdindexPartTable(pmp, pmda, pidxinfo, pmdidIndex, pmdrel);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PidxinfoLookup
//
//	@doc:
//		Lookup an index given its id from the logical indexes structure
//
//---------------------------------------------------------------------------
LogicalIndexInfo *
CTranslatorRelcacheToDXL::PidxinfoLookup
	(
	LogicalIndexes *plind, 
	OID oid
	)
{
	GPOS_ASSERT(NULL != plind && 0 <= plind->numLogicalIndexes);
	
	const ULONG ulIndexes = plind->numLogicalIndexes;
	
	for (ULONG ul = 0; ul < ulIndexes; ul++)
	{
		LogicalIndexInfo *pidxinfo = (plind->logicalIndexInfo)[ul];
		
		if (oid == pidxinfo->logicalIndexOid)
		{
			return pidxinfo;
		}
	}
	
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdindexPartTable
//
//	@doc:
//		Construct an MD cache index object given its logical index representation
//
//---------------------------------------------------------------------------
IMDIndex *
CTranslatorRelcacheToDXL::PmdindexPartTable
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	LogicalIndexInfo *pidxinfo,
	IMDId *pmdidIndex,
	const IMDRelation *pmdrel
	)
{
	OID oidIndex = pidxinfo->logicalIndexOid;
	
	Relation relIndex = gpdb::RelGetRelation(oidIndex);

	if (NULL == relIndex)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdidIndex->Wsz());
	}

	if (!FIndexSupported(relIndex))
	{
		gpdb::CloseRelation(relIndex);
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Index type"));
	}
	
	// get the index name
	GPOS_ASSERT(NULL != relIndex->rd_index);
	Form_pg_index pgIndex = relIndex->rd_index;
	
	CHAR *szIndexName = NameStr(relIndex->rd_rel->relname);
	CMDName *pmdname = CDXLUtils::PmdnameFromSz(pmp, szIndexName);
	gpdb::CloseRelation(relIndex);

	OID oidRel = CMDIdGPDB::PmdidConvert(pmdrel->Pmdid())->OidObjectId();
	Relation relTable = gpdb::RelGetRelation(oidRel);
	ULONG ulRgSize = GPDXL_SYSTEM_COLUMNS + (ULONG) relTable->rd_att->natts + 1;
	gpdb::CloseRelation(relTable);

	ULONG *pulAttrMap = PulAttnoPositionMap(pmp, pmdrel, ulRgSize);

	DrgPul *pdrgpulIncludeCols = PdrgpulIndexIncludedColumns(pmp, pmdrel);

	// extract the position of the key columns
	DrgPul *pdrgpulKeyCols = GPOS_NEW(pmp) DrgPul(pmp);
	const ULONG ulKeys = pidxinfo->nColumns;
	for (ULONG ul = 0; ul < ulKeys; ul++)
	{
		INT iAttno = pidxinfo->indexKeys[ul];
		GPOS_ASSERT(0 != iAttno && "Index expressions not supported");

		pdrgpulKeyCols->Append(GPOS_NEW(pmp) ULONG(UlPosition(iAttno, pulAttrMap)));
	}
	
	/*
	 * If an index exists only on a leaf part, pnodePartCnstr refers to the expression
	 * identifying the path to reach the partition holding the index. For indexes
	 * available on all parts it is set to NULL.
	 */
	Node *pnodePartCnstr = pidxinfo->partCons;
	
	/*
	 * If an index exists all on the parts including default, the logical index
	 * info created marks defaultLevels as NIL. However, if an index exists only on
	 * leaf parts plDefaultLevel contains the default part level which come across while
	 * reaching to the leaf part from root.
	 */
	List *plDefaultLevels = pidxinfo->defaultLevels;
	
	// get number of partitioning levels
	List *plPartKeys = gpdb::PlPartitionAttrs(oidRel);
	const ULONG ulLevels = gpdb::UlListLength(plPartKeys);
	gpdb::FreeList(plPartKeys);

	/* get relation constraints
	 * plDefaultLevelsRel indicates the levels on which default partitions exists
	 * for the partitioned table
	 */
	List *plDefaultLevelsRel = NIL;
	Node *pnodePartCnstrRel = gpdb::PnodePartConstraintRel(oidRel, &plDefaultLevelsRel);

	BOOL fUnbounded = (NULL == pnodePartCnstr) && (NIL == plDefaultLevels);
	for (ULONG ul = 0; ul < ulLevels; ul++)
	{
		fUnbounded = fUnbounded && FDefaultPartition(plDefaultLevelsRel, ul);
	}

	/*
	 * If pnodePartCnstr is NULL and plDefaultLevels is NIL,
	 * it indicates that the index is available on all the parts including
	 * default part. So, we can say that levels on which default partitions
	 * exists for the relation applies to the index as well and the relative
	 * scan will not be partial.
	 */
	List *plDefaultLevelsDerived = NIL;
	if (NULL == pnodePartCnstr && NIL == plDefaultLevels)
		plDefaultLevelsDerived = plDefaultLevelsRel;
	else
		plDefaultLevelsDerived = plDefaultLevels;
	
	DrgPul *pdrgpulDefaultLevels = GPOS_NEW(pmp) DrgPul(pmp);
	for (ULONG ul = 0; ul < ulLevels; ul++)
	{
		if (fUnbounded || FDefaultPartition(plDefaultLevelsDerived, ul))
		{
			pdrgpulDefaultLevels->Append(GPOS_NEW(pmp) ULONG(ul));
		}
	}
	gpdb::FreeList(plDefaultLevelsDerived);

	if (NULL == pnodePartCnstr)
	{
		if (NIL == plDefaultLevels)
		{
			// NULL part constraints means all non-default partitions -> get constraint from the part table
			pnodePartCnstr = pnodePartCnstrRel;
		}
		else
		{
			pnodePartCnstr = gpdb::PnodeMakeBoolConst(false /*value*/, false /*isull*/);
		}
	}
		
	CMDPartConstraintGPDB *pmdpartcnstr = PmdpartcnstrIndex(pmp, pmda, pmdrel, pnodePartCnstr, pdrgpulDefaultLevels, fUnbounded);

	pdrgpulDefaultLevels->Release();
	pmdidIndex->AddRef();
	
	GPOS_ASSERT(INDTYPE_BITMAP == pidxinfo->indType || INDTYPE_BTREE == pidxinfo->indType);
	
	IMDIndex::EmdindexType emdindt = IMDIndex::EmdindBtree;
	IMDId *pmdidItemType = NULL;
	if (INDTYPE_BITMAP == pidxinfo->indType)
	{
		emdindt = IMDIndex::EmdindBitmap;
		pmdidItemType = GPOS_NEW(pmp) CMDIdGPDB(GPDB_ANY);
	}
	
	DrgPmdid *pdrgpmdidOpFamilies = PdrgpmdidIndexOpFamilies(pmp, pmdidIndex);
	
	CMDIndexGPDB *pmdindex = GPOS_NEW(pmp) CMDIndexGPDB
										(
										pmp,
										pmdidIndex,
										pmdname,
										pgIndex->indisclustered,
										emdindt,
										pmdidItemType,
										pdrgpulKeyCols,
										pdrgpulIncludeCols,
										pdrgpmdidOpFamilies,
										pmdpartcnstr
										);
	
	GPOS_DELETE_ARRAY(pulAttrMap);
	
	return pmdindex;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::FDefaultPartition
//
//	@doc:
//		Check whether the default partition at level one is included
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::FDefaultPartition
	(
	List *plDefaultLevels,
	ULONG ulLevel
	)
{
	if (NIL == plDefaultLevels)
	{
		return false;
	}
	
	ListCell *plc = NULL;
	ForEach (plc, plDefaultLevels)
	{
		ULONG ulDefaultLevel = (ULONG) lfirst_int(plc);
		if (ulLevel == ulDefaultLevel)
		{
			return true;
		}
	}
	
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpulIndexIncludedColumns
//
//	@doc:
//		Compute the included columns in an index
//
//---------------------------------------------------------------------------
DrgPul *
CTranslatorRelcacheToDXL::PdrgpulIndexIncludedColumns
	(
	IMemoryPool *pmp,
	const IMDRelation *pmdrel
	)
{
	// TODO: 3/19/2012; currently we assume that all the columns
	// in the table are available from the index.

	DrgPul *pdrgpulIncludeCols = GPOS_NEW(pmp) DrgPul(pmp);
	const ULONG ulIncludedCols = pmdrel->UlColumns();
	for (ULONG ul = 0;  ul < ulIncludedCols; ul++)
	{
		if (!pmdrel->Pmdcol(ul)->FDropped())
		{
			pdrgpulIncludeCols->Append(GPOS_NEW(pmp) ULONG(ul));
		}
	}
	
	return pdrgpulIncludeCols;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::UlPosition
//
//	@doc:
//		Return the position of a given attribute
//
//---------------------------------------------------------------------------
ULONG
CTranslatorRelcacheToDXL::UlPosition
	(
	INT iAttno,
	ULONG *pul
	)
{
	ULONG ulIndex = (ULONG) (GPDXL_SYSTEM_COLUMNS + iAttno);
	ULONG ulPos = pul[ulIndex];
	GPOS_ASSERT(ULONG_MAX != ulPos);

	return ulPos;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PulAttnoPositionMap
//
//	@doc:
//		Populate the attribute to position mapping
//
//---------------------------------------------------------------------------
ULONG *
CTranslatorRelcacheToDXL::PulAttnoPositionMap
	(
	IMemoryPool *pmp,
	const IMDRelation *pmdrel,
	ULONG ulSize
	)
{
	GPOS_ASSERT(NULL != pmdrel);
	const ULONG ulIncludedCols = pmdrel->UlColumns();

	GPOS_ASSERT(ulIncludedCols <= ulSize);
	ULONG *pul = GPOS_NEW_ARRAY(pmp , ULONG, ulSize);

	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		pul[ul] = ULONG_MAX;
	}

	for (ULONG ul = 0;  ul < ulIncludedCols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);

		INT iAttno = pmdcol->IAttno();

		ULONG ulIndex = (ULONG) (GPDXL_SYSTEM_COLUMNS + iAttno);
		GPOS_ASSERT(ulSize > ulIndex);
		pul[ulIndex] = ul;
	}

	return pul;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdtype
//
//	@doc:
//		Retrieve a type from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
IMDType *
CTranslatorRelcacheToDXL::Pmdtype
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidType = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();
	GPOS_ASSERT(InvalidOid != oidType);
	
	// check for supported base types
	switch (oidType)
	{
		case GPDB_INT2_OID:
			return GPOS_NEW(pmp) CMDTypeInt2GPDB(pmp);

		case GPDB_INT4_OID:
			return GPOS_NEW(pmp) CMDTypeInt4GPDB(pmp);

		case GPDB_INT8_OID:
			return GPOS_NEW(pmp) CMDTypeInt8GPDB(pmp);

		case GPDB_BOOL:
			return GPOS_NEW(pmp) CMDTypeBoolGPDB(pmp);

		case GPDB_OID_OID:
			return GPOS_NEW(pmp) CMDTypeOidGPDB(pmp);
	}

	// continue to construct a generic type
	INT iFlags = TYPECACHE_EQ_OPR | TYPECACHE_LT_OPR | TYPECACHE_GT_OPR |
				 TYPECACHE_CMP_PROC | TYPECACHE_EQ_OPR_FINFO | TYPECACHE_CMP_PROC_FINFO | TYPECACHE_TUPDESC;

	TypeCacheEntry *ptce = gpdb::PtceLookup(oidType, iFlags);

	// get type name
	CMDName *pmdname = PmdnameType(pmp, pmdid);

	BOOL fFixedLength = false;
	ULONG ulLength = 0;

	if (0 < ptce->typlen)
	{
		fFixedLength = true;
		ulLength = ptce->typlen;
	}

	BOOL fByValue = ptce->typbyval;

	// collect ids of different comparison operators for types
	CMDIdGPDB *pmdidOpEq = GPOS_NEW(pmp) CMDIdGPDB(ptce->eq_opr);
	CMDIdGPDB *pmdidOpNEq = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidInverseOp(ptce->eq_opr));
	CMDIdGPDB *pmdidOpLT = GPOS_NEW(pmp) CMDIdGPDB(ptce->lt_opr);
	CMDIdGPDB *pmdidOpLEq = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidInverseOp(ptce->gt_opr));
	CMDIdGPDB *pmdidOpGT = GPOS_NEW(pmp) CMDIdGPDB(ptce->gt_opr);
	CMDIdGPDB *pmdidOpGEq = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidInverseOp(ptce->lt_opr));
	CMDIdGPDB *pmdidOpComp = GPOS_NEW(pmp) CMDIdGPDB(ptce->cmp_proc);
	BOOL fHashable = gpdb::FOpHashJoinable(ptce->eq_opr);
	BOOL fComposite = gpdb::FCompositeType(oidType);

	// get standard aggregates
	CMDIdGPDB *pmdidMin = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidAggregate("min", oidType));
	CMDIdGPDB *pmdidMax = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidAggregate("max", oidType));
	CMDIdGPDB *pmdidAvg = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidAggregate("avg", oidType));
	CMDIdGPDB *pmdidSum = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidAggregate("sum", oidType));
	
	// count aggregate is the same for all types
	CMDIdGPDB *pmdidCount = GPOS_NEW(pmp) CMDIdGPDB(COUNT_ANY_OID);
	
	// check if type is composite
	CMDIdGPDB *pmdidTypeRelid = NULL;
	if (fComposite)
	{
		pmdidTypeRelid = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidTypeRelid(oidType));
	}

	// get array type mdid
	CMDIdGPDB *pmdidTypeArray = GPOS_NEW(pmp) CMDIdGPDB(gpdb::OidArrayType(oidType));

	BOOL fRedistributable = gpdb::FGreenplumDbHashable(oidType);

	pmdid->AddRef();

	return GPOS_NEW(pmp) CMDTypeGenericGPDB
						 (
						 pmp,
						 pmdid,
						 pmdname,
						 fRedistributable,
						 fFixedLength,
						 ulLength,
						 fByValue,
						 pmdidOpEq,
						 pmdidOpNEq,
						 pmdidOpLT,
						 pmdidOpLEq,
						 pmdidOpGT,
						 pmdidOpGEq,
						 pmdidOpComp,
						 pmdidMin,
						 pmdidMax,
						 pmdidAvg,
						 pmdidSum,
						 pmdidCount,
						 fHashable,
						 fComposite,
						 pmdidTypeRelid,
						 pmdidTypeArray,
						 ptce->typlen
						 );
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdscop
//
//	@doc:
//		Retrieve a scalar operator from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDScalarOpGPDB *
CTranslatorRelcacheToDXL::Pmdscop
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidOp = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidOp);

	// get operator name
	CHAR *szName = gpdb::SzOpName(oidOp);

	if (NULL == szName)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	CMDName *pmdname = CDXLUtils::PmdnameFromSz(pmp, szName);
	
	OID oidLeft = InvalidOid;
	OID oidRight = InvalidOid;

	// get operator argument types
	gpdb::GetOpInputTypes(oidOp, &oidLeft, &oidRight);

	CMDIdGPDB *pmdidTypeLeft = NULL;
	CMDIdGPDB *pmdidTypeRight = NULL;

	if (InvalidOid != oidLeft)
	{
		pmdidTypeLeft = GPOS_NEW(pmp) CMDIdGPDB(oidLeft);
	}

	if (InvalidOid != oidRight)
	{
		pmdidTypeRight = GPOS_NEW(pmp) CMDIdGPDB(oidRight);
	}

	// get comparison type
	CmpType cmpt = (CmpType) gpdb::UlCmpt(oidOp, oidLeft, oidRight);
	IMDType::ECmpType ecmpt = Ecmpt(cmpt);
	
	// get func oid
	OID oidFunc = gpdb::OidOpFunc(oidOp);
	GPOS_ASSERT(InvalidOid != oidFunc);

	CMDIdGPDB *pmdidFunc = GPOS_NEW(pmp) CMDIdGPDB(oidFunc);

	// get result type
	OID oidResult = gpdb::OidFuncRetType(oidFunc);

	GPOS_ASSERT(InvalidOid != oidResult);

	CMDIdGPDB *pmdidTypeResult = GPOS_NEW(pmp) CMDIdGPDB(oidResult);

	// get commutator and inverse
	CMDIdGPDB *pmdidOpCommute = NULL;

	OID oidCommute = gpdb::OidCommutatorOp(oidOp);

	if(InvalidOid != oidCommute)
	{
		pmdidOpCommute = GPOS_NEW(pmp) CMDIdGPDB(oidCommute);
	}

	CMDIdGPDB *pmdidOpInverse = NULL;

	OID oidInverse = gpdb::OidInverseOp(oidOp);

	if(InvalidOid != oidInverse)
	{
		pmdidOpInverse = GPOS_NEW(pmp) CMDIdGPDB(oidInverse);
	}

	BOOL fReturnsNullOnNullInput = gpdb::FOpStrict(oidOp);

	pmdid->AddRef();
	CMDScalarOpGPDB *pmdscop = GPOS_NEW(pmp) CMDScalarOpGPDB
											(
											pmp,
											pmdid,
											pmdname,
											pmdidTypeLeft,
											pmdidTypeRight,
											pmdidTypeResult,
											pmdidFunc,
											pmdidOpCommute,
											pmdidOpInverse,
											ecmpt,
											fReturnsNullOnNullInput,
											PdrgpmdidScOpOpFamilies(pmp, pmdid)
											);
	return pmdscop;
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
CTranslatorRelcacheToDXL::LookupFuncProps
	(
	OID oidFunc,
	IMDFunction::EFuncStbl *pefs, // output: function stability
	IMDFunction::EFuncDataAcc *pefda, // output: function datya access
	BOOL *fStrict, // output: is function strict?
	BOOL *fReturnsSet // output: does function return set?
	)
{
	GPOS_ASSERT(NULL != pefs);
	GPOS_ASSERT(NULL != pefda);
	GPOS_ASSERT(NULL != fStrict);
	GPOS_ASSERT(NULL != fReturnsSet);

	CHAR cFuncStability = gpdb::CFuncStability(oidFunc);
	*pefs = EFuncStability(cFuncStability);

	CHAR cFuncDataAccess = gpdb::CFuncDataAccess(oidFunc);
	*pefda = EFuncDataAccess(cFuncDataAccess);

	CHAR cFuncExecLocation = gpdb::CFuncExecLocation(oidFunc);
	if (cFuncExecLocation != PROEXECLOCATION_ANY)
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("unsupported exec location"));

	*fReturnsSet = gpdb::FFuncRetset(oidFunc);
	*fStrict = gpdb::FFuncStrict(oidFunc);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdfunc
//
//	@doc:
//		Retrieve a function from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDFunctionGPDB *
CTranslatorRelcacheToDXL::Pmdfunc
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidFunc = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();


	// GPDB_84_MERGE_FIXME: The OIDS of a few built-in window
	// functions have been hard-coded in ORCA. But the OIDs
	// were changed when we merged the upstream window function
	// implementation, to match the upstream OIDs. Map the old
	// OIDs to the upstream ones.
	if (oidFunc == 7000)	// ROW_NUMBER()
		oidFunc = 3100;
	if (oidFunc == 7002)	// DENSE_RANK()
		oidFunc = 3102;
	if (oidFunc == 7003)	// PERCENT_RANK()
		oidFunc = 3103;
	if (oidFunc == 7004)	// CUME_DIST()
		oidFunc = 3104;
	if (oidFunc == 7005)	// NTILE(int4)
		oidFunc = 3105;

	GPOS_ASSERT(InvalidOid != oidFunc);

	// get func name
	CHAR *szName = gpdb::SzFuncName(oidFunc);

	if (NULL == szName)
	{

		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	CWStringDynamic *pstrFuncName = CDXLUtils::PstrFromSz(pmp, szName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrFuncName);

	// CMDName ctor created a copy of the string
	GPOS_DELETE(pstrFuncName);

	// get result type
	OID oidResult = gpdb::OidFuncRetType(oidFunc);

	GPOS_ASSERT(InvalidOid != oidResult);

	CMDIdGPDB *pmdidTypeResult = GPOS_NEW(pmp) CMDIdGPDB(oidResult);

	// get output argument types if any
	List *plOutArgTypes = gpdb::PlFuncOutputArgTypes(oidFunc);

	DrgPmdid *pdrgpmdidArgTypes = NULL;
	if (NULL != plOutArgTypes)
	{
		ListCell *plc = NULL;
		pdrgpmdidArgTypes = GPOS_NEW(pmp) DrgPmdid(pmp);

		ForEach (plc, plOutArgTypes)
		{
			OID oidArgType = lfirst_oid(plc);
			GPOS_ASSERT(InvalidOid != oidArgType);
			CMDIdGPDB *pmdidArgType = GPOS_NEW(pmp) CMDIdGPDB(oidArgType);
			pdrgpmdidArgTypes->Append(pmdidArgType);
		}

		gpdb::GPDBFree(plOutArgTypes);
	}

	IMDFunction::EFuncStbl efs = IMDFunction::EfsImmutable;
	IMDFunction::EFuncDataAcc efda = IMDFunction::EfdaNoSQL;
	BOOL fStrict = true;
	BOOL fReturnsSet = true;
	LookupFuncProps(oidFunc, &efs, &efda, &fStrict, &fReturnsSet);

	pmdid->AddRef();
	CMDFunctionGPDB *pmdfunc = GPOS_NEW(pmp) CMDFunctionGPDB
											(
											pmp,
											pmdid,
											pmdname,
											pmdidTypeResult,
											pdrgpmdidArgTypes,
											fReturnsSet,
											efs,
											efda,
											fStrict
											);

	return pmdfunc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdagg
//
//	@doc:
//		Retrieve an aggregate from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDAggregateGPDB *
CTranslatorRelcacheToDXL::Pmdagg
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidAgg = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidAgg);

	// get agg name
	CHAR *szName = gpdb::SzFuncName(oidAgg);

	if (NULL == szName)
	{

		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	CWStringDynamic *pstrAggName = CDXLUtils::PstrFromSz(pmp, szName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrAggName);

	// CMDName ctor created a copy of the string
	GPOS_DELETE(pstrAggName);

	// get result type
	OID oidResult = gpdb::OidFuncRetType(oidAgg);

	GPOS_ASSERT(InvalidOid != oidResult);

	CMDIdGPDB *pmdidTypeResult = GPOS_NEW(pmp) CMDIdGPDB(oidResult);
	IMDId *pmdidTypeIntermediate = PmdidAggIntermediateResultType(pmp, pmdid);

	pmdid->AddRef();
	
	BOOL fOrdered = gpdb::FOrderedAgg(oidAgg);
	
	// GPDB does not support splitting of ordered aggs and aggs without a
	// preliminary function
	BOOL fSplittable = !fOrdered && gpdb::FAggHasPrelimFunc(oidAgg);
	
	// cannot use hash agg for ordered aggs or aggs without a prelim func
	// due to the fact that hashAgg may spill
	BOOL fHashAggCapable = !fOrdered && gpdb::FAggHasPrelimFunc(oidAgg);

	CMDAggregateGPDB *pmdagg = GPOS_NEW(pmp) CMDAggregateGPDB
											(
											pmp,
											pmdid,
											pmdname,
											pmdidTypeResult,
											pmdidTypeIntermediate,
											fOrdered,
											fSplittable,
											fHashAggCapable
											);
	return pmdagg;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdtrigger
//
//	@doc:
//		Retrieve a trigger from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDTriggerGPDB *
CTranslatorRelcacheToDXL::Pmdtrigger
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidTrigger = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidTrigger);

	// get trigger name
	CHAR *szName = gpdb::SzTriggerName(oidTrigger);

	if (NULL == szName)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	CWStringDynamic *pstrTriggerName = CDXLUtils::PstrFromSz(pmp, szName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrTriggerName);
	GPOS_DELETE(pstrTriggerName);

	// get relation oid
	OID oidRel = gpdb::OidTriggerRelid(oidTrigger);
	GPOS_ASSERT(InvalidOid != oidRel);
	CMDIdGPDB *pmdidRel = GPOS_NEW(pmp) CMDIdGPDB(oidRel);

	// get function oid
	OID oidFunc = gpdb::OidTriggerFuncid(oidTrigger);
	GPOS_ASSERT(InvalidOid != oidFunc);
	CMDIdGPDB *pmdidFunc = GPOS_NEW(pmp) CMDIdGPDB(oidFunc);

	// get type
	INT iType = gpdb::ITriggerType(oidTrigger);

	// is trigger enabled
	BOOL fEnabled = gpdb::FTriggerEnabled(oidTrigger);

	pmdid->AddRef();
	CMDTriggerGPDB *pmdtrigger = GPOS_NEW(pmp) CMDTriggerGPDB
											(
											pmp,
											pmdid,
											pmdname,
											pmdidRel,
											pmdidFunc,
											iType,
											fEnabled
											);
	return pmdtrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pmdcheckconstraint
//
//	@doc:
//		Retrieve a check constraint from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDCheckConstraintGPDB *
CTranslatorRelcacheToDXL::Pmdcheckconstraint
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
{
	OID oidCheckConstraint = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();
	GPOS_ASSERT(InvalidOid != oidCheckConstraint);

	// get name of the check constraint
	CHAR *szName = gpdb::SzCheckConstraintName(oidCheckConstraint);
	if (NULL == szName)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}
	CWStringDynamic *pstrCheckConstraintName = CDXLUtils::PstrFromSz(pmp, szName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrCheckConstraintName);
	GPOS_DELETE(pstrCheckConstraintName);

	// get relation oid associated with the check constraint
	OID oidRel = gpdb::OidCheckConstraintRelid(oidCheckConstraint);
	GPOS_ASSERT(InvalidOid != oidRel);
	CMDIdGPDB *pmdidRel = GPOS_NEW(pmp) CMDIdGPDB(oidRel);

	// translate the check constraint expression
	Node *pnode = gpdb::PnodeCheckConstraint(oidCheckConstraint);
	GPOS_ASSERT(NULL != pnode);

	CTranslatorScalarToDXL sctranslator
							(
							pmp,
							pmda,
							NULL, /* pulidgtorCol */
							NULL, /* pulidgtorCTE */
							0, /* ulQueryLevel */
							true, /* m_fQuery */
							NULL, /* phmulCTEEntries */
							NULL /* pdrgpdxlnCTE */
							);

	// generate a mock mapping between var to column information
	CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);
	DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(pmp) DrgPdxlcd(pmp);
	const IMDRelation *pmdrel = pmda->Pmdrel(pmdidRel);
	const ULONG ulLen = pmdrel->UlColumns();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);
		CMDName *pmdnameCol = GPOS_NEW(pmp) CMDName(pmp, pmdcol->Mdname().Pstr());
		CMDIdGPDB *pmdidColType = CMDIdGPDB::PmdidConvert(pmdcol->PmdidType());
		pmdidColType->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *pdxlcd = GPOS_NEW(pmp) CDXLColDescr
										(
										pmp,
										pmdnameCol,
										ul + 1 /*ulColId*/,
										pmdcol->IAttno(),
										pmdidColType,
										false /* fColDropped */
										);
		pdrgpdxlcd->Append(pdxlcd);
	}
	pmapvarcolid->LoadColumns(0 /*ulQueryLevel */, 1 /* rteIndex */, pdrgpdxlcd);

	// translate the check constraint expression
	CDXLNode *pdxlnScalar = sctranslator.PdxlnScOpFromExpr((Expr *) pnode, pmapvarcolid);

	// cleanup
	pdrgpdxlcd->Release();
	GPOS_DELETE(pmapvarcolid);

	pmdid->AddRef();

	return GPOS_NEW(pmp) CMDCheckConstraintGPDB
						(
						pmp,
						pmdid,
						pmdname,
						pmdidRel,
						pdxlnScalar
						);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdnameType
//
//	@doc:
//		Retrieve a type's name from the relcache given its metadata id.
//
//---------------------------------------------------------------------------
CMDName *
CTranslatorRelcacheToDXL::PmdnameType
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidType = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidType);

	CHAR *szTypeName = gpdb::SzTypeName(oidType);
	GPOS_ASSERT(NULL != szTypeName);

	CWStringDynamic *pstrName = CDXLUtils::PstrFromSz(pmp, szTypeName);
	CMDName *pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrName);

	// cleanup
	GPOS_DELETE(pstrName);
	return pmdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::EFuncStability
//
//	@doc:
//		Get function stability property from the GPDB character representation
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncStbl
CTranslatorRelcacheToDXL::EFuncStability
	(
	CHAR c
	)
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
//		CTranslatorRelcacheToDXL::EFuncDataAccess
//
//	@doc:
//		Get function data access property from the GPDB character representation
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncDataAcc
CTranslatorRelcacheToDXL::EFuncDataAccess
	(
	CHAR c
	)
{
	CMDFunctionGPDB::EFuncDataAcc efda = CMDFunctionGPDB::EfdaSentinel;

	switch (c)
	{
		case 'n':
			efda = CMDFunctionGPDB::EfdaNoSQL;
			break;
		case 'c':
			efda = CMDFunctionGPDB::EfdaContainsSQL;
			break;
		case 'r':
			efda = CMDFunctionGPDB::EfdaReadsSQLData;
			break;
		case 'm':
			efda = CMDFunctionGPDB::EfdaModifiesSQLData;
			break;
		case 's':
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("unknown data access"));
		default:
			GPOS_ASSERT(!"Invalid data access property");
	}

	return efda;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdidAggIntermediateResultType
//
//	@doc:
//		Retrieve the type id of an aggregate's intermediate results
//
//---------------------------------------------------------------------------
IMDId *
CTranslatorRelcacheToDXL::PmdidAggIntermediateResultType
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	OID oidAgg = CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidAgg);

	OID oidTypeIntermediateResult = gpdb::OidAggIntermediateResultType(oidAgg);
	return GPOS_NEW(pmp) CMDIdGPDB(oidTypeIntermediateResult);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PimdobjRelStats
//
//	@doc:
//		Retrieve relation statistics from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::PimdobjRelStats
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	CMDIdRelStats *pmdidRelStats = CMDIdRelStats::PmdidConvert(pmdid);
	IMDId *pmdidRel = pmdidRelStats->PmdidRel();
	OID oidRelation = CMDIdGPDB::PmdidConvert(pmdidRel)->OidObjectId();

	Relation rel = gpdb::RelGetRelation(oidRelation);
	if (NULL == rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	double rows = 0.0;
	CMDName *pmdname = NULL;

	GPOS_TRY
	{
		// get rel name
		CHAR *szRelName = NameStr(rel->rd_rel->relname);
		CWStringDynamic *pstrRelName = CDXLUtils::PstrFromSz(pmp, szRelName);
		pmdname = GPOS_NEW(pmp) CMDName(pmp, pstrRelName);
		// CMDName ctor created a copy of the string
		GPOS_DELETE(pstrRelName);

		BlockNumber pages = 0;
		GpPolicy *pgppolicy = gpdb::Pdistrpolicy(rel);
		if (!pgppolicy ||pgppolicy->ptype != POLICYTYPE_PARTITIONED)
		{
			gpdb::EstimateRelationSize(rel, NULL, &pages, &rows);
		}
		else
		{
			rows = rel->rd_rel->reltuples;
		}

		pmdidRelStats->AddRef();
		gpdb::CloseRelation(rel);
	}
	GPOS_CATCH_EX(ex)
	{
		gpdb::CloseRelation(rel);
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	
	BOOL fEmptyStats = false;
	if (rows == 0.0)
	{
		fEmptyStats = true;
	}
		
	CDXLRelStats *pdxlrelstats = GPOS_NEW(pmp) CDXLRelStats
												(
												pmp,
												pmdidRelStats,
												pmdname,
												CDouble(rows),
												fEmptyStats
												);


	return pdxlrelstats;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PimdobjColStats
//
//	@doc:
//		Retrieve column statistics from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::PimdobjColStats
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
{
	CMDIdColStats *pmdidColStats = CMDIdColStats::PmdidConvert(pmdid);
	IMDId *pmdidRel = pmdidColStats->PmdidRel();
	ULONG ulPos = pmdidColStats->UlPos();
	OID oidRelation = CMDIdGPDB::PmdidConvert(pmdidRel)->OidObjectId();

	Relation rel = gpdb::RelGetRelation(oidRelation);
	if (NULL == rel)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	const IMDRelation *pmdrel = pmda->Pmdrel(pmdidRel);
	const IMDColumn *pmdcol = pmdrel->Pmdcol(ulPos);
	AttrNumber attrnum = (AttrNumber) pmdcol->IAttno();

	// number of rows from pg_class
	CDouble dRows(rel->rd_rel->reltuples);

	// extract column name and type
	CMDName *pmdnameCol = GPOS_NEW(pmp) CMDName(pmp, pmdcol->Mdname().Pstr());
	OID oidAttType = CMDIdGPDB::PmdidConvert(pmdcol->PmdidType())->OidObjectId();
	gpdb::CloseRelation(rel);

	DrgPdxlbucket *pdrgpdxlbucket = GPOS_NEW(pmp) DrgPdxlbucket(pmp);

	if (0 > attrnum)
	{
		pmdidColStats->AddRef();
		return PdxlcolstatsSystemColumn
				(
				pmp,
				oidRelation,
				pmdidColStats,
				pmdnameCol,
				oidAttType,
				attrnum,
				pdrgpdxlbucket,
				dRows
				);
	}

	// extract out histogram and mcv information from pg_statistic
	HeapTuple heaptupleStats = gpdb::HtAttrStats(oidRelation, attrnum);

	// if there is no colstats
	if (!HeapTupleIsValid(heaptupleStats))
	{
		pdrgpdxlbucket->Release();
		pmdidColStats->AddRef();

		CDouble dWidth = CStatistics::DDefaultColumnWidth;

		if (!pmdcol->FDropped())
		{
			CMDIdGPDB *pmdidAttType = GPOS_NEW(pmp) CMDIdGPDB(oidAttType);
			IMDType *pmdtype = Pmdtype(pmp, pmdidAttType);
			dWidth = CStatisticsUtils::DDefaultColumnWidth(pmdtype);
			pmdtype->Release();
			pmdidAttType->Release();
		}

		return CDXLColStats::PdxlcolstatsDummy(pmp, pmdidColStats, pmdnameCol, dWidth);
	}


	Datum	   *pdrgdatumMCVValues = NULL;
	int			iNumMCVValues = 0;
	float4	   *pdrgfMCVFrequencies = NULL;
	int			iNumMCVFrequencies = 0;
	Datum		*pdrgdatumHistValues = NULL;
	int			iNumHistValues = 0;

	(void)	gpdb::FGetAttrStatsSlot
			(
					heaptupleStats,
					oidAttType,
					-1,
					STATISTIC_KIND_MCV,
					InvalidOid,
					&pdrgdatumMCVValues, &iNumMCVValues,
					&pdrgfMCVFrequencies, &iNumMCVFrequencies
			);

	if (iNumMCVValues != iNumMCVFrequencies)
	{
		// if the number of MCVs and number of MCFs do not match, we discard the MCVs and MCFs
		gpdb::FreeAttrStatsSlot(oidAttType, pdrgdatumMCVValues, iNumMCVValues, pdrgfMCVFrequencies, iNumMCVFrequencies);
		iNumMCVValues = 0;
		iNumMCVFrequencies = 0;
		pdrgdatumMCVValues = NULL;
		pdrgfMCVFrequencies = NULL;

		char msgbuf[NAMEDATALEN * 2 + 100];
		snprintf(msgbuf, sizeof(msgbuf), "The number of most common values and frequencies do not match on column %ls of table %ls.",
				pmdcol->Mdname().Pstr()->Wsz(), pmdrel->Mdname().Pstr()->Wsz());
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION,
					   LOG,
					   msgbuf,
					   NULL);
	}

	Form_pg_statistic fpsStats = (Form_pg_statistic) GETSTRUCT(heaptupleStats);

	// null frequency and NDV
	CDouble dNullFrequency(0.0);
	int iNullNDV = 0;
	if (CStatistics::DEpsilon < fpsStats->stanullfrac)
	{
		dNullFrequency = fpsStats->stanullfrac;
		iNullNDV = 1;
	}

	// fix mcv and null frequencies (sometimes they can add up to more than 1.0)
	NormalizeFrequencies(pdrgfMCVFrequencies, (ULONG) iNumMCVValues, &dNullFrequency);

	// column width
	CDouble dWidth = CDouble(fpsStats->stawidth);

	// calculate total number of distinct values
	CDouble dDistinct(1.0);
	if (fpsStats->stadistinct < 0)
	{
		GPOS_ASSERT(fpsStats->stadistinct > -1.01);
		dDistinct = dRows * CDouble(-fpsStats->stadistinct);
	}
	else
	{
		dDistinct = CDouble(fpsStats->stadistinct);
	}
	dDistinct = dDistinct.FpCeil();

	// total MCV frequency
	CDouble dMCFSum = 0.0;
	for (int i = 0; i < iNumMCVValues; i++)
	{
		dMCFSum = dMCFSum + CDouble(pdrgfMCVFrequencies[i]);
	}

	// get histogram datums from pg_statistic entry
	(void) gpdb::FGetAttrStatsSlot
			(
					heaptupleStats,
					oidAttType,
					-1,
					STATISTIC_KIND_HISTOGRAM,
					InvalidOid,
					&pdrgdatumHistValues, &iNumHistValues,
					NULL, NULL);

	CDouble dNDVBuckets(0.0);
	CDouble dFreqBuckets(0.0);

	// We only want to create statistics buckets if the column is NOT a text, varchar, char or bpchar type
	// For the above column types we will use NDVRemain and NullFreq to do cardinality estimation.

	if (CTranslatorUtils::FCreateStatsBucket(oidAttType))
	{
		// transform all the bits and pieces from pg_statistic
		// to a single bucket structure
		DrgPdxlbucket *pdrgpdxlbucketTransformed =
		PdrgpdxlbucketTransformStats
		(
		 pmp,
		 oidAttType,
		 dDistinct,
		 dNullFrequency,
		 pdrgdatumMCVValues,
		 pdrgfMCVFrequencies,
		 ULONG(iNumMCVValues),
		 pdrgdatumHistValues,
		 ULONG(iNumHistValues)
		 );

		GPOS_ASSERT(NULL != pdrgpdxlbucketTransformed);

		const ULONG ulBuckets = pdrgpdxlbucketTransformed->UlLength();
		for (ULONG ul = 0; ul < ulBuckets; ul++)
		{
			CDXLBucket *pdxlbucket = (*pdrgpdxlbucketTransformed)[ul];
			dNDVBuckets = dNDVBuckets + pdxlbucket->DDistinct();
			dFreqBuckets = dFreqBuckets + pdxlbucket->DFrequency();
		}

		CUtils::AddRefAppend(pdrgpdxlbucket, pdrgpdxlbucketTransformed);
		pdrgpdxlbucketTransformed->Release();
	}

	// there will be remaining tuples if the merged histogram and the NULLS do not cover
	// the total number of distinct values
	CDouble dDistinctRemain(0.0);
	CDouble dFreqRemain(0.0);

 	if ((1 - CStatistics::DEpsilon > dFreqBuckets + dNullFrequency) &&
	 	(0 < dDistinct - dNDVBuckets - iNullNDV))
	{
 		dDistinctRemain = std::max(CDouble(0.0), (dDistinct - dNDVBuckets - iNullNDV));
 		dFreqRemain = std::max(CDouble(0.0), (1 - dFreqBuckets - dNullFrequency));
	}

	// free up allocated datum and float4 arrays
	gpdb::FreeAttrStatsSlot(oidAttType, pdrgdatumMCVValues, iNumMCVValues, pdrgfMCVFrequencies, iNumMCVFrequencies);
	gpdb::FreeAttrStatsSlot(oidAttType, pdrgdatumHistValues, iNumHistValues, NULL, 0);

	gpdb::FreeHeapTuple(heaptupleStats);

	// create col stats object
	pmdidColStats->AddRef();
	CDXLColStats *pdxlcolstats = GPOS_NEW(pmp) CDXLColStats
											(
											pmp,
											pmdidColStats,
											pmdnameCol,
											dWidth,
											dNullFrequency,
											dDistinctRemain,
											dFreqRemain,
											pdrgpdxlbucket,
											false /* fColStatsMissing */
											);

	return pdxlcolstats;
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorRelcacheToDXL::PdxlcolstatsSystemColumn
//
//      @doc:
//              Generate statistics for the system level columns
//
//---------------------------------------------------------------------------
CDXLColStats *
CTranslatorRelcacheToDXL::PdxlcolstatsSystemColumn
       (
       IMemoryPool *pmp,
       OID oidRelation,
       CMDIdColStats *pmdidColStats,
       CMDName *pmdnameCol,
       OID oidAttType,
       AttrNumber attrnum,
       DrgPdxlbucket *pdrgpdxlbucket,
       CDouble dRows
       )
{
       GPOS_ASSERT(NULL != pmdidColStats);
       GPOS_ASSERT(NULL != pmdnameCol);
       GPOS_ASSERT(InvalidOid != oidAttType);
       GPOS_ASSERT(0 > attrnum);
       GPOS_ASSERT(NULL != pdrgpdxlbucket);

       CMDIdGPDB *pmdidAttType = GPOS_NEW(pmp) CMDIdGPDB(oidAttType);
       IMDType *pmdtype = Pmdtype(pmp, pmdidAttType);
       GPOS_ASSERT(pmdtype->FFixedLength());

       BOOL fColStatsMissing = true;
       CDouble dNullFrequency(0.0);
       CDouble dWidth(pmdtype->UlLength());
       CDouble dDistinctRemain(0.0);
       CDouble dFreqRemain(0.0);

       if (CStatistics::DMinRows <= dRows)
	   {
		   switch(attrnum)
			{
				case GpSegmentIdAttributeNumber: // gp_segment_id
					{
						fColStatsMissing = false;
						dFreqRemain = CDouble(1.0);
						dDistinctRemain = CDouble(gpdb::UlSegmentCountGP());
						break;
					}
				case TableOidAttributeNumber: // tableoid
					{
						fColStatsMissing = false;
						dFreqRemain = CDouble(1.0);
						dDistinctRemain = CDouble(UlTableCount(oidRelation));
						break;
					}
				case SelfItemPointerAttributeNumber: // ctid
					{
						fColStatsMissing = false;
						dFreqRemain = CDouble(1.0);
						dDistinctRemain = dRows;
						break;
					}
				default:
					break;
			}
        }

       // cleanup
       pmdidAttType->Release();
       pmdtype->Release();

       return GPOS_NEW(pmp) CDXLColStats
                       (
                       pmp,
                       pmdidColStats,
                       pmdnameCol,
                       dWidth,
                       dNullFrequency,
                       dDistinctRemain,
                       dFreqRemain,
                       pdrgpdxlbucket,
                       fColStatsMissing
                       );
}


//---------------------------------------------------------------------------
//     @function:
//     CTranslatorRelcacheToDXL::UlTableCount
//
//  @doc:
//      For non-leaf partition tables return the number of child partitions
//      else return 1
//
//---------------------------------------------------------------------------
ULONG
CTranslatorRelcacheToDXL::UlTableCount
       (
       OID oidRelation
       )
{
       GPOS_ASSERT(InvalidOid != oidRelation);

       ULONG ulTableCount = ULONG_MAX;
       if (gpdb::FRelPartIsNone(oidRelation))
       {
    	   // not a partitioned table
            ulTableCount = 1;
       }
       else if (gpdb::FLeafPartition(oidRelation))
       {
           // leaf partition
           ulTableCount = 1;
       }
       else
       {
           ulTableCount = gpdb::UlLeafPartitions(oidRelation);
       }
       GPOS_ASSERT(ULONG_MAX != ulTableCount);

       return ulTableCount;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PimdobjCast
//
//	@doc:
//		Retrieve a cast function from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::PimdobjCast
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	CMDIdCast *pmdidCast = CMDIdCast::PmdidConvert(pmdid);
	IMDId *pmdidSrc = pmdidCast->PmdidSrc();
	IMDId *pmdidDest = pmdidCast->PmdidDest();
	IMDCast::EmdCoercepathType coercePathType;

	OID oidSrc = CMDIdGPDB::PmdidConvert(pmdidSrc)->OidObjectId();
	OID oidDest = CMDIdGPDB::PmdidConvert(pmdidDest)->OidObjectId();
	CoercionPathType	pathtype;

	OID oidCastFunc = 0;
	BOOL fBinaryCoercible = false;
	
	BOOL fCastExists = gpdb::FCastFunc(oidSrc, oidDest, &fBinaryCoercible, &oidCastFunc, &pathtype);
	
	if (!fCastExists)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	} 
	
	CHAR *szFuncName = NULL;
	if (InvalidOid != oidCastFunc)
	{
		szFuncName = gpdb::SzFuncName(oidCastFunc);
	}
	else
	{
		// no explicit cast function: use the destination type name as the cast name
		szFuncName = gpdb::SzTypeName(oidDest);
	}
	
	if (NULL == szFuncName)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	pmdid->AddRef();
	pmdidSrc->AddRef();
	pmdidDest->AddRef();

	CMDName *pmdname = CDXLUtils::PmdnameFromSz(pmp, szFuncName);
	
	switch (pathtype) {
		case COERCION_PATH_ARRAYCOERCE:
		{
			coercePathType = IMDCast::EmdtArrayCoerce;
			return GPOS_NEW(pmp) CMDArrayCoerceCastGPDB(pmp, pmdid, pmdname, pmdidSrc, pmdidDest, fBinaryCoercible, GPOS_NEW(pmp) CMDIdGPDB(oidCastFunc), IMDCast::EmdtArrayCoerce, -1, false, EdxlcfImplicitCast, -1);
		}
			break;
		case COERCION_PATH_FUNC:
			return GPOS_NEW(pmp) CMDCastGPDB(pmp, pmdid, pmdname, pmdidSrc, pmdidDest, fBinaryCoercible, GPOS_NEW(pmp) CMDIdGPDB(oidCastFunc), IMDCast::EmdtFunc);
			break;
		default:
			break;
	}

	// fall back for none path types
	return GPOS_NEW(pmp) CMDCastGPDB(pmp, pmdid, pmdname, pmdidSrc, pmdidDest, fBinaryCoercible, GPOS_NEW(pmp) CMDIdGPDB(oidCastFunc));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdobjScCmp
//
//	@doc:
//		Retrieve a scalar comparison from relcache
//
//---------------------------------------------------------------------------
IMDCacheObject *
CTranslatorRelcacheToDXL::PmdobjScCmp
	(
	IMemoryPool *pmp,
	IMDId *pmdid
	)
{
	CMDIdScCmp *pmdidScCmp = CMDIdScCmp::PmdidConvert(pmdid);
	IMDId *pmdidLeft = pmdidScCmp->PmdidLeft();
	IMDId *pmdidRight = pmdidScCmp->PmdidRight();
	
	IMDType::ECmpType ecmpt = pmdidScCmp->Ecmpt();

	OID oidLeft = CMDIdGPDB::PmdidConvert(pmdidLeft)->OidObjectId();
	OID oidRight = CMDIdGPDB::PmdidConvert(pmdidRight)->OidObjectId();
	CmpType cmpt = (CmpType) UlCmpt(ecmpt);
	
	OID oidScCmp = gpdb::OidScCmp(oidLeft, oidRight, cmpt);
	
	if (InvalidOid == oidScCmp)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	} 

	CHAR *szName = gpdb::SzOpName(oidScCmp);

	if (NULL == szName)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound, pmdid->Wsz());
	}

	pmdid->AddRef();
	pmdidLeft->AddRef();
	pmdidRight->AddRef();

	CMDName *pmdname = CDXLUtils::PmdnameFromSz(pmp, szName);

	return GPOS_NEW(pmp) CMDScCmpGPDB(pmp, pmdid, pmdname, pmdidLeft, pmdidRight, ecmpt, GPOS_NEW(pmp) CMDIdGPDB(oidScCmp));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpdxlbucketTransformStats
//
//	@doc:
//		transform stats from pg_stats form to optimizer's preferred form
//
//---------------------------------------------------------------------------
DrgPdxlbucket *
CTranslatorRelcacheToDXL::PdrgpdxlbucketTransformStats
	(
	IMemoryPool *pmp,
	OID oidAttType,
	CDouble dDistinct,
	CDouble dNullFreq,
	const Datum *pdrgdatumMCVValues,
	const float4 *pdrgfMCVFrequencies,
	ULONG ulNumMCVValues,
	const Datum *pdrgdatumHistValues,
	ULONG ulNumHistValues
	)
{
	CMDIdGPDB *pmdidAttType = GPOS_NEW(pmp) CMDIdGPDB(oidAttType);
	IMDType *pmdtype = Pmdtype(pmp, pmdidAttType);

	// translate MCVs to Orca histogram. Create an empty histogram if there are no MCVs.
	CHistogram *phistGPDBMCV = PhistTransformGPDBMCV
							(
							pmp,
							pmdtype,
							pdrgdatumMCVValues,
							pdrgfMCVFrequencies,
							ulNumMCVValues
							);

	GPOS_ASSERT(phistGPDBMCV->FValid());

	CDouble dMCVFreq = phistGPDBMCV->DFrequency();
	BOOL fHasMCV = 0 < ulNumMCVValues && CStatistics::DEpsilon < dMCVFreq;

	CDouble dHistFreq = 0.0;
	if (1 < ulNumHistValues)
	{
		dHistFreq = CDouble(1.0) - dNullFreq - dMCVFreq;
	}
	BOOL fHasHist = 1 < ulNumHistValues && CStatistics::DEpsilon < dHistFreq;

	CHistogram *phistGPDBHist = NULL;

	// if histogram has any significant information, then extract it
	if (fHasHist)
	{
		// histogram from gpdb histogram
		phistGPDBHist = PhistTransformGPDBHist
						(
						pmp,
						pmdtype,
						pdrgdatumHistValues,
						ulNumHistValues,
						dDistinct,
						dHistFreq
						);
		if (0 == phistGPDBHist->UlBuckets())
		{
			fHasHist = false;
		}
	}

	DrgPdxlbucket *pdrgpdxlbucket = NULL;

	if (fHasHist && !fHasMCV)
	{
		// if histogram exists and dominates, use histogram only
		pdrgpdxlbucket = Pdrgpdxlbucket(pmp, pmdtype, phistGPDBHist);
	}
	else if (!fHasHist && fHasMCV)
	{
		// if MCVs exist and dominate, use MCVs only
		pdrgpdxlbucket = Pdrgpdxlbucket(pmp, pmdtype, phistGPDBMCV);
	}
	else if (fHasHist && fHasMCV)
	{
		// both histogram and MCVs exist and have significant info, merge MCV and histogram buckets
		CHistogram *phistMerged = CStatisticsUtils::PhistMergeMcvHist(pmp, phistGPDBMCV, phistGPDBHist);
		pdrgpdxlbucket = Pdrgpdxlbucket(pmp, pmdtype, phistMerged);
		GPOS_DELETE(phistMerged);
	}
	else
	{
		// no MCVs nor histogram
		GPOS_ASSERT(!fHasHist && !fHasMCV);
		pdrgpdxlbucket = GPOS_NEW(pmp) DrgPdxlbucket(pmp);
	}

	// cleanup
	pmdidAttType->Release();
	pmdtype->Release();
	GPOS_DELETE(phistGPDBMCV);

	if (NULL != phistGPDBHist)
	{
		GPOS_DELETE(phistGPDBHist);
	}

	return pdrgpdxlbucket;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PhistTransformGPDBMCV
//
//	@doc:
//		Transform gpdb's mcv info to optimizer histogram
//
//---------------------------------------------------------------------------
CHistogram *
CTranslatorRelcacheToDXL::PhistTransformGPDBMCV
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	const Datum *pdrgdatumMCVValues,
	const float4 *pdrgfMCVFrequencies,
	ULONG ulNumMCVValues
	)
{
	DrgPdatum *pdrgpdatum = GPOS_NEW(pmp) DrgPdatum(pmp);
	DrgPdouble *pdrgpdFreq = GPOS_NEW(pmp) DrgPdouble(pmp);

	for (ULONG ul = 0; ul < ulNumMCVValues; ul++)
	{
		Datum datumMCV = pdrgdatumMCVValues[ul];
		IDatum *pdatum = CTranslatorScalarToDXL::Pdatum(pmp, pmdtype, false /* fNull */, datumMCV);
		pdrgpdatum->Append(pdatum);
		pdrgpdFreq->Append(GPOS_NEW(pmp) CDouble(pdrgfMCVFrequencies[ul]));

		if (!pdatum->FStatsComparable(pdatum))
		{
			// if less than operation is not supported on this datum, then no point
			// building a histogram. return an empty histogram
			pdrgpdatum->Release();
			pdrgpdFreq->Release();
			return GPOS_NEW(pmp) CHistogram(GPOS_NEW(pmp) DrgPbucket(pmp));
		}
	}

	CHistogram *phist = CStatisticsUtils::PhistTransformMCV
												(
												pmp,
												pmdtype,
												pdrgpdatum,
												pdrgpdFreq,
												ulNumMCVValues
												);

	pdrgpdatum->Release();
	pdrgpdFreq->Release();
	return phist;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PhistTransformGPDBHist
//
//	@doc:
//		Transform GPDB's hist info to optimizer's histogram
//
//---------------------------------------------------------------------------
CHistogram *
CTranslatorRelcacheToDXL::PhistTransformGPDBHist
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	const Datum *pdrgdatumHistValues,
	ULONG ulNumHistValues,
	CDouble dDistinctHist,
	CDouble dFreqHist
	)
{
	GPOS_ASSERT(1 < ulNumHistValues);

	ULONG ulNumBuckets = ulNumHistValues - 1;
	CDouble dDistinctPerBucket = dDistinctHist / CDouble(ulNumBuckets);
	CDouble dFreqPerBucket = dFreqHist / CDouble(ulNumBuckets);

	const ULONG ulBuckets = ulNumHistValues - 1;
	BOOL fLastBucketWasSingleton = false;
	// create buckets
	DrgPbucket *pdrgppbucket = GPOS_NEW(pmp) DrgPbucket(pmp);
	for (ULONG ul = 0; ul < ulBuckets; ul++)
	{
		Datum datumMin = pdrgdatumHistValues[ul];
		IDatum *pdatumMin = CTranslatorScalarToDXL::Pdatum(pmp, pmdtype, false /* fNull */, datumMin);

		Datum datumMax = pdrgdatumHistValues[ul + 1];
		IDatum *pdatumMax = CTranslatorScalarToDXL::Pdatum(pmp, pmdtype, false /* fNull */, datumMax);
		BOOL fLowerClosed, fUpperClosed;

		if (pdatumMin->FStatsEqual(pdatumMax))
		{
			// Singleton bucket !!!!!!!!!!!!!
			fLowerClosed = true;
			fUpperClosed = true;
			fLastBucketWasSingleton = true;
		}
		else if (fLastBucketWasSingleton)
		{
			// Last bucket was a singleton, so lower must be open now.
			fLowerClosed = false;
			fUpperClosed = false;
			fLastBucketWasSingleton = false;
		}
		else
		{
			// Normal bucket
			// GPDB histograms assumes lower bound to be closed and upper bound to be open
			fLowerClosed = true;
			fUpperClosed = false;
		}

		if (ul == ulBuckets - 1)
		{
			// last bucket upper bound is also closed
			fUpperClosed = true;
		}

		CBucket *pbucket = GPOS_NEW(pmp) CBucket
									(
									GPOS_NEW(pmp) CPoint(pdatumMin),
									GPOS_NEW(pmp) CPoint(pdatumMax),
									fLowerClosed,
									fUpperClosed,
									dFreqPerBucket,
									dDistinctPerBucket
									);
		pdrgppbucket->Append(pbucket);

		if (!pdatumMin->FStatsComparable(pdatumMin) || !pdatumMin->FStatsLessThan(pdatumMax))
		{
			// if less than operation is not supported on this datum,
			// or the translated histogram does not conform to GPDB sort order (e.g. text column in Linux platform),
			// then no point building a histogram. return an empty histogram

			// TODO: 03/01/2014 translate histogram into Orca even if sort
			// order is different in GPDB, and use const expression eval to compare
			// datums in Orca (MPP-22780)
			pdrgppbucket->Release();
			return GPOS_NEW(pmp) CHistogram(GPOS_NEW(pmp) DrgPbucket(pmp));
		}
	}

	CHistogram *phist = GPOS_NEW(pmp) CHistogram(pdrgppbucket);
	return phist;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Pdrgpdxlbucket
//
//	@doc:
//		Histogram to array of dxl buckets
//
//---------------------------------------------------------------------------
DrgPdxlbucket *
CTranslatorRelcacheToDXL::Pdrgpdxlbucket
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	const CHistogram *phist
	)
{
	DrgPdxlbucket *pdrgpdxlbucket = GPOS_NEW(pmp) DrgPdxlbucket(pmp);
	const DrgPbucket *pdrgpbucket = phist->Pdrgpbucket();
	ULONG ulNumBuckets = pdrgpbucket->UlLength();
	for (ULONG ul = 0; ul < ulNumBuckets; ul++)
	{
		CBucket *pbucket = (*pdrgpbucket)[ul];
		IDatum *pdatumLB = pbucket->PpLower()->Pdatum();
		CDXLDatum *pdxldatumLB = pmdtype->Pdxldatum(pmp, pdatumLB);
		IDatum *pdatumUB = pbucket->PpUpper()->Pdatum();
		CDXLDatum *pdxldatumUB = pmdtype->Pdxldatum(pmp, pdatumUB);
		CDXLBucket *pdxlbucket = GPOS_NEW(pmp) CDXLBucket
											(
											pdxldatumLB,
											pdxldatumUB,
											pbucket->FLowerClosed(),
											pbucket->FUpperClosed(),
											pbucket->DFrequency(),
											pbucket->DDistinct()
											);
		pdrgpdxlbucket->Append(pdxlbucket);
	}
	return pdrgpdxlbucket;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Erelstorage
//
//	@doc:
//		Get relation storage type
//
//---------------------------------------------------------------------------
IMDRelation::Erelstoragetype
CTranslatorRelcacheToDXL::Erelstorage
	(
	CHAR cStorageType
	)
{
	IMDRelation::Erelstoragetype erelstorage = IMDRelation::ErelstorageSentinel;

	switch (cStorageType)
	{
		case RELSTORAGE_HEAP:
			erelstorage = IMDRelation::ErelstorageHeap;
			break;
		case RELSTORAGE_AOCOLS:
			erelstorage = IMDRelation::ErelstorageAppendOnlyCols;
			break;
		case RELSTORAGE_AOROWS:
			erelstorage = IMDRelation::ErelstorageAppendOnlyRows;
			break;
		case RELSTORAGE_VIRTUAL:
			erelstorage = IMDRelation::ErelstorageVirtual;
			break;
		case RELSTORAGE_EXTERNAL:
			erelstorage = IMDRelation::ErelstorageExternal;
			break;
		default:
			GPOS_ASSERT(!"Unsupported relation type");
	}

	return erelstorage;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::GetPartKeysAndTypes
//
//	@doc:
//		Get partition keys and types for relation or NULL if relation not partitioned.
//		Caller responsible for closing the relation if an exception is raised
//
//---------------------------------------------------------------------------
void
CTranslatorRelcacheToDXL::GetPartKeysAndTypes
	(
	IMemoryPool *pmp,
	Relation rel,
	OID oid,
	DrgPul **pdrgpulPartKeys,
	DrgPsz **pdrgpszPartTypes
	)
{
	GPOS_ASSERT(NULL != rel);

	if (!gpdb::FRelPartIsRoot(oid))
	{
		// not a partitioned table
		*pdrgpulPartKeys = NULL;
		*pdrgpszPartTypes = NULL;
		return;
	}

	// TODO: Feb 23, 2012; support intermediate levels

	*pdrgpulPartKeys = GPOS_NEW(pmp) DrgPul(pmp);
	*pdrgpszPartTypes = GPOS_NEW(pmp) DrgPsz(pmp);

	List *plPartKeys = NIL;
	List *plPartTypes = NIL;
	gpdb::GetOrderedPartKeysAndKinds(oid, &plPartKeys, &plPartTypes);

	ListCell *plcKey = NULL;
	ListCell *plcType = NULL;
	ForBoth (plcKey, plPartKeys, plcType, plPartTypes)
	{
		List *plPartKey = (List *) lfirst(plcKey);

		if (1 < gpdb::UlListLength(plPartKey))
		{
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported, GPOS_WSZ_LIT("Composite part key"));
		}

		INT iAttno = linitial_int(plPartKey);
		CHAR partType = (CHAR) lfirst_int(plcType);
		GPOS_ASSERT(0 < iAttno);
		(*pdrgpulPartKeys)->Append(GPOS_NEW(pmp) ULONG(iAttno - 1));
		(*pdrgpszPartTypes)->Append(GPOS_NEW(pmp) CHAR(partType));
	}

	gpdb::FreeList(plPartKeys);
	gpdb::FreeList(plPartTypes);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PulAttnoMapping
//
//	@doc:
//		Construct a mapping for GPDB attnos to positions in the columns array
//
//---------------------------------------------------------------------------
ULONG *
CTranslatorRelcacheToDXL::PulAttnoMapping
	(
	IMemoryPool *pmp,
	DrgPmdcol *pdrgpmdcol,
	ULONG ulMaxCols
	)
{
	GPOS_ASSERT(NULL != pdrgpmdcol);
	GPOS_ASSERT(0 < pdrgpmdcol->UlLength());
	GPOS_ASSERT(ulMaxCols > pdrgpmdcol->UlLength());

	// build a mapping for attnos->positions
	const ULONG ulCols = pdrgpmdcol->UlLength();
	ULONG *pul = GPOS_NEW_ARRAY(pmp, ULONG, ulMaxCols);

	// initialize all positions to ULONG_MAX
	for (ULONG ul = 0;  ul < ulMaxCols; ul++)
	{
		pul[ul] = ULONG_MAX;
	}
	
	for (ULONG ul = 0;  ul < ulCols; ul++)
	{
		const IMDColumn *pmdcol = (*pdrgpmdcol)[ul];
		INT iAttno = pmdcol->IAttno();

		ULONG ulIndex = (ULONG) (GPDXL_SYSTEM_COLUMNS + iAttno);
		pul[ulIndex] = ul;
	}

	return pul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpdrgpulKeys
//
//	@doc:
//		Get key sets for relation
//
//---------------------------------------------------------------------------
DrgPdrgPul *
CTranslatorRelcacheToDXL::PdrgpdrgpulKeys
	(
	IMemoryPool *pmp,
	OID oid,
	BOOL fAddDefaultKeys,
	BOOL fPartitioned,
	ULONG *pulMapping
	)
{
	DrgPdrgPul *pdrgpdrgpul = GPOS_NEW(pmp) DrgPdrgPul(pmp);

	List *plKeys = gpdb::PlRelationKeys(oid);

	ListCell *plcKey = NULL;
	ForEach (plcKey, plKeys)
	{
		List *plKey = (List *) lfirst(plcKey);

		DrgPul *pdrgpulKey = GPOS_NEW(pmp) DrgPul(pmp);

		ListCell *plcKeyElem = NULL;
		ForEach (plcKeyElem, plKey)
		{
			INT iKey = lfirst_int(plcKeyElem);
			ULONG ulPos = UlPosition(iKey, pulMapping);
			pdrgpulKey->Append(GPOS_NEW(pmp) ULONG(ulPos));
		}
		GPOS_ASSERT(0 < pdrgpulKey->UlLength());

		pdrgpdrgpul->Append(pdrgpulKey);
	}
	
	// add {segid, ctid} as a key
	
	if (fAddDefaultKeys)
	{
		DrgPul *pdrgpulKey = GPOS_NEW(pmp) DrgPul(pmp);
		if (fPartitioned)
		{
			// TableOid is part of default key for partitioned tables
			ULONG ulPosTableOid = UlPosition(TableOidAttributeNumber, pulMapping);
			pdrgpulKey->Append(GPOS_NEW(pmp) ULONG(ulPosTableOid));
		}
		ULONG ulPosSegid= UlPosition(GpSegmentIdAttributeNumber, pulMapping);
		ULONG ulPosCtid = UlPosition(SelfItemPointerAttributeNumber, pulMapping);
		pdrgpulKey->Append(GPOS_NEW(pmp) ULONG(ulPosSegid));
		pdrgpulKey->Append(GPOS_NEW(pmp) ULONG(ulPosCtid));
		
		pdrgpdrgpul->Append(pdrgpulKey);
	}
	
	return pdrgpdrgpul;
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
CTranslatorRelcacheToDXL::NormalizeFrequencies
	(
	float4 *pdrgf,
	ULONG ulLength,
	CDouble *pdNullFrequency
	)
{
	if (ulLength == 0 && (*pdNullFrequency) < 1.0)
	{
		return;
	}

	CDouble dTotal = *pdNullFrequency;
	for (ULONG ul = 0; ul < ulLength; ul++)
	{
		dTotal = dTotal + CDouble(pdrgf[ul]);
	}

	if (dTotal > CDouble(1.0))
	{
		float4 fDenom = (float4) (dTotal + CStatistics::DEpsilon).DVal();

		// divide all values by the total
		for (ULONG ul = 0; ul < ulLength; ul++)
		{
			pdrgf[ul] = pdrgf[ul] / fDenom;
		}
		*pdNullFrequency = *pdNullFrequency / fDenom;
	}

#ifdef GPOS_DEBUG
	// recheck
	CDouble dTotalRecheck = *pdNullFrequency;
	for (ULONG ul = 0; ul < ulLength; ul++)
	{
		dTotalRecheck = dTotalRecheck + CDouble(pdrgf[ul]);
	}
	GPOS_ASSERT(dTotalRecheck <= CDouble(1.0));
#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::FIndexSupported
//
//	@doc:
//		Check if index type is supported
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::FIndexSupported
	(
	Relation relIndex
	)
{
	HeapTupleData *pht = relIndex->rd_indextuple;
	
	// index expressions and index constraints not supported
	return gpdb::FHeapAttIsNull(pht, Anum_pg_index_indexprs) &&
		gpdb::FHeapAttIsNull(pht, Anum_pg_index_indpred) && 
		(BTREE_AM_OID == relIndex->rd_rel->relam || BITMAP_AM_OID == relIndex->rd_rel->relam);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdpartcnstrIndex
//
//	@doc:
//		Retrieve part constraint for index
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB *
CTranslatorRelcacheToDXL::PmdpartcnstrIndex
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	const IMDRelation *pmdrel,
	Node *pnodePartCnstr,
	DrgPul *pdrgpulDefaultParts,
	BOOL fUnbounded
	)
{
	DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(pmp) DrgPdxlcd(pmp);
	const ULONG ulColumns = pmdrel->UlColumns();
	
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->Pmdcol(ul);
		CMDName *pmdnameCol = GPOS_NEW(pmp) CMDName(pmp, pmdcol->Mdname().Pstr());
		CMDIdGPDB *pmdidColType = CMDIdGPDB::PmdidConvert(pmdcol->PmdidType());
		pmdidColType->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *pdxlcd = GPOS_NEW(pmp) CDXLColDescr
										(
										pmp,
										pmdnameCol,
										ul + 1, // ulColId
										pmdcol->IAttno(),
										pmdidColType,
										false // fColDropped
										);
		pdrgpdxlcd->Append(pdxlcd);
	}
	
	CMDPartConstraintGPDB *pmdpartcnstr = PmdpartcnstrFromNode(pmp, pmda, pdrgpdxlcd, pnodePartCnstr, pdrgpulDefaultParts, fUnbounded);
	
	pdrgpdxlcd->Release();

	return pmdpartcnstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdpartcnstrRelation
//
//	@doc:
//		Retrieve part constraint for relation
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB *
CTranslatorRelcacheToDXL::PmdpartcnstrRelation
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	OID oidRel,
	DrgPmdcol *pdrgpmdcol,
	bool fhasIndex
	)
{
	// get the part constraints
	List *plDefaultLevelsRel = NIL;
	Node *pnode = gpdb::PnodePartConstraintRel(oidRel, &plDefaultLevelsRel);

	// don't retrieve part constraints if there are no indices
	// and no default partitions at any level
	if (!fhasIndex && NIL == plDefaultLevelsRel)
	{
		return NULL;
	}

	List *plPartKeys = gpdb::PlPartitionAttrs(oidRel);
	const ULONG ulLevels = gpdb::UlListLength(plPartKeys);
	gpdb::FreeList(plPartKeys);

	BOOL fUnbounded = true;
	DrgPul *pdrgpulDefaultLevels = GPOS_NEW(pmp) DrgPul(pmp);
	for (ULONG ul = 0; ul < ulLevels; ul++)
	{
		if (FDefaultPartition(plDefaultLevelsRel, ul))
		{
			pdrgpulDefaultLevels->Append(GPOS_NEW(pmp) ULONG(ul));
		}
		else
		{
			fUnbounded = false;
		}
	}

	CMDPartConstraintGPDB *pmdpartcnstr = NULL;

	if (!fhasIndex)
	{
		// if there are no indices then we don't need to construct the partition constraint
		// expression since ORCA is never going to use it.
		// only send the default partition information.
		pdrgpulDefaultLevels->AddRef();
		pmdpartcnstr = GPOS_NEW(pmp) CMDPartConstraintGPDB(pmp, pdrgpulDefaultLevels, fUnbounded, NULL);
	}
	else
	{
		DrgPdxlcd *pdrgpdxlcd = GPOS_NEW(pmp) DrgPdxlcd(pmp);
		const ULONG ulColumns = pdrgpmdcol->UlLength();
		for (ULONG ul = 0; ul < ulColumns; ul++)
		{
			const IMDColumn *pmdcol = (*pdrgpmdcol)[ul];
			CMDName *pmdnameCol = GPOS_NEW(pmp) CMDName(pmp, pmdcol->Mdname().Pstr());
			CMDIdGPDB *pmdidColType = CMDIdGPDB::PmdidConvert(pmdcol->PmdidType());
			pmdidColType->AddRef();

			// create a column descriptor for the column
			CDXLColDescr *pdxlcd = GPOS_NEW(pmp) CDXLColDescr
											(
											pmp,
											pmdnameCol,
											ul + 1, // ulColId
											pmdcol->IAttno(),
											pmdidColType,
											false // fColDropped
											);
			pdrgpdxlcd->Append(pdxlcd);
		}

		pmdpartcnstr = PmdpartcnstrFromNode(pmp, pmda, pdrgpdxlcd, pnode, pdrgpulDefaultLevels, fUnbounded);
		pdrgpdxlcd->Release();
	}

	gpdb::FreeList(plDefaultLevelsRel);
	pdrgpulDefaultLevels->Release();

	return pmdpartcnstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PmdpartcnstrFromNode
//
//	@doc:
//		Retrieve part constraint from GPDB node
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB *
CTranslatorRelcacheToDXL::PmdpartcnstrFromNode
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	DrgPdxlcd *pdrgpdxlcd,
	Node *pnodeCnstr,
	DrgPul *pdrgpulDefaultParts,
	BOOL fUnbounded
	)
{
	if (NULL == pnodeCnstr)
	{
		return NULL;
	}

	CTranslatorScalarToDXL sctranslator
							(
							pmp,
							pmda,
							NULL, // pulidgtorCol
							NULL, // pulidgtorCTE
							0, // ulQueryLevel
							true, // m_fQuery
							NULL, // phmulCTEEntries
							NULL // pdrgpdxlnCTE
							);

	// generate a mock mapping between var to column information
	CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);

	pmapvarcolid->LoadColumns(0 /*ulQueryLevel */, 1 /* rteIndex */, pdrgpdxlcd);

	// translate the check constraint expression
	CDXLNode *pdxlnScalar = sctranslator.PdxlnScOpFromExpr((Expr *) pnodeCnstr, pmapvarcolid);

	// cleanup
	GPOS_DELETE(pmapvarcolid);

	pdrgpulDefaultParts->AddRef();
	return GPOS_NEW(pmp) CMDPartConstraintGPDB(pmp, pdrgpulDefaultParts, fUnbounded, pdxlnScalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::FHasSystemColumns
//
//	@doc:
//		Does given relation type have system columns.
//		Currently only regular relations, sequences, toast values relations and
//		AO segment relations have system columns
//
//---------------------------------------------------------------------------
BOOL
CTranslatorRelcacheToDXL::FHasSystemColumns
	(
	char cRelKind
	)
{
	return RELKIND_RELATION == cRelKind || 
			RELKIND_SEQUENCE == cRelKind || 
			RELKIND_AOSEGMENTS == cRelKind ||
			RELKIND_TOASTVALUE == cRelKind;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::Ecmpt
//
//	@doc:
//		Translate GPDB comparison types into optimizer comparison types
//
//---------------------------------------------------------------------------
IMDType::ECmpType
CTranslatorRelcacheToDXL::Ecmpt
	(
	ULONG ulCmpt
	)
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgulCmpTypeMappings); ul++)
	{
		const ULONG *pul = rgulCmpTypeMappings[ul];
		if (pul[1] == ulCmpt)
		{
			return (IMDType::ECmpType) pul[0];
		}
	}
	
	return IMDType::EcmptOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::UlCmpt
//
//	@doc:
//		Translate optimizer comparison types into GPDB comparison types
//
//---------------------------------------------------------------------------
ULONG 
CTranslatorRelcacheToDXL::UlCmpt
	(
	IMDType::ECmpType ecmpt
	)
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgulCmpTypeMappings); ul++)
	{
		const ULONG *pul = rgulCmpTypeMappings[ul];
		if (pul[0] == ecmpt)
		{
			return (ULONG) pul[1];
		}
	}
	
	return CmptOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpmdidIndexOpFamilies
//
//	@doc:
//		Retrieve the opfamilies for the keys of the given index
//
//---------------------------------------------------------------------------
DrgPmdid * 
CTranslatorRelcacheToDXL::PdrgpmdidIndexOpFamilies
	(
	IMemoryPool *pmp,
	IMDId *pmdidIndex
	)
{
	List *plOpFamilies = gpdb::PlIndexOpFamilies(CMDIdGPDB::PmdidConvert(pmdidIndex)->OidObjectId());
	DrgPmdid *pdrgpmdid = GPOS_NEW(pmp) DrgPmdid(pmp);
	
	ListCell *plc = NULL;
	
	ForEach(plc, plOpFamilies)
	{
		OID oidOpFamily = lfirst_oid(plc);
		pdrgpmdid->Append(GPOS_NEW(pmp) CMDIdGPDB(oidOpFamily));
	}
	
	return pdrgpmdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorRelcacheToDXL::PdrgpmdidScOpOpFamilies
//
//	@doc:
//		Retrieve the families for the keys of the given scalar operator
//
//---------------------------------------------------------------------------
DrgPmdid * 
CTranslatorRelcacheToDXL::PdrgpmdidScOpOpFamilies
	(
	IMemoryPool *pmp,
	IMDId *pmdidScOp
	)
{
	List *plOpFamilies = gpdb::PlScOpOpFamilies(CMDIdGPDB::PmdidConvert(pmdidScOp)->OidObjectId());
	DrgPmdid *pdrgpmdid = GPOS_NEW(pmp) DrgPmdid(pmp);
	
	ListCell *plc = NULL;
	
	ForEach(plc, plOpFamilies)
	{
		OID oidOpFamily = lfirst_oid(plc);
		pdrgpmdid->Append(GPOS_NEW(pmp) CMDIdGPDB(oidOpFamily));
	}
	
	return pdrgpmdid;
}

// EOF

