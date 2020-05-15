//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerCondList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing condition lists.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerCondList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarHashCondList.h"
#include "naucrates/dxl/operators/CDXLScalarMergeCondList.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCondList::CParseHandlerCondList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerCondList::CParseHandlerCondList
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
//		CParseHandlerCondList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCondList::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarHashCondList), element_local_name))
	{
		// start the hash cond list
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode (m_mp, GPOS_NEW(m_mp) CDXLScalarHashCondList(m_mp));
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMergeCondList), element_local_name))
	{
		// start the merge cond list
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode (m_mp, GPOS_NEW(m_mp) CDXLScalarMergeCondList(m_mp));
	}
	else
	{
		// we must have seen a cond list already and initialized the cond list node
		GPOS_ASSERT(NULL != m_dxl_node);
		// start new hash cond element
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);
		
		// store parse handler
		this->Append(child_parse_handler);
		
		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCondList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCondList::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarHashCondList), element_local_name) &&
			0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarMergeCondList), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	const ULONG arity = this->Length();
	// add conditions from child parse handlers
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		
		AddChildFromParseHandler(child_parse_handler);
	}
		
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
