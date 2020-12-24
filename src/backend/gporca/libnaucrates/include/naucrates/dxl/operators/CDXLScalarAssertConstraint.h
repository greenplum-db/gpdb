//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarAssertConstraint.h
//
//	@doc:
//		Class for representing DXL scalar assert constraints
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarAssertConstraint_H
#define GPDXL_CDXLScalarAssertConstraint_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarAssertConstraint
//
//	@doc:
//		Class for representing individual DXL scalar assert constraints
//
//---------------------------------------------------------------------------
class CDXLScalarAssertConstraint : public CDXLScalar
{
private:
	// error message
	CWStringBase *m_error_msg;

public:
	CDXLScalarAssertConstraint(const CDXLScalarAssertConstraint &) = delete;

	// ctor
	CDXLScalarAssertConstraint(CMemoryPool *mp, CWStringBase *error_msg);

	// dtor
	~CDXLScalarAssertConstraint() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// error message
	CWStringBase *GetErrorMsgStr() const;

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
	static CDXLScalarAssertConstraint *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarAssertConstraint == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarAssertConstraint *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarAssertConstraint_H

// EOF
