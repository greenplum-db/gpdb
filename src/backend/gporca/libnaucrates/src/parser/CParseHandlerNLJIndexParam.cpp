//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerNLJIndexParam.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing a NLJ index Param
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/parser/CParseHandlerNLJIndexParam.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE


// ctor
CParseHandlerNLJIndexParam::CParseHandlerNLJIndexParam(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_manager,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_manager, parse_handler_root),
	  m_nest_param_colref_dxl(NULL)
{
}


// dtor
CParseHandlerNLJIndexParam::~CParseHandlerNLJIndexParam()
{
	m_nest_param_colref_dxl->Release();
}


// processes a Xerces start element event
void
CParseHandlerNLJIndexParam::StartElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname,
										 const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_nest_param_colref_dxl = CDXLOperatorFactory::MakeDXLColRef(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenNLJIndexParam);
}


// processes a Xerces end element event
void
CParseHandlerNLJIndexParam::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
