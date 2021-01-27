//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarLimitOffset.h
//
//	@doc:
//		Class for representing DXL scalar LimitOffset (This is only used by CDXLPhysicalLimit)
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarLimitOffset_H
#define GPDXL_CDXLScalarLimitOffset_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarLimitOffset
//
//	@doc:
//		Class for representing DXL scalar LimitOffset
//
//---------------------------------------------------------------------------
class CDXLScalarLimitOffset : public CDXLScalar
{
private:
	// private copy ctor
	CDXLScalarLimitOffset(const CDXLScalarLimitOffset &);

public:
	// ctor/dtor
	explicit CDXLScalarLimitOffset(CMemoryPool *mp);

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarLimitOffset *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarLimitOffset == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarLimitOffset *>(dxl_op);
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
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarLimitOffset_H

// EOF
