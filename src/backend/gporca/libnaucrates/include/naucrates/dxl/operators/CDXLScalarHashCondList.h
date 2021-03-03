//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarHashCondList.h
//
//	@doc:
//		Class for representing the list of hash conditions in DXL Hash join nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarHashCondList_H
#define GPDXL_CDXLScalarHashCondList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarHashCondList
//
//	@doc:
//		Class for representing the list of hash conditions in DXL Hash join nodes.
//
//---------------------------------------------------------------------------
class CDXLScalarHashCondList : public CDXLScalar
{
private:
public:
	CDXLScalarHashCondList(CDXLScalarHashCondList &) = delete;

	// ctor
	explicit CDXLScalarHashCondList(CMemoryPool *mp);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarHashCondList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarHashCondList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarHashCondList *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarHashCondList_H

// EOF
