//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerSortColList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing sorting column lists.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSortColList.h"
#include "naucrates/dxl/parser/CParseHandlerSortCol.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSortColList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSortColList::CParseHandlerSortColList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerSortColList::CParseHandlerSortColList
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSortColList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSortColList::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSortColList), element_local_name))
	{
		// start the sorting column list
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode (m_mp, GPOS_NEW(m_mp) CDXLScalarSortColList(m_mp));
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSortCol), element_local_name))
	{
		// we must have seen a sorting col list already and initialized the sort col list node
		GPOS_ASSERT(NULL != m_dxl_node);

		// start new sort column
		CParseHandlerBase *sort_col_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSortCol), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(sort_col_parse_handler);
		
		// store parse handler
		this->Append(sort_col_parse_handler);
		
		sort_col_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSortColList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSortColList::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSortColList), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLUnexpectedTag,
			str->GetBuffer()
			);
	}
	
	const ULONG length = this->Length();
	// add sorting columns from child parse handlers
	for (ULONG ul = 0; ul < length; ul++)
	{
		CParseHandlerSortCol *sort_col_parse_handler = dynamic_cast<CParseHandlerSortCol *>((*this)[ul]);
		AddChildFromParseHandler(sort_col_parse_handler);
	}
	
#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG
		
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
