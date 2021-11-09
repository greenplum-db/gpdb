//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowKey.h
//
//	@doc:
//		Class for representing DXL window key
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLWindowKey_H
#define GPDXL_CDXLWindowKey_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLWindowFrame.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLWindowKey
//
//	@doc:
//		Class for representing DXL window key
//
//---------------------------------------------------------------------------
class CDXLWindowKey : public CRefCount
{
private:
	// window frame associated with the window key
	CDXLWindowFrame *m_window_frame_dxl{nullptr};

	// sorting columns
	CDXLNode *m_sort_col_list_dxlnode{nullptr};

public:
	CDXLWindowKey(const CDXLWindowKey &) = delete;

	// ctor
	CDXLWindowKey();

	// dtor
	~CDXLWindowKey() override;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *) const;

	// set window frame definition
	void SetWindowFrame(CDXLWindowFrame *window_frame);

	// return window frame
	CDXLWindowFrame *
	GetWindowFrame() const
	{
		return m_window_frame_dxl;
	}

	// set the list of sort columns
	void SetSortColList(CDXLNode *sort_col_list_dxlnode);

	// sort columns
	CDXLNode *
	GetSortColListDXL() const
	{
		return m_sort_col_list_dxlnode;
	}
};

using CDXLWindowKeyArray = CDynamicPtrArray<CDXLWindowKey, CleanupRelease>;
}  // namespace gpdxl
#endif	// !GPDXL_CDXLWindowKey_H

// EOF
