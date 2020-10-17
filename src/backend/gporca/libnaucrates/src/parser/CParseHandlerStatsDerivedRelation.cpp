//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatsDerivedRelation.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing derived relation
//		statistics.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerStatsDerivedRelation.h"
#include "naucrates/dxl/parser/CParseHandlerStatsDerivedColumn.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedRelation::CParseHandlerStatsDerivedRelation
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerStatsDerivedRelation::CParseHandlerStatsDerivedRelation(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_rows(CStatistics::DefaultColumnWidth),
	  m_empty(false),
	  m_dxl_stats_derived_relation(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedRelation::CParseHandlerStatsDerivedRelation
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerStatsDerivedRelation::~CParseHandlerStatsDerivedRelation()
{
	m_dxl_stats_derived_relation->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedRelation::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsDerivedRelation::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsDerivedColumn),
				 element_local_name))
	{
		// start new derived column element
		CParseHandlerBase *parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenStatsDerivedColumn),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handler
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name,
										 element_qname, attrs);
	}
	else
	{
		GPOS_ASSERT(0 == this->Length());

		// parse rows
		const XMLCh *xml_rows = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenRows, EdxltokenStatsDerivedRelation);

		m_rows = CDouble(CDXLOperatorFactory::ConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_rows, EdxltokenRows,
			EdxltokenStatsDerivedRelation));

		m_empty = false;
		const XMLCh *xml_is_empty =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenEmptyRelation));
		if (NULL != xml_is_empty)
		{
			m_empty = CDXLOperatorFactory::ConvertAttrValueToBool(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_is_empty,
				EdxltokenEmptyRelation, EdxltokenStatsDerivedRelation);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedRelation::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsDerivedRelation::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsDerivedRelation),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// must have at least one column stats
	GPOS_ASSERT(0 < this->Length());

	// array of derived column statistics
	CDXLStatsDerivedColumnArray *dxl_stats_derived_col_array =
		GPOS_NEW(m_mp) CDXLStatsDerivedColumnArray(m_mp);
	const ULONG num_of_drvd_col_stats = this->Length();
	for (ULONG idx = 0; idx < num_of_drvd_col_stats; idx++)
	{
		CParseHandlerStatsDerivedColumn *stats_derived_col_parse_handler =
			dynamic_cast<CParseHandlerStatsDerivedColumn *>((*this)[idx]);

		CDXLStatsDerivedColumn *pdxlstatdercol =
			stats_derived_col_parse_handler->GetDxlStatsDerivedCol();
		pdxlstatdercol->AddRef();
		dxl_stats_derived_col_array->Append(pdxlstatdercol);
	}

	m_dxl_stats_derived_relation = GPOS_NEW(m_mp)
		CDXLStatsDerivedRelation(m_rows, m_empty, dxl_stats_derived_col_array);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
