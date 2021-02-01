//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogical.cpp
//
//	@doc:
//		Implementation of DXL logical operators
//
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLLogical.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogical::CDXLLogical
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLLogical::CDXLLogical(CMemoryPool *mp) : CDXLOperator(mp)
{
}

//---------------------------------------------------------------------------
//      @function:
//              CDXLLogical::GetDXLOperatorType
//
//      @doc:
//              Operator Type
//
//---------------------------------------------------------------------------
Edxloptype
CDXLLogical::GetDXLOperatorType() const
{
	return EdxloptypeLogical;
}



// EOF
