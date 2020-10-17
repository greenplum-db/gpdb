//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
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

	// private copy ctor
	CDXLScalarAssertConstraint(const CDXLScalarAssertConstraint &);

public:
	// ctor
	CDXLScalarAssertConstraint(CMemoryPool *mp, CWStringBase *error_msg);

	// dtor
	virtual ~CDXLScalarAssertConstraint();

	// ident accessors
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// error message
	CWStringBase *GetErrorMsgStr() const;

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
