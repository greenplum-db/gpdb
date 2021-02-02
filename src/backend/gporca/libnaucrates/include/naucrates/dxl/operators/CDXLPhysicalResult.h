//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalResult.h
//
//	@doc:
//		Class for representing DXL physical result operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalResult_H
#define GPDXL_CDXLPhysicalResult_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// indices of result elements in the children array
enum Edxlresult
{
	EdxlresultIndexProjList = 0,
	EdxlresultIndexFilter,
	EdxlresultIndexOneTimeFilter,
	EdxlresultIndexChild,
	EdxlresultIndexSentinel
};
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalResult
//
//	@doc:
//		Class for representing DXL result operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalResult : public CDXLPhysical
{
private:
public:
	CDXLPhysicalResult(CDXLPhysicalResult &) = delete;

	// ctor/dtor
	explicit CDXLPhysicalResult(CMemoryPool *mp);

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalResult *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalResult == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalResult *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalResult_H

// EOF
