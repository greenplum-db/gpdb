//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarFuncExpr.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar FuncExpr.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarFuncExpr.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarFuncExpr::CParseHandlerScalarFuncExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarFuncExpr::CParseHandlerScalarFuncExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_inside_func_expr(false)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarFuncExpr::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarFuncExpr::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarFuncExpr),
				 element_local_name))
	{
		if (!m_inside_func_expr)
		{
			// parse and create scalar FuncExpr
			CDXLScalarFuncExpr *dxl_op =
				(CDXLScalarFuncExpr *) CDXLOperatorFactory::MakeDXLFuncExpr(
					m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

			// construct node from the created scalar FuncExpr
			m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

			m_inside_func_expr = true;
		}
		else
		{
			// This is to support nested FuncExpr
			CParseHandlerBase *func_parse_handler =
				CParseHandlerFactory::GetParseHandler(
					m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFuncExpr),
					m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(func_parse_handler);

			// store parse handlers
			this->Append(func_parse_handler);

			func_parse_handler->startElement(element_uri, element_local_name,
											 element_qname, attrs);
		}
	}
	else
	{
		GPOS_ASSERT(m_inside_func_expr);

		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handlers
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarFuncExpr::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarFuncExpr::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarFuncExpr),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG size = this->Length();
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
