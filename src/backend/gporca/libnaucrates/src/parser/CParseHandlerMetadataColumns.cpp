//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataColumns.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a list of
//		columns in a relation's metadata.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMetadataColumns.h"

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataColumn.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumns::CParseHandlerMetadataColumns
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataColumns::CParseHandlerMetadataColumns(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_md_col_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumns::~CParseHandlerMetadataColumns
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataColumns::~CParseHandlerMetadataColumns()
{
	CRefCount::SafeRelease(m_md_col_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumns::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadataColumns::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumns),
									  element_local_name))
	{
		// start of a columns' list
		GPOS_ASSERT(NULL == m_md_col_array);

		m_md_col_array = GPOS_NEW(m_mp) CMDColumnArray(m_mp);
	}
	else if (0 ==
			 XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumn),
									  element_local_name))
	{
		// column list must be initialized already
		GPOS_ASSERT(NULL != m_md_col_array);

		// activate parse handler to parse the column info
		CParseHandlerBase *col_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataColumn),
				m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(col_parse_handler);
		this->Append(col_parse_handler);

		col_parse_handler->startElement(element_uri, element_local_name,
										element_qname, attrs);
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
//		CParseHandlerMetadataColumns::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadataColumns::EndElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const	 // element_qname
)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumns),
									  element_local_name))
	{
		// end of the columns' list
		GPOS_ASSERT(NULL != m_md_col_array);

		const ULONG size = this->Length();
		// add parsed columns to the list
		for (ULONG ul = 0; ul < size; ul++)
		{
			CParseHandlerMetadataColumn *md_col_parse_handler =
				dynamic_cast<CParseHandlerMetadataColumn *>((*this)[ul]);

			GPOS_ASSERT(NULL != md_col_parse_handler->GetMdCol());

			CMDColumn *md_col = md_col_parse_handler->GetMdCol();
			md_col->AddRef();

			m_md_col_array->Append(md_col);
		}
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
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
//		CParseHandlerMetadataColumns::GetMdColArray
//
//	@doc:
//		Return the constructed list of metadata columns
//
//---------------------------------------------------------------------------
CMDColumnArray *
CParseHandlerMetadataColumns::GetMdColArray()
{
	return m_md_col_array;
}

// EOF
