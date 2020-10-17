//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerGroupingColList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing grouping column
//		id lists.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerGroupingColList.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerGroupingColList::CParseHandlerGroupingColList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerGroupingColList::CParseHandlerGroupingColList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_grouping_colids_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerGroupingColList::~CParseHandlerGroupingColList
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerGroupingColList::~CParseHandlerGroupingColList()
{
	CRefCount::SafeRelease(m_grouping_colids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerGroupingColList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerGroupingColList::StartElement(
	const XMLCh *const,	 //element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 //element_qname,
	const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarGroupingColList),
				 element_local_name))
	{
		// start the grouping column list
		m_grouping_colids_array = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenGroupingCol),
					  element_local_name))
	{
		// we must have seen a grouping cols list already and initialized the grouping cols array
		GPOS_ASSERT(NULL != m_grouping_colids_array);

		// parse grouping col id
		ULONG *pulColId =
			GPOS_NEW(m_mp) ULONG(CDXLOperatorFactory::ParseGroupingColId(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs));

		m_grouping_colids_array->Append(pulColId);
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
//		CParseHandlerGroupingColList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerGroupingColList::EndElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const	 // element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarGroupingColList),
				 element_local_name))
	{
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenGroupingCol),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerGroupingColList::GetGroupingColidArray
//
//	@doc:
//		Returns the array of parsed grouping column ids
//
//---------------------------------------------------------------------------
ULongPtrArray *
CParseHandlerGroupingColList::GetGroupingColidArray()
{
	return m_grouping_colids_array;
}
// EOF
