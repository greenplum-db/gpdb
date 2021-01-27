//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarCaseTest.h
//
//	@doc:
//
//		Class for representing a DXL case test
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarCaseTest_H
#define GPDXL_CDXLScalarCaseTest_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarCaseTest
//
//	@doc:
//		Class for representing DXL Case Test
//
//---------------------------------------------------------------------------
class CDXLScalarCaseTest : public CDXLScalar
{
private:
	// expression type
	IMDId *m_mdid_type;

	// private copy ctor
	CDXLScalarCaseTest(const CDXLScalarCaseTest &);

public:
	// ctor
	CDXLScalarCaseTest(CMemoryPool *mp, IMDId *mdid_type);

	// dtor
	virtual ~CDXLScalarCaseTest();

	// name of the operator
	virtual const CWStringConst *GetOpNameStr() const;

	// expression type
	virtual IMDId *MdidType() const;

	// DXL Operator ID
	virtual Edxlopid GetDXLOperator() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

	// conversion function
	static CDXLScalarCaseTest *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarCaseTest == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarCaseTest *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarCaseTest_H

// EOF
