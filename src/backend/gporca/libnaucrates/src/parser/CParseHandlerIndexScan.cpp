//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for the index scan operator
//
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerIndexScan.h"

#include "naucrates/dxl/parser/CParseHandlerIndexCondList.h"
#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerIndexDescr.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLPhysicalDynamicIndexScan.h"
#include "naucrates/dxl/operators/CDXLPhysicalIndexOnlyScan.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexScan::CParseHandlerIndexScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerIndexScan::CParseHandlerIndexScan
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_index_scan_dir(EdxlisdSentinel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexScan::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	StartElementHelper(element_local_name, attrs, EdxltokenPhysicalIndexScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexScan::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	EndElementHelper(element_local_name, EdxltokenPhysicalIndexScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexScan::StartElementHelper
//
//	@doc:
//		Common StartElement functionality for IndexScan and IndexOnlyScan
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexScan::StartElementHelper
	(
	const XMLCh* const element_local_name,
	const Attributes& attrs,
	Edxltoken token_type
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(token_type), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// get the index scan direction from the attribute
	const XMLCh *index_scan_direction = CDXLOperatorFactory::ExtractAttrValue
																	(
																	attrs,
																	EdxltokenIndexScanDirection,
																	token_type
																	);
	m_index_scan_dir = CDXLOperatorFactory::ParseIndexScanDirection
										(
										index_scan_direction,
										CDXLTokens::GetDXLTokenStr(token_type)
										);
	GPOS_ASSERT(EdxlisdSentinel != m_index_scan_dir);

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

	CParseHandlerBase *table_descr_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenTableDescr), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(table_descr_parse_handler);

	// parse handler for the index descriptor
	CParseHandlerBase *index_descr_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenIndexDescr), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(index_descr_parse_handler);

	// parse handler for the index condition list
	CParseHandlerBase *index_condition_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarIndexCondList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(index_condition_list_parse_handler);

	// parse handler for the filter
	CParseHandlerBase *filter_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFilter), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(filter_parse_handler);

	// parse handler for the proj list
	CParseHandlerBase *proj_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

	//parse handler for the properties of the operator
	CParseHandlerBase *prop_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	// store parse handlers
	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(filter_parse_handler);
	this->Append(index_condition_list_parse_handler);
	this->Append(index_descr_parse_handler);
	this->Append(table_descr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexScan::EndElementHelper
//
//	@doc:
//		Common EndElement functionality for IndexScan and IndexOnlyScan
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexScan::EndElementHelper
	(
	const XMLCh* const element_local_name,
	Edxltoken token_type,
	ULONG part_idx_id,
	ULONG part_idx_id_printable
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(token_type), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node from the created child nodes
	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	CParseHandlerFilter *filter_parse_handler = dynamic_cast<CParseHandlerFilter *>((*this)[2]);
	CParseHandlerIndexCondList *index_condition_list_parse_handler = dynamic_cast<CParseHandlerIndexCondList *>((*this)[3]);
	CParseHandlerIndexDescr *index_descr_parse_handler = dynamic_cast<CParseHandlerIndexDescr *>((*this)[4]);
	CParseHandlerTableDescr *table_descr_parse_handler = dynamic_cast<CParseHandlerTableDescr *>((*this)[5]);

	CDXLTableDescr *dxl_table_descr = table_descr_parse_handler->GetDXLTableDescr();
	dxl_table_descr->AddRef();

	CDXLIndexDescr *dxl_index_descr = index_descr_parse_handler->GetDXLIndexDescr();
	dxl_index_descr->AddRef();

	CDXLPhysical *dxl_op = NULL;
	if (EdxltokenPhysicalIndexOnlyScan == token_type)
	{
		dxl_op = GPOS_NEW(m_mp) CDXLPhysicalIndexOnlyScan(m_mp, dxl_table_descr, dxl_index_descr, m_index_scan_dir);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else if (EdxltokenPhysicalIndexScan == token_type)
	{
		dxl_op = GPOS_NEW(m_mp) CDXLPhysicalIndexScan(m_mp, dxl_table_descr, dxl_index_descr, m_index_scan_dir);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else
	{
		GPOS_ASSERT(EdxltokenPhysicalDynamicIndexScan == token_type);

		dxl_op = GPOS_NEW(m_mp) CDXLPhysicalDynamicIndexScan(m_mp, dxl_table_descr, part_idx_id, part_idx_id_printable, dxl_index_descr, m_index_scan_dir);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);
	AddChildFromParseHandler(index_condition_list_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
