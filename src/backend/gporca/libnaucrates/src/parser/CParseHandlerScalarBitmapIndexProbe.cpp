//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerScalarBitmapIndexProbe.cpp
//
//	@doc:
//		SAX parse handler class for parsing bitmap index probe operator nodes
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarBitmapIndexProbe.h"

#include "naucrates/dxl/operators/CDXLScalarBitmapIndexProbe.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerIndexCondList.h"
#include "naucrates/dxl/parser/CParseHandlerIndexDescr.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapIndexProbe::CParseHandlerScalarBitmapIndexProbe
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarBitmapIndexProbe::CParseHandlerScalarBitmapIndexProbe(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapIndexProbe::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBitmapIndexProbe::StartElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &	 // attrs
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBitmapIndexProbe),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

	// parse handler for the index descriptor
	CParseHandlerBase *index_descr_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenIndexDescr),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(index_descr_parse_handler);

	// parse handler for the index condition list
	CParseHandlerBase *index_cond_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarIndexCondList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(index_cond_list_parse_handler);

	// store parse handlers
	this->Append(index_cond_list_parse_handler);
	this->Append(index_descr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapIndexProbe::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBitmapIndexProbe::EndElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBitmapIndexProbe),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct node from the created child nodes
	CParseHandlerIndexCondList *index_cond_list_parse_handler =
		dynamic_cast<CParseHandlerIndexCondList *>((*this)[0]);
	CParseHandlerIndexDescr *index_descr_parse_handler =
		dynamic_cast<CParseHandlerIndexDescr *>((*this)[1]);

	CDXLIndexDescr *dxl_index_descr =
		index_descr_parse_handler->GetDXLIndexDescr();
	dxl_index_descr->AddRef();

	CDXLScalar *dxl_op =
		GPOS_NEW(m_mp) CDXLScalarBitmapIndexProbe(m_mp, dxl_index_descr);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// add children
	AddChildFromParseHandler(index_cond_list_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
