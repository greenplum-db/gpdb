//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerWindowFrame.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a window frame
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerWindowFrame.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowFrame::CParseHandlerWindowFrame
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerWindowFrame::CParseHandlerWindowFrame(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_win_frame_spec(EdxlfsSentinel),
	  m_dxl_frame_exclusion_strategy(EdxlfesSentinel),
	  m_window_frame(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowFrame::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowFrame::StartElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const,	// element_qname,
									   const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
								 element_local_name))
	{
		m_dxl_win_frame_spec = CDXLOperatorFactory::ParseDXLFrameSpec(attrs);
		m_dxl_frame_exclusion_strategy =
			CDXLOperatorFactory::ParseFrameExclusionStrategy(attrs);

		// parse handler for the trailing window frame edge
		CParseHandlerBase *trailing_val_parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp,
				CDXLTokens::XmlstrToken(EdxltokenScalarWindowFrameTrailingEdge),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			trailing_val_parse_handler_base);

		// parse handler for the leading scalar values
		CParseHandlerBase *leading_val_parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp,
				CDXLTokens::XmlstrToken(EdxltokenScalarWindowFrameLeadingEdge),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			leading_val_parse_handler_base);

		this->Append(leading_val_parse_handler_base);
		this->Append(trailing_val_parse_handler_base);
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
//		CParseHandlerWindowFrame::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowFrame::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	GPOS_ASSERT(NULL == m_window_frame);
	GPOS_ASSERT(2 == this->Length());

	CParseHandlerScalarOp *trailing_val_parse_handler_base =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	GPOS_ASSERT(NULL != trailing_val_parse_handler_base);
	CDXLNode *dxlnode_trailing =
		trailing_val_parse_handler_base->CreateDXLNode();
	dxlnode_trailing->AddRef();

	CParseHandlerScalarOp *leading_val_parse_handler_base =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[1]);
	GPOS_ASSERT(NULL != leading_val_parse_handler_base);
	CDXLNode *dxlnode_leading = leading_val_parse_handler_base->CreateDXLNode();
	dxlnode_leading->AddRef();

	m_window_frame = GPOS_NEW(m_mp) CDXLWindowFrame(
		m_mp, m_dxl_win_frame_spec, m_dxl_frame_exclusion_strategy,
		dxlnode_leading, dxlnode_trailing);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
