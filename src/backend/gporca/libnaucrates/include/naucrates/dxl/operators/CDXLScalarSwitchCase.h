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
public:
	CDXLScalarSwitchCase(const CDXLScalarSwitchCase &) = delete;

	// ctor
	explicit CDXLScalarSwitchCase(CMemoryPool *mp);

	//dtor
	~CDXLScalarSwitchCase() override = default;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// DXL Operator ID
	Edxlopid GetDXLOperator() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLScalarSwitchCase *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSwitchCase == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSwitchCase *>(dxl_op);
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
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarSwitchCase_H

// EOF
