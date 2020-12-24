//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalCTAS.h
//
//	@doc:
//		Class for representing DXL physical CTAS operators
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLPhysicalCTAS_H
#define GPDXL_CDXLPhysicalCTAS_H

#include "gpos/base.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// fwd decl
class CDXLCtasStorageOptions;

using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalCTAS
//
//	@doc:
//		Class for representing DXL physical CTAS operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalCTAS : public CDXLPhysical
{
private:
	// schema name
	CMDName *m_mdname_schema;

	// table name
	CMDName *m_mdname_rel;

	// list of columns
	CDXLColDescrArray *m_col_descr_array;

	// storage options
	CDXLCtasStorageOptions *m_dxl_ctas_storage_option;

	// distribution policy
	IMDRelation::Ereldistrpolicy m_rel_distr_policy;

	// list of distribution column positions
	ULongPtrArray *m_distr_column_pos_array;

	// list of distriution column opclasses
	IMdIdArray *m_distr_opclasses;

	// is this a temporary table
	BOOL m_is_temp_table;

	// does table have oids
	BOOL m_has_oids;

	// storage type
	IMDRelation::Erelstoragetype m_rel_storage_type;

	// list of source column ids
	ULongPtrArray *m_src_colids_array;

	// list of vartypmod
	IntPtrArray *m_vartypemod_array;

public:
	CDXLPhysicalCTAS(CDXLPhysicalCTAS &) = delete;

	// ctor
	CDXLPhysicalCTAS(CMemoryPool *mp, CMDName *mdname_schema,
					 CMDName *mdname_rel,
					 CDXLColDescrArray *dxl_col_descr_array,
					 CDXLCtasStorageOptions *dxl_ctas_storage_options,
					 IMDRelation::Ereldistrpolicy rel_distr_policy,
					 ULongPtrArray *distr_column_pos_array,
					 IMdIdArray *distr_opclasses, BOOL is_temporary,
					 BOOL has_oids,
					 IMDRelation::Erelstoragetype rel_storage_type,
					 ULongPtrArray *src_colids_array,
					 IntPtrArray *vartypemod_array);

	// dtor
	~CDXLPhysicalCTAS() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// column descriptors
	CDXLColDescrArray *
	GetDXLColumnDescrArray() const
	{
		return m_col_descr_array;
	}

	// distribution type
	IMDRelation::Ereldistrpolicy
	Ereldistrpolicy() const
	{
		return m_rel_distr_policy;
	}

	// distribution column positions
	ULongPtrArray *
	GetDistrColPosArray() const
	{
		return m_distr_column_pos_array;
	}

	// source column ids
	ULongPtrArray *
	GetSrcColidsArray() const
	{
		return m_src_colids_array;
	}

	// list of vartypmod for target expressions
	IntPtrArray *
	GetVarTypeModArray() const
	{
		return m_vartypemod_array;
	}

	// table name
	CMDName *
	GetMdNameSchema() const
	{
		return m_mdname_schema;
	}

	// table name
	CMDName *
	MdName() const
	{
		return m_mdname_rel;
	}

	// is temporary
	BOOL
	IsTemporary() const
	{
		return m_is_temp_table;
	}

	// CTAS storage options
	CDXLCtasStorageOptions *
	GetDxlCtasStorageOption() const
	{
		return m_dxl_ctas_storage_option;
	}

	IMdIdArray *
	GetDistrOpclasses() const
	{
		return m_distr_opclasses;
	}
	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLPhysicalCTAS *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalCTAS == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLPhysicalCTAS *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalCTAS_H

// EOF
