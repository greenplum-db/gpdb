//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataObject.cpp
//
//	@doc:
//		Implementation of the base SAX parse handler class for parsing metadata objects.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataObject::CParseHandlerMetadataObject
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataObject::CParseHandlerMetadataObject(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_imd_obj(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataObject::~CParseHandlerMetadataObject
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataObject::~CParseHandlerMetadataObject()
{
	CRefCount::SafeRelease(m_imd_obj);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataObject::GetImdObj
//
//	@doc:
//		Returns the constructed metadata object.
//
//---------------------------------------------------------------------------
IMDCacheObject *
CParseHandlerMetadataObject::GetImdObj() const
{
	return m_imd_obj;
}



// EOF
