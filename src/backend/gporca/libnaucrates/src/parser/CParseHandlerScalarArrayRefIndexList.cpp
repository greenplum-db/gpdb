//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerScalarArrayRefIndexList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing arrayref indexes
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarArrayRefIndexList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarArrayRefIndexList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarArrayRefIndexList::CParseHandlerScalarArrayRefIndexList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarArrayRefIndexList::CParseHandlerScalarArrayRefIndexList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarArrayRefIndexList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarArrayRefIndexList::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefIndexList),
				 element_local_name))
	{
		// start the index list
		const XMLCh *xmlszOpType = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenScalarArrayRefIndexListBound,
			EdxltokenScalarArrayRefIndexList);

		CDXLScalarArrayRefIndexList::EIndexListBound eilb =
			CDXLScalarArrayRefIndexList::EilbUpper;
		if (0 ==
			XMLString::compareString(
				CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefIndexListLower),
				xmlszOpType))
		{
			eilb = CDXLScalarArrayRefIndexList::EilbLower;
		}
		else if (0 != XMLString::compareString(
						  CDXLTokens::XmlstrToken(
							  EdxltokenScalarArrayRefIndexListUpper),
						  xmlszOpType))
		{
			GPOS_RAISE(
				gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue,
				CDXLTokens::GetDXLTokenStr(
					EdxltokenScalarArrayRefIndexListBound)
					->GetBuffer(),
				CDXLTokens::GetDXLTokenStr(EdxltokenScalarArrayRefIndexList)
					->GetBuffer());
		}

		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(
			m_mp, GPOS_NEW(m_mp) CDXLScalarArrayRefIndexList(m_mp, eilb));
	}
	else
	{
		// we must have seen a index list already and initialized the index list node
		GPOS_ASSERT(NULL != m_dxl_node);

		// parse scalar child
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
//		CParseHandlerScalarArrayRefIndexList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarArrayRefIndexList::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefIndexList),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// add constructed children from child parse handlers
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
