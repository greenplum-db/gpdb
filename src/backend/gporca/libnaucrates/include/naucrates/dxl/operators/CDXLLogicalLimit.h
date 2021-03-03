//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLLogicalLimit.h
//
//	@doc:
//		Class for representing DXL logical limit operators
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalLimit_H
#define GPDXL_CDXLLogicalLimit_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpdxl
{
// indices of limit elements in the children array
enum EdxlLogicalLimit
{
	EdxllogicallimitIndexSortColList = 0,
	EdxllogicallimitIndexLimitCount,
	EdxllogicallimitIndexLimitOffset,
	EdxllogicallimitIndexChildPlan
};


//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalLimit
//
//	@doc:
//		Class for representing DXL Logical limit operators
//
//---------------------------------------------------------------------------
class CDXLLogicalLimit : public CDXLLogical
{
private:
	BOOL m_top_limit_under_dml;

public:
	CDXLLogicalLimit(CDXLLogicalLimit &) = delete;

	// ctor/dtor
	CDXLLogicalLimit(CMemoryPool *mp, BOOL fNonRemovableLimit);

	~CDXLLogicalLimit() override;

	// accessors
	Edxlopid GetDXLOperator() const override;

	const CWStringConst *GetOpNameStr() const override;

	// the limit is right under a DML or CTAS
	BOOL
	IsTopLimitUnderDMLorCTAS() const
	{
		return m_top_limit_under_dml;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLLogicalLimit *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopLogicalLimit == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalLimit *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalLimit_H

// EOF
