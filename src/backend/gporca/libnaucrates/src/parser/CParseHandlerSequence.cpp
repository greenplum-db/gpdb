//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerSequence.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing sequence 
//		operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSequence.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalDynamicTableScan.h"
#include "naucrates/dxl/operators/CDXLPhysicalSequence.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSequence::CParseHandlerSequence
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerSequence::CParseHandlerSequence
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_is_inside_sequence(false)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSequence::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSequence::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if (!m_is_inside_sequence && 0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSequence), element_local_name))
	{
		// new sequence operator
		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler =
				CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		//parse handler for the properties of the operator
		CParseHandlerBase *prop_parse_handler =
				CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

		// store child parse handlers in array
		this->Append(prop_parse_handler);
		this->Append(proj_list_parse_handler);
		m_is_inside_sequence = true;
	}
	else
	{
		// child of the sequence operator
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, element_local_name, m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);
		this->Append(child_parse_handler);
		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}	
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSequence::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSequence::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSequence), 
										element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node from the created child nodes
	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
		
	CDXLPhysicalSequence *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalSequence(m_mp);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);	

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add project list
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	GPOS_ASSERT(NULL != proj_list_parse_handler);
	AddChildFromParseHandler(proj_list_parse_handler);
			
	const ULONG size = this->Length();
	// add constructed children from child parse handlers
	for (ULONG idx = 2; idx < size; idx++)
	{
		CParseHandlerPhysicalOp *child_parse_handler = dynamic_cast<CParseHandlerPhysicalOp*>((*this)[idx]);
		GPOS_ASSERT(NULL != child_parse_handler);
		AddChildFromParseHandler(child_parse_handler);
	}

#ifdef GPOS_DEBUG
	dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG
	
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

