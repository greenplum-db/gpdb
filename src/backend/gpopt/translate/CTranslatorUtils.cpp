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
	IMDId *mdid
	)
{
	const IMDIndex *index = md_accessor->Pmdindex(mdid);
	const CWStringConst *index_name = index->Mdname().GetMDName();
	CMDName *index_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, index_name);

	return GPOS_NEW(memory_pool) CDXLIndexDescr(memory_pool, mdid, index_mdname);
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
	const RangeTblEntry *rte,
	BOOL *is_distributed_table // output
	)
{
	// generate an MDId for the table desc.
	OID rel_oid = rte->relid;

	if (gpdb::HasExternalPartition(rel_oid))
	{
		// fall back to the planner for queries with partition tables that has an external table in one of its leaf
		// partitions.
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Query over external partitions"));
	}

	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(rel_oid);

	const IMDRelation *rel = md_accessor->Pmdrel(mdid);
	
	// look up table name
	const CWStringConst *tablename = rel->Mdname().GetMDName();
	CMDName *table_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, tablename);

	CDXLTableDescr *table_descr = GPOS_NEW(memory_pool) CDXLTableDescr(memory_pool, mdid, table_mdname, rte->checkAsUser);

	const ULONG len = rel->ColumnCount();

	IMDRelation::Ereldistrpolicy distribution_policy = rel->GetRelDistribution();

	if (NULL != is_distributed_table &&
		(IMDRelation::EreldistrHash == distribution_policy || IMDRelation::EreldistrRandom == distribution_policy))
	{
		*is_distributed_table = true;
	}
	else if (!optimizer_enable_master_only_queries && (IMDRelation::EreldistrMasterOnly == distribution_policy))
		{
			// fall back to the planner for queries on master-only table if they are disabled with Orca. This is due to
			// the fact that catalog tables (master-only) are not analyzed often and will result in Orca producing
			// inferior plans.

			GPOS_THROW_EXCEPTION(gpdxl::ExmaDXL, // major
								 gpdxl::ExmiQuery2DXLUnsupportedFeature, // minor
								 CException::ExsevDebug1, // ulSeverityLevel mapped to GPDB severity level
								 GPOS_WSZ_LIT("Queries on master-only tables"));
		}

	// add columns from md cache relation object to table descriptor
	for (ULONG ul = 0; ul < len; ul++)
	{
		const IMDColumn *md_col = rel->GetMdCol(ul);
		if (md_col->IsDropped())
		{
			continue;
		}
		
		CMDName *col = GPOS_NEW(memory_pool) CMDName(memory_pool, md_col->Mdname().GetMDName());
		CMDIdGPDB *col_type = CMDIdGPDB::CastMdid(md_col->MDIdType());
		col_type->AddRef();

		// create a column descriptor for the column
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
											(
											memory_pool,
											col,
											id_generator->next_id(),
											md_col->AttrNum(),
											col_type,
											md_col->TypeModifier(), /* type_modifier */
											false, /* fColDropped */
											md_col->Length()
											);
		table_descr->AddColumnDescr(dxl_col_descr);
	}

	return table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsSirvFunc
//
//	@doc:
//		Check if the given function is a SIRV (single row volatile) that reads
//		or modifies SQL data
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsSirvFunc
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	OID func_oid
	)
{
	// we exempt the following 3 functions to avoid falling back to the planner
	// for DML on tables with sequences. The same exemption is also in the planner
	if (GPDB_NEXTVAL == func_oid ||
		GPDB_CURRVAL == func_oid ||
		GPDB_SETVAL == func_oid)
	{
		return false;
	}

	CMDIdGPDB *mdid_func = GPOS_NEW(memory_pool) CMDIdGPDB(func_oid);
	const IMDFunction *func = md_accessor->Pmdfunc(mdid_func);

	BOOL is_sirv = (!func->ReturnsSet() &&
				  IMDFunction::EfsVolatile == func->GetFuncStability());

	mdid_func->Release();

	return is_sirv;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::HasSubquery
//
//	@doc:
//		Check if the given tree contains a subquery
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::HasSubquery
	(
	Node *node
	)
{
	List *unsupported_list = ListMake1Int(T_SubLink);
	INT unsupported = gpdb::FindNodes(node, unsupported_list);
	gpdb::GPDBFree(unsupported_list);

	return (0 <= unsupported);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::ConvertToCDXLLogicalTVF
//
//	@doc:
//		Create a DXL logical TVF from a GPDB range table entry
//
//---------------------------------------------------------------------------
CDXLLogicalTVF *
CTranslatorUtils::ConvertToCDXLLogicalTVF
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *id_generator,
	const RangeTblEntry *rte
	)
{
	GPOS_ASSERT(NULL != rte->funcexpr);
	FuncExpr *funcexpr = (FuncExpr *)rte->funcexpr;

	// get function id
	CMDIdGPDB *mdid_func = GPOS_NEW(memory_pool) CMDIdGPDB(funcexpr->funcid);
	CMDIdGPDB *mdid_return_type =  GPOS_NEW(memory_pool) CMDIdGPDB(funcexpr->funcresulttype);
	const IMDType *type = md_accessor->Pmdtype(mdid_return_type);

	// In the planner, scalar functions that are volatile (SIRV) or read or modify SQL
	// data get patched into an InitPlan. This is not supported in the optimizer
	if (IsSirvFunc(memory_pool, md_accessor, funcexpr->funcid))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("SIRV functions"));
	}

	// get function from MDcache
	const IMDFunction *func = md_accessor->Pmdfunc(mdid_func);

	MdidPtrArray *out_arg_types = func->OutputArgTypesMdidArray();

	ColumnDescrDXLArray *column_descrs = NULL;

	if (NULL != rte->funccoltypes)
	{
		// function returns record - use col names and types from query
		column_descrs = GetColumnDescriptorsFromRecord(memory_pool, id_generator, rte->eref->colnames, rte->funccoltypes, rte->funccoltypmods);
	}
	else if (type->IsComposite() && IMDId::IsValid(type->GetBaseRelMdid()))
	{
		// function returns a "table" type or a user defined type
		column_descrs = GetColumnDescriptorsFromComposite(memory_pool, md_accessor, id_generator, type);
	}
	else if (NULL != out_arg_types)
	{
		// function returns record - but output col types are defined in catalog
		out_arg_types->AddRef();
		if (ContainsPolymorphicTypes(out_arg_types))
		{
			// resolve polymorphic types (anyelement/anyarray) using the
			// argument types from the query
			List *arg_types = gpdb::GetFuncArgTypes(funcexpr->funcid);
			MdidPtrArray *resolved_types = ResolvePolymorphicTypes
												(
												memory_pool,
												out_arg_types,
												arg_types,
												funcexpr
												);
			out_arg_types->Release();
			out_arg_types = resolved_types;
		}

		column_descrs = GetColumnDescriptorsFromRecord(memory_pool, id_generator, rte->eref->colnames, out_arg_types);
		out_arg_types->Release();
	}
	else
	{
		// function returns base type
		CMDName func_mdname = func->Mdname();
		// table valued functions don't describe the returned column type modifiers, hence the -1
		column_descrs = GetColumnDescriptorsFromBase(memory_pool, id_generator, mdid_return_type, default_type_modifier, &func_mdname);
	}

	CMDName *pmdfuncname = GPOS_NEW(memory_pool) CMDName(memory_pool, func->Mdname().GetMDName());

	CDXLLogicalTVF *tvf_dxl = GPOS_NEW(memory_pool) CDXLLogicalTVF(memory_pool, mdid_func, mdid_return_type, pmdfuncname, column_descrs);

	return tvf_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::ResolvePolymorphicTypes
//
//	@doc:
//		Resolve polymorphic types in the given array of type ids, replacing
//		them with the actual types obtained from the query
//
//---------------------------------------------------------------------------
MdidPtrArray *
CTranslatorUtils::ResolvePolymorphicTypes
	(
	IMemoryPool *memory_pool,
	MdidPtrArray *mdid_array,
	List *arg_types_list,
	FuncExpr *funcexpr
	)
{
	ULONG arg_index = 0;

	const ULONG num_arg_types = gpdb::ListLength(arg_types_list);
	const ULONG num_args_from_query = gpdb::ListLength(funcexpr->args);
	const ULONG num_return_args = mdid_array->Size();
	const ULONG num_args = num_arg_types < num_args_from_query ? num_arg_types : num_args_from_query;
	const ULONG total_args = num_args + num_return_args;

	OID arg_types[num_args];
	char arg_modes[total_args];

	// copy function argument types
	ListCell *arg_type = NULL;
	ForEach (arg_type, arg_types_list)
	{
		arg_types[arg_index] = lfirst_oid(arg_type);
		arg_modes[arg_index++] = PROARGMODE_IN;
	}

	// copy function return types
	for (ULONG ul = 0; ul < num_return_args; ul++)
	{
		IMDId *mdid = (*mdid_array)[ul];
		arg_types[arg_index] = CMDIdGPDB::CastMdid(mdid)->OidObjectId();
		arg_modes[arg_index++] = PROARGMODE_TABLE;
	}

	if(!gpdb::ResolvePolymorphicArgType(total_args, arg_types, arg_modes, funcexpr))
	{
		GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiDXLUnrecognizedType,
				GPOS_WSZ_LIT("could not determine actual argument/return type for polymorphic function")
				);
	}

	// generate a new array of mdids based on the resolved types
	MdidPtrArray *resolved_types = GPOS_NEW(memory_pool) MdidPtrArray(memory_pool);

	// get the resolved return types
	for (ULONG ul = num_args; ul < total_args ; ul++)
	{

		IMDId *resolved_mdid = NULL;
		resolved_mdid = GPOS_NEW(memory_pool) CMDIdGPDB(arg_types[ul]);
		resolved_types->Append(resolved_mdid);
	}

	return resolved_types;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::ContainsPolymorphicTypes
//
//	@doc:
//		Check if the given mdid array contains any of the polymorphic
//		types (ANYELEMENT, ANYARRAY, ANYENUM, ANYNONARRAY)
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::ContainsPolymorphicTypes
	(
	MdidPtrArray *mdid_array
	)
{
	GPOS_ASSERT(NULL != mdid_array);
	const ULONG len = mdid_array->Size();
	for (ULONG ul = 0; ul < len; ul++)
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
//		CTranslatorUtils::GetColumnDescriptorsFromRecord
//
//	@doc:
//		Get column descriptors from a record type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::GetColumnDescriptorsFromRecord
	(
	IMemoryPool *memory_pool,
	CIdGenerator *id_generator,
	List *col_names,
	List *col_types,
	List *col_type_modifiers
	)
{
	ListCell *col_name = NULL;
	ListCell *col_type = NULL;
	ListCell *col_type_modifier = NULL;

	ULONG ul = 0;
	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	ForThree (col_name, col_names,
			col_type, col_types,
			col_type_modifier, col_type_modifiers)
	{
		Value *value = (Value *) lfirst(col_name);
		Oid coltype = lfirst_oid(col_type);
		INT type_modifier = lfirst_int(col_type_modifier);

		CHAR *col_name_char_array = strVal(value);
		CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, col_name_char_array);
		CMDName *col_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, column_name);
		GPOS_DELETE(column_name);

		IMDId *col_type = GPOS_NEW(memory_pool) CMDIdGPDB(coltype);

		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										col_mdname,
										id_generator->next_id(),
										INT(ul + 1) /* attno */,
										col_type,
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
//		CTranslatorUtils::GetColumnDescriptorsFromRecord
//
//	@doc:
//		Get column descriptors from a record type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::GetColumnDescriptorsFromRecord
	(
	IMemoryPool *memory_pool,
	CIdGenerator *id_generator,
	List *col_names,
	MdidPtrArray *out_arg_types
	)
{
	GPOS_ASSERT(out_arg_types->Size() == (ULONG) gpdb::ListLength(col_names));
	ListCell *col_name = NULL;

	ULONG ul = 0;
	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	ForEach (col_name, col_names)
	{
		Value *value = (Value *) lfirst(col_name);

		CHAR *col_name_char_array = strVal(value);
		CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, col_name_char_array);
		CMDName *col_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, column_name);
		GPOS_DELETE(column_name);

		IMDId *col_type = (*out_arg_types)[ul];
		col_type->AddRef();

		// This function is only called to construct column descriptors for table-valued functions
		// which won't have type modifiers for columns of the returned table
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										col_mdname,
										id_generator->next_id(),
										INT(ul + 1) /* attno */,
										col_type,
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
//		CTranslatorUtils::GetColumnDescriptorsFromBase
//
//	@doc:
//		Get column descriptor from a base type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::GetColumnDescriptorsFromBase
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
	CMDName *col_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, pmdName->GetMDName());

	CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
									(
									memory_pool,
									col_mdname,
									id_generator->next_id(),
									INT(1) /* attno */,
									mdid_return_type,
									type_modifier, /* type_modifier */
									false /* fColDropped */
									);

	column_descrs->Append(dxl_col_descr);

	return column_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColumnDescriptorsFromComposite
//
//	@doc:
//		Get column descriptors from a composite type
//
//---------------------------------------------------------------------------
ColumnDescrDXLArray *
CTranslatorUtils::GetColumnDescriptorsFromComposite
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *id_generator,
	const IMDType *type
	)
{
	MDColumnPtrArray *col_ptr_arr = ExpandCompositeType(memory_pool, md_accessor, type);

	ColumnDescrDXLArray *column_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);

	for (ULONG ul = 0; ul < col_ptr_arr->Size(); ul++)
	{
		IMDColumn *md_col = (*col_ptr_arr)[ul];

		CMDName *col_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, md_col->Mdname().GetMDName());
		IMDId *col_type = md_col->MDIdType();

		col_type->AddRef();
		CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
										(
										memory_pool,
										col_mdname,
										id_generator->next_id(),
										INT(ul + 1) /* attno */,
										col_type,
										md_col->TypeModifier(), /* type_modifier */
										false /* fColDropped */
										);
		column_descrs->Append(dxl_col_descr);
	}

	col_ptr_arr->Release();

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
	const IMDType *type
	)
{
	GPOS_ASSERT(NULL != type);
	GPOS_ASSERT(type->IsComposite());

	IMDId *rel_mdid = type->GetBaseRelMdid();
	const IMDRelation *rel = md_accessor->Pmdrel(rel_mdid);
	GPOS_ASSERT(NULL != rel);

	MDColumnPtrArray *pdrgPmdcol = GPOS_NEW(memory_pool) MDColumnPtrArray(memory_pool);

	for(ULONG ul = 0; ul < rel->ColumnCount(); ul++)
	{
		CMDColumn *md_col = (CMDColumn *) rel->GetMdCol(ul);

		if (!md_col->IsSystemColumn())
		{
			md_col->AddRef();
			pdrgPmdcol->Append(md_col);
		}
	}

	return pdrgPmdcol;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::ConvertToDXLJoinType
//
//	@doc:
//		Translates the join type from its GPDB representation into the DXL one
//
//---------------------------------------------------------------------------
EdxlJoinType
CTranslatorUtils::ConvertToDXLJoinType
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
//		CTranslatorUtils::ConvertToDXLIndexScanDirection
//
//	@doc:
//		Translates the DXL index scan direction from GPDB representation
//
//---------------------------------------------------------------------------
EdxlIndexScanDirection
CTranslatorUtils::ConvertToDXLIndexScanDirection
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
	ULONG pos
	)
{
	GPOS_ASSERT(0 != pos);
	GPOS_ASSERT(pos < table_descr->Arity());

	return table_descr->GetColumnDescrAt(pos);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetSystemColName
//
//	@doc:
//		Return the name for the system attribute with the given attribute number.
//
//---------------------------------------------------------------------------
const CWStringConst *
CTranslatorUtils::GetSystemColName
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
//		CTranslatorUtils::GetSystemColType
//
//	@doc:
//		Return the type id for the system attribute with the given attribute number.
//
//---------------------------------------------------------------------------
CMDIdGPDB *
CTranslatorUtils::GetSystemColType
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
CTranslatorUtils::GetSystemColLength
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
//		CTranslatorUtils::GetScanDirection
//
//	@doc:
//		Return the GPDB specific scan direction from its corresponding DXL
//		representation
//
//---------------------------------------------------------------------------
ScanDirection
CTranslatorUtils::GetScanDirection
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
	Expr* expr
	)
{
	GPOS_ASSERT(IsA(expr, OpExpr) || IsA(expr, ScalarArrayOpExpr) || IsA(expr, RowCompareExpr));

	switch (expr->type)
	{
		case T_OpExpr:
			return ((OpExpr *) expr)->opno;
			
		case T_ScalarArrayOpExpr:
			return ((ScalarArrayOpExpr*) expr)->opno;

		case T_RowCompareExpr:
			return LInitialOID(((RowCompareExpr *) expr)->opnos);
			
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
//		CTranslatorUtils::GetOpFamilyForIndexQual
//
//	@doc:
//		Extract comparison operator family for the given index column
//
//---------------------------------------------------------------------------
OID
CTranslatorUtils::GetOpFamilyForIndexQual
	(
	INT attno,
	OID index_oid
	)
{
	Relation rel = gpdb::GetRelation(index_oid);
	GPOS_ASSERT(NULL != rel);
	GPOS_ASSERT(attno <= rel->rd_index->indnatts);
	
	OID op_family_oid = rel->rd_opfamily[attno - 1];
	gpdb::CloseRelation(rel);
	
	return op_family_oid;
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
	BOOL is_all
	)
{
	if (SETOP_UNION == setop && is_all)
	{
		return EdxlsetopUnionAll;
	}

	if (SETOP_INTERSECT == setop && is_all)
	{
		return EdxlsetopIntersectAll;
	}

	if (SETOP_EXCEPT == setop && is_all)
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
	CBitSet *group_by_cols,
	IntUlongHashMap *sort_group_cols_to_col_id_map
	)
{
	ULongPtrArray *col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	if (NULL != group_by_cols)
	{
		CBitSetIter bsi(*group_by_cols);

		while (bsi.Advance())
		{
			const ULONG col_id = GetColId(bsi.Bit(), sort_group_cols_to_col_id_map);
			col_ids->Append(GPOS_NEW(memory_pool) ULONG(col_id));
		}
	}

	return col_ids;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetColumnAttnosForGroupBy
//
//	@doc:
//		Construct a dynamic array of sets of column attnos corresponding to the
// 		group by clause
//
//---------------------------------------------------------------------------
BitSetArray *
CTranslatorUtils::GetColumnAttnosForGroupBy
	(
	IMemoryPool *memory_pool,
	List *group_clause_list,
	ULONG num_cols,
	UlongUlongHashMap *group_col_pos,	// mapping of grouping col positions to SortGroupRef ids
	CBitSet *group_cols			// existing uniqueue grouping columns
	)
{
	GPOS_ASSERT(NULL != group_clause_list);
	GPOS_ASSERT(0 < gpdb::ListLength(group_clause_list));
	GPOS_ASSERT(NULL != group_col_pos);

	Node *node = (Node*) LInitial(group_clause_list);

	if (NULL == node || IsA(node, SortGroupClause))
	{
		// simple group by
		CBitSet *col_attnos = CreateAttnoSetForGroupingSet(memory_pool, group_clause_list, num_cols, group_col_pos, group_cols);
		BitSetArray *col_attnos_arr = GPOS_NEW(memory_pool) BitSetArray(memory_pool);
		col_attnos_arr->Append(col_attnos);
		return col_attnos_arr;
	}

	if (!IsA(node, GroupingClause))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Group by clause"));
	}

	const ULONG num_group_clauses = gpdb::ListLength(group_clause_list);
	GPOS_ASSERT(0 < num_group_clauses);
	if (1 < num_group_clauses)
	{
		// multiple grouping sets
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple grouping sets specifications"));
	}

	GroupingClause *grouping_clause = (GroupingClause *) node;

	if (GROUPINGTYPE_ROLLUP == grouping_clause->groupType)
	{
		return CreateGroupingSetsForRollup(memory_pool, grouping_clause, num_cols, group_col_pos, group_cols);
	}

	if (GROUPINGTYPE_CUBE == grouping_clause->groupType)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Cube"));
	}

	BitSetArray *col_attnos_arr = GPOS_NEW(memory_pool) BitSetArray(memory_pool);

	ListCell *grouping_set = NULL;
	ForEach (grouping_set, grouping_clause->groupsets)
	{
		Node *grouping_set_node = (Node *) lfirst(grouping_set);

		CBitSet *bs = NULL;
		if (IsA(grouping_set_node, SortGroupClause))
		{
			// grouping set contains a single grouping column
			bs = GPOS_NEW(memory_pool) CBitSet(memory_pool, num_cols);
			ULONG sort_group_ref = ((SortGroupClause *) grouping_set_node)->tleSortGroupRef;
			bs->ExchangeSet(sort_group_ref);
			UpdateGrpColMapping(memory_pool, group_col_pos, group_cols, sort_group_ref);
		}
		else if (IsA(grouping_set_node, GroupingClause))
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Multiple grouping sets specifications"));
		}
		else
		{
			// grouping set contains a list of columns
			GPOS_ASSERT(IsA(grouping_set_node, List));

			List *grouping_set_list = (List *) grouping_set_node;
			bs = CreateAttnoSetForGroupingSet(memory_pool, grouping_set_list, num_cols, group_col_pos, group_cols);
		}
		col_attnos_arr->Append(bs);
	}

	return col_attnos_arr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateGroupingSetsForRollup
//
//	@doc:
//		Construct a dynamic array of sets of column attnos for a rollup
//
//---------------------------------------------------------------------------
BitSetArray *
CTranslatorUtils::CreateGroupingSetsForRollup
	(
	IMemoryPool *memory_pool,
	GroupingClause *grouping_clause,
	ULONG num_cols,
	UlongUlongHashMap *group_col_pos,	// mapping of grouping col positions to SortGroupRef ids,
	CBitSet *group_cols			// existing grouping columns
	)
{
	GPOS_ASSERT(NULL != grouping_clause);

	BitSetArray *grouping_sets = GPOS_NEW(memory_pool) BitSetArray(memory_pool);
	ListCell *grouping_set = NULL;
	ForEach (grouping_set, grouping_clause->groupsets)
	{
		Node *node = (Node *) lfirst(grouping_set);
		CBitSet *bs = GPOS_NEW(memory_pool) CBitSet(memory_pool);
		if (IsA(node, SortGroupClause))
		{
			// simple group clause, create a singleton grouping set
			SortGroupClause *sort_group_clause = (SortGroupClause *) node;
			ULONG sort_group_ref = sort_group_clause->tleSortGroupRef;
			(void) bs->ExchangeSet(sort_group_ref);
			grouping_sets->Append(bs);
			UpdateGrpColMapping(memory_pool, group_col_pos, group_cols, sort_group_ref);
		}
		else if (IsA(node, List))
		{
			// list of group clauses, add all clauses into one grouping set
			// for example, rollup((a,b),(c,d));
			List *list = (List *) node;
			ListCell *group_clause = NULL;
			ForEach (group_clause, list)
			{
				Node *group_clause_node = (Node *) lfirst(group_clause);
				if (!IsA(group_clause_node, SortGroupClause))
				{
					// each list entry must be a group clause
					// for example, rollup((a,b),(c,(d,e)));
					GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Nested grouping sets"));
				}

				SortGroupClause *sort_group_clause = (SortGroupClause *) group_clause_node;
				ULONG sort_group_ref = sort_group_clause->tleSortGroupRef;
				(void) bs->ExchangeSet(sort_group_ref);
				UpdateGrpColMapping(memory_pool, group_col_pos, group_cols, sort_group_ref);
			}
			grouping_sets->Append(bs);
		}
		else
		{
			// unsupported rollup operation
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Nested grouping sets"));
		}
	}

	const ULONG num_grouping_sets = grouping_sets->Size();
	BitSetArray * col_attnos_arr = GPOS_NEW(memory_pool) BitSetArray(memory_pool);

	// compute prefixes of grouping sets array
	for (ULONG ulPrefix = 0; ulPrefix <= num_grouping_sets; ulPrefix++)
	{
		CBitSet *bs = GPOS_NEW(memory_pool) CBitSet(memory_pool);
		for (ULONG idx = 0; idx < ulPrefix; idx++)
		{
			CBitSet *current = (*grouping_sets)[idx];
			bs->Union(current);
		}
		col_attnos_arr->Append(bs);
	}
	grouping_sets->Release();

	return col_attnos_arr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateAttnoSetForGroupingSet
//
//	@doc:
//		Construct a set of column attnos corresponding to a grouping set
//
//---------------------------------------------------------------------------
CBitSet *
CTranslatorUtils::CreateAttnoSetForGroupingSet
	(
	IMemoryPool *memory_pool,
	List *group_elems,
	ULONG num_cols,
	UlongUlongHashMap *group_col_pos,	// mapping of grouping col positions to SortGroupRef ids,
	CBitSet *group_cols			// existing grouping columns
	)
{
	GPOS_ASSERT(NULL != group_elems);
	GPOS_ASSERT(0 < gpdb::ListLength(group_elems));

	CBitSet *bs = GPOS_NEW(memory_pool) CBitSet(memory_pool, num_cols);

	ListCell *lc = NULL;
	ForEach (lc, group_elems)
	{
		Node *elem_node = (Node*) lfirst(lc);

		if (NULL == elem_node)
		{
			continue;
		}

		if (!IsA(elem_node, SortGroupClause))
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Mixing grouping sets with simple group by lists"));
		}

		ULONG sort_group_ref = ((SortGroupClause *) elem_node)->tleSortGroupRef;
		bs->ExchangeSet(sort_group_ref);
		
		UpdateGrpColMapping(memory_pool, group_col_pos, group_cols, sort_group_ref);
	}
	
	return bs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GenerateColIds
//
//	@doc:
//		Construct an array of DXL column identifiers for a target list
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::GenerateColIds
	(
	IMemoryPool *memory_pool,
	List *target_list,
	MdidPtrArray *input_mdid_arr,
	ULongPtrArray *input_col_ids,
	BOOL *is_outer_ref,  // array of flags indicating if input columns are outer references
	CIdGenerator *col_id_generator
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != input_mdid_arr);
	GPOS_ASSERT(NULL != input_col_ids);
	GPOS_ASSERT(NULL != is_outer_ref);
	GPOS_ASSERT(NULL != col_id_generator);

	GPOS_ASSERT(input_mdid_arr->Size() == input_col_ids->Size());

	ULONG col_pos = 0;
	ListCell *target_entry_cell = NULL;
	ULongPtrArray *col_id_array = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	ForEach (target_entry_cell, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(target_entry_cell);
		GPOS_ASSERT(NULL != target_entry->expr);

		OID expr_type_oid = gpdb::ExprType((Node*) target_entry->expr);
		if (!target_entry->resjunk)
		{
			ULONG col_id = gpos::ulong_max;
			IMDId *mdid = (*input_mdid_arr)[col_pos];
			if (CMDIdGPDB::CastMdid(mdid)->OidObjectId() != expr_type_oid || 
				is_outer_ref[col_pos])
			{
				// generate a new column when:
				//  (1) the type of input column does not match that of the output column, or
				//  (2) input column is an outer reference 
				col_id = col_id_generator->next_id();
			}
			else
			{
				// use the column identifier of the input
				col_id = *(*input_col_ids)[col_pos];
			}
			GPOS_ASSERT(gpos::ulong_max != col_id);
			
			col_id_array->Append(GPOS_NEW(memory_pool) ULONG(col_id));

			col_pos++;
		}
	}

	return col_id_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::FixUnknownTypeConstant
//
//	@doc:
//		If the query has constant of unknown type, then return a copy of the
//		query with all constants of unknown type being coerced to the common data type
//		of the output target list; otherwise return the original query
//---------------------------------------------------------------------------
Query *
CTranslatorUtils::FixUnknownTypeConstant
	(
	Query *old_query,
	List *output_target_list
	)
{
	GPOS_ASSERT(NULL != old_query);
	GPOS_ASSERT(NULL != output_target_list);

	Query *new_query = NULL;

	ULONG pos = 0;
	ULONG col_pos = 0;
	ListCell *target_entry_cell = NULL;
	ForEach (target_entry_cell, old_query->targetList)
	{
		TargetEntry *old_target_entry = (TargetEntry *) lfirst(target_entry_cell);
		GPOS_ASSERT(NULL != old_target_entry->expr);

		if (!old_target_entry->resjunk)
		{
			if (IsA(old_target_entry->expr, Const) && (GPDB_UNKNOWN == gpdb::ExprType((Node*) old_target_entry->expr) ))
			{
				if (NULL == new_query)
				{
					new_query = (Query*) gpdb::CopyObject(const_cast<Query*>(old_query));
				}

				TargetEntry *new_target_entry = (TargetEntry *) gpdb::ListNth(new_query->targetList, pos);
				GPOS_ASSERT(old_target_entry->resno == new_target_entry->resno);
				// implicitly cast the unknown constants to the target data type
				OID target_type_oid = GetTargetListReturnTypeOid(output_target_list, col_pos);
				GPOS_ASSERT(InvalidOid != target_type_oid);
				Node *old_node = (Node *) new_target_entry->expr;
				new_target_entry->expr = (Expr*) gpdb::CoerceToCommonType
											(
											NULL,	/* pstate */
											(Node *) old_node,
											target_type_oid,
											"UNION/INTERSECT/EXCEPT"
											);

				gpdb::GPDBFree(old_node);
			}
			col_pos++;
		}

		pos++;
	}

	if (NULL == new_query)
	{
		return old_query;
	}

	gpdb::GPDBFree(old_query);

	return new_query;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetTargetListReturnTypeOid
//
//	@doc:
//		Return the type of the nth non-resjunked target list entry
//
//---------------------------------------------------------------------------
OID
CTranslatorUtils::GetTargetListReturnTypeOid
	(
	List *target_list,
	ULONG col_pos
	)
{
	ULONG col_idx = 0;
	ListCell *target_entry_cell = NULL;

	ForEach (target_entry_cell, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(target_entry_cell);
		GPOS_ASSERT(NULL != target_entry->expr);

		if (!target_entry->resjunk)
		{
			if (col_idx == col_pos)
			{
				return gpdb::ExprType((Node*) target_entry->expr);
			}

			col_idx++;
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
	ULongPtrArray *col_ids,
	BOOL keep_res_junked
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != col_ids);

	ListCell *target_entry_cell = NULL;
	ColumnDescrDXLArray *dxl_col_descrs = GPOS_NEW(memory_pool) ColumnDescrDXLArray(memory_pool);
	ULONG ul = 0;
	ForEach (target_entry_cell, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(target_entry_cell);

		if (target_entry->resjunk && !keep_res_junked)
		{
			continue;
		}

		ULONG col_id = *(*col_ids)[ul];
		CDXLColDescr *dxl_col_descr = GetColumnDescrAt(memory_pool, target_entry, col_id, ul+1 /*pos*/);
		dxl_col_descrs->Append(dxl_col_descr);
		ul++;
	}

	GPOS_ASSERT(dxl_col_descrs->Size() == col_ids->Size());

	return dxl_col_descrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetPosInTargetList
//
//	@doc:
//		Return the positions of the target list entries included in the output
//		target list
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorUtils::GetPosInTargetList
	(
	IMemoryPool *memory_pool,
	List *target_list,
	BOOL keep_res_junked
	)
{
	GPOS_ASSERT(NULL != target_list);

	ListCell *target_entry_cell = NULL;
	ULongPtrArray *positions = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	ULONG ul = 0;
	ForEach (target_entry_cell, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(target_entry_cell);

		if (target_entry->resjunk && !keep_res_junked)
		{
			continue;
		}

		positions->Append(GPOS_NEW(memory_pool) ULONG(ul));
		ul++;
	}

	return positions;
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
	ULONG pos
	)
{
	GPOS_ASSERT(NULL != target_entry);
	GPOS_ASSERT(gpos::ulong_max != col_id);

	CMDName *mdname = NULL;
	if (NULL == target_entry->resname)
	{
		CWStringConst unnamed_col(GPOS_WSZ_LIT("?column?"));
		mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, &unnamed_col);
	}
	else
	{
		CWStringDynamic *alias = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, target_entry->resname);
		mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, alias);
		// CName constructor copies string
		GPOS_DELETE(alias);
	}

	// create a column descriptor
	OID type_oid = gpdb::ExprType((Node *) target_entry->expr);
	INT type_modifier = gpdb::ExprTypeMod((Node *) target_entry->expr);
	CMDIdGPDB *col_type = GPOS_NEW(memory_pool) CMDIdGPDB(type_oid);
	CDXLColDescr *dxl_col_descr = GPOS_NEW(memory_pool) CDXLColDescr
									(
									memory_pool,
									mdname,
									col_id,
									pos, /* attno */
									col_type,
									type_modifier, /* type_modifier */
									false /* fColDropped */
									);

	return dxl_col_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateDummyProjectElem
//
//	@doc:
//		Create a dummy project element to rename the input column identifier
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::CreateDummyProjectElem
	(
	IMemoryPool *memory_pool,
	ULONG col_id_input,
	ULONG col_id_output,
	CDXLColDescr *dxl_col_descr
	)
{
	CMDIdGPDB *original_mdid = CMDIdGPDB::CastMdid(dxl_col_descr->MDIdType());
	CMDIdGPDB *copy_mdid = GPOS_NEW(memory_pool) CMDIdGPDB(original_mdid->OidObjectId(), original_mdid->VersionMajor(), original_mdid->VersionMinor());

	// create a column reference for the scalar identifier to be casted
	CMDName *mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, dxl_col_descr->MdName()->GetMDName());
	CDXLColRef *dxl_colref = GPOS_NEW(memory_pool) CDXLColRef(memory_pool, mdname, col_id_input, copy_mdid, dxl_col_descr->TypeModifier());
	CDXLScalarIdent *dxl_scalar_ident = GPOS_NEW(memory_pool) CDXLScalarIdent(memory_pool, dxl_colref);

	CDXLNode *dxl_project_element = GPOS_NEW(memory_pool) CDXLNode
										(
										memory_pool,
										GPOS_NEW(memory_pool) CDXLScalarProjElem
													(
													memory_pool,
													col_id_output,
													GPOS_NEW(memory_pool) CMDName(memory_pool, dxl_col_descr->MdName()->GetMDName())
													),
										GPOS_NEW(memory_pool) CDXLNode(memory_pool, dxl_scalar_ident)
										);

	return dxl_project_element;
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
	IntUlongHashMap *attno_to_col_id_map
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(NULL != attno_to_col_id_map);

	ULongPtrArray *col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	ListCell *target_entry_cell = NULL;
	ForEach (target_entry_cell, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(target_entry_cell);
		ULONG resno = (ULONG) target_entry->resno;
		INT attno = (INT) target_entry->resno;
		const ULONG *ul = attno_to_col_id_map->Find(&attno);

		if (NULL == ul)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLAttributeNotFound, resno);
		}

		col_ids->Append(GPOS_NEW(memory_pool) ULONG(*ul));
	}

	return col_ids;
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
	INT index,
	IntUlongHashMap *col_id_map
	)
{
	GPOS_ASSERT(0 < index);

	const ULONG *ul = col_id_map->Find(&index);

	if (NULL == ul)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLAttributeNotFound, index);
	}

	return *ul;
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
	INT varno,
	INT var_attno,
	IMDId *mdid,
	CMappingVarColId *var_col_id_mapping
	)
{
	OID oid = CMDIdGPDB::CastMdid(mdid)->OidObjectId();
	Var *var = gpdb::MakeVar(varno, var_attno, oid, -1, 0);
	ULONG col_id = var_col_id_mapping->GetColId(query_level, var, EpspotNone);
	gpdb::GPDBFree(var);

	return col_id;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetWindowSpecTargetEntry
//
//	@doc:
//		Extract a matching target entry that is a window spec
//		
//---------------------------------------------------------------------------
TargetEntry *
CTranslatorUtils::GetWindowSpecTargetEntry
	(
	Node *node,
	List *window_clause_list,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != node);
	List *target_list_subset = gpdb::FindMatchingMembersInTargetList(node, target_list);

	ListCell *target_entry_cell = NULL;
	ForEach (target_entry_cell, target_list_subset)
	{
		TargetEntry *cur_target_entry = (TargetEntry*) lfirst(target_entry_cell);
		if (IsWindowSpec(cur_target_entry, window_clause_list))
		{
			gpdb::GPDBFree(target_list_subset);
			return cur_target_entry;
		}
	}

	if (NIL != target_list_subset)
	{
		gpdb::GPDBFree(target_list_subset);
	}
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsWindowSpec
//
//	@doc:
//		Check if the expression has a matching target entry that is a window spec
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsWindowSpec
	(
	Node *node,
	List *window_clause_list,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != node);
	
	TargetEntry *window_spec = GetWindowSpecTargetEntry(node, window_clause_list, target_list);

	return (NULL != window_spec);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsWindowSpec
//
//	@doc:
//		Check if the TargetEntry is a used in the window specification
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsWindowSpec
	(
	const TargetEntry *target_entry,
	List *window_clause_list
	)
{
	ListCell *window_clause_cell;
	ForEach (window_clause_cell, window_clause_list)
	{
		WindowClause *window_clause = (WindowClause *) lfirst(window_clause_cell);
		if (IsSortingColumn(target_entry, window_clause->orderClause) ||
		    IsSortingColumn(target_entry, window_clause->partitionClause))
		{
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateDXLProjElemFromInt8Const
//
//	@doc:
// 		Construct a scalar const value expression for the given BIGINT value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::CreateDXLProjElemFromInt8Const
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	INT val
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	const IMDTypeInt8 *md_type_int8 = md_accessor->PtMDType<IMDTypeInt8>();
	md_type_int8->MDId()->AddRef();

	CDXLDatumInt8 *datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, md_type_int8->MDId(), false /*fConstNull*/, val);

	CDXLScalarConstValue *dxl_scalar_const = GPOS_NEW(memory_pool) CDXLScalarConstValue(memory_pool, datum_dxl);

	return GPOS_NEW(memory_pool) CDXLNode(memory_pool, dxl_scalar_const);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsSortingColumn
//
//	@doc:
//		Check if the TargetEntry is a sorting column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsSortingColumn
	(
	const TargetEntry *target_entry,
	List *sort_clause_list
	)
{
	ListCell *sort_clause_cell = NULL;
	ForEach (sort_clause_cell, sort_clause_list)
	{
		Node *sort_clause = (Node*) lfirst(sort_clause_cell);
		if (IsA(sort_clause, SortGroupClause) &&
		    target_entry->ressortgroupref == ((SortGroupClause *) sort_clause)->tleSortGroupRef)
		{
			return true;
		}
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetGroupingColumnTargetEntry
//
//	@doc:
//		Extract a matching target entry that is a grouping column
//---------------------------------------------------------------------------
TargetEntry *
CTranslatorUtils::GetGroupingColumnTargetEntry
	(
	Node *node,
	List *group_clause,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != node);
	List *target_list_subset = gpdb::FindMatchingMembersInTargetList(node, target_list);

	ListCell *target_entry_cell = NULL;
	ForEach (target_entry_cell, target_list_subset)
	{
		TargetEntry *next_target_entry = (TargetEntry*) lfirst(target_entry_cell);
		if (IsGroupingColumn(next_target_entry, group_clause))
		{
			gpdb::GPDBFree(target_list_subset);
			return next_target_entry;
		}
	}

	if (NIL != target_list_subset)
	{
		gpdb::GPDBFree(target_list_subset);
	}
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsGroupingColumn
//
//	@doc:
//		Check if the expression has a matching target entry that is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsGroupingColumn
	(
	Node *node,
	List *group_clause,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != node);

	TargetEntry *grouping_col_target_etnry = GetGroupingColumnTargetEntry(node, group_clause, target_list);

	return (NULL != grouping_col_target_etnry);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsGroupingColumn
//
//	@doc:
//		Check if the TargetEntry is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsGroupingColumn
	(
	const TargetEntry *target_entry,
	List *group_clause
	)
{
	ListCell *group_clause_cell = NULL;
	ForEach (group_clause_cell, group_clause)
	{
		Node *group_clause_node = (Node*) lfirst(group_clause_cell);

		if (NULL == group_clause_node)
		{
			continue;
		}

		if (IsA(group_clause_node, SortGroupClause) &&
		    IsGroupingColumn(target_entry, (SortGroupClause*) group_clause_node))
		{
			return true;
		}

		if (IsA(group_clause_node, GroupingClause))
		{
			GroupingClause *grouping_clause = (GroupingClause *) group_clause_node;

			ListCell *grouping_set = NULL;
			ForEach (grouping_set, grouping_clause->groupsets)
			{
				Node *grouping_set_node = (Node *) lfirst(grouping_set);

				if (IsA(grouping_set_node, SortGroupClause) &&
				    IsGroupingColumn(target_entry, ((SortGroupClause *) grouping_set_node)))
				{
					return true;
				}

				if (IsA(grouping_set_node, List) && IsGroupingColumn(target_entry, (List *) grouping_set_node))
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
//		CTranslatorUtils::IsGroupingColumn
//
//	@doc:
//		Check if the TargetEntry is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsGroupingColumn
	(
	const TargetEntry *target_entry,
	const SortGroupClause *grouping_clause
	)
{
	GPOS_ASSERT(NULL != grouping_clause);

	return (target_entry->ressortgroupref == grouping_clause->tleSortGroupRef);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsGroupingColumn
//
//	@doc:
//		Check if the sorting column is a grouping column
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsGroupingColumn
	(
	const SortGroupClause *sort_group_clause,
	List *group_clause
	)
{
	ListCell *group_clause_cell = NULL;
	ForEach (group_clause_cell, group_clause)
	{
		Node *group_clause_node = (Node*) lfirst(group_clause_cell);
		GPOS_ASSERT(IsA(group_clause_node, SortGroupClause) && "We currently do not support grouping sets.");

		SortGroupClause *sort_group_clause = (SortGroupClause *) group_clause_node;
		if (sort_group_clause->tleSortGroupRef == sort_group_clause->tleSortGroupRef)
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
CTranslatorUtils::ConvertColidToAttnos
	(
	ULongPtrArray *col_ids,
	CDXLTranslateContext *translate_ctxt
	)
{
	GPOS_ASSERT(NULL != col_ids);
	GPOS_ASSERT(NULL != translate_ctxt);
	
	List *result = NIL;
	
	const ULONG length = col_ids->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG col_id = *((*col_ids)[ul]);
		const TargetEntry *target_entry = translate_ctxt->GetTargetEntry(col_id);
		GPOS_ASSERT(NULL != target_entry);
		result = gpdb::LAppendInt(result, target_entry->resno);
	}
	
	return result;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetLongFromStr
//
//	@doc:
//		Parses a long integer value from a string
//
//---------------------------------------------------------------------------
LINT
CTranslatorUtils::GetLongFromStr
	(
	const CWStringBase *wcstr
	)
{
	CHAR *str = CreateMultiByteCharStringFromWCString(wcstr->GetBuffer());
	CHAR *end = NULL;
	return gpos::clib::Strtol(str, &end, 10);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetIntFromStr
//
//	@doc:
//		Parses an integer value from a string
//
//---------------------------------------------------------------------------
INT
CTranslatorUtils::GetIntFromStr
	(
	const CWStringBase *wcstr
	)
{
	return (INT) GetLongFromStr(wcstr);
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
	const WCHAR *wcstr
	)
{
	GPOS_ASSERT(NULL != wcstr);

	ULONG max_len = GPOS_WSZ_LENGTH(wcstr) * GPOS_SIZEOF(WCHAR) + 1;
	CHAR *str = (CHAR *) gpdb::GPDBAlloc(max_len);
#ifdef GPOS_DEBUG
	LINT li = (INT)
#endif
	clib::Wcstombs(str, const_cast<WCHAR *>(wcstr), max_len);
	GPOS_ASSERT(0 <= li);

	str[max_len - 1] = '\0';

	return str;
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
	const ULONG num_cols = old_col_ids->Size();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		ULONG old_col_id = *((*old_col_ids)[ul]);
		ULONG new_col_id = *((*new_col_ids)[ul]);
#ifdef GPOS_DEBUG
		BOOL result = 
#endif // GPOS_DEBUG
		old_new_col_mapping->Insert(GPOS_NEW(memory_pool) ULONG(old_col_id), GPOS_NEW(memory_pool) ULONG(new_col_id));
		GPOS_ASSERT(result);
	}
	
	return old_new_col_mapping;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsDuplicateSensitiveMotion
//
//	@doc:
//		Is this a motion sensitive to duplicates
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsDuplicateSensitiveMotion
	(
	CDXLPhysicalMotion *dxl_motion
	)
{
	Edxlopid dxl_opid = dxl_motion->GetDXLOperator();
	
	if (EdxlopPhysicalMotionRedistribute == dxl_opid)
	{
		return CDXLPhysicalRedistributeMotion::Cast(dxl_motion)->IsDuplicateSensitive();
	}
	
	if (EdxlopPhysicalMotionRandom == dxl_opid)
	{
		return CDXLPhysicalRandomMotion::Cast(dxl_motion)->IsDuplicateSensitive();
	}
	
	// other motion operators are not sensitive to duplicates
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::HasProjElem
//
//	@doc:
//		Check whether the given project list has a project element of the given
//		operator type
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::HasProjElem
	(
	CDXLNode *project_list_dxl,
	Edxlopid dxl_opid
	)
{
	GPOS_ASSERT(NULL != project_list_dxl);
	GPOS_ASSERT(EdxlopScalarProjectList == project_list_dxl->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopSentinel > dxl_opid);

	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *dxl_project_element = (*project_list_dxl)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == dxl_project_element->GetOperator()->GetDXLOperator());

		CDXLNode *dxl_child_node = (*dxl_project_element)[0];
		if (dxl_opid == dxl_child_node->GetOperator()->GetDXLOperator())
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateDXLProjElemConstNULL
//
//	@doc:
//		Create a DXL project element node with a Const NULL of type provided
//		by the column descriptor. The function raises an exception if the
//		column is not nullable.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::CreateDXLProjElemConstNULL
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *pidgtorCol,
	const IMDColumn *md_col
	)
{
	GPOS_ASSERT(NULL != md_col);
	GPOS_ASSERT(!md_col->IsSystemColumn());

	const WCHAR *col_name = md_col->Mdname().GetMDName()->GetBuffer();
	if (!md_col->IsNullable())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLNotNullViolation, col_name);
	}

	ULONG col_id = pidgtorCol->next_id();
	CDXLNode *dxl_project_element = CreateDXLProjElemConstNULL(memory_pool, md_accessor, md_col->MDIdType(), col_id, col_name);

	return dxl_project_element;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateDXLProjElemConstNULL
//
//	@doc:
//		Create a DXL project element node with a Const NULL expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::CreateDXLProjElemConstNULL
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *mdid,
	ULONG col_id,
	const WCHAR *col_name
	)
{
	CHAR *column_name = CDXLUtils::CreateMultiByteCharStringFromWCString(memory_pool, col_name);
	CDXLNode *dxl_project_element = CreateDXLProjElemConstNULL(memory_pool, md_accessor, mdid, col_id, column_name);

	GPOS_DELETE_ARRAY(column_name);

	return dxl_project_element;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::CreateDXLProjElemConstNULL
//
//	@doc:
//		Create a DXL project element node with a Const NULL expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorUtils::CreateDXLProjElemConstNULL
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *mdid,
	ULONG col_id,
	CHAR *alias_name
	)
{
	BOOL is_passed_by_value = md_accessor->Pmdtype(mdid)->IsPassedByValue();

	// get the id and alias for the proj elem
	CMDName *alias_mdname = NULL;

	if (NULL == alias_name)
	{
		CWStringConst unnamed_col(GPOS_WSZ_LIT("?column?"));
		alias_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, &unnamed_col);
	}
	else
	{
		CWStringDynamic *alias = CDXLUtils::CreateDynamicStringFromCharArray(memory_pool, alias_name);
		alias_mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, alias);
		GPOS_DELETE(alias);
	}

	mdid->AddRef();
	CDXLDatum *datum_dxl = NULL;
	if (mdid->Equals(&CMDIdGPDB::m_mdid_int2))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt2(memory_pool, mdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_int4))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt4(memory_pool, mdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_int8))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, mdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_bool))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumBool(memory_pool, mdid, true /*fConstNull*/, 0 /*value*/);
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_oid))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumOid(memory_pool, mdid, true /*fConstNull*/, 0 /*value*/);
	}
	else
	{
		datum_dxl = CMDTypeGenericGPDB::CreateDXLDatumVal
										(
										memory_pool,
										mdid,
										default_type_modifier,
										is_passed_by_value /*fConstByVal*/,
										true /*fConstNull*/,
										NULL, /*pba */
										0 /*length*/,
										0 /*lValue*/,
										0 /*dValue*/
										);
	}

	CDXLNode *dxl_const_node = GPOS_NEW(memory_pool) CDXLNode(memory_pool, GPOS_NEW(memory_pool) CDXLScalarConstValue(memory_pool, datum_dxl));

	return GPOS_NEW(memory_pool) CDXLNode(memory_pool, GPOS_NEW(memory_pool) CDXLScalarProjElem(memory_pool, col_id, alias_mdname), dxl_const_node);
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
	List *range_table_list
	)
{
	gpdb::CheckRTPermissions(range_table_list);
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
	Node *node
	)
{
	GPOS_ASSERT(NULL != node);
	GPOS_ASSERT(IsA(node, WindowFunc));

	WindowFunc *winfunc = (WindowFunc *) node;

	if (gpdb::AggregateExists(winfunc->winfnoid) && !gpdb::AggHasPrelimOrInvPrelimFunc(winfunc->winfnoid))
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
	UlongUlongHashMap *group_col_pos, 
	CBitSet *group_cols,
	ULONG sort_group_ref
	)
{
	GPOS_ASSERT(NULL != group_col_pos);
	GPOS_ASSERT(NULL != group_cols);
		
	if (!group_cols->Get(sort_group_ref))
	{
		ULONG num_unique_grouping_cols = group_cols->Size();
		group_col_pos->Insert(GPOS_NEW(memory_pool) ULONG (num_unique_grouping_cols), GPOS_NEW(memory_pool) ULONG(sort_group_ref));
		(void) group_cols->ExchangeSet(sort_group_ref);
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
	ULONG *col_ids,  // array of column ids to be checked
	BOOL *is_outer_ref,  // array of outer ref indicators, initially all set to true by caller 
	ULONG num_columns,  // number of columns
	CDXLNode *dxl_node
	)
{
	GPOS_ASSERT(NULL != col_ids);
	GPOS_ASSERT(NULL != is_outer_ref);
	GPOS_ASSERT(NULL != dxl_node);
	
	const CDXLOperator *dxl_op = dxl_node->GetOperator();
	for (ULONG ulCol = 0; ulCol < num_columns; ulCol++)
	{
		ULONG col_id = col_ids[ulCol];
		if (is_outer_ref[ulCol] && dxl_op->IsColDefined(col_id))
		{
			// column is defined by operator, reset outer reference flag
			is_outer_ref[ulCol] = false;
		}
	}

	// recursively process children
	const ULONG arity = dxl_node->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		MarkOuterRefs(col_ids, is_outer_ref, num_columns, (*dxl_node)[ul]);
	}
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorUtils::MapDXLSubplanToSublinkType
//
//      @doc:
//              Map DXL Subplan type to GPDB SubLinkType
//
//---------------------------------------------------------------------------
SubLinkType
CTranslatorUtils::MapDXLSubplanToSublinkType
        (
        EdxlSubPlanType dxl_subplan_type
        )
{
        GPOS_ASSERT(EdxlSubPlanTypeSentinel > dxl_subplan_type);
        ULONG mapping[][2] =
                {
                {EdxlSubPlanTypeScalar, EXPR_SUBLINK},
                {EdxlSubPlanTypeExists, EXISTS_SUBLINK},
                {EdxlSubPlanTypeNotExists, NOT_EXISTS_SUBLINK},
                {EdxlSubPlanTypeAny, ANY_SUBLINK},
                {EdxlSubPlanTypeAll, ALL_SUBLINK}
                };

        const ULONG arity = GPOS_ARRAY_SIZE(mapping);
        SubLinkType slink = EXPR_SUBLINK;
	BOOL found = false;
        for (ULONG ul = 0; ul < arity; ul++)
        {
                ULONG *elem = mapping[ul];
                if ((ULONG) dxl_subplan_type == elem[0])
                {
                        slink = (SubLinkType) elem[1];
                        found = true;
			break;
                }
        }

	GPOS_ASSERT(found && "Invalid SubPlanType");

        return slink;
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorUtils::MapSublinkTypeToDXLSubplan
//
//      @doc:
//              Map GPDB SubLinkType to DXL subplan type
//
//---------------------------------------------------------------------------
EdxlSubPlanType
CTranslatorUtils::MapSublinkTypeToDXLSubplan
        (
        SubLinkType slink
        )
{
        ULONG mapping[][2] =
                {
                {EXPR_SUBLINK, EdxlSubPlanTypeScalar},
                {EXISTS_SUBLINK , EdxlSubPlanTypeExists},
                {NOT_EXISTS_SUBLINK, EdxlSubPlanTypeNotExists},
                {ANY_SUBLINK, EdxlSubPlanTypeAny},
                {ALL_SUBLINK, EdxlSubPlanTypeAll}
                };

        const ULONG arity = GPOS_ARRAY_SIZE(mapping);
        EdxlSubPlanType dxl_subplan_type = EdxlSubPlanTypeScalar;
	BOOL found = false;
        for (ULONG ul = 0; ul < arity; ul++)
        {
                ULONG *elem = mapping[ul];
                if ((ULONG) slink == elem[0])
                {
                        dxl_subplan_type = (EdxlSubPlanType) elem[1];
                        found = true;
			break;
                }
        }

	 GPOS_ASSERT(found && "Invalid SubLinkType");

        return dxl_subplan_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::RelHasTriggers
//
//	@doc:
//		Check whether there are triggers for the given operation on
//		the given relation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::RelHasTriggers
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	const IMDRelation *rel,
	const EdxlDmlType dml_type_dxl
	)
{
	const ULONG num_triggers = rel->TriggerCount();
	for (ULONG ul = 0; ul < num_triggers; ul++)
	{
		if (IsApplicableTrigger(md_accessor, rel->TriggerMDidAt(ul), dml_type_dxl))
		{
			return true;
		}
	}

	// if table is partitioned, check for triggers on child partitions as well
	INT type = 0;
	if (Edxldmlinsert == dml_type_dxl)
	{
		type = TRIGGER_TYPE_INSERT;
	}
	else if (Edxldmldelete == dml_type_dxl)
	{
		type = TRIGGER_TYPE_DELETE;
	}
	else
	{
		GPOS_ASSERT(Edxldmlupdate == dml_type_dxl);
		type = TRIGGER_TYPE_UPDATE;
	}

	OID rel_oid = CMDIdGPDB::CastMdid(rel->MDId())->OidObjectId();
	return gpdb::ChildPartHasTriggers(rel_oid, type);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::IsApplicableTrigger
//
//	@doc:
//		Check whether the given trigger is applicable to the given DML operation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::IsApplicableTrigger
	(
	CMDAccessor *md_accessor,
	IMDId *trigger_mdid,
	const EdxlDmlType dml_type_dxl
	)
{
	const IMDTrigger *trigger = md_accessor->Pmdtrigger(trigger_mdid);
	if (!trigger->IsEnabled())
	{
		return false;
	}

	return ((Edxldmlinsert == dml_type_dxl && trigger->IsInsert()) ||
			(Edxldmldelete == dml_type_dxl && trigger->IsDelete()) ||
			(Edxldmlupdate == dml_type_dxl && trigger->IsUpdate()));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::RelHasConstraints
//
//	@doc:
//		Check whether there are constraints for the given relation
//
//---------------------------------------------------------------------------
BOOL
CTranslatorUtils::RelHasConstraints
	(
	const IMDRelation *rel
	)
{
	if (0 < rel->CheckConstraintCount())
	{
		return true;
	}
	
	const ULONG num_cols = rel->ColumnCount();
	
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		const IMDColumn *md_col = rel->GetMdCol(ul);
		if (!md_col->IsSystemColumn() && !md_col->IsNullable())
		{
			return true;
		}
	}
	
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetAssertErrorMsgs
//
//	@doc:
//		Construct a list of error messages from a list of assert constraints 
//
//---------------------------------------------------------------------------
List *
CTranslatorUtils::GetAssertErrorMsgs
	(
	CDXLNode *assert_constraint_list
	)
{
	GPOS_ASSERT(NULL != assert_constraint_list);
	GPOS_ASSERT(EdxlopScalarAssertConstraintList == assert_constraint_list->GetOperator()->GetDXLOperator());
	
	List *error_msgs_list = NIL;
	const ULONG num_constraints = assert_constraint_list->Arity();
	
	for (ULONG ul = 0; ul < num_constraints; ul++)
	{
		CDXLNode *dxl_constraint_node = (*assert_constraint_list)[ul];
		CDXLScalarAssertConstraint *dxl_constraint_op = CDXLScalarAssertConstraint::Cast(dxl_constraint_node->GetOperator());
		CWStringBase *error_msg = dxl_constraint_op->GetErrorMsgStr();
		error_msgs_list = gpdb::LAppend(error_msgs_list, gpdb::MakeStringValue(CreateMultiByteCharStringFromWCString(error_msg->GetBuffer())));
	}
	
	return error_msgs_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorUtils::GetNumNonSystemColumns
//
//	@doc:
//		Return the count of non-system columns in the relation
//
//---------------------------------------------------------------------------
ULONG
CTranslatorUtils::GetNumNonSystemColumns
	(
	const IMDRelation *rel
	)
{
	GPOS_ASSERT(NULL != rel);

	ULONG num_non_system_cols = 0;

	const ULONG num_cols = rel->ColumnCount();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		const IMDColumn *md_col  = rel->GetMdCol(ul);

		if (!md_col->IsSystemColumn())
		{
			num_non_system_cols++;
		}
	}

	return num_non_system_cols;
}

// Function to check if we should create stats bucket in DXL
// Returns true if column datatype is not text/char/varchar/bpchar
BOOL
CTranslatorUtils::ShouldCreateStatsBucket
	(
	OID att_type_oid
	)
{
	if (att_type_oid != TEXTOID && att_type_oid != CHAROID && att_type_oid != VARCHAROID && att_type_oid != BPCHAROID)
		return true;

	return false;
}

// EOF
