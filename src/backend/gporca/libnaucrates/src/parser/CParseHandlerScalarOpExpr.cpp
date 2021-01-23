//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarOpExpr.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar OpExpr.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarOpExpr.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpExpr::CParseHandlerScalarOpExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarOpExpr::CParseHandlerScalarOpExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_num_of_children(0)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpExpr::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOpExpr::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarOpExpr),
				 element_local_name) &&
		(NULL == m_dxl_node))
	{
		// parse and create scalar OpExpr
		CDXLScalarOpExpr *dxl_op =
			(CDXLScalarOpExpr *) CDXLOperatorFactory::MakeDXLOpExpr(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

		// construct node from the created child nodes
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else if (NULL != m_dxl_node)
	{
		if (2 > m_num_of_children)
		{
			CParseHandlerBase *op_parse_handler =
				CParseHandlerFactory::GetParseHandler(
					m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
					m_parse_handler_mgr, this);

			m_parse_handler_mgr->ActivateParseHandler(op_parse_handler);

			// store parse handlers
			this->Append(op_parse_handler);

			op_parse_handler->startElement(element_uri, element_local_name,
										   element_qname, attrs);

			m_num_of_children++;
		}
		else
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   str->GetBuffer());
		}
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);

		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLIncorrectNumberOfChildren,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpExpr::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOpExpr::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarOpExpr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG arity = this->Length();
	GPOS_ASSERT(1 == arity || 2 == arity);

	// add constructed children from child parse handlers
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *op_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(op_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
