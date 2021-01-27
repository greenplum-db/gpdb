//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarNullTest.h
//
//	@doc:
//		Class for representing DXL NullTest that tests if the given expression
//		is null or is not null.
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarNullTest_H
#define GPDXL_CDXLScalarNullTest_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarNullTest
//
//	@doc:
//		Class for representing DXL NullTest
//
//---------------------------------------------------------------------------
class CDXLScalarNullTest : public CDXLScalar
{
private:
	// is nul or is not null operation
	BOOL m_is_null;

	// private copy ctor
	CDXLScalarNullTest(const CDXLScalarNullTest &);

public:
	// ctor/
	CDXLScalarNullTest(CMemoryPool *mp, BOOL is_null);

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator name
	const CWStringConst *GetOpNameStr() const;

	// NullTest operator type
	BOOL IsNullTest() const;

	// name of the operator
	const CWStringConst *PstrTestName() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarNullTest *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarNullTest == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarNullTest *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarNullTest_H

// EOF
