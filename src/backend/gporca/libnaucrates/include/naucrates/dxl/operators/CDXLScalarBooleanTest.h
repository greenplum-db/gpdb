//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarBooleanTest.h
//
//	@doc:
//		Class for representing DXL BooleanTest
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarBooleanTest_H
#define GPDXL_CDXLScalarBooleanTest_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
using namespace gpos;

enum EdxlBooleanTestType
{
	EdxlbooleantestIsTrue,
	EdxlbooleantestIsNotTrue,
	EdxlbooleantestIsFalse,
	EdxlbooleantestIsNotFalse,
	EdxlbooleantestIsUnknown,
	EdxlbooleantestIsNotUnknown,
	EdxlbooleantestSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarBooleanTest
//
//	@doc:
//		Class for representing DXL BooleanTest
//
//---------------------------------------------------------------------------
class CDXLScalarBooleanTest : public CDXLScalar
{
private:
	// operator type
	const EdxlBooleanTestType m_dxl_bool_test_type;

	// name of the DXL operator name
	const CWStringConst *GetOpNameStr() const override;

public:
	CDXLScalarBooleanTest(const CDXLScalarBooleanTest &) = delete;

	// ctor/dtor
	CDXLScalarBooleanTest(CMemoryPool *mp,
						  const EdxlBooleanTestType dxl_bool_type);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// BooleanTest operator type
	EdxlBooleanTestType GetDxlBoolTypeStr() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLScalarBooleanTest *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarBooleanTest == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarBooleanTest *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarBooleanTest_H

// EOF
