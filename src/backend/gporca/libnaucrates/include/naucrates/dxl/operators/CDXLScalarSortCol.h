//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarSortCol.h
//
//	@doc:
//		Class for representing sorting column info in DXL Sort and Motion nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSortCol_H
#define GPDXL_CDXLScalarSortCol_H


#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSortCol
//
//	@doc:
//		Sorting column info in DXL Sort and Motion nodes
//
//---------------------------------------------------------------------------
class CDXLScalarSortCol : public CDXLScalar
{
private:
	// id of the sorting column
	ULONG m_colid;

	// catalog Oid of the sorting operator
	IMDId *m_mdid_sort_op;

	// name of sorting operator
	CWStringConst *m_sort_op_name_str;

	// sort nulls before other values
	BOOL m_must_sort_nulls_first;

public:
	CDXLScalarSortCol(CDXLScalarSortCol &) = delete;

	// ctor/dtor
	CDXLScalarSortCol(CMemoryPool *mp, ULONG colid, IMDId *sort_op_id,
					  CWStringConst *pstrTypeName, BOOL fSortNullsFirst);

	~CDXLScalarSortCol() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// Id of the sorting column
	ULONG GetColId() const;

	// mdid of the sorting operator
	IMDId *GetMdIdSortOp() const;

	// whether nulls are sorted before other values
	BOOL IsSortedNullsFirst() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarSortCol *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSortCol == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSortCol *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for this operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarSortCol_H

// EOF
