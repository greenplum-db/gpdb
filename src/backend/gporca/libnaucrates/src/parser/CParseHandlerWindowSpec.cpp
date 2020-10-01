//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerWindowSpec.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the
//		window specification node
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerWindowSpec.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerWindowFrame.h"
#include "naucrates/dxl/parser/CParseHandlerSortColList.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowSpec::CParseHandlerWindowSpec
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerWindowSpec::CParseHandlerWindowSpec(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_part_by_colid_array(NULL),
	  m_dxl_window_spec_gen(NULL),
	  m_mdname(NULL),
	  m_has_window_frame(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowSpec::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowSpec::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowSpec),
								 element_local_name))
	{
		GPOS_ASSERT(0 == this->Length());
		GPOS_ASSERT(NULL == m_dxl_window_spec_gen);
		GPOS_ASSERT(NULL == m_mdname);

		// parse alias from attributes
		const XMLCh *xml_alias =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenAlias));
		if (NULL != xml_alias)
		{
			CWStringDynamic *str_alias =
				CDXLUtils::CreateDynamicStringFromXMLChArray(
					m_parse_handler_mgr->GetDXLMemoryManager(), xml_alias);
			m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_alias);
			GPOS_DELETE(str_alias);
		}

		const XMLCh *xml_part_cols = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenPartKeys, EdxltokenPhysicalWindow);
		m_part_by_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_part_cols,
			EdxltokenPartKeys, EdxltokenPhysicalWindow);
		GPOS_ASSERT(NULL != m_part_by_colid_array);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarSortColList),
					  element_local_name))
	{
		// parse handler for the sorting column list
		CParseHandlerBase *sort_col_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSortColList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(sort_col_list_parse_handler);

		// store parse handler
		this->Append(sort_col_list_parse_handler);
		sort_col_list_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
					  element_local_name))
	{
		m_has_window_frame = true;

		// parse handler for the leading and trailing scalar values
		CParseHandlerBase *window_frame_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(window_frame_parse_handler);

		// store parse handler
		this->Append(window_frame_parse_handler);
		window_frame_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);
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
//		CParseHandlerWindowSpec::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowSpec::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowSpec),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	// sorting columns
	CDXLNode *sort_col_list_dxlnode = NULL;

	// window frame associated with the window key
	CDXLWindowFrame *window_frame = NULL;

	if (1 == this->Length())
	{
		if (m_has_window_frame)
		{
			// In GPDB 5 and before, window specification cannot have a window frame specification
			// without sort columns. This changed in GPDB 6/Postgres 8.4+ where a query
			// select b,c, count(c) over (partition by b) from (select * from foo) s;
			// adds a window frame that is unbounded.
			CParseHandlerWindowFrame *window_frame_parse_handler =
				dynamic_cast<CParseHandlerWindowFrame *>((*this)[0]);
			window_frame = window_frame_parse_handler->GetWindowFrame();
		}
		else
		{
			CParseHandlerSortColList *sort_col_list_parse_handler =
				dynamic_cast<CParseHandlerSortColList *>((*this)[0]);
			sort_col_list_dxlnode =
				sort_col_list_parse_handler->CreateDXLNode();
			sort_col_list_dxlnode->AddRef();
		}
	}
	else if (2 == this->Length())
	{
		CParseHandlerSortColList *sort_col_list_parse_handler =
			dynamic_cast<CParseHandlerSortColList *>((*this)[0]);
		sort_col_list_dxlnode = sort_col_list_parse_handler->CreateDXLNode();
		sort_col_list_dxlnode->AddRef();

		CParseHandlerWindowFrame *window_frame_parse_handler =
			dynamic_cast<CParseHandlerWindowFrame *>((*this)[1]);
		window_frame = window_frame_parse_handler->GetWindowFrame();
	}
	m_dxl_window_spec_gen =
		GPOS_NEW(m_mp) CDXLWindowSpec(m_mp, m_part_by_colid_array, m_mdname,
									  sort_col_list_dxlnode, window_frame);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
