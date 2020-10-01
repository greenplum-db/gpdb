//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLPhysicalAbstractBitmapScan.cpp
//
//	@doc:
//		Parent class for representing DXL bitmap table scan operators, both
//		not partitioned and dynamic.
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalAbstractBitmapScan.h"

#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAbstractBitmapScan::~CDXLPhysicalAbstractBitmapScan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalAbstractBitmapScan::~CDXLPhysicalAbstractBitmapScan()
{
	m_dxl_table_descr->Release();
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAbstractBitmapScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalAbstractBitmapScan::AssertValid(const CDXLNode *node,
											BOOL validate_children) const
{
	GPOS_ASSERT(4 == node->Arity());

	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(node, validate_children);

	GPOS_ASSERT(EdxlopScalarRecheckCondFilter ==
				(*node)[2]->GetOperator()->GetDXLOperator());

	// assert bitmap access path is valid
	CDXLNode *bitmap_dxlnode = (*node)[3];
	GPOS_ASSERT(EdxlopScalarBitmapIndexProbe ==
					bitmap_dxlnode->GetOperator()->GetDXLOperator() ||
				EdxlopScalarBitmapBoolOp ==
					bitmap_dxlnode->GetOperator()->GetDXLOperator());

	// assert validity of table descriptor
	GPOS_ASSERT(NULL != m_dxl_table_descr);
	GPOS_ASSERT(NULL != m_dxl_table_descr->MdName());
	GPOS_ASSERT(m_dxl_table_descr->MdName()->GetMDName()->IsValid());

	if (validate_children)
	{
		bitmap_dxlnode->GetOperator()->AssertValid(bitmap_dxlnode,
												   validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
