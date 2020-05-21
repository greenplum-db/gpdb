//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalSplit.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing physical split
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalSplit.h"

#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalSplit.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalSplit::CParseHandlerPhysicalSplit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalSplit::CParseHandlerPhysicalSplit
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_deletion_colid_array(NULL),
	m_insert_colid_array(NULL),
	m_action_colid(0),
	m_ctid_colid(0),
	m_segid_colid(0),	
	m_preserve_oids(false),
	m_tuple_oid_col_oid(0)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalSplit::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalSplit::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes &attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSplit), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	const XMLCh *delete_colids = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenDeleteCols, EdxltokenPhysicalSplit);
	m_deletion_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), delete_colids, EdxltokenDeleteCols, EdxltokenPhysicalSplit);

	const XMLCh *insert_colids = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenInsertCols, EdxltokenPhysicalSplit);
	m_insert_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), insert_colids, EdxltokenInsertCols, EdxltokenPhysicalSplit);

	m_action_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong( m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenActionColId, EdxltokenPhysicalSplit);
	m_ctid_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong( m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenCtidColId, EdxltokenPhysicalSplit);
	m_segid_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong( m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenGpSegmentIdColId, EdxltokenPhysicalSplit);

	const XMLCh *update_with_preserved_oids = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenUpdatePreservesOids));
	if (NULL != update_with_preserved_oids)
	{
		m_preserve_oids = CDXLOperatorFactory::ConvertAttrValueToBool
											(
											m_parse_handler_mgr->GetDXLMemoryManager(),
											update_with_preserved_oids,
											EdxltokenUpdatePreservesOids,
											EdxltokenPhysicalSplit
											);
	}	
	if (m_preserve_oids)
	{
		m_tuple_oid_col_oid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTupleOidColId, EdxltokenPhysicalSplit);
	}

	// parse handler for physical operator
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// parse handler for the proj list
	CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

	//parse handler for the properties of the operator
	CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	// store child parse handlers in array
	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalSplit::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalSplit::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSplit), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(3 == this->Length());

	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	GPOS_ASSERT(NULL != proj_list_parse_handler->CreateDXLNode());

	CParseHandlerPhysicalOp *child_parse_handler = dynamic_cast<CParseHandlerPhysicalOp*>((*this)[2]);
	GPOS_ASSERT(NULL != child_parse_handler->CreateDXLNode());

	CDXLPhysicalSplit *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalSplit
												(
												m_mp,
												m_deletion_colid_array,
												m_insert_colid_array,
												m_action_colid,
												m_ctid_colid,
												m_segid_colid,
												m_preserve_oids,
												m_tuple_oid_col_oid
												);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
