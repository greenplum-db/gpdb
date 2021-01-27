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

	// private copy ctor
	CDXLLogicalLimit(CDXLLogicalLimit &);

public:
	// ctor/dtor
	CDXLLogicalLimit(CMemoryPool *mp, BOOL fNonRemovableLimit);

	virtual ~CDXLLogicalLimit();

	// accessors
	Edxlopid GetDXLOperator() const;

	const CWStringConst *GetOpNameStr() const;

	// the limit is right under a DML or CTAS
	BOOL
	IsTopLimitUnderDMLorCTAS() const
	{
		return m_top_limit_under_dml;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalLimit *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalLimit == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalLimit *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalLimit_H

// EOF
