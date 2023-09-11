//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalAppend.h
//
//	@doc:
//		Class for representing DXL append operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalAppend_H
#define GPDXL_CDXLPhysicalAppend_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"

namespace gpdxl
{
// indices of append elements in the children array
enum Edxlappend
{
	EdxlappendIndexProjList = 0,
	EdxlappendIndexFilter,
	EdxlappendIndexFirstChild,
	EdxlappendIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalAppend
//
//	@doc:
//		Class for representing DXL Append operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalAppend : public CDXLPhysical
{
private:
	// is the append node used in an update/delete statement
	BOOL m_used_in_upd_del = false;

	// TODO:  - Apr 12, 2011; find a better name (and comments) for this variable
	BOOL m_is_zapped = false;

public:
	CDXLPhysicalAppend(const CDXLPhysicalAppend &) = delete;

	// ctor/dtor
	CDXLPhysicalAppend(CMemoryPool *mp, BOOL fIsTarget, BOOL fIsZapped);

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	BOOL IsUsedInUpdDel() const;
	BOOL IsZapped() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalAppend *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalAppend == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalAppend *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalAppend_H

// EOF
