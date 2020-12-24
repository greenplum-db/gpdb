//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarAssertConstraintList.h
//
//	@doc:
//		Class for representing DXL scalar assert constraint lists for Assert operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarAssertConstraintList_H
#define GPDXL_CDXLScalarAssertConstraintList_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarAssertConstraintList
//
//	@doc:
//		Class for representing DXL scalar assert constraint lists
//
//---------------------------------------------------------------------------
class CDXLScalarAssertConstraintList : public CDXLScalar
{
private:
public:
	CDXLScalarAssertConstraintList(const CDXLScalarAssertConstraintList &) =
		delete;

	// ctor
	explicit CDXLScalarAssertConstraintList(CMemoryPool *mp);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarAssertConstraintList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarAssertConstraintList ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarAssertConstraintList *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarAssertConstraintList_H

// EOF
