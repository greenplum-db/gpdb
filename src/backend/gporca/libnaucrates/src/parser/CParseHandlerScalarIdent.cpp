//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarIdent.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing
//		scalar identifier  operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarIdent.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIdent::CParseHandlerScalarIdent
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarIdent::CParseHandlerScalarIdent(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIdent::~CParseHandlerScalarIdent
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarIdent::~CParseHandlerScalarIdent()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIdent::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarIdent::StartElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const,	// element_qname
									   const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarIdent),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create identifier operator
	m_dxl_op = (CDXLScalarIdent *) CDXLOperatorFactory::MakeDXLScalarIdent(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIdent::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarIdent::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarIdent),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct scalar ident node
	GPOS_ASSERT(NULL != m_dxl_op);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp);
	m_dxl_node->SetOperator(m_dxl_op);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
