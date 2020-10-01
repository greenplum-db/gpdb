//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarWindowFrameEdge.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a window frame
//		edge
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarWindowFrameEdge.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowFrameEdge::CParseHandlerScalarWindowFrameEdge
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarWindowFrameEdge::CParseHandlerScalarWindowFrameEdge(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root, BOOL leading_edge)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_leading_edge(leading_edge)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowFrameEdge::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarWindowFrameEdge::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarWindowFrameLeadingEdge),
				 element_local_name))
	{
		GPOS_ASSERT(NULL == m_dxl_node);
		EdxlFrameBoundary dxl_frame_bound =
			CDXLOperatorFactory::ParseDXLFrameBoundary(
				attrs, EdxltokenWindowLeadingBoundary);
		m_dxl_node = GPOS_NEW(m_mp)
			CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarWindowFrameEdge(
							   m_mp, true /*fLeading*/, dxl_frame_bound));
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(
						  EdxltokenScalarWindowFrameTrailingEdge),
					  element_local_name))
	{
		GPOS_ASSERT(NULL == m_dxl_node);
		EdxlFrameBoundary dxl_frame_bound =
			CDXLOperatorFactory::ParseDXLFrameBoundary(
				attrs, EdxltokenWindowTrailingBoundary);
		m_dxl_node = GPOS_NEW(m_mp)
			CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarWindowFrameEdge(
							   m_mp, false /*fLeading*/, dxl_frame_bound));
	}
	else
	{
		// we must have seen a Window Frame Edge already and initialized its corresponding node
		if (NULL == m_dxl_node)
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   str->GetBuffer());
		}

		// install a scalar element parser for parsing the frame edge value
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowFrameEdge::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarWindowFrameEdge::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarWindowFrameLeadingEdge),
				 element_local_name) ||
		0 ==
			XMLString::compareString(
				CDXLTokens::XmlstrToken(EdxltokenScalarWindowFrameTrailingEdge),
				element_local_name))
	{
		const ULONG arity = this->Length();
		if (0 < arity)
		{
			GPOS_ASSERT(1 == arity);
			// limit count node was not empty
			CParseHandlerScalarOp *child_parse_handler =
				dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);

			AddChildFromParseHandler(child_parse_handler);
		}

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// EOF
