//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerTableDescr.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a table descriptor portion
//		of a table scan node.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"

#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableDescr::CParseHandlerTableDescr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerTableDescr::CParseHandlerTableDescr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_table_descr(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableDescr::~CParseHandlerTableDescr
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerTableDescr::~CParseHandlerTableDescr()
{
	CRefCount::SafeRelease(m_dxl_table_descr);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableDescr::GetDXLTableDescr
//
//	@doc:
//		Returns the table descriptor constructed by the parse handler
//
//---------------------------------------------------------------------------
CDXLTableDescr *
CParseHandlerTableDescr::GetDXLTableDescr()
{
	return m_dxl_table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableDescr::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTableDescr::StartElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  // element_qname
									  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenTableDescr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse table name from attributes
	m_dxl_table_descr = CDXLOperatorFactory::MakeDXLTableDescr(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

	// install column descriptor parsers
	CParseHandlerBase *col_descr_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(col_descr_parse_handler);

	// store parse handler
	this->Append(col_descr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableDescr::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTableDescr::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenTableDescr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct node from the created child nodes

	GPOS_ASSERT(1 == this->Length());

	// assemble the properties container from the cost
	CParseHandlerColDescr *col_descr_parse_handler =
		dynamic_cast<CParseHandlerColDescr *>((*this)[0]);

	GPOS_ASSERT(NULL != col_descr_parse_handler->GetDXLColumnDescrArray());

	CDXLColDescrArray *dxl_column_descr_array =
		col_descr_parse_handler->GetDXLColumnDescrArray();
	dxl_column_descr_array->AddRef();
	m_dxl_table_descr->SetColumnDescriptors(dxl_column_descr_array);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
