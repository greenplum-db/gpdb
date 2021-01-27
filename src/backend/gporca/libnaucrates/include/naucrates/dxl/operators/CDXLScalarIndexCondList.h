//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarIndexCondList.h
//
//	@doc:
//		Class for representing the list of index conditions in DXL index scan
//		operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarIndexCondList_H
#define GPDXL_CDXLScalarIndexCondList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarIndexCondList
//
//	@doc:
//		Class for representing the list of index conditions in DXL index scan
// 		operator
//
//---------------------------------------------------------------------------
class CDXLScalarIndexCondList : public CDXLScalar
{
private:
	// private copy ctor
	CDXLScalarIndexCondList(CDXLScalarIndexCondList &);

public:
	// ctor
	explicit CDXLScalarIndexCondList(CMemoryPool *mp);

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the operator
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLScalarIndexCondList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarIndexCondList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarIndexCondList *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		GPOS_ASSERT(!"Invalid function call on a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarIndexCondList_H

// EOF
