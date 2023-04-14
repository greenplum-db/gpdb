//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowFrame.h
//
//	@doc:
//		Class for representing DXL window frame
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLWindowFrame_H
#define GPDXL_CDXLWindowFrame_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

enum EdxlFrameSpec
{
	EdxlfsRow = 0,
	EdxlfsRange,
	EdxlfsGroups,
	EdxlfsSentinel
};

enum EdxlFrameExclusionStrategy
{
	EdxlfesNone = 0,
	EdxlfesNulls,
	EdxlfesCurrentRow,
	EdxlfesGroup,
	EdxlfesTies,
	EdxlfesSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLWindowFrame
//
//	@doc:
//		Class for representing DXL window frame
//
//---------------------------------------------------------------------------
class CDXLWindowFrame : public CRefCount
{
private:
	// row or range based window specification method
	EdxlFrameSpec m_dxl_win_frame_spec;

	// exclusion strategy
	EdxlFrameExclusionStrategy m_dxl_frame_exclusion_strategy;

	// scalar value representing the boundary leading
	CDXLNode *m_dxlnode_leading;

	// scalar value representing the boundary trailing
	CDXLNode *m_dxlnode_trailing;

	// in_range function for startOffset
	OID m_start_in_range_func;

	// in_range function for endOffset
	OID m_end_in_range_func;

	// collation for in_range tests
	OID m_in_range_coll;

	// use ASC sort order for in_range tests
	BOOL m_in_range_asc;

	// nulls sort first for in_range tests
	BOOL m_in_range_nulls_first;

public:
	CDXLWindowFrame(const CDXLWindowFrame &) = delete;

	// ctor
	CDXLWindowFrame(EdxlFrameSpec edxlfs,
					EdxlFrameExclusionStrategy frame_exc_strategy,
					CDXLNode *dxlnode_leading, CDXLNode *dxlnode_trailing,
					OID start_in_range_func, OID end_in_range_func,
					OID in_range_coll, bool in_range_asc,
					bool in_range_nulls_first);

	//dtor
	~CDXLWindowFrame() override;

	EdxlFrameSpec
	ParseDXLFrameSpec() const
	{
		return m_dxl_win_frame_spec;
	}

	// exclusion strategy
	EdxlFrameExclusionStrategy
	ParseFrameExclusionStrategy() const
	{
		return m_dxl_frame_exclusion_strategy;
	}

	// return window boundary trailing
	CDXLNode *
	PdxlnTrailing() const
	{
		return m_dxlnode_trailing;
	}

	// return window boundary leading
	CDXLNode *
	PdxlnLeading() const
	{
		return m_dxlnode_leading;
	}

	OID
	PdxlnStartInRangeFunc() const
	{
		return m_start_in_range_func;
	}

	OID
	PdxlnEndInRangeFunc() const
	{
		return m_end_in_range_func;
	}

	OID
	PdxlnInRangeColl() const
	{
		return m_in_range_coll;
	}

	BOOL
	PdxlnInRangeAsc() const
	{
		return m_in_range_asc;
	}

	BOOL
	PdxlnInRangeNullsFirst() const
	{
		return m_in_range_nulls_first;
	}

	// return the string representation of the exclusion strategy
	static const CWStringConst *PstrES(EdxlFrameExclusionStrategy edxles);

	// return the string representation of the frame specification (row or range)
	static const CWStringConst *PstrFS(EdxlFrameSpec edxlfs);

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer) const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLWindowFrame_H

// EOF
