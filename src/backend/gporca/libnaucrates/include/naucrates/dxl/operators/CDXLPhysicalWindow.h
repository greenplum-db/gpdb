//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalWindow.h
//
//	@doc:
//		Class for representing DXL window operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalWindow_H
#define GPDXL_CDXLPhysicalWindow_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLWindowKey.h"

namespace gpdxl
{
// indices of window elements in the children array
enum Edxlwindow
{
	EdxlwindowIndexProjList = 0,
	EdxlwindowIndexFilter,
	EdxlwindowIndexChild,
	EdxlwindowIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalWindow
//
//	@doc:
//		Class for representing DXL window operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalWindow : public CDXLPhysical
{
private:
	// partition columns
	ULongPtrArray *m_part_by_colid_array;

	// window keys
	CDXLWindowKeyArray *m_dxl_window_key_array;

	// private copy ctor
	CDXLPhysicalWindow(CDXLPhysicalWindow &);

public:
	//ctor
	CDXLPhysicalWindow(CMemoryPool *mp, ULongPtrArray *part_by_colid_array,
					   CDXLWindowKeyArray *window_key_array);

	//dtor
	virtual ~CDXLPhysicalWindow();

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// number of partition columns
	ULONG PartByColsCount() const;

	// return partition columns
	const ULongPtrArray *
	GetPartByColsArray() const
	{
		return m_part_by_colid_array;
	}

	// number of window keys
	ULONG WindowKeysCount() const;

	// return the window key at a given position
	CDXLWindowKey *GetDXLWindowKeyAt(ULONG ulPos) const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalWindow *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalWindow == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalWindow *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalWindow_H

// EOF
