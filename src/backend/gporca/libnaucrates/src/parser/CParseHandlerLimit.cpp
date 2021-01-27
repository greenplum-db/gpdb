//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLimit.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing Limit operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLimit.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLimit::CParseHandlerLimit
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLimit::CParseHandlerLimit(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLimit::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLimit::StartElement(const XMLCh *const,  // element_uri,
								 const XMLCh *const element_local_name,
								 const XMLCh *const,  // element_qname,
								 const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalLimit),
				 element_local_name))
	{
		// parse and create Limit operator
		m_dxl_op = (CDXLPhysicalLimit *) CDXLOperatorFactory::MakeDXLLimit(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);

		// create and activate the parse handler for the children nodes in reverse
		// order of their expected appearance

		CParseHandlerBase *offset_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitOffset),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(offset_parse_handler);

		CParseHandlerBase *count_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitCount),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(count_parse_handler);

		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		//parse handler for the properties of the operator
		CParseHandlerBase *prop_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

		// store parse handlers
		this->Append(prop_parse_handler);
		this->Append(proj_list_parse_handler);
		this->Append(child_parse_handler);
		this->Append(count_parse_handler);
		this->Append(offset_parse_handler);
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
//		CParseHandlerLimit::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLimit::EndElement(const XMLCh *const,	// element_uri,
							   const XMLCh *const element_local_name,
							   const XMLCh *const  // element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalLimit),
				 element_local_name))
	{
		GPOS_ASSERT(5 == this->Length());

		// construct node from the created child nodes
		CParseHandlerProperties *prop_parse_handler =
			dynamic_cast<CParseHandlerProperties *>((*this)[0]);

		CParseHandlerProjList *proj_list_parse_handler =
			dynamic_cast<CParseHandlerProjList *>((*this)[1]);
		CParseHandlerPhysicalOp *child_parse_handler =
			dynamic_cast<CParseHandlerPhysicalOp *>((*this)[2]);
		CParseHandlerScalarOp *count_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[3]);
		CParseHandlerScalarOp *offset_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[4]);

		// set statistics and physical properties
		CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

		// add constructed children
		AddChildFromParseHandler(proj_list_parse_handler);
		AddChildFromParseHandler(child_parse_handler);
		AddChildFromParseHandler(count_parse_handler);
		AddChildFromParseHandler(offset_parse_handler);

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
