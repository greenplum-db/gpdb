//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 Broadcom
//
//	@filename:
//		CParseHandlerScalarParam.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing
//		scalar param operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarParam.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarParam::CParseHandlerScalarParam
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarParam::CParseHandlerScalarParam(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(nullptr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarParam::~CParseHandlerScalarParam
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarParam::~CParseHandlerScalarParam() = default;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarParam::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarParam::StartElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const,	// element_qname
									   const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarParam),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create param operator
	m_dxl_op = (CDXLScalarParam *) CDXLOperatorFactory::MakeDXLScalarParam(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarParam::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarParam::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarParam),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct scalar param node
	GPOS_ASSERT(nullptr != m_dxl_op);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp);
	m_dxl_node->SetOperator(m_dxl_op);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
