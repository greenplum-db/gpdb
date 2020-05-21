//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalUpdate.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		update operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalUpdate.h"

#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLLogicalUpdate.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalUpdate::CParseHandlerLogicalUpdate
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalUpdate::CParseHandlerLogicalUpdate
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_ctid_colid(0),
	m_segid_colid(0),
	m_deletion_colid_array(NULL),
	m_insert_colid_array(NULL),
	m_preserve_oids(false),
	m_tuple_oid_col_oid(0)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalUpdate::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalUpdate::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes &attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalUpdate), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	m_ctid_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenCtidColId, EdxltokenLogicalUpdate);
	m_segid_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenGpSegmentIdColId, EdxltokenLogicalUpdate);

	const XMLCh *delete_colids_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenDeleteCols, EdxltokenLogicalUpdate);
	m_deletion_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), delete_colids_xml, EdxltokenDeleteCols, EdxltokenLogicalUpdate);

	const XMLCh *insert_colids_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenInsertCols, EdxltokenLogicalUpdate);
	m_insert_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), insert_colids_xml, EdxltokenInsertCols, EdxltokenLogicalUpdate);

	const XMLCh *preserve_oids_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenUpdatePreservesOids));
	if (NULL != preserve_oids_xml)
	{
		m_preserve_oids = CDXLOperatorFactory::ConvertAttrValueToBool
											(
											m_parse_handler_mgr->GetDXLMemoryManager(),
											preserve_oids_xml,
											EdxltokenUpdatePreservesOids,
											EdxltokenLogicalUpdate
											);
	}
	
	if (m_preserve_oids)
	{
		m_tuple_oid_col_oid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTupleOidColId, EdxltokenLogicalUpdate);
	}

	// parse handler for logical operator
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	//parse handler for the table descriptor
	CParseHandlerBase *table_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenTableDescr), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(table_descr_parse_handler);

	// store child parse handler in array
	this->Append(table_descr_parse_handler);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalUpdate::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalUpdate::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalUpdate), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(2 == this->Length());

	CParseHandlerTableDescr *table_descr_parse_handler = dynamic_cast<CParseHandlerTableDescr*>((*this)[0]);
	CParseHandlerLogicalOp *child_parse_handler = dynamic_cast<CParseHandlerLogicalOp*>((*this)[1]);

	GPOS_ASSERT(NULL != table_descr_parse_handler->GetDXLTableDescr());
	GPOS_ASSERT(NULL != child_parse_handler->CreateDXLNode());

	CDXLTableDescr *table_descr = table_descr_parse_handler->GetDXLTableDescr();
	table_descr->AddRef();

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode
							(
							m_mp,
							GPOS_NEW(m_mp) CDXLLogicalUpdate(m_mp, table_descr, m_ctid_colid, m_segid_colid, m_deletion_colid_array, m_insert_colid_array, m_preserve_oids, m_tuple_oid_col_oid)
							);

	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
