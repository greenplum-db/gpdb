//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalWindow.h
//
//	@doc:
//		Class for representing DXL logical window operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalWindow_H
#define GPDXL_CDXLLogicalWindow_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLWindowSpec.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalWindow
//
//	@doc:
//		Class for representing DXL logical window operators
//
//---------------------------------------------------------------------------
class CDXLLogicalWindow : public CDXLLogical
{
private:
	// array of window specifications
	CDXLWindowSpecArray *m_window_spec_array;

	// private copy ctor
	CDXLLogicalWindow(CDXLLogicalWindow &);

public:
	//ctor
	CDXLLogicalWindow(CMemoryPool *mp, CDXLWindowSpecArray *pdrgpdxlwinspec);

	//dtor
	virtual ~CDXLLogicalWindow();

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// number of window specs
	ULONG NumOfWindowSpecs() const;

	// return the window key at a given position
	CDXLWindowSpec *GetWindowKeyAt(ULONG idx) const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalWindow *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalWindow == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalWindow *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalWindow_H

// EOF
