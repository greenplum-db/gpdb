//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalCTEAnchor.h
//
//	@doc:
//		Class for representing DXL logical CTE anchors
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalCTEAnchor_H
#define GPDXL_CDXLLogicalCTEAnchor_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalCTEAnchor
//
//	@doc:
//		Class for representing DXL logical CTE producers
//
//---------------------------------------------------------------------------
class CDXLLogicalCTEAnchor : public CDXLLogical
{
private:
	// cte id
	ULONG m_id;

	// private copy ctor
	CDXLLogicalCTEAnchor(CDXLLogicalCTEAnchor &);

public:
	// ctor
	CDXLLogicalCTEAnchor(CMemoryPool *mp, ULONG id);

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// cte identifier
	ULONG
	Id() const
	{
		return m_id;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;


#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLLogicalCTEAnchor *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalCTEAnchor == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLLogicalCTEAnchor *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalCTEAnchor_H

// EOF
