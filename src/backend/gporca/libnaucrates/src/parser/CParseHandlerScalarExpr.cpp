//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarExpr.cpp
//
//	@doc:
//		@see CParseHandlerScalarExpr.h
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarExpr.h"

#include "naucrates/dxl/parser/CParseHandlerOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::CParseHandlerScalarExpr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarExpr::CParseHandlerScalarExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_node(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::~CParseHandlerScalarExpr
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerScalarExpr::~CParseHandlerScalarExpr()
{
	CRefCount::SafeRelease(m_dxl_node);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::CreateDXLNode
//
//	@doc:
//		Root of constructed DXL expression
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerScalarExpr::CreateDXLNode() const
{
	return m_dxl_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerScalarExpr::GetParseHandlerType() const
{
	return EdxlphScalarExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag for a scalar expression.
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarExpr::StartElement(const XMLCh *const,
									  const XMLCh *const element_local_name,
									  const XMLCh *const, const Attributes &)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarExpr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	GPOS_ASSERT(NULL != m_mp);

	// parse handler for child node
	CParseHandlerBase *child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr,
			this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);
	Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarExpr::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag.
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarExpr::EndElement(const XMLCh *const,	 //= element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname,
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarExpr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerScalarOp *child_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	// extract constructed element
	GPOS_ASSERT(NULL != child_parse_handler &&
				NULL != child_parse_handler->CreateDXLNode());
	m_dxl_node = child_parse_handler->CreateDXLNode();
	m_dxl_node->AddRef();

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
