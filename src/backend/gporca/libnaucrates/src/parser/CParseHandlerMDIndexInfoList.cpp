//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerMDIndexInfoList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing indexinfo list
//---------------------------------------------------------------------------

#include "naucrates/md/CMDIndexInfo.h"

#include "naucrates/dxl/parser/CParseHandlerMDIndexInfoList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

// ctor
CParseHandlerMDIndexInfoList::CParseHandlerMDIndexInfoList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_mdindex_info_array(NULL)
{
}

// dtor
CParseHandlerMDIndexInfoList::~CParseHandlerMDIndexInfoList()
{
	CRefCount::SafeRelease(m_mdindex_info_array);
}

// returns array of indexinfo
CMDIndexInfoArray *
CParseHandlerMDIndexInfoList::GetMdIndexInfoArray()
{
	return m_mdindex_info_array;
}

// invoked by Xerces to process an opening tag
void
CParseHandlerMDIndexInfoList::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname,
	const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenIndexInfoList),
				 element_local_name))
	{
		m_mdindex_info_array = GPOS_NEW(m_mp) CMDIndexInfoArray(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenIndexInfo),
					  element_local_name))
	{
		// parse mdid
		IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
			EdxltokenIndexInfo);

		// parse index partial info
		BOOL is_partial = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenIndexPartial, EdxltokenIndexInfo);

		CMDIndexInfo *md_index_info =
			GPOS_NEW(m_mp) CMDIndexInfo(mdid, is_partial);
		m_mdindex_info_array->Append(md_index_info);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// invoked by Xerces to process a closing tag
void
CParseHandlerMDIndexInfoList::EndElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const	 // element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenIndexInfoList),
				 element_local_name))
	{
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenIndexInfo),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// EOF
