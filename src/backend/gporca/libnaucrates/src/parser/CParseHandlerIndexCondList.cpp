//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexCondList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the index
//		condition lists
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerIndexCondList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarIndexCondList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexCondList::CParseHandlerIndexCondList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerIndexCondList::CParseHandlerIndexCondList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexCondList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexCondList::StartElement(const XMLCh *const element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const element_qname,
										 const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarIndexCondList),
				 element_local_name))
	{
		// start the index condition list
		m_dxl_node = GPOS_NEW(m_mp)
			CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIndexCondList(m_mp));
	}
	else
	{
		if (NULL == m_dxl_node)
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   str->GetBuffer());
		}

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
//		CParseHandlerIndexCondList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexCondList::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	GPOS_ASSERT(NULL != m_dxl_node);

	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarIndexCondList),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// add constructed children from child parse handlers
	for (ULONG ul = 0; ul < this->Length(); ul++)
	{
		CParseHandlerScalarOp *op_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(op_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
