//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMotion.h
//
//	@doc:
//		Base class for representing DXL motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalMotion_H
#define GPDXL_CDXLPhysicalMotion_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalMotion
//
//	@doc:
//		Base class for representing DXL motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalMotion : public CDXLPhysical
{
private:
	// private copy ctor
	CDXLPhysicalMotion(CDXLPhysicalMotion &);

	// serialize the given list of segment ids into a comma-separated string
	CWStringDynamic *GetSegIdsCommaSeparatedStr(
		const IntPtrArray *segment_ids_array) const;

	// serialize input and output segment ids into a comma-separated string
	CWStringDynamic *GetInputSegIdsStr() const;
	CWStringDynamic *GetOutputSegIdsStr() const;

protected:
	// list of input segment ids
	IntPtrArray *m_input_segids_array;

	// list of output segment ids
	IntPtrArray *m_output_segids_array;

	void SerializeSegmentInfoToDXL(CXMLSerializer *xml_serializer) const;


public:
	// ctor/dtor
	explicit CDXLPhysicalMotion(CMemoryPool *mp);

	virtual ~CDXLPhysicalMotion();

	// accessors
	const IntPtrArray *GetInputSegIdsArray() const;
	const IntPtrArray *GetOutputSegIdsArray() const;

	// setters
	void SetInputSegIds(IntPtrArray *input_segids_array);
	void SetOutputSegIds(IntPtrArray *output_segids_array);
	void SetSegmentInfo(IntPtrArray *input_segids_array,
						IntPtrArray *output_segids_array);

	// index of relational child node in the children array
	virtual ULONG GetRelationChildIdx() const = 0;

	// conversion function
	static CDXLPhysicalMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(
			EdxlopPhysicalMotionGather == dxl_op->GetDXLOperator() ||
			EdxlopPhysicalMotionBroadcast == dxl_op->GetDXLOperator() ||
			EdxlopPhysicalMotionRedistribute == dxl_op->GetDXLOperator() ||
			EdxlopPhysicalMotionRoutedDistribute == dxl_op->GetDXLOperator() ||
			EdxlopPhysicalMotionRandom == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalMotion *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalMotion_H

// EOF
