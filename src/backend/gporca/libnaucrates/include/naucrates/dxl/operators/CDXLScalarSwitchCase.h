//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarSwitchCase.h
//
//	@doc:
//
//		Class for representing a single DXL switch case
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSwitchCase_H
#define GPDXL_CDXLScalarSwitchCase_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSwitchCase
//
//	@doc:
//		Class for representing DXL Switch Case
//
//---------------------------------------------------------------------------
class CDXLScalarSwitchCase : public CDXLScalar
{
private:
	// private copy ctor
	CDXLScalarSwitchCase(const CDXLScalarSwitchCase &);

public:
	// ctor
	explicit CDXLScalarSwitchCase(CMemoryPool *mp);

	//dtor
	virtual ~CDXLScalarSwitchCase()
	{
	}

	// name of the operator
	virtual const CWStringConst *GetOpNameStr() const;

	// DXL Operator ID
	virtual Edxlopid GetDXLOperator() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarSwitchCase *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSwitchCase == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSwitchCase *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarSwitchCase_H

// EOF
