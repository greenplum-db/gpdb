//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
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
	// private copy ctor
	CDXLScalarAssertConstraintList(const CDXLScalarAssertConstraintList &);


public:
	// ctor
	explicit CDXLScalarAssertConstraintList(CMemoryPool *mp);

	// ident accessors
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const;
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
