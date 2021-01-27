//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalJoin.h
//
//	@doc:
//		Class for representing DXL logical Join operators
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalJoin_H
#define GPDXL_CDXLLogicalJoin_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalJoin
//
//	@doc:
//		Class for representing DXL logical Join operators
//
//---------------------------------------------------------------------------
class CDXLLogicalJoin : public CDXLLogical
{
private:
	// private copy ctor
	CDXLLogicalJoin(CDXLLogicalJoin &);

	// join type (inner, outer, ...)
	EdxlJoinType m_join_type;

public:
	// ctor/dtor
	CDXLLogicalJoin(CMemoryPool *, EdxlJoinType);

	// accessors
	Edxlopid GetDXLOperator() const;

	const CWStringConst *GetOpNameStr() const;

	// join type
	EdxlJoinType GetJoinType() const;

	const CWStringConst *GetJoinTypeNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalJoin *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalJoin == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLLogicalJoin *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalJoin_H

// EOF
