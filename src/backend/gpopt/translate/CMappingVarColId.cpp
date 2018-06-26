//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMappingVarColId.cpp
//
//	@doc:
//		Implementation of base (abstract) var mapping class
//
//	@test:
//
//
//---------------------------------------------------------------------------
#include "postgres.h"
#include "gpopt/translate/CMappingVarColId.h"
#include "gpopt/translate/CTranslatorUtils.h"

#include "nodes/primnodes.h"
#include "nodes/value.h"


#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/md/IMDIndex.h"

#include "gpos/error/CAutoTrace.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::CMappingVarColId
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMappingVarColId::CMappingVarColId
	(
	IMemoryPool *memory_pool
	)
	:
	m_memory_pool(memory_pool)
{
	m_gpdb_att_opt_col_mapping = GPOS_NEW(m_memory_pool) GPDBAttOptColHashMap(m_memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::GetGPDBAttOptColMapping
//
//	@doc:
//		Given a gpdb attribute, return the mapping info to opt col
//
//---------------------------------------------------------------------------
const CGPDBAttOptCol *
CMappingVarColId::GetGPDBAttOptColMapping
	(
	ULONG current_query_level,
	const Var *var,
	EPlStmtPhysicalOpType plstmt_physical_op_type
	)
	const
{
	GPOS_ASSERT(NULL != var);
	GPOS_ASSERT(current_query_level >= var->varlevelsup);

	// absolute query level of var
	ULONG abs_query_level = current_query_level - var->varlevelsup;

	// extract varno
	ULONG var_no = var->varno;
	if (EpspotWindow == plstmt_physical_op_type || EpspotAgg == plstmt_physical_op_type || EpspotMaterialize == plstmt_physical_op_type)
	{
		// Agg and Materialize need to employ OUTER, since they have other
		// values in GPDB world
		var_no = OUTER;
	}

	CGPDBAttInfo *gpdb_att_info = GPOS_NEW(m_memory_pool) CGPDBAttInfo(abs_query_level, var_no, var->varattno);
	CGPDBAttOptCol *gpdb_att_opt_col_info = m_gpdb_att_opt_col_mapping->Find(gpdb_att_info);
	
	if (NULL == gpdb_att_opt_col_info)
	{
		// TODO: Sept 09 2013, remove temporary fix (revert exception to assert) to avoid crash during algebrization
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLError, GPOS_WSZ_LIT("No variable"));
	}

	gpdb_att_info->Release();
	return gpdb_att_opt_col_info;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::GetOptColName
//
//	@doc:
//		Given a gpdb attribute, return a column name in optimizer world
//
//---------------------------------------------------------------------------
const CWStringBase *
CMappingVarColId::GetOptColName
	(
	ULONG current_query_level,
	const Var *var,
	EPlStmtPhysicalOpType plstmt_physical_op_type
	)
	const
{
	return GetGPDBAttOptColMapping(current_query_level, var, plstmt_physical_op_type)->GetOptColInfo()->GetOptColName();
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::GetColId
//
//	@doc:
//		given a gpdb attribute, return a column id in optimizer world
//
//---------------------------------------------------------------------------
ULONG
CMappingVarColId::GetColId
	(
	ULONG current_query_level,
	const Var *var,
	EPlStmtPhysicalOpType plstmt_physical_op_type
	)
	const
{
	return GetGPDBAttOptColMapping(current_query_level, var, plstmt_physical_op_type)->GetOptColInfo()->GetColId();
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::Insert
//
//	@doc:
//		Insert a single entry into the hash map
//
//---------------------------------------------------------------------------
void
CMappingVarColId::Insert
	(
	ULONG query_level,
	ULONG var_no,
	INT attrnum,
	ULONG col_id,
	CWStringBase *column_name
	)
{
	// GPDB agg node uses 0 in Var, but that should've been taken care of
	// by translator
	GPOS_ASSERT(var_no > 0);

	// create key
	CGPDBAttInfo *gpdb_att_info = GPOS_NEW(m_memory_pool) CGPDBAttInfo(query_level, var_no, attrnum);

	// create value
	COptColInfo *opt_col_info = GPOS_NEW(m_memory_pool) COptColInfo(col_id, column_name);

	// key is part of value, bump up refcount
	gpdb_att_info->AddRef();
	CGPDBAttOptCol *gpdb_att_opt_col_info = GPOS_NEW(m_memory_pool) CGPDBAttOptCol(gpdb_att_info, opt_col_info);

#ifdef GPOS_DEBUG
	BOOL result =
#endif // GPOS_DEBUG
			m_gpdb_att_opt_col_mapping->Insert(gpdb_att_info, gpdb_att_opt_col_info);

	GPOS_ASSERT(result);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadTblColumns
//
//	@doc:
//		Load up information from GPDB's base table RTE and corresponding
//		optimizer table descriptor
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadTblColumns
	(
	ULONG query_level,
	ULONG RTE_index,
	const CDXLTableDescr *table_descr
	)
{
	GPOS_ASSERT(NULL != table_descr);
	const ULONG size = table_descr->Arity();

	// add mapping information for columns
	for (ULONG i = 0; i < size; i++)
	{
		const CDXLColDescr *dxl_col_descr = table_descr->GetColumnDescrAt(i);
		this->Insert
				(
				query_level,
				RTE_index,
				dxl_col_descr->AttrNum(),
				dxl_col_descr->Id(),
				dxl_col_descr->MdName()->GetMDName()->Copy(m_memory_pool)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadIndexColumns
//
//	@doc:
//		Load up information from GPDB index and corresponding
//		optimizer table descriptor
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadIndexColumns
	(
	ULONG query_level,
	ULONG RTE_index,
	const IMDIndex *index,
	const CDXLTableDescr *table_descr
	)
{
	GPOS_ASSERT(NULL != table_descr);

	const ULONG size = index->Keys();

	// add mapping information for columns
	for (ULONG i = 0; i < size; i++)
	{
		ULONG pos = index->KeyAt(i);
		const CDXLColDescr *dxl_col_descr = table_descr->GetColumnDescrAt(pos);
		this->Insert
				(
				query_level,
				RTE_index,
				INT(i + 1),
				dxl_col_descr->Id(),
				dxl_col_descr->MdName()->GetMDName()->Copy(m_memory_pool)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::Load
//
//	@doc:
//		Load column mapping information from list of column names
//
//---------------------------------------------------------------------------
void
CMappingVarColId::Load
	(
	ULONG query_level,
	ULONG RTE_index,
	CIdGenerator *id_generator,
	List *col_names
	)
{
	ListCell *col_name = NULL;
	ULONG i = 0;

	// add mapping information for columns
	ForEach(col_name, col_names)
	{
		Value *value = (Value *) lfirst(col_name);
		CHAR *col_name_char_array = strVal(value);

		CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, col_name_char_array);

		this->Insert
				(
				query_level,
				RTE_index,
				INT(i + 1),
				id_generator->next_id(),
				column_name->Copy(m_memory_pool)
				);

		i ++;
		GPOS_DELETE(column_name);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadColumns
//
//	@doc:
//		Load up columns information from the array of column descriptors
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadColumns
	(
	ULONG query_level,
	ULONG RTE_index,
	const ColumnDescrDXLArray *column_descrs
	)
{
	GPOS_ASSERT(NULL != column_descrs);
	const ULONG size = column_descrs->Size();

	// add mapping information for columns
	for (ULONG i = 0; i < size; i++)
	{
		const CDXLColDescr *dxl_col_descr = (*column_descrs)[i];
		this->Insert
				(
				query_level,
				RTE_index,
				dxl_col_descr->AttrNum(),
				dxl_col_descr->Id(),
				dxl_col_descr->MdName()->GetMDName()->Copy(m_memory_pool)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadDerivedTblColumns
//
//	@doc:
//		Load up information from column information in derived tables
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadDerivedTblColumns
	(
	ULONG query_level,
	ULONG RTE_index,
	const DXLNodeArray *derived_columns_dxl,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != derived_columns_dxl);
	GPOS_ASSERT( (ULONG) gpdb::ListLength(target_list) >= derived_columns_dxl->Size());

	ULONG drvd_tbl_col_counter = 0; // counter for the dynamic array of DXL nodes
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry  = (TargetEntry*) lfirst(lc);
		if (!target_entry->resjunk)
		{
			GPOS_ASSERT(0 < target_entry->resno);
			CDXLNode *dxlnode = (*derived_columns_dxl)[drvd_tbl_col_counter];
			GPOS_ASSERT(NULL != dxlnode);
			CDXLScalarIdent *dxl_sc_ident = CDXLScalarIdent::Cast(dxlnode->GetOperator());
			const CDXLColRef *dxl_colref = dxl_sc_ident->MakeDXLColRef();
			this->Insert
					(
					query_level,
					RTE_index,
					INT(target_entry->resno),
					dxl_colref->Id(),
					dxl_colref->MdName()->GetMDName()->Copy(m_memory_pool)
					);
			drvd_tbl_col_counter++;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadCTEColumns
//
//	@doc:
//		Load CTE column mappings
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadCTEColumns
	(
	ULONG query_level,
	ULONG RTE_index,
	const ULongPtrArray *CTE_columns,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != CTE_columns);
	GPOS_ASSERT( (ULONG) gpdb::ListLength(target_list) >= CTE_columns->Size());

	ULONG idx = 0;
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry  = (TargetEntry*) lfirst(lc);
		if (!target_entry->resjunk)
		{
			GPOS_ASSERT(0 < target_entry->resno);
			ULONG CTE_col_id = *((*CTE_columns)[idx]);
			
			CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, target_entry->resname);
			this->Insert
					(
					query_level,
					RTE_index,
					INT(target_entry->resno),
					CTE_col_id,
					column_name
					);
			idx++;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadProjectElements
//
//	@doc:
//		Load up information from projection list created from GPDB join expression
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadProjectElements
	(
	ULONG query_level,
	ULONG RTE_index,
	const CDXLNode *project_list_dxl
	)
{
	GPOS_ASSERT(NULL != project_list_dxl);
	const ULONG size = project_list_dxl->Arity();
	// add mapping information for columns
	for (ULONG i = 0; i < size; i++)
	{
		CDXLNode *dxlnode = (*project_list_dxl)[i];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(dxlnode->GetOperator());
		this->Insert
				(
				query_level,
				RTE_index,
				INT(i + 1),
				dxl_proj_elem->Id(),
				dxl_proj_elem->GetMdNameAlias()->GetMDName()->Copy(m_memory_pool)
				);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::CopyMapColId
//
//	@doc:
//		Create a deep copy
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::CopyMapColId
	(
	ULONG query_level
	)
	const
{
	CMappingVarColId *var_col_id_mapping = GPOS_NEW(m_memory_pool) CMappingVarColId(m_memory_pool);

	// iterate over full map
	GPDBAttOptColHashMapIter col_map_iterator(this->m_gpdb_att_opt_col_mapping);
	while (col_map_iterator.Advance())
	{
		const CGPDBAttOptCol *gpdb_att_opt_col_info = col_map_iterator.Value();
		const CGPDBAttInfo *gpdb_att_info = gpdb_att_opt_col_info->GetGPDBAttInfo();
		const COptColInfo *opt_col_info = gpdb_att_opt_col_info->GetOptColInfo();

		if (gpdb_att_info->GetQueryLevel() <= query_level)
		{
			// include all variables defined at same query level or before
			CGPDBAttInfo *gpdb_att_info_new = GPOS_NEW(m_memory_pool) CGPDBAttInfo(gpdb_att_info->GetQueryLevel(), gpdb_att_info->GetVarNo(), gpdb_att_info->GetAttNo());
			COptColInfo *opt_col_info_new = GPOS_NEW(m_memory_pool) COptColInfo(opt_col_info->GetColId(), GPOS_NEW(m_memory_pool) CWStringConst(m_memory_pool, opt_col_info->GetOptColName()->GetBuffer()));
			gpdb_att_info_new->AddRef();
			CGPDBAttOptCol *gpdb_att_opt_col_new = GPOS_NEW(m_memory_pool) CGPDBAttOptCol(gpdb_att_info_new, opt_col_info_new);

			// insert into hashmap
#ifdef GPOS_DEBUG
			BOOL result =
#endif // GPOS_DEBUG
					var_col_id_mapping->m_gpdb_att_opt_col_mapping->Insert(gpdb_att_info_new, gpdb_att_opt_col_new);
			GPOS_ASSERT(result);
		}
	}

	return var_col_id_mapping;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::CopyMapColId
//
//	@doc:
//		Create a deep copy
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::CopyMapColId
	(
	IMemoryPool *memory_pool
	)
	const
{
	CMappingVarColId *var_col_id_mapping = GPOS_NEW(memory_pool) CMappingVarColId(memory_pool);

	// iterate over full map
	GPDBAttOptColHashMapIter col_map_iterator(this->m_gpdb_att_opt_col_mapping);
	while (col_map_iterator.Advance())
	{
		const CGPDBAttOptCol *gpdb_att_opt_col_info = col_map_iterator.Value();
		const CGPDBAttInfo *gpdb_att_info = gpdb_att_opt_col_info->GetGPDBAttInfo();
		const COptColInfo *opt_col_info = gpdb_att_opt_col_info->GetOptColInfo();

		CGPDBAttInfo *gpdb_att_info_new = GPOS_NEW(memory_pool) CGPDBAttInfo(gpdb_att_info->GetQueryLevel(), gpdb_att_info->GetVarNo(), gpdb_att_info->GetAttNo());
		COptColInfo *opt_col_info_new = GPOS_NEW(memory_pool) COptColInfo(opt_col_info->GetColId(), GPOS_NEW(memory_pool) CWStringConst(memory_pool, opt_col_info->GetOptColName()->GetBuffer()));
		gpdb_att_info_new->AddRef();
		CGPDBAttOptCol *gpdb_att_opt_col_new = GPOS_NEW(memory_pool) CGPDBAttOptCol(gpdb_att_info_new, opt_col_info_new);

		// insert into hashmap
#ifdef GPOS_DEBUG
	BOOL result =
#endif // GPOS_DEBUG
		var_col_id_mapping->m_gpdb_att_opt_col_mapping->Insert(gpdb_att_info_new, gpdb_att_opt_col_new);
		GPOS_ASSERT(result);
	}

	return var_col_id_mapping;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::CopyRemapColId
//
//	@doc:
//		Create a copy of the mapping replacing the old column ids by new ones
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::CopyRemapColId
	(
	IMemoryPool *memory_pool,
	ULongPtrArray *old_col_ids,
	ULongPtrArray *new_col_ids
	)
	const
{
	GPOS_ASSERT(NULL != old_col_ids);
	GPOS_ASSERT(NULL != new_col_ids);
	GPOS_ASSERT(new_col_ids->Size() == old_col_ids->Size());
	
	// construct a mapping old cols -> new cols
	UlongUlongHashMap *old_new_col_mapping = CTranslatorUtils::MakeNewToOldColMapping(memory_pool, old_col_ids, new_col_ids);
		
	CMappingVarColId *var_col_id_mapping = GPOS_NEW(memory_pool) CMappingVarColId(memory_pool);

	GPDBAttOptColHashMapIter col_map_iterator(this->m_gpdb_att_opt_col_mapping);
	while (col_map_iterator.Advance())
	{
		const CGPDBAttOptCol *gpdb_att_opt_col_info = col_map_iterator.Value();
		const CGPDBAttInfo *gpdb_att_info = gpdb_att_opt_col_info->GetGPDBAttInfo();
		const COptColInfo *opt_col_info = gpdb_att_opt_col_info->GetOptColInfo();

		CGPDBAttInfo *gpdb_att_info_new = GPOS_NEW(memory_pool) CGPDBAttInfo(gpdb_att_info->GetQueryLevel(), gpdb_att_info->GetVarNo(), gpdb_att_info->GetAttNo());
		ULONG col_id = opt_col_info->GetColId();
		ULONG *new_col_id = old_new_col_mapping->Find(&col_id);
		if (NULL != new_col_id)
		{
			col_id = *new_col_id;
		}
		
		COptColInfo *opt_col_info_new = GPOS_NEW(memory_pool) COptColInfo(col_id, GPOS_NEW(memory_pool) CWStringConst(memory_pool, opt_col_info->GetOptColName()->GetBuffer()));
		gpdb_att_info_new->AddRef();
		CGPDBAttOptCol *gpdb_att_opt_col_new = GPOS_NEW(memory_pool) CGPDBAttOptCol(gpdb_att_info_new, opt_col_info_new);

#ifdef GPOS_DEBUG
		BOOL result =
#endif // GPOS_DEBUG
		var_col_id_mapping->m_gpdb_att_opt_col_mapping->Insert(gpdb_att_info_new, gpdb_att_opt_col_new);
		GPOS_ASSERT(result);
	}
	
	old_new_col_mapping->Release();

	return var_col_id_mapping;
}

// EOF
