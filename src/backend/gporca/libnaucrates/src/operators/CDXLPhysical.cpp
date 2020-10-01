//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysical.cpp
//
//	@doc:
//		Implementation of DXL physical operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysical::CDXLPhysical
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysical::CDXLPhysical(CMemoryPool *mp) : CDXLOperator(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysical::~CDXLPhysical
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysical::~CDXLPhysical()
{
}

//---------------------------------------------------------------------------
//      @function:
//              CDXLPhysical::GetDXLOperatorType
//
//      @doc:
//              Operator Type
//
//---------------------------------------------------------------------------
Edxloptype
CDXLPhysical::GetDXLOperatorType() const
{
	return EdxloptypePhysical;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysical::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysical::AssertValid(const CDXLNode *node, BOOL validate_children) const
{
	GPOS_ASSERT(NULL != node);

	GPOS_ASSERT(2 <= node->Arity());

	CDXLNode *proj_list_dxlnode = (*node)[0];
	CDXLNode *filter_dxlnode = (*node)[1];

	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list_dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopScalarFilter ==
				filter_dxlnode->GetOperator()->GetDXLOperator());

	if (validate_children)
	{
		proj_list_dxlnode->GetOperator()->AssertValid(proj_list_dxlnode,
													  validate_children);
		filter_dxlnode->GetOperator()->AssertValid(filter_dxlnode,
												   validate_children);
	}
}
#endif	// GPOS_DEBUG


// EOF
