//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerRelStats.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing base relation
//		statistics.
//---------------------------------------------------------------------------

#include "naucrates/md/CDXLRelStats.h"

#include "naucrates/dxl/parser/CParseHandlerRelStats.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelStats::CParseHandlerRelStats
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerRelStats::CParseHandlerRelStats(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelStats::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerRelStats::StartElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const,	 // element_qname,
									const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenRelationStats),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse table name
	const XMLCh *xml_str_table_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenRelationStats);

	CWStringDynamic *str_table_name =
		CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_table_name);

	// create a copy of the string in the CMDName constructor
	CMDName *mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_table_name);

	GPOS_DELETE(str_table_name);


	// parse metadata id info
	IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenRelationStats);

	// parse rows

	CDouble rows = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRows,
		EdxltokenRelationStats);

	BOOL is_empty = false;
	const XMLCh *xml_str_is_empty =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenEmptyRelation));
	if (NULL != xml_str_is_empty)
	{
		is_empty = CDXLOperatorFactory::ConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_is_empty,
			EdxltokenEmptyRelation, EdxltokenStatsDerivedRelation);
	}

	m_imd_obj = GPOS_NEW(m_mp) CDXLRelStats(m_mp, CMDIdRelStats::CastMdid(mdid),
											mdname, rows, is_empty);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelStats::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerRelStats::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenRelationStats),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
