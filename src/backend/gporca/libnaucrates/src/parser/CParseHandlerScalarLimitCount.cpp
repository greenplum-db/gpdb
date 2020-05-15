//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarLimitCount.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing LimitCount
//		
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarLimitCount.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarLimitCount.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarLimitCount::CParseHandlerScalarLimitCount
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarLimitCount::CParseHandlerScalarLimitCount
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
//		CParseHandlerScalarLimitCount::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarLimitCount::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarLimitCount), element_local_name))
	{
		// parse and create scalar limit count
		CDXLScalarLimitCount *dxl_op = (CDXLScalarLimitCount*) CDXLOperatorFactory::MakeDXLLimitCount(m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode (m_mp,dxl_op);
	}
	else
	{
		// we must have seen a LIMITCOUNT already and initialized its corresponding node
		if (NULL == m_dxl_node)
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
		}
		// install a scalar element parser for parsing the limit count element
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarLimitCount::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarLimitCount::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarLimitCount), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	const ULONG size = this->Length();
	if (0 < size)
	{
		GPOS_ASSERT(1 == size);
		// limit count node was not empty
		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);

		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
