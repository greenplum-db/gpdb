//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CTranslatorUtils.cpp
//
//	@doc:
//		Implementation of the utility methods for translating GPDB's
//		Query / PlannedStmt into DXL Tree
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "utils/guc.h"

#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_statistic.h"
#include "optimizer/walkers.h"
#include "utils/rel.h"

#define GPDB_NEXTVAL 1574
#define GPDB_CURRVAL 1575
#define GPDB_SETVAL 1576

#include "gpos/base.h"
#include "gpos/common/CAutoTimer.h"
#include "gpos/common/CBitSetIter.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLSpoolInfo.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"
#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/md/IMDTrigger.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeInt2.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/IMDTypeOid.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

#include "naucrates/dxl/operators/CDXLDatumInt2.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"

#include "naucrates/traceflags/traceflags.h"

#include "gpopt/gpdbwrappers.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpos;
using namespace gpopt;

extern bool optimizer_enable_master_only_queries;
extern bool optimizer_multilevel_partitioning;

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetIndexDescr
//
//	@doc:
//		Create a DXL index descriptor from an index MD id
//
//---------------------------------------------------------------------------
CDXLIndexDescr *
CTranslatorUtils::GetIndexDescr
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *pmdid
	)
{
	const IMDIndex *index = md_accessor->Pmdindex(pmdid);
	const CWStringConst *pstrIndexName = index->Mdname().GetMDName();
	CMDName *pmdnameIdx = GPOS_NEW(memory_pool) CMDName(memory_pool, pstrIndexName);

	return GPOS_NEW(memory_pool) CDXLIndexDescr(memory_pool, pmdid, pmdnameIdx);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetTableDescr
//
//	@doc:
//		Create a DXL table descriptor from a GPDB range table entry
//
//---------------------------------------------------------------------------
CDXLTableDescr *
CTranslatorUtils::GetTableDescr
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *id_generator,
	const RangeTblEntry *prte,
	BOOL *pfDistributedTable // output
	)
{
	// generate an MDId for the table desc.
	OID oidRel = prte->relid;

	if (gpdb::FHasExternalPartition(oidRel))
	{
		// fall back to the planner for queries with partition tables that has an external table in one of its leaf
		// partitions.
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Query over external partitions"));
	}

	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(oidRel);

	const IMDRelation *pmdrel = md_accessor->Pmdrel(pmdid);
	
	// look up table name
	const CWStringConst *pstrTblName = pmdrel->Mdname().GetMDName();
	CMDName *pmdnameTbl = GPOS_NEW(memory_pool) CMDName(memory_pool, pstrTblName);

	CDXLTableDescr *table_descr = GPOS_NEW(memory_pool) CDXLTableDescr(memory_pool, pmdid, pmdnameTbl, prte->checkAsUser);

	const ULONG ulLen = pmdrel->ColumnCount();

	IMDRelation::Ereldistrpolicy ereldist = pmdrel->GetRelDistribution();

	if (NULL != pfDistributedTable &&
		(IMDRelation::EreldistrHash == ereldist || IMDRelation::EreldistrRandom == ereldist))
	{
		*pfDistributedTable = true;
	}
	else if (!optimizer_enable_master_only_queries && (IMDRelation::EreldistrMasterOnly == ereldist))
		{
			// fall back to the planner for queries on master-only table if they are disabled with Orca. This is due to
			// the fact that catalog tables (master-only) are not analyzed often and will result in Orca producing
			// inferior plans.

			GPOS_THROW_EXCEPTION(gpdxl::ExmaDXL, // ulMajor
								 gpdxl::ExmiQuery2DXLUnsupportedFeature, // ulMinor
								 CException::ExsevDebug1, // ulSeverityLevel mapped to GPDB severity level
								 GPOS_WSZ_LIT("Queries on master-only tables"));
		}

	// add columns from md cache relation object to table descriptor
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		if (pmdcol->IsDropped())
		{
			continue;
		}
		
		CMDName *pmdnameCol = GPOS_NEW(memory_pool) CMDName(memory_pool, pmdcol->Mdname().GetMDName());
		CMDIdGPDB *pmdidColType = CMDIdGPDB::CastMdid(pmdcol->MDIdType());
		pmdidColType->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
											(
											memory_pool,
											pmdnameCol,
											id_generator->next_id(),
											pmdcol->AttrNum(),
											pmdidColType,
											pmdcol->TypeModifier(), /* type_modifier */
											false, /* fColDropped */
											pmdcol->Length()
											);
		table_descr->AddColumnDescr(dxl_col_descr);
	}

	return table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FSirvFunc
//
//	@doc:
//		Check if the given function is a SIRV (single row volatile) that reads
//		or modifies SQL data
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FSirvFunc
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	OID oidFunc
	)
{
	// we exempt the following 3 functions to avoid falling back to the planner
	// for DML on tables with sequences. The same exemption is also in the planner
	if (GPDB_NEXTVAL == oidFunc ||
		GPDB_CURRVAL == oidFunc ||
		GPDB_SETVAL == oidFunc)
	{
		return false;
	}

	CMDIdGPDB *mdid_func = GPOS_NEW(memory_pool) CMDIdGPDB(oidFunc);
	const IMDFunction *pmdfunc = md_accessor->Pmdfunc(mdid_func);

	BOOL fSirv = (!pmdfunc->ReturnsSet() &&
				  IMDFunction::EfsVolatile == pmdfunc->GetFuncStability());

	mdid_func->Release();

	return fSirv;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FHasSubquery
//
//	@doc:
//		Check if the given tree contains a subquery
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FHasSubquery
	(
	Node *pnode
	)
{
	List *plUnsupported = ListMake1Int(T_SubLink);
	INT iUnsupported = gpdb::IFindNodes(pnode, plUnsupported);
	gpdb::GPDBFree(plUnsupported);

	return (0 <= iUnsupported);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::Pdxltvf
//
//	@doc:
//		Create a DXL logical TVF from a GPDB range table entry
//
//---------------------------------------------------------------------------
CDXLLogicalTVF *
CTranslatorUtils::Pdxltvf
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *id_generator,
	const RangeTblEntry *prte
	)
{
	GPOS_ASSERT(NULL != prte->funcexpr);
	FuncExpr *pfuncexpr = (FuncExpr *)prte->funcexpr;

	// get function id
	CMDIdGPDB *mdid_func = GPOS_NEW(memory_pool) CMDIdGPDB(pfuncexpr->funcid);
	CMDIdGPDB *mdid_return_type =  GPOS_NEW(memory_pool) CMDIdGPDB(pfuncexpr->funcresulttype);
	const IMDType *pmdType = md_accessor->Pmdtype(mdid_return_type);

	// In the planner, scalar functions that are volatile (SIRV) or read or modify SQL
	// data get patched into an InitPlan. This is not supported in the optimizer
	if (FSirvFunc(memory_pool, md_accessor, pfuncexpr->funcid))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}

	// get function from MDcache
	const IMDFunction *pmdfunc = md_accessor->Pmdfunc(mdid_func);

	MdidPtrArray *pdrgpmdidOutArgTypes = pmdfunc->OutputArgTypesMdidArray();

	ColumnDescrDXLArray *column_descrs = NULL;

	if (NULL != prte->funccoltypes)
	{
		// function returns record - use col names and types from query
		column_descrs = PdrgdxlcdRecord(memory_pool, id_generator, prte->eref->colnames, prte->funccoltypes, prte->funccoltypmods);
	}
	else if (pmdType->IsComposite() && IMDId::IsValid(pmdType->GetBaseRelMdid()))
	{
		// function returns a "table" type or a user defined type
		column_descrs = PdrgdxlcdComposite(memory_pool, md_accessor, id_generator, pmdType);
	}
	else if (NULL != pdrgpmdidOutArgTypes)
	{
		// function returns record - but output col types are defined in catalog
		pdrgpmdidOutArgTypes->AddRef();
		if (FContainsPolymorphicTypes(pdrgpmdidOutArgTypes))
		{
			// resolve polymorphic types (anyelement/anyarray) using the
			// argument types from the query
			List *plArgTypes = gpdb::PlFuncArgTypes(pfuncexpr->funcid);
			MdidPtrArray *pdrgpmdidResolved = PdrgpmdidResolvePolymorphicTypes
												(
												memory_pool,
												pdrgpmdidOutArgTypes,
												plArgTypes,
												pfuncexpr
												);
			pdrgpmdidOutArgTypes->Release();
			pdrgpmdidOutArgTypes = pdrgpmdidResolved;
		}

		column_descrs = PdrgdxlcdRecord(memory_pool, id_generator, prte->eref->colnames, pdrgpmdidOutArgTypes);
		pdrgpmdidOutArgTypes->Release();
	}
	else
	{
		// function returns base type
		CMDName mdnameFunc = pmdfunc->Mdname();
		// table valued functions don't describe the returned column type modifiers, hence the -1
		column_descrs = PdrgdxlcdBase(memory_pool, id_generator, mdid_return_type, default_type_modifier, &mdnameFunc);
	}

	CMDName *pmdfuncname = GPOS_NEW(memory_pool) CMDName(memory_pool, pmdfunc->Mdname().GetMDName());

	CDXLLogicalTVF *pdxlopTVF = GPOS_NEW(memory_pool) CDXLLogicalTVF(memory_pool, mdid_func, mdid_return_type, pmdfuncname, column_descrs);

	return pdxlopTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgpmdidResolvePolymorphicTypes
//
//	@doc:
//		Resolve polymorphic types in the given array of type ids, replacing
//		them with the actual types obtained from the query
//
//---------------------------------------------------------------------------
MdidPtrArray *
CTranslatorUtils::PdrgpmdidResolvePolymorphicTypes
	(
	IMemoryPool *memory_pool,
	MdidPtrArray *mdid_array,
	List *plArgTypes,
	FuncExpr *pfuncexpr
	)
{
	ULONG ulArgIndex = 0;

	const ULONG ulArgTypes = gpdb::ListLength(plArgTypes);
	const ULONG ulArgsFromQuery = gpdb::ListLength(pfuncexpr->args);
	const ULONG ulNumReturnArgs = mdid_array->Size();
	const ULONG ulNumArgs = ulArgTypes < ulArgsFromQuery ? ulArgTypes : ulArgsFromQuery;
	const ULONG ulTotalArgs = ulNumArgs + ulNumReturnArgs;

	OID argTypes[ulNumArgs];
	char argModes[ulTotalArgs];

	// copy function argument types
	ListCell *plcArgType = NULL;
	ForEach (plcArgType, plArgTypes)
	{
		argTypes[ulArgIndex] = lfirst_oid(plcArgType);
		argModes[ulArgIndex++] = PROARGMODE_IN;
	}

	// copy function return types
	for (ULONG ul = 0; ul < ulNumReturnArgs; ul++)
	{
		IMDId *pmdid = (*mdid_array)[ul];
		argTypes[ulArgIndex] = CMDIdGPDB::CastMdid(pmdid)->OidObjectId();
		argModes[ulArgIndex++] = PROARGMODE_TABLE;
	}

	if(!gpdb::FResolvePolymorphicType(ulTotalArgs, argTypes, argModes, pfuncexpr))
	{
		GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiDXLUnrecognizedType,
				GPOS_WSZ_LIT("could not determine actual argument/return type for polymorphic function")
				);
	}

	// generate a new array of mdids based on the resolved types
	MdidPtrArray *pdrgpmdidResolved = GPOS_NEW(memory_pool) MdidPtrArray(memory_pool);

	// get the resolved return types
	for (ULONG ul = ulNumArgs; ul < ulTotalArgs ; ul++)
	{

		IMDId *pmdidResolved = NULL;
		pmdidResolved = GPOS_NEW(memory_pool) CMDIdGPDB(argTypes[ul]);
		pdrgpmdidResolved->Append(pmdidResolved);
	}

	return pdrgpmdidResolved;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FContainsPolymorphicTypes
//
//	@doc:
//		Check if the given mdid array contains any of the polymorphic
//		types (ANYELEMENT, ANYARRAY, ANYENUM, ANYNONARRAY)
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FContainsPolymorphicTypes
	(
	MdidPtrArray *mdid_array
	)
{
	GPOS_ASSERT(NULL != mdid_array);
	const ULONG ulLen = mdid_array->Size();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		IMDId *mdid_type = (*mdid_array)[ul];
		if (IsPolymorphicType(CMDIdGPDB::CastMdid(mdid_type)->OidObjectId()))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgdxlcdRecord
//
//	@doc:
//		Get column descriptors from a record type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::PdrgdxlcdRecord
	(
	IMemoryPool *memory_pool,
	CIdGenerator *id_generator,
	List *col_names,
	List *plColTypes,
	List *plColTypeModifiers
	)
{
	ListCell *plcColName = NULL;
	ListCell *plcColType = NULL;
	ListCell *plcColTypeModifier = NULL;

	ULONG ul = 0;
	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	ForThree (plcColName, col_names,
			plcColType, plColTypes,
			plcColTypeModifier, plColTypeModifiers)
	{
		Value *value = (Value *) lfirst(plcColName);
		Oid coltype = lfirst_oid(plcColType);
		INT type_modifier = lfirst_int(plcColTypeModifier);

		CHAR *col_name_char_array = strVal(value);
		CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, col_name_char_array);
		CMDName *pmdColName = GPOS_NEW(memory_pool) CMDName(memory_pool, column_name);
		GPOS_DELETE(column_name);

		IMDId *pmdidColType = GPOS_NEW(memory_pool) CMDIdGPDB(coltype);

		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										pmdColName,
										id_generator->next_id(),
										INT(ul + 1) /* iAttno */,
										pmdidColType,
										type_modifier,
										false /* fColDropped */
										);
		column_descrs->Append(dxl_col_descr);
		ul++;
	}

	return column_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgdxlcdRecord
//
//	@doc:
//		Get column descriptors from a record type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::PdrgdxlcdRecord
	(
	IMemoryPool *memory_pool,
	CIdGenerator *id_generator,
	List *col_names,
	MdidPtrArray *pdrgpmdidOutArgTypes
	)
{
	GPOS_ASSERT(pdrgpmdidOutArgTypes->Size() == (ULONG) gpdb::ListLength(col_names));
	ListCell *plcColName = NULL;

	ULONG ul = 0;
	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	ForEach (plcColName, col_names)
	{
		Value *value = (Value *) lfirst(plcColName);

		CHAR *col_name_char_array = strVal(value);
		CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, col_name_char_array);
		CMDName *pmdColName = GPOS_NEW(memory_pool) CMDName(memory_pool, column_name);
		GPOS_DELETE(column_name);

		IMDId *pmdidColType = (*pdrgpmdidOutArgTypes)[ul];
		pmdidColType->AddRef();

		// This function is only called to construct column descriptors for table-valued functions
		// which won't have type modifiers for columns of the returned table
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										pmdColName,
										id_generator->next_id(),
										INT(ul + 1) /* iAttno */,
										pmdidColType,
										default_type_modifier,
										false /* fColDropped */
										);
		column_descrs->Append(dxl_col_descr);
		ul++;
	}

	return column_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgdxlcdBase
//
//	@doc:
//		Get column descriptor from a base type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::PdrgdxlcdBase
	(
	IMemoryPool *memory_pool,
	CIdGenerator *id_generator,
	IMDId *mdid_return_type,
	INT type_modifier,
	CMDName *pmdName
	)
{
	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	mdid_return_type->AddRef();
	CMDName *pmdColName = GPOS_NEW(memory_pool) CMDName(memory_pool, pmdName->GetMDName());

	CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
									(
									memory_pool,
									pmdColName,
									id_generator->next_id(),
									INT(1) /* iAttno */,
									mdid_return_type,
									type_modifier, /* type_modifier */
									false /* fColDropped */
									);

	column_descrs->Append(dxl_col_descr);

	return column_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgdxlcdComposite
//
//	@doc:
//		Get column descriptors from a composite type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::PdrgdxlcdComposite
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *id_generator,
	const IMDType *pmdType
	)
{
	MDColumnPtrArray *pdrgPmdCol = ExpandCompositeType(memory_pool, md_accessor, pmdType);

	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	for (ULONG ul = 0; ul < pdrgPmdCol->Size(); ul++)
	{
		IMDColumn *pmdcol = (*pdrgPmdCol)[ul];

		CMDName *pmdColName = GPOS_NEW(memory_pool) CMDName(memory_pool, pmdcol->Mdname().GetMDName());
		IMDId *pmdidColType = pmdcol->MDIdType();

		pmdidColType->AddRef();
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										pmdColName,
										id_generator->next_id(),
										INT(ul + 1) /* iAttno */,
										pmdidColType,
										pmdcol->TypeModifier(), /* type_modifier */
										false /* fColDropped */
										);
		column_descrs->Append(dxl_col_descr);
	}

	pdrgPmdCol->Release();

	return column_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::ExpandCompositeType
//
//	@doc:
//		Expand a composite type into an array of IMDColumns
//
//---------------------------------------------------------------------------
MDColumnPtrArray *
CTranslatorUtils::ExpandCompositeType
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	const IMDType *pmdType
	)
{
	GPOS_ASSERT(NULL != pmdType);
	GPOS_ASSERT(pmdType->IsComposite());

	IMDId *pmdidRel = pmdType->GetBaseRelMdid();
	const IMDRelation *pmdrel = md_accessor->Pmdrel(pmdidRel);
	GPOS_ASSERT(NULL != pmdrel);

	MDColumnPtrArray *pdrgPmdcol = GPOS_NEW(memory_pool) MDColumnPtrArray(memory_pool);

	for(ULONG ul = 0; ul < pmdrel->ColumnCount(); ul++)
	{
		CMDColumn *pmdcol = (CMDColumn *) pmdrel->GetMdCol(ul);

		if (!pmdcol->IsSystemColumn())
		{
			pmdcol->AddRef();
			pdrgPmdcol->Append(pmdcol);
		}
	}

	return pdrgPmdcol;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::EdxljtFromJoinType
//
//	@doc:
//		Translates the join type from its GPDB representation into the DXL one
//
//---------------------------------------------------------------------------
EdxlJoinType
CTranslatorUtils::EdxljtFromJoinType
	(
	JoinType jt
	)
{
	EdxlJoinType join_type = EdxljtSentinel;

	switch (jt)
	{
		case JOIN_INNER:
			join_type = EdxljtInner;
			break;

		case JOIN_LEFT:
			join_type = EdxljtLeft;
			break;

		case JOIN_FULL:
			join_type = EdxljtFull;
			break;

		case JOIN_RIGHT:
			join_type = EdxljtRight;
			break;

		case JOIN_SEMI:
			join_type = EdxljtIn;
			break;

		case JOIN_ANTI:
			join_type = EdxljtLeftAntiSemijoin;
			break;

		case JOIN_LASJ_NOTIN:
			join_type = EdxljtLeftAntiSemijoinNotIn;
			break;

		default:
			GPOS_ASSERT(!"Unrecognized join type");
	}

	GPOS_ASSERT(EdxljtSentinel > join_type);

	return join_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::EdxlIndexDirection
//
//	@doc:
//		Translates the DXL index scan direction from GPDB representation
//
//---------------------------------------------------------------------------
EdxlIndexScanDirection
CTranslatorUtils::EdxlIndexDirection
	(
	ScanDirection sd
	)
{
	EdxlIndexScanDirection idx_scan_direction = EdxlisdSentinel;

	switch (sd)
	{
		case BackwardScanDirection:
			idx_scan_direction = EdxlisdBackward;
			break;

		case ForwardScanDirection:
			idx_scan_direction = EdxlisdForward;
			break;

		case NoMovementScanDirection:
			idx_scan_direction = EdxlisdNoMovement;
			break;

		default:
			GPOS_ASSERT(!"Unrecognized index scan direction");
	}

	GPOS_ASSERT(EdxlisdSentinel > idx_scan_direction);

	return idx_scan_direction;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColumnDescrAt
//
//	@doc:
//		Find the n-th col descr entry
//
//---------------------------------------------------------------------------
const CDXLColDescr *
CTranslatorUtils::GetColumnDescrAt
	(
	const CDXLTableDescr *table_descr,
	ULONG ulPos
	)
{
	GPOS_ASSERT(0 != ulPos);
	GPOS_ASSERT(ulPos < table_descr->Arity());

	return table_descr->GetColumnDescrAt(ulPos);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PstrSystemColName
//
//	@doc:
//		Return the name for the system attribute with the given attribute number.
//
//---------------------------------------------------------------------------
const CWStringConst *
CTranslatorUtils::PstrSystemColName
	(
	AttrNumber attno
	)
{
	GPOS_ASSERT(FirstLowInvalidHeapAttributeNumber < attno && 0 > attno);

	switch (attno)
	{
		case SelfItemPointerAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenCtidColName);

		case ObjectIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOidColName);

		case MinTransactionIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenXminColName);

		case MinCommandIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenCminColName);

		case MaxTransactionIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenXmaxColName);

		case MaxCommandIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenCmaxColName);

		case TableOidAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenTableOidColName);

		case GpSegmentIdAttributeNumber:
			return CDXLTokens::GetDXLTokenStr(EdxltokenGpSegmentIdColName);

		default:
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Invalid attribute number")
				);
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PmdidSystemColType
//
//	@doc:
//		Return the type id for the system attribute with the given attribute number.
//
//---------------------------------------------------------------------------
CMDIdGPDB *
CTranslatorUtils::PmdidSystemColType
	(
	IMemoryPool *memory_pool,
	AttrNumber attno
	)
{
	GPOS_ASSERT(FirstLowInvalidHeapAttributeNumber < attno && 0 > attno);

	switch (attno)
	{
		case SelfItemPointerAttributeNumber:
			// tid type
			return GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_TID);

		case ObjectIdAttributeNumber:
		case TableOidAttributeNumber:
			// OID type
			return GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_OID);

		case MinTransactionIdAttributeNumber:
		case MaxTransactionIdAttributeNumber:
			// xid type
			return GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_XID);

		case MinCommandIdAttributeNumber:
		case MaxCommandIdAttributeNumber:
			// cid type
			return GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_CID);

		case GpSegmentIdAttributeNumber:
			// int4
			return GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4);

		default:
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Invalid attribute number")
				);
			return NULL;
	}
}


// Returns the length for the system column with given attno number
const ULONG
CTranslatorUtils::UlSystemColLength
	(
	AttrNumber attno
	)
{
	GPOS_ASSERT(FirstLowInvalidHeapAttributeNumber < attno && 0 > attno);

	switch (attno)
	{
		case SelfItemPointerAttributeNumber:
			// tid type
			return 6;

		case ObjectIdAttributeNumber:
		case TableOidAttributeNumber:
			// OID type

		case MinTransactionIdAttributeNumber:
		case MaxTransactionIdAttributeNumber:
			// xid type

		case MinCommandIdAttributeNumber:
		case MaxCommandIdAttributeNumber:
			// cid type

		case GpSegmentIdAttributeNumber:
			// int4
			return 4;

		default:
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Invalid attribute number")
				);
			return gpos::ulong_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::Scandirection
//
//	@doc:
//		Return the GPDB specific scan direction from its corresponding DXL
//		representation
//
//---------------------------------------------------------------------------
ScanDirection
CTranslatorUtils::Scandirection
	(
	EdxlIndexScanDirection idx_scan_direction
	)
{
	if (EdxlisdBackward == idx_scan_direction)
	{
		return BackwardScanDirection;
	}

	if (EdxlisdForward == idx_scan_direction)
	{
		return ForwardScanDirection;
	}

	return NoMovementScanDirection;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::OidCmpOperator
//
//	@doc:
//		Extract comparison operator from an OpExpr, ScalarArrayOpExpr or RowCompareExpr
//
//---------------------------------------------------------------------------
OID
CTranslatorUtils::OidCmpOperator
	(
	Expr* pexpr
	)
{
	GPOS_ASSERT(IsA(pexpr, OpExpr) || IsA(pexpr, ScalarArrayOpExpr) || IsA(pexpr, RowCompareExpr));

	switch (pexpr->type)
	{
		case T_OpExpr:
			return ((OpExpr *) pexpr)->opno;
			
		case T_ScalarArrayOpExpr:
			return ((ScalarArrayOpExpr*) pexpr)->opno;

		case T_RowCompareExpr:
			return LInitialOID(((RowCompareExpr *) pexpr)->opnos);
			
		default:
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Unsupported comparison")
				);
			return InvalidOid;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::OidIndexQualOpFamily
//
//	@doc:
//		Extract comparison operator family for the given index column
//
//---------------------------------------------------------------------------
OID
CTranslatorUtils::OidIndexQualOpFamily
	(
	INT iAttno,
	OID oidIndex
	)
{
	Relation relIndex = gpdb::RelGetRelation(oidIndex);
	GPOS_ASSERT(NULL != relIndex);
	GPOS_ASSERT(iAttno <= relIndex->rd_index->indnatts);
	
	OID oidOpfamily = relIndex->rd_opfamily[iAttno - 1];
	gpdb::CloseRelation(relIndex);
	
	return oidOpfamily;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetSetOpType
//
//	@doc:
//		Return the DXL representation of the set operation
//
//---------------------------------------------------------------------------
EdxlSetOpType
CTranslatorUtils::GetSetOpType
	(
	SetOperation setop,
	BOOL fAll
	)
{
	if (SETOP_UNION == setop && fAll)
	{
		return EdxlsetopUnionAll;
	}

	if (SETOP_INTERSECT == setop && fAll)
	{
		return EdxlsetopIntersectAll;
	}

	if (SETOP_EXCEPT == setop && fAll)
	{
		return EdxlsetopDifferenceAll;
	}

	if (SETOP_UNION == setop)
	{
		return EdxlsetopUnion;
	}

	if (SETOP_INTERSECT == setop)
	{
		return EdxlsetopIntersect;
	}

	if (SETOP_EXCEPT == setop)
	{
		return EdxlsetopDifference;
	}

	GPOS_ASSERT(!"Unrecognized set operator type");
	return EdxlsetopSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetGroupingColidArray
//
//	@doc:
//		Construct a dynamic array of column ids for the given set of grouping
// 		col attnos
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::GetGroupingColidArray
	(
	IMemoryPool *memory_pool,
	CBitSet *pbsGroupByCols,
	IntUlongHashMap *phmiulSortGrpColsColId
	)
{
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	if (NULL != pbsGroupByCols)
	{
		CBitSetIter bsi(*pbsGroupByCols);

		while (bsi.Advance())
		{
			const ULONG col_id = GetColId(bsi.Bit(), phmiulSortGrpColsColId);
			pdrgpul->Append(GPOS_NEW(memory_pool) ULONG(col_id));
		}
	}

	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgpbsGroupBy
//
//	@doc:
//		Construct a dynamic array of sets of column attnos corresponding to the
// 		group by clause
//
//---------------------------------------------------------------------------
DrgPbs *
CTranslatorUtils::PdrgpbsGroupBy
	(
	IMemoryPool *memory_pool,
	List *plGroupClause,
	ULONG ulCols,
	UlongUlongHashMap *phmululGrpColPos,	// mapping of grouping col positions to SortGroupRef ids
	CBitSet *pbsGrpCols			// existing uniqueue grouping columns
	)
{
	GPOS_ASSERT(NULL != plGroupClause);
	GPOS_ASSERT(0 < gpdb::ListLength(plGroupClause));
	GPOS_ASSERT(NULL != phmululGrpColPos);

	Node *pnode = (Node*) LInitial(plGroupClause);

	if (NULL == pnode || IsA(pnode, SortGroupClause))
	{
		// simple group by
		CBitSet *pbsGroupingSet = PbsGroupingSet(memory_pool, plGroupClause, ulCols, phmululGrpColPos, pbsGrpCols);
		DrgPbs *pdrgpbs = GPOS_NEW(memory_pool) DrgPbs(memory_pool);
		pdrgpbs->Append(pbsGroupingSet);
		return pdrgpbs;
	}

	if (!IsA(pnode, GroupingClause))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Group by clause"));
	}

	const ULONG ulGroupClause = gpdb::ListLength(plGroupClause);
	GPOS_ASSERT(0 < ulGroupClause);
	if (1 < ulGroupClause)
	{
		// multiple grouping sets
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple grouping sets specifications"));
	}

	GroupingClause *pgrcl = (GroupingClause *) pnode;

	if (GROUPINGTYPE_ROLLUP == pgrcl->groupType)
	{
		return PdrgpbsRollup(memory_pool, pgrcl, ulCols, phmululGrpColPos, pbsGrpCols);
	}

	if (GROUPINGTYPE_CUBE == pgrcl->groupType)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Cube"));
	}

	DrgPbs *pdrgpbs = GPOS_NEW(memory_pool) DrgPbs(memory_pool);

	ListCell *plcGroupingSet = NULL;
	ForEach (plcGroupingSet, pgrcl->groupsets)
	{
		Node *pnodeGroupingSet = (Node *) lfirst(plcGroupingSet);

		CBitSet *pbs = NULL;
		if (IsA(pnodeGroupingSet, SortGroupClause))
		{
			// grouping set contains a single grouping column
			pbs = GPOS_NEW(memory_pool) CBitSet(memory_pool, ulCols);
			ULONG ulSortGrpRef = ((SortGroupClause *) pnodeGroupingSet)->tleSortGroupRef;
			pbs->ExchangeSet(ulSortGrpRef);
			UpdateGrpColMapping(memory_pool, phmululGrpColPos, pbsGrpCols, ulSortGrpRef);
		}
		else if (IsA(pnodeGroupingSet, GroupingClause))
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple grouping sets specifications"));
		}
		else
		{
			// grouping set contains a list of columns
			GPOS_ASSERT(IsA(pnodeGroupingSet, List));

			List *plGroupingSet = (List *) pnodeGroupingSet;
			pbs = PbsGroupingSet(memory_pool, plGroupingSet, ulCols, phmululGrpColPos, pbsGrpCols);
		}
		pdrgpbs->Append(pbs);
	}

	return pdrgpbs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgpbsRollup
//
//	@doc:
//		Construct a dynamic array of sets of column attnos for a rollup
//
//---------------------------------------------------------------------------
DrgPbs *
CTranslatorUtils::PdrgpbsRollup
	(
	IMemoryPool *memory_pool,
	GroupingClause *pgrcl,
	ULONG ulCols,
	UlongUlongHashMap *phmululGrpColPos,	// mapping of grouping col positions to SortGroupRef ids,
	CBitSet *pbsGrpCols			// existing grouping columns
	)
{
	GPOS_ASSERT(NULL != pgrcl);

	DrgPbs *pdrgpbsGroupingSets = GPOS_NEW(memory_pool) DrgPbs(memory_pool);
	ListCell *plcGroupingSet = NULL;
	ForEach (plcGroupingSet, pgrcl->groupsets)
	{
		Node *pnode = (Node *) lfirst(plcGroupingSet);
		CBitSet *pbs = GPOS_NEW(memory_pool) CBitSet(memory_pool);
		if (IsA(pnode, SortGroupClause))
		{
			// simple group clause, create a singleton grouping set
			SortGroupClause *pgrpcl = (SortGroupClause *) pnode;
			ULONG ulSortGrpRef = pgrpcl->tleSortGroupRef;
			(void) pbs->ExchangeSet(ulSortGrpRef);
			pdrgpbsGroupingSets->Append(pbs);
			UpdateGrpColMapping(memory_pool, phmululGrpColPos, pbsGrpCols, ulSortGrpRef);
		}
		else if (IsA(pnode, List))
		{
			// list of group clauses, add all clauses into one grouping set
			// for example, rollup((a,b),(c,d));
			List *plist = (List *) pnode;
			ListCell *plcGrpCl = NULL;
			ForEach (plcGrpCl, plist)
			{
				Node *pnodeGrpCl = (Node *) lfirst(plcGrpCl);
				if (!IsA(pnodeGrpCl, SortGroupClause))
				{
					// each list entry must be a group clause
					// for example, rollup((a,b),(c,(d,e)));
					GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Nested grouping sets"));
				}

				SortGroupClause *pgrpcl = (SortGroupClause *) pnodeGrpCl;
				ULONG ulSortGrpRef = pgrpcl->tleSortGroupRef;
				(void) pbs->ExchangeSet(ulSortGrpRef);
				UpdateGrpColMapping(memory_pool, phmululGrpColPos, pbsGrpCols, ulSortGrpRef);
			}
			pdrgpbsGroupingSets->Append(pbs);
		}
		else
		{
			// unsupported rollup operation
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Nested grouping sets"));
		}
	}

	const ULONG ulGroupingSets = pdrgpbsGroupingSets->Size();
	DrgPbs *pdrgpbs = GPOS_NEW(memory_pool) DrgPbs(memory_pool);

	// compute prefixes of grouping sets array
	for (ULONG ulPrefix = 0; ulPrefix <= ulGroupingSets; ulPrefix++)
	{
		CBitSet *pbs = GPOS_NEW(memory_pool) CBitSet(memory_pool);
		for (ULONG ulIdx = 0; ulIdx < ulPrefix; ulIdx++)
		{
			CBitSet *pbsCurrent = (*pdrgpbsGroupingSets)[ulIdx];
			pbs->Union(pbsCurrent);
		}
		pdrgpbs->Append(pbs);
	}
	pdrgpbsGroupingSets->Release();

	return pdrgpbs;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PbsGroupingSet
//
//	@doc:
//		Construct a set of column attnos corresponding to a grouping set
//
//---------------------------------------------------------------------------
CBitSet *
CTranslatorUtils::PbsGroupingSet
	(
	IMemoryPool *memory_pool,
	List *plGroupElems,
	ULONG ulCols,
	UlongUlongHashMap *phmululGrpColPos,	// mapping of grouping col positions to SortGroupRef ids,
	CBitSet *pbsGrpCols			// existing grouping columns
	)
{
	GPOS_ASSERT(NULL != plGroupElems);
	GPOS_ASSERT(0 < gpdb::ListLength(plGroupElems));

	CBitSet *pbs = GPOS_NEW(memory_pool) CBitSet(memory_pool, ulCols);

	ListCell *lc = NULL;
	ForEach (lc, plGroupElems)
	{
		Node *pnodeElem = (Node*) lfirst(lc);

		if (NULL == pnodeElem)
		{
			continue;
		}

		if (!IsA(pnodeElem, SortGroupClause))
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Mixing grouping sets with simple group by lists"));
		}

		ULONG ulSortGrpRef = ((SortGroupClause *) pnodeElem)->tleSortGroupRef;
		pbs->ExchangeSet(ulSortGrpRef);
		
		UpdateGrpColMapping(memory_pool, phmululGrpColPos, pbsGrpCols, ulSortGrpRef);
	}
	
	return pbs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgpulGenerateColIds
//
//	@doc:
//		Construct an array of DXL column identifiers for a target list
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::PdrgpulGenerateColIds
	(
	IMemoryPool *memory_pool,
	List *target_list,
	MdidPtrArray *pdrgpmdidInput,
	ULongPtrArray *pdrgpulInput,
	BOOL *pfOuterRef,  // array of flags indicating if input columns are outer references
	CIdGenerator *pidgtorColId
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != pdrgpmdidInput);
	GPOS_ASSERT(NULL != pdrgpulInput);
	GPOS_ASSERT(NULL != pfOuterRef);
	GPOS_ASSERT(NULL != pidgtorColId);

	GPOS_ASSERT(pdrgpmdidInput->Size() == pdrgpulInput->Size());

	ULONG ulColPos = 0;
	ListCell *plcTE = NULL;
	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(NULL != target_entry->expr);

		OID oidExprType = gpdb::OidExprType((Node*) target_entry->expr);
		if (!target_entry->resjunk)
		{
			ULONG col_id = gpos::ulong_max;
			IMDId *pmdid = (*pdrgpmdidInput)[ulColPos];
			if (CMDIdGPDB::CastMdid(pmdid)->OidObjectId() != oidExprType || 
				pfOuterRef[ulColPos])
			{
				// generate a new column when:
				//  (1) the type of input column does not match that of the output column, or
				//  (2) input column is an outer reference 
				col_id = pidgtorColId->next_id();
			}
			else
			{
				// use the column identifier of the input
				col_id = *(*pdrgpulInput)[ulColPos];
			}
			GPOS_ASSERT(gpos::ulong_max != col_id);
			
			pdrgpul->Append(GPOS_NEW(memory_pool) ULONG(col_id));

			ulColPos++;
		}
	}

	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PqueryFixUnknownTypeConstant
//
//	@doc:
//		If the query has constant of unknown type, then return a copy of the
//		query with all constants of unknown type being coerced to the common data type
//		of the output target list; otherwise return the original query
//---------------------------------------------------------------------------
Query *
CTranslatorUtils::PqueryFixUnknownTypeConstant
	(
	Query *pqueryOld,
	List *plTargetListOutput
	)
{
	GPOS_ASSERT(NULL != pqueryOld);
	GPOS_ASSERT(NULL != plTargetListOutput);

	Query *pqueryNew = NULL;

	ULONG ulPos = 0;
	ULONG ulColPos = 0;
	ListCell *plcTE = NULL;
	ForEach (plcTE, pqueryOld->targetList)
	{
		TargetEntry *pteOld = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(NULL != pteOld->expr);

		if (!pteOld->resjunk)
		{
			if (IsA(pteOld->expr, Const) && (GPDB_UNKNOWN == gpdb::OidExprType((Node*) pteOld->expr) ))
			{
				if (NULL == pqueryNew)
				{
					pqueryNew = (Query*) gpdb::PvCopyObject(const_cast<Query*>(pqueryOld));
				}

				TargetEntry *pteNew = (TargetEntry *) gpdb::PvListNth(pqueryNew->targetList, ulPos);
				GPOS_ASSERT(pteOld->resno == pteNew->resno);
				// implicitly cast the unknown constants to the target data type
				OID oidTargetType = OidTargetListReturnType(plTargetListOutput, ulColPos);
				GPOS_ASSERT(InvalidOid != oidTargetType);
				Node *pnodeOld = (Node *) pteNew->expr;
				pteNew->expr = (Expr*) gpdb::PnodeCoerceToCommonType
											(
											NULL,	/* pstate */
											(Node *) pnodeOld,
											oidTargetType,
											"UNION/INTERSECT/EXCEPT"
											);

				gpdb::GPDBFree(pnodeOld);
			}
			ulColPos++;
		}

		ulPos++;
	}

	if (NULL == pqueryNew)
	{
		return pqueryOld;
	}

	gpdb::GPDBFree(pqueryOld);

	return pqueryNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::OidTargetListReturnType
//
//	@doc:
//		Return the type of the nth non-resjunked target list entry
//
//---------------------------------------------------------------------------
OID
CTranslatorUtils::OidTargetListReturnType
	(
	List *target_list,
	ULONG ulColPos
	)
{
	ULONG ulColIdx = 0;
	ListCell *plcTE = NULL;

	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		GPOS_ASSERT(NULL != target_entry->expr);

		if (!target_entry->resjunk)
		{
			if (ulColIdx == ulColPos)
			{
				return gpdb::OidExprType((Node*) target_entry->expr);
			}

			ulColIdx++;
		}
	}

	return InvalidOid;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColumnDescrDXLArray
//
//	@doc:
//		Construct an array of DXL column descriptors for a target list using the
// 		column ids in the given array
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::GetColumnDescrDXLArray
	(
	IMemoryPool *memory_pool,
	List *target_list,
	ULongPtrArray *pdrgpulColIds,
	BOOL fKeepResjunked
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != pdrgpulColIds);

	ListCell *plcTE = NULL;
	ColumnDescrDXLArray *pdrgpdxlcd = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);
	ULONG ul = 0;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);

		if (target_entry->resjunk && !fKeepResjunked)
		{
			continue;
		}

		ULONG col_id = *(*pdrgpulColIds)[ul];
		CDXLColDescr *dxl_col_descr = GetColumnDescrAt(memory_pool, target_entry, col_id, ul+1 /*ulPos*/);
		pdrgpdxlcd->Append(dxl_col_descr);
		ul++;
	}

	GPOS_ASSERT(pdrgpdxlcd->Size() == pdrgpulColIds->Size());

	return pdrgpdxlcd;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdrgpulPosInTargetList
//
//	@doc:
//		Return the positions of the target list entries included in the output
//		target list
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::PdrgpulPosInTargetList
	(
	IMemoryPool *memory_pool,
	List *target_list,
	BOOL fKeepResjunked
	)
{
	GPOS_ASSERT(NULL != target_list);

	ListCell *plcTE = NULL;
	ULongPtrArray *pdrgul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	ULONG ul = 0;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);

		if (target_entry->resjunk && !fKeepResjunked)
		{
			continue;
		}

		pdrgul->Append(GPOS_NEW(memory_pool) ULONG(ul));
		ul++;
	}

	return pdrgul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColumnDescrAt
//
//	@doc:
//		Construct a column descriptor from the given target entry and column
//		identifier
//---------------------------------------------------------------------------
CDXLColDescr *
CTranslatorUtils::GetColumnDescrAt
	(
	IMemoryPool *memory_pool,
	TargetEntry *target_entry,
	ULONG col_id,
	ULONG ulPos
	)
{
	GPOS_ASSERT(NULL != target_entry);
	GPOS_ASSERT(gpos::ulong_max != col_id);

	CMDName *mdname = NULL;
	if (NULL == target_entry->resname)
	{
		CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
		mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, &strUnnamedCol);
	}
	else
	{
		CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, target_entry->resname);
		mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, pstrAlias);
		// CName constructor copies string
		GPOS_DELETE(pstrAlias);
	}

	// create a column descriptor
	OID oidType = gpdb::OidExprType((Node *) target_entry->expr);
	INT type_modifier = gpdb::IExprTypeMod((Node *) target_entry->expr);
	CMDIdGPDB *pmdidColType = GPOS_NEW(memory_pool) CMDIdGPDB(oidType);
	CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
									(
									memory_pool,
									mdname,
									col_id,
									ulPos, /* attno */
									pmdidColType,
									type_modifier, /* type_modifier */
									false /* fColDropped */
									);

	return dxl_col_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdxlnDummyPrElem
//
//	@doc:
//		Create a dummy project element to rename the input column identifier
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::PdxlnDummyPrElem
	(
	IMemoryPool *memory_pool,
	ULONG ulColIdInput,
	ULONG ulColIdOutput,
	CDXLColDescr *pdxlcdOutput
	)
{
	CMDIdGPDB *pmdidOriginal = CMDIdGPDB::CastMdid(pdxlcdOutput->MDIdType());
	CMDIdGPDB *pmdidCopy = GPOS_NEW(memory_pool) CMDIdGPDB(pmdidOriginal->OidObjectId(), pmdidOriginal->VersionMajor(), pmdidOriginal->VersionMinor());

	// create a column reference for the scalar identifier to be casted
	CMDName *mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, pdxlcdOutput->MdName()->GetMDName());
	CDXLColRef *dxl_colref = GPOS_NEW(memory_pool) CDXLColRef(memory_pool, mdname, ulColIdInput, pmdidCopy, pdxlcdOutput->TypeModifier());
	CDXLScalarIdent *pdxlopIdent = GPOS_NEW(memory_pool) CDXLScalarIdent(memory_pool, dxl_colref);

	CDXLNode *pdxlnPrEl = GPOS_NEW(memory_pool) CDXLNode
										(
										memory_pool,
										GPOS_NEW(memory_pool) CDXLScalarProjElem
													(
													memory_pool,
													ulColIdOutput,
													GPOS_NEW(memory_pool) CMDName(memory_pool, pdxlcdOutput->MdName()->GetMDName())
													),
										GPOS_NEW(memory_pool) CDXLNode(memory_pool, pdxlopIdent)
										);

	return pdxlnPrEl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetOutputColIdsArray
//
//	@doc:
//		Construct an array of colids for the given target list
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::GetOutputColIdsArray
	(
	IMemoryPool *memory_pool,
	List *target_list,
	IntUlongHashMap *phmiulAttnoColId
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != phmiulAttnoColId);

	ULongPtrArray *pdrgpul = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	ListCell *plcTE = NULL;
	ForEach (plcTE, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);
		ULONG ulResNo = (ULONG) target_entry->resno;
		INT iAttno = (INT) target_entry->resno;
		const ULONG *pul = phmiulAttnoColId->Find(&iAttno);

		if (NULL == pul)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLAttributeNotFound, ulResNo);
		}

		pdrgpul->Append(GPOS_NEW(memory_pool) ULONG(*pul));
	}

	return pdrgpul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColId
//
//	@doc:
//		Return the corresponding ColId for the given index into the target list
//
//---------------------------------------------------------------------------
ULONG
CTranslatorUtils::GetColId
	(
	INT iIndex,
	IntUlongHashMap *phmiulColId
	)
{
	GPOS_ASSERT(0 < iIndex);

	const ULONG *pul = phmiulColId->Find(&iIndex);

	if (NULL == pul)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLAttributeNotFound, iIndex);
	}

	return *pul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColId
//
//	@doc:
//		Return the corresponding ColId for the given varno, varattno and querylevel
//
//---------------------------------------------------------------------------
ULONG
CTranslatorUtils::GetColId
	(
	ULONG query_level,
	INT iVarno,
	INT iVarAttno,
	IMDId *pmdid,
	CMappingVarColId *var_col_id_mapping
	)
{
	OID oid = CMDIdGPDB::CastMdid(pmdid)->OidObjectId();
	Var *var = gpdb::PvarMakeVar(iVarno, iVarAttno, oid, -1, 0);
	ULONG col_id = var_col_id_mapping->GetColId(query_level, var, EpspotNone);
	gpdb::GPDBFree(var);

	return col_id;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PteWindowSpec
//
//	@doc:
//		Extract a matching target entry that is a window spec
//		
//---------------------------------------------------------------------------
TargetEntry *
CTranslatorUtils::PteWindowSpec
	(
	Node *pnode,
	List *plWindowClause,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != pnode);
	List *plTargetListSubset = gpdb::PteMembers(pnode, target_list);

	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetListSubset)
	{
		TargetEntry *pteCurr = (TargetEntry*) lfirst(plcTE);
		if (FWindowSpec(pteCurr, plWindowClause))
		{
			gpdb::GPDBFree(plTargetListSubset);
			return pteCurr;
		}
	}

	if (NIL != plTargetListSubset)
	{
		gpdb::GPDBFree(plTargetListSubset);
	}
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FWindowSpec
//
//	@doc:
//		Check if the expression has a matching target entry that is a window spec
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FWindowSpec
	(
	Node *pnode,
	List *plWindowClause,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != pnode);
	
	TargetEntry *pteWindoSpec = PteWindowSpec(pnode, plWindowClause, target_list);

	return (NULL != pteWindoSpec);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FWindowSpec
//
//	@doc:
//		Check if the TargetEntry is a used in the window specification
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FWindowSpec
	(
	const TargetEntry *target_entry,
	List *plWindowClause
	)
{
	ListCell *plcWindowCl;
	ForEach (plcWindowCl, plWindowClause)
	{
		WindowClause *pwc = (WindowClause *) lfirst(plcWindowCl);
		if (FSortingColumn(target_entry, pwc->orderClause) ||
		    FSortingColumn(target_entry, pwc->partitionClause))
		{
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdxlnInt8Const
//
//	@doc:
// 		Construct a scalar const value expression for the given BIGINT value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::PdxlnInt8Const
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	INT iVal
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	const IMDTypeInt8 *pmdtypeint8 = md_accessor->PtMDType<IMDTypeInt8>();
	pmdtypeint8->MDId()->AddRef();

	CDXLDatumInt8 *datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, pmdtypeint8->MDId(), false /*fConstNull*/, iVal);

	CDXLScalarConstValue *pdxlConst = GPOS_NEW(memory_pool) CDXLScalarConstValue(memory_pool, datum_dxl);

	return GPOS_NEW(memory_pool) CDXLNode(memory_pool, pdxlConst);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FSortingColumn
//
//	@doc:
//		Check if the TargetEntry is a sorting column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FSortingColumn
	(
	const TargetEntry *target_entry,
	List *plSortCl
	)
{
	ListCell *plcSortCl = NULL;
	ForEach (plcSortCl, plSortCl)
	{
		Node *pnodeSortCl = (Node*) lfirst(plcSortCl);
		if (IsA(pnodeSortCl, SortGroupClause) &&
		    target_entry->ressortgroupref == ((SortGroupClause *) pnodeSortCl)->tleSortGroupRef)
		{
			return true;
		}
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PteGroupingColumn
//
//	@doc:
//		Extract a matching target entry that is a grouping column
//---------------------------------------------------------------------------
TargetEntry *
CTranslatorUtils::PteGroupingColumn
	(
	Node *pnode,
	List *plGrpCl,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != pnode);
	List *plTargetListSubset = gpdb::PteMembers(pnode, target_list);

	ListCell *plcTE = NULL;
	ForEach (plcTE, plTargetListSubset)
	{
		TargetEntry *pteNext = (TargetEntry*) lfirst(plcTE);
		if (FGroupingColumn(pteNext, plGrpCl))
		{
			gpdb::GPDBFree(plTargetListSubset);
			return pteNext;
		}
	}

	if (NIL != plTargetListSubset)
	{
		gpdb::GPDBFree(plTargetListSubset);
	}
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FGroupingColumn
//
//	@doc:
//		Check if the expression has a matching target entry that is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FGroupingColumn
	(
	Node *pnode,
	List *plGrpCl,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != pnode);

	TargetEntry *pteGroupingCol = PteGroupingColumn(pnode, plGrpCl, target_list);

	return (NULL != pteGroupingCol);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FGroupingColumn
//
//	@doc:
//		Check if the TargetEntry is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FGroupingColumn
	(
	const TargetEntry *target_entry,
	List *plGrpCl
	)
{
	ListCell *plcGrpCl = NULL;
	ForEach (plcGrpCl, plGrpCl)
	{
		Node *pnodeGrpCl = (Node*) lfirst(plcGrpCl);

		if (NULL == pnodeGrpCl)
		{
			continue;
		}

		if (IsA(pnodeGrpCl, SortGroupClause) &&
		    FGroupingColumn(target_entry, (SortGroupClause*) pnodeGrpCl))
		{
			return true;
		}

		if (IsA(pnodeGrpCl, GroupingClause))
		{
			GroupingClause *pgrcl = (GroupingClause *) pnodeGrpCl;

			ListCell *plcGroupingSet = NULL;
			ForEach (plcGroupingSet, pgrcl->groupsets)
			{
				Node *pnodeGroupingSet = (Node *) lfirst(plcGroupingSet);

				if (IsA(pnodeGroupingSet, SortGroupClause) &&
				    FGroupingColumn(target_entry, ((SortGroupClause *) pnodeGroupingSet)))
				{
					return true;
				}

				if (IsA(pnodeGroupingSet, List) && FGroupingColumn(target_entry, (List *) pnodeGroupingSet))
				{
					return true;
				}
			}
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FGroupingColumn
//
//	@doc:
//		Check if the TargetEntry is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FGroupingColumn
	(
	const TargetEntry *target_entry,
	const SortGroupClause *pgrcl
	)
{
	GPOS_ASSERT(NULL != pgrcl);

	return (target_entry->ressortgroupref == pgrcl->tleSortGroupRef);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FGroupingColumn
//
//	@doc:
//		Check if the sorting column is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FGroupingColumn
	(
	const SortGroupClause *psortcl,
	List *plGrpCl
	)
{
	ListCell *plcGrpCl = NULL;
	ForEach (plcGrpCl, plGrpCl)
	{
		Node *pnodeGrpCl = (Node*) lfirst(plcGrpCl);
		GPOS_ASSERT(IsA(pnodeGrpCl, SortGroupClause) && "We currently do not support grouping sets.");

		SortGroupClause *pgrpcl = (SortGroupClause *) pnodeGrpCl;
		if (psortcl->tleSortGroupRef == pgrpcl->tleSortGroupRef)
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PlAttnosFromColids
//
//	@doc:
//		Translate an array of colids to a list of attribute numbers using
//		the mappings in the provided context
//---------------------------------------------------------------------------
List *
CTranslatorUtils::PlAttnosFromColids
	(
	ULongPtrArray *pdrgpul,
	CDXLTranslateContext *pdxltrctx
	)
{
	GPOS_ASSERT(NULL != pdrgpul);
	GPOS_ASSERT(NULL != pdxltrctx);
	
	List *plResult = NIL;
	
	const ULONG length = pdrgpul->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG col_id = *((*pdrgpul)[ul]);
		const TargetEntry *target_entry = pdxltrctx->GetTargetEntry(col_id);
		GPOS_ASSERT(NULL != target_entry);
		plResult = gpdb::PlAppendInt(plResult, target_entry->resno);
	}
	
	return plResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::LFromStr
//
//	@doc:
//		Parses a long integer value from a string
//
//---------------------------------------------------------------------------
LINT
CTranslatorUtils::LFromStr
	(
	const CWStringBase *str
	)
{
	CHAR *sz = CreateMultiByteCharStringFromWCString(str->GetBuffer());
	CHAR *pcEnd = NULL;
	return gpos::clib::Strtol(sz, &pcEnd, 10);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IFromStr
//
//	@doc:
//		Parses an integer value from a string
//
//---------------------------------------------------------------------------
INT
CTranslatorUtils::IFromStr
	(
	const CWStringBase *str
	)
{
	return (INT) LFromStr(str);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateMultiByteCharStringFromWCString
//
//	@doc:
//		Converts a wide character string into a character array
//
//---------------------------------------------------------------------------
CHAR *
CTranslatorUtils::CreateMultiByteCharStringFromWCString
	(
	const WCHAR *wsz
	)
{
	GPOS_ASSERT(NULL != wsz);

	ULONG ulMaxLength = GPOS_WSZ_LENGTH(wsz) * GPOS_SIZEOF(WCHAR) + 1;
	CHAR *sz = (CHAR *) gpdb::GPDBAlloc(ulMaxLength);
#ifdef GPOS_DEBUG
	LINT li = (INT)
#endif
	clib::Wcstombs(sz, const_cast<WCHAR *>(wsz), ulMaxLength);
	GPOS_ASSERT(0 <= li);

	sz[ulMaxLength - 1] = '\0';

	return sz;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::MakeNewToOldColMapping
//
//	@doc:
//		Create a mapping from old columns to the corresponding new column
//
//---------------------------------------------------------------------------
UlongUlongHashMap *
CTranslatorUtils::MakeNewToOldColMapping
	(
	IMemoryPool *memory_pool,
	ULongPtrArray *old_col_ids,
	ULongPtrArray *new_col_ids
	)
{
	GPOS_ASSERT(NULL != old_col_ids);
	GPOS_ASSERT(NULL != new_col_ids);
	GPOS_ASSERT(new_col_ids->Size() == old_col_ids->Size());
	
	UlongUlongHashMap *old_new_col_mapping = GPOS_NEW(memory_pool) UlongUlongHashMap(memory_pool);
	const ULONG ulCols = old_col_ids->Size();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		ULONG ulColIdOld = *((*old_col_ids)[ul]);
		ULONG ulColIdNew = *((*new_col_ids)[ul]);
#ifdef GPOS_DEBUG
		BOOL result = 
#endif // GPOS_DEBUG
		old_new_col_mapping->Insert(GPOS_NEW(memory_pool) ULONG(ulColIdOld), GPOS_NEW(memory_pool) ULONG(ulColIdNew));
		GPOS_ASSERT(result);
	}
	
	return old_new_col_mapping;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FDuplicateSensitiveMotion
//
//	@doc:
//		Is this a motion sensitive to duplicates
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FDuplicateSensitiveMotion
	(
	CDXLPhysicalMotion *pdxlopMotion
	)
{
	Edxlopid edxlopid = pdxlopMotion->GetDXLOperator();
	
	if (EdxlopPhysicalMotionRedistribute == edxlopid)
	{
		return CDXLPhysicalRedistributeMotion::Cast(pdxlopMotion)->IsDuplicateSensitive();
	}
	
	if (EdxlopPhysicalMotionRandom == edxlopid)
	{
		return CDXLPhysicalRandomMotion::Cast(pdxlopMotion)->IsDuplicateSensitive();
	}
	
	// other motion operators are not sensitive to duplicates
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FHasProjElem
//
//	@doc:
//		Check whether the given project list has a project element of the given
//		operator type
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FHasProjElem
	(
	CDXLNode *project_list_dxl,
	Edxlopid edxlopid
	)
{
	GPOS_ASSERT(NULL != project_list_dxl);
	GPOS_ASSERT(EdxlopScalarProjectList == project_list_dxl->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopSentinel > edxlopid);

	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnPrEl = (*project_list_dxl)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == pdxlnPrEl->GetOperator()->GetDXLOperator());

		CDXLNode *pdxlnChild = (*pdxlnPrEl)[0];
		if (edxlopid == pdxlnChild->GetOperator()->GetDXLOperator())
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdxlnPrElNull
//
//	@doc:
//		Create a DXL project element node with a Const NULL of type provided
//		by the column descriptor. The function raises an exception if the
//		column is not nullable.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::PdxlnPrElNull
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *pidgtorCol,
	const IMDColumn *pmdcol
	)
{
	GPOS_ASSERT(NULL != pmdcol);
	GPOS_ASSERT(!pmdcol->IsSystemColumn());

	const WCHAR *wszColName = pmdcol->Mdname().GetMDName()->GetBuffer();
	if (!pmdcol->IsNullable())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLNotNullViolation, wszColName);
	}

	ULONG col_id = pidgtorCol->next_id();
	CDXLNode *pdxlnPrE = PdxlnPrElNull(memory_pool, md_accessor, pmdcol->MDIdType(), col_id, wszColName);

	return pdxlnPrE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdxlnPrElNull
//
//	@doc:
//		Create a DXL project element node with a Const NULL expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::PdxlnPrElNull
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *pmdid,
	ULONG col_id,
	const WCHAR *wszColName
	)
{
	CHAR *szColumnName = CDXLUtils::CreateMultiByteCharStringFromWCString(memory_pool, wszColName);
	CDXLNode *pdxlnPrE = PdxlnPrElNull(memory_pool, md_accessor, pmdid, col_id, szColumnName);

	GPOS_DELETE_ARRAY(szColumnName);

	return pdxlnPrE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PdxlnPrElNull
//
//	@doc:
//		Create a DXL project element node with a Const NULL expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::PdxlnPrElNull
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *pmdid,
	ULONG col_id,
	CHAR *szAliasName
	)
{
	BOOL is_passed_by_value = md_accessor->Pmdtype(pmdid)->IsPassedByValue();

	// get the id and alias for the proj elem
	CMDName *pmdnameAlias = NULL;

	if (NULL == szAliasName)
	{
		CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
		pmdnameAlias = GPOS_NEW(memory_pool) CMDName(memory_pool, &strUnnamedCol);
	}
	else
	{
		CWStringDynamic *pstrAlias = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, szAliasName);
		pmdnameAlias = GPOS_NEW(memory_pool) CMDName(memory_pool, pstrAlias);
		GPOS_DELETE(pstrAlias);
	}

	pmdid->AddRef();
	CDXLDatum *datum_dxl = NULL;
	if (pmdid->Equals(&CMDIdGPDB::m_mdid_int2))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt2(memory_pool, pmdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_int4))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt4(memory_pool, pmdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_int8))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, pmdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_bool))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumBool(memory_pool, pmdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_oid))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumOid(memory_pool, pmdid, true /*fConstNull*/, 0 /*value*/);
	}
	else
	{
		datum_dxl = CMDTypeGenericGPDB::CreateDXLDatumVal
										(
										memory_pool,
										pmdid,
										default_type_modifier,
										is_passed_by_value /*fConstByVal*/,
										true /*fConstNull*/,
										NULL, /*pba */
										0 /*length*/,
										0 /*lValue*/,
										0 /*dValue*/
										);
	}

	CDXLNode *pdxlnConst = GPOS_NEW(memory_pool) CDXLNode(memory_pool, GPOS_NEW(memory_pool) CDXLScalarConstValue(memory_pool, datum_dxl));

	return GPOS_NEW(memory_pool) CDXLNode(memory_pool, GPOS_NEW(memory_pool) CDXLScalarProjElem(memory_pool, col_id, pmdnameAlias), pdxlnConst);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CheckRTEPermissions
//
//	@doc:
//		Check permissions on range table
//
//---------------------------------------------------------------------------
void
CTranslatorUtils::CheckRTEPermissions
	(
	List *plRangeTable
	)
{
	gpdb::CheckRTPermissions(plRangeTable);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CheckAggregateWindowFn
//
//	@doc:
//		Check if the window function is an aggregate and has either
//      prelim or inverse prelim function
//
//---------------------------------------------------------------------------
void
CTranslatorUtils::CheckAggregateWindowFn
	(
	Node *pnode
	)
{
	GPOS_ASSERT(NULL != pnode);
	GPOS_ASSERT(IsA(pnode, WindowFunc));

	WindowFunc *pwinfunc = (WindowFunc *) pnode;

	if (gpdb::FAggregateExists(pwinfunc->winfnoid) && !gpdb::FAggHasPrelimOrInvPrelimFunc(pwinfunc->winfnoid))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
				GPOS_WSZ_LIT("Aggregate window function without prelim or inverse prelim function"));
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::UpdateGrpColMapping
//
//	@doc:
//		Update grouping columns permission mappings
//
//---------------------------------------------------------------------------
void
CTranslatorUtils::UpdateGrpColMapping
	(
	IMemoryPool *memory_pool,
	UlongUlongHashMap *phmululGrpColPos, 
	CBitSet *pbsGrpCols,
	ULONG ulSortGrpRef
	)
{
	GPOS_ASSERT(NULL != phmululGrpColPos);
	GPOS_ASSERT(NULL != pbsGrpCols);
		
	if (!pbsGrpCols->Get(ulSortGrpRef))
	{
		ULONG ulUniqueGrpCols = pbsGrpCols->Size();
		phmululGrpColPos->Insert(GPOS_NEW(memory_pool) ULONG (ulUniqueGrpCols), GPOS_NEW(memory_pool) ULONG(ulSortGrpRef));
		(void) pbsGrpCols->ExchangeSet(ulSortGrpRef);
	}
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorUtils::MarkOuterRefs
//
//      @doc:
//		check if given column ids are outer refs in the tree rooted by 
//		given node
//---------------------------------------------------------------------------
void
CTranslatorUtils::MarkOuterRefs
	(
	ULONG *pulColId,  // array of column ids to be checked
	BOOL *pfOuterRef,  // array of outer ref indicators, initially all set to true by caller 
	ULONG ulColumns,  // number of columns
	CDXLNode *dxlnode
	)
{
	GPOS_ASSERT(NULL != pulColId);
	GPOS_ASSERT(NULL != pfOuterRef);
	GPOS_ASSERT(NULL != dxlnode);
	
	const CDXLOperator *pdxlop = dxlnode->GetOperator();
	for (ULONG ulCol = 0; ulCol < ulColumns; ulCol++)
	{
		ULONG col_id = pulColId[ulCol];
		if (pfOuterRef[ulCol] && pdxlop->IsColDefined(col_id))
		{
			// column is defined by operator, reset outer reference flag
			pfOuterRef[ulCol] = false;
		}
	}

	// recursively process children
	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		MarkOuterRefs(pulColId, pfOuterRef, ulColumns, (*dxlnode)[ul]);
	}
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorUtils::Slink
//
//      @doc:
//              Map DXL Subplan type to GPDB SubLinkType
//
//---------------------------------------------------------------------------
SubLinkType
CTranslatorUtils::Slink
        (
        EdxlSubPlanType dxl_subplan_type
        )
{
        GPOS_ASSERT(EdxlSubPlanTypeSentinel > dxl_subplan_type);
        ULONG rgrgulMapping[][2] =
                {
                {EdxlSubPlanTypeScalar, EXPR_SUBLINK},
                {EdxlSubPlanTypeExists, EXISTS_SUBLINK},
                {EdxlSubPlanTypeNotExists, NOT_EXISTS_SUBLINK},
                {EdxlSubPlanTypeAny, ANY_SUBLINK},
                {EdxlSubPlanTypeAll, ALL_SUBLINK}
                };

        const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
        SubLinkType slink = EXPR_SUBLINK;
	BOOL fFound = false;
        for (ULONG ul = 0; ul < arity; ul++)
        {
                ULONG *pulElem = rgrgulMapping[ul];
                if ((ULONG) dxl_subplan_type == pulElem[0])
                {
                        slink = (SubLinkType) pulElem[1];
                        fFound = true;
			break;
                }
        }

	GPOS_ASSERT(fFound && "Invalid SubPlanType");

        return slink;
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorUtils::Edxlsubplantype
//
//      @doc:
//              Map GPDB SubLinkType to DXL subplan type
//
//---------------------------------------------------------------------------
EdxlSubPlanType
CTranslatorUtils::Edxlsubplantype
        (
        SubLinkType slink
        )
{
        ULONG rgrgulMapping[][2] =
                {
                {EXPR_SUBLINK, EdxlSubPlanTypeScalar},
                {EXISTS_SUBLINK , EdxlSubPlanTypeExists},
                {NOT_EXISTS_SUBLINK, EdxlSubPlanTypeNotExists},
                {ANY_SUBLINK, EdxlSubPlanTypeAny},
                {ALL_SUBLINK, EdxlSubPlanTypeAll}
                };

        const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
        EdxlSubPlanType dxl_subplan_type = EdxlSubPlanTypeScalar;
	BOOL fFound = false;
        for (ULONG ul = 0; ul < arity; ul++)
        {
                ULONG *pulElem = rgrgulMapping[ul];
                if ((ULONG) slink == pulElem[0])
                {
                        dxl_subplan_type = (EdxlSubPlanType) pulElem[1];
                        fFound = true;
			break;
                }
        }

	 GPOS_ASSERT(fFound && "Invalid SubLinkType");

        return dxl_subplan_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FRelHasTriggers
//
//	@doc:
//		Check whether there are triggers for the given operation on
//		the given relation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FRelHasTriggers
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	const IMDRelation *pmdrel,
	const EdxlDmlType dml_type_dxl
	)
{
	const ULONG ulTriggers = pmdrel->TriggerCount();
	for (ULONG ul = 0; ul < ulTriggers; ul++)
	{
		if (FApplicableTrigger(md_accessor, pmdrel->TriggerMDidAt(ul), dml_type_dxl))
		{
			return true;
		}
	}

	// if table is partitioned, check for triggers on child partitions as well
	INT iType = 0;
	if (Edxldmlinsert == dml_type_dxl)
	{
		iType = TRIGGER_TYPE_INSERT;
	}
	else if (Edxldmldelete == dml_type_dxl)
	{
		iType = TRIGGER_TYPE_DELETE;
	}
	else
	{
		GPOS_ASSERT(Edxldmlupdate == dml_type_dxl);
		iType = TRIGGER_TYPE_UPDATE;
	}

	OID oidRel = CMDIdGPDB::CastMdid(pmdrel->MDId())->OidObjectId();
	return gpdb::FChildTriggers(oidRel, iType);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FApplicableTrigger
//
//	@doc:
//		Check whether the given trigger is applicable to the given DML operation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FApplicableTrigger
	(
	CMDAccessor *md_accessor,
	IMDId *pmdidTrigger,
	const EdxlDmlType dml_type_dxl
	)
{
	const IMDTrigger *pmdtrigger = md_accessor->Pmdtrigger(pmdidTrigger);
	if (!pmdtrigger->IsEnabled())
	{
		return false;
	}

	return ((Edxldmlinsert == dml_type_dxl && pmdtrigger->IsInsert()) ||
			(Edxldmldelete == dml_type_dxl && pmdtrigger->IsDelete()) ||
			(Edxldmlupdate == dml_type_dxl && pmdtrigger->IsUpdate()));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FRelHasConstraints
//
//	@doc:
//		Check whether there are constraints for the given relation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::FRelHasConstraints
	(
	const IMDRelation *pmdrel
	)
{
	if (0 < pmdrel->CheckConstraintCount())
	{
		return true;
	}
	
	const ULONG ulCols = pmdrel->ColumnCount();
	
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		if (!pmdcol->IsSystemColumn() && !pmdcol->IsNullable())
		{
			return true;
		}
	}
	
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::PlAssertErrorMsgs
//
//	@doc:
//		Construct a list of error messages from a list of assert constraints 
//
//---------------------------------------------------------------------------
List *
CTranslatorUtils::PlAssertErrorMsgs
	(
	CDXLNode *pdxlnAssertConstraintList
	)
{
	GPOS_ASSERT(NULL != pdxlnAssertConstraintList);
	GPOS_ASSERT(EdxlopScalarAssertConstraintList == pdxlnAssertConstraintList->GetOperator()->GetDXLOperator());
	
	List *plErrorMsgs = NIL;
	const ULONG ulConstraints = pdxlnAssertConstraintList->Arity();
	
	for (ULONG ul = 0; ul < ulConstraints; ul++)
	{
		CDXLNode *pdxlnConstraint = (*pdxlnAssertConstraintList)[ul];
		CDXLScalarAssertConstraint *pdxlopConstraint = CDXLScalarAssertConstraint::Cast(pdxlnConstraint->GetOperator());
		CWStringBase *pstrErrorMsg = pdxlopConstraint->GetErrorMsgStr();
		plErrorMsgs = gpdb::PlAppendElement(plErrorMsgs, gpdb::PvalMakeString(CreateMultiByteCharStringFromWCString(pstrErrorMsg->GetBuffer())));
	}
	
	return plErrorMsgs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::UlNonSystemColumns
//
//	@doc:
//		Return the count of non-system columns in the relation
//
//---------------------------------------------------------------------------
ULONG
CTranslatorUtils::UlNonSystemColumns
	(
	const IMDRelation *pmdrel
	)
{
	GPOS_ASSERT(NULL != pmdrel);

	ULONG ulNonSystemCols = 0;

	const ULONG ulCols = pmdrel->ColumnCount();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		const IMDColumn *pmdcol  = pmdrel->GetMdCol(ul);

		if (!pmdcol->IsSystemColumn())
		{
			ulNonSystemCols++;
		}
	}

	return ulNonSystemCols;
}

// Function to check if we should create stats bucket in DXL
// Returns true if column datatype is not text/char/varchar/bpchar
BOOL
CTranslatorUtils::FCreateStatsBucket
	(
	OID oidAttType
	)
{
	if (oidAttType != TEXTOID && oidAttType != CHAROID && oidAttType != VARCHAROID && oidAttType != BPCHAROID)
		return true;

	return false;
}

// EOF
