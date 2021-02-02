//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalSequence.h
//
//	@doc:
//		Class for representing DXL physical sequence operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalSequence_H
#define GPDXL_CDXLPhysicalSequence_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLSpoolInfo.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalSequence
//
//	@doc:
//		Class for representing DXL physical sequence operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalSequence : public CDXLPhysical
{
private:
public:
	CDXLPhysicalSequence(CDXLPhysicalSequence &) = delete;

	// ctor
	CDXLPhysicalSequence(CMemoryPool *mp);

	// dtor
	~CDXLPhysicalSequence() override;

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalSequence *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalSequence == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalSequence *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalSequence_H

// EOF
