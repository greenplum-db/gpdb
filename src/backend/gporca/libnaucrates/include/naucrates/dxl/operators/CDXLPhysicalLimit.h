//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalLimit.h
//
//	@doc:
//		Class for representing DXL physical LIMIT operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalLimit_H
#define GPDXL_CDXLPhysicalLimit_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// indices of limit elements in the children array
enum EdxlLimit
{
	EdxllimitIndexProjList = 0,
	EdxllimitIndexChildPlan,
	EdxllimitIndexLimitCount,
	EdxllimitIndexLimitOffset
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalLimit
//
//	@doc:
//		Class for representing DXL physical LIMIT operator
//
//---------------------------------------------------------------------------
class CDXLPhysicalLimit : public CDXLPhysical
{
private:
public:
	CDXLPhysicalLimit(CDXLPhysicalLimit &) = delete;

	// ctor/dtor
	explicit CDXLPhysicalLimit(CMemoryPool *mp);

	Edxlopid GetDXLOperator() const override;

	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLPhysicalLimit *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalLimit == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalLimit *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalLimit_H

//EOF
