//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerPartitionSelector.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing partition
//		selectors
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerPartitionSelector.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalPartitionSelector.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPartitionSelector::CParseHandlerPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerPartitionSelector::CParseHandlerPartitionSelector
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_rel_mdid(NULL),
	m_num_of_part_levels(0),
	m_scan_id(0)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPartitionSelector::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerPartitionSelector::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes &attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalPartitionSelector), element_local_name))
	{
		// PartitionSelector node may have another PartitionSelector node as a child
		if (NULL != m_rel_mdid)
		{
			// instantiate the parse handler
			CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, element_local_name, m_parse_handler_mgr, this);
			
			GPOS_ASSERT(NULL != child_parse_handler);
			
			// activate the parse handler
			m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);
			this->Append(child_parse_handler);
			
			// pass the startElement message for the specialized parse handler to process
			child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
		}
		else
		{
			// parse table id
			m_rel_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRelationMdid, EdxltokenPhysicalPartitionSelector);
			
			// parse number of levels
			m_num_of_part_levels = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenPhysicalPartitionSelectorLevels, EdxltokenPhysicalPartitionSelector);
			
			// parse scan id
			m_scan_id = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenPhysicalPartitionSelectorScanId, EdxltokenPhysicalPartitionSelector);
			
			// parse handlers for all the scalar children
			CParseHandlerBase *op_list_filters_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarOpList), m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(op_list_filters_parse_handler);
			
			CParseHandlerBase *op_list_eq_filters_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarOpList), m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(op_list_eq_filters_parse_handler);
			
			// parse handler for the proj list
			CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);
			
			// parse handler for the properties of the operator
			CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);
			
			// store parse handlers
			this->Append(prop_parse_handler);
			this->Append(proj_list_parse_handler);
			this->Append(op_list_eq_filters_parse_handler);
			this->Append(op_list_filters_parse_handler);
		}
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarResidualFilter), element_local_name))
	{
		CParseHandlerBase *residual_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(residual_parse_handler);
		this->Append(residual_parse_handler);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarPropagationExpr), element_local_name))
	{
		CParseHandlerBase *propagation_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(propagation_parse_handler);
		this->Append(propagation_parse_handler);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarPrintableFilter), element_local_name))
	{
		CParseHandlerBase *printable_filter_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(printable_filter_parse_handler);
		this->Append(printable_filter_parse_handler);
	}
	else
	{
		// parse physical child
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPartitionSelector::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerPartitionSelector::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarResidualFilter), element_local_name) ||
		0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarPropagationExpr), element_local_name) ||
		0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarPrintableFilter), element_local_name))
	{
		return;
	}

	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalPartitionSelector), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	CDXLPhysicalPartitionSelector *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalPartitionSelector(m_mp, m_rel_mdid, m_num_of_part_levels, m_scan_id);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// scalar children
	for (ULONG idx = 1; idx < 7; idx++)
	{
		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// optional physical child
	if (8 == this->Length())
	{
		CParseHandlerPhysicalOp *child_parse_handler = dynamic_cast<CParseHandlerPhysicalOp *>((*this)[7]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

