//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSwitch.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for a Switch operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarSwitch.h"
#include "naucrates/dxl/parser/CParseHandlerScalarSwitchCase.h"


using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSwitch::CParseHandlerScalarSwitch
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSwitch::CParseHandlerScalarSwitch(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_type(NULL),
	  m_arg_processed(false),
	  m_default_val_processed(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSwitch::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSwitch::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSwitch),
				 element_local_name) &&
		NULL == m_mdid_type)
	{
		// parse type id
		m_mdid_type = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId,
			EdxltokenScalarSwitch);

		// construct node
		CDXLScalarSwitch *dxl_op =
			GPOS_NEW(m_mp) CDXLScalarSwitch(m_mp, m_mdid_type);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarSwitchCase),
					  element_local_name))
	{
		// we must have already seen the arg child, but have not seen the DEFAULT child
		GPOS_ASSERT(NULL != m_dxl_node && m_arg_processed &&
					!m_default_val_processed);

		// parse case
		CParseHandlerBase *parse_handler_case =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSwitchCase),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_case);

		// store parse handlers
		this->Append(parse_handler_case);

		parse_handler_case->startElement(element_uri, element_local_name,
										 element_qname, attrs);
	}
	else
	{
		GPOS_ASSERT(NULL != m_dxl_node && !m_default_val_processed);

		// parse scalar child
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handlers
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);

		if (!m_arg_processed)
		{
			// this child was the arg child
			m_arg_processed = true;
		}
		else
		{
			// that was the default expr child
			m_default_val_processed = true;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSwitch::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSwitch::EndElement(const XMLCh *const,  // element_uri
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSwitch),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG arity = this->Length();
	GPOS_ASSERT(1 < arity);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//EOF
