//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalSelect.h
//
//	@doc:
//		Class for representing DXL logical select operators
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalSelect_H
#define GPDXL_CDXLLogicalSelect_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalSelect
//
//	@doc:
//		Class for representing DXL Logical Select operators
//
//---------------------------------------------------------------------------
class CDXLLogicalSelect : public CDXLLogical
{
private:
	// private copy ctor
	CDXLLogicalSelect(CDXLLogicalSelect &);

public:
	// ctor/dtor
	explicit CDXLLogicalSelect(CMemoryPool *);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLLogicalSelect *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalSelect == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalSelect *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalSelect_H

// EOF
