//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarWindowFrameEdge.h
//
//	@doc:
//		Class for representing a DXL scalar window frame edge
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarWindowFrameEdge_H
#define GPDXL_CDXLScalarWindowFrameEdge_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpos;

enum EdxlFrameBoundary
{
	EdxlfbUnboundedPreceding = 0,
	EdxlfbBoundedPreceding,
	EdxlfbCurrentRow,
	EdxlfbUnboundedFollowing,
	EdxlfbBoundedFollowing,
	EdxlfbDelayedBoundedPreceding,
	EdxlfbDelayedBoundedFollowing,
	EdxlfbSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarWindowFrameEdge
//
//	@doc:
//		Class for representing a DXL scalar window frame edge
//
//---------------------------------------------------------------------------
class CDXLScalarWindowFrameEdge : public CDXLScalar
{
private:
	// identify if it is a leading or trailing edge
	BOOL m_leading_edge;

	// frame boundary
	EdxlFrameBoundary m_dxl_frame_boundary;

public:
	CDXLScalarWindowFrameEdge(const CDXLScalarWindowFrameEdge &) = delete;

	// ctor
	CDXLScalarWindowFrameEdge(CMemoryPool *mp, BOOL fLeading,
							  EdxlFrameBoundary frame_boundary);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const override;

	// is it a leading or trailing edge
	BOOL
	IsLeadingEdge() const
	{
		return m_leading_edge;
	}

	// return the dxl representation the frame boundary
	EdxlFrameBoundary
	ParseDXLFrameBoundary() const
	{
		return m_dxl_frame_boundary;
	}

	// return the string representation of frame boundary
	static const CWStringConst *GetFrameBoundaryStr(
		EdxlFrameBoundary frame_boundary);

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarWindowFrameEdge *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarWindowFrameEdge == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarWindowFrameEdge *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarWindowFrameEdge_H

// EOF
