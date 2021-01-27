//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarIfStmt.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for an if statement.
//---------------------------------------------------------------------------


#include "naucrates/dxl/parser/CParseHandlerScalarIfStmt.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIfStmt::CParseHandlerScalarIfStmt
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarIfStmt::CParseHandlerScalarIfStmt(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIfStmt::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarIfStmt::StartElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const,	 // element_qname,
										const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarIfStmt),
								 element_local_name))
	{
		// parse and create scalar if statment
		CDXLScalarIfStmt *dxl_op =
			(CDXLScalarIfStmt *) CDXLOperatorFactory::MakeDXLIfStmt(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

		// construct node
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

		// create and activate the parse handler for the children nodes in reverse
		// order of their expected appearance

		// parse handler for handling else result expression scalar node
		CParseHandlerBase *else_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(else_parse_handler);

		// parse handler for handling result expression scalar node
		CParseHandlerBase *result_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(result_parse_handler);

		// parse handler for the when condition clause
		CParseHandlerBase *when_cond_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(when_cond_parse_handler);

		// store parse handlers
		this->Append(when_cond_parse_handler);
		this->Append(result_parse_handler);
		this->Append(else_parse_handler);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarIfStmt::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarIfStmt::EndElement(const XMLCh *const,  // element_uri
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarIfStmt),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerScalarOp *when_cond_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	CParseHandlerScalarOp *result_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[1]);
	CParseHandlerScalarOp *else_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[2]);

	// add constructed children
	AddChildFromParseHandler(when_cond_parse_handler);
	AddChildFromParseHandler(result_parse_handler);
	AddChildFromParseHandler(else_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}


// EOF
