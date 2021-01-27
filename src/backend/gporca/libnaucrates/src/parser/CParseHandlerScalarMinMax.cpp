//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CParseHandlerScalarMinMax.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for a MinMax operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/parser/CParseHandlerScalarMinMax.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"


using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarMinMax::CParseHandlerScalarMinMax
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarMinMax::CParseHandlerScalarMinMax(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_type(NULL),
	  m_min_max_type(CDXLScalarMinMax::EmmtSentinel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarMinMax::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarMinMax::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	if (((0 ==
		  XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMin),
								   element_local_name)) ||
		 (0 ==
		  XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMax),
								   element_local_name))) &&
		CDXLScalarMinMax::EmmtSentinel == m_min_max_type)
	{
		m_min_max_type = GetMinMaxType(element_local_name);
		GPOS_ASSERT(CDXLScalarMinMax::EmmtSentinel != m_min_max_type);

		Edxltoken token_type = EdxltokenScalarMin;
		if (0 == XMLString::compareString(
					 CDXLTokens::XmlstrToken(EdxltokenScalarMax),
					 element_local_name))
		{
			token_type = EdxltokenScalarMax;
		}

		// parse type id
		m_mdid_type = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId,
			token_type);
	}
	else
	{
		// parse child
		CParseHandlerBase *op_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(op_parse_handler);

		// store parse handlers
		this->Append(op_parse_handler);

		op_parse_handler->startElement(element_uri, element_local_name,
									   element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarMinMax::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarMinMax::EndElement(const XMLCh *const,  // element_uri
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	CDXLScalarMinMax::EdxlMinMaxType min_max_type =
		GetMinMaxType(element_local_name);

	if (CDXLScalarMinMax::EmmtSentinel == min_max_type ||
		m_min_max_type != min_max_type)
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct node
	m_dxl_node = GPOS_NEW(m_mp)
		CDXLNode(m_mp, GPOS_NEW(m_mp)
						   CDXLScalarMinMax(m_mp, m_mdid_type, m_min_max_type));

	// loop over children and add them to this parsehandler
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

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarMinMax::GetMinMaxType
//
//	@doc:
//		Parse the min/max type from the attribute value
//
//---------------------------------------------------------------------------
CDXLScalarMinMax::EdxlMinMaxType
CParseHandlerScalarMinMax::GetMinMaxType(const XMLCh *element_local_name)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMin),
								 element_local_name))
	{
		return CDXLScalarMinMax::EmmtMin;
	}

	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMax),
								 element_local_name))
	{
		return CDXLScalarMinMax::EmmtMax;
	}

	return CDXLScalarMinMax::EmmtSentinel;
}


//EOF
