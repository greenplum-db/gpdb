//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerQueryOutput.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class parsing the list of
//		output column references in a DXL query.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerQueryOutput.h"
#include "naucrates/dxl/parser/CParseHandlerScalarIdent.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"



using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQueryOutput::CParseHandlerQueryOutput
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerQueryOutput::CParseHandlerQueryOutput(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQueryOutput::~CParseHandlerQueryOutput
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerQueryOutput::~CParseHandlerQueryOutput()
{
	m_dxl_array->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQueryOutput::GetOutputColumnsDXLArray
//
//	@doc:
//		Return the list of query output columns
//
//---------------------------------------------------------------------------
CDXLNodeArray *
CParseHandlerQueryOutput::GetOutputColumnsDXLArray()
{
	GPOS_ASSERT(NULL != m_dxl_array);
	return m_dxl_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQueryOutput::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerQueryOutput::StartElement(const XMLCh *const element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const element_qname,
									   const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenQueryOutput),
								 element_local_name))
	{
		// start the query output section in the DXL document
		GPOS_ASSERT(NULL == m_dxl_array);

		m_dxl_array = GPOS_NEW(m_mp) CDXLNodeArray(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarIdent),
					  element_local_name))
	{
		// we must have seen a proj list already and initialized the proj list node
		GPOS_ASSERT(NULL != m_dxl_array);

		// start new scalar ident element
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarIdent),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
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
//		CParseHandlerQueryOutput::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerQueryOutput::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenQueryOutput),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG size = this->Length();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CParseHandlerScalarIdent *child_parse_handler =
			dynamic_cast<CParseHandlerScalarIdent *>((*this)[ul]);

		GPOS_ASSERT(NULL != child_parse_handler);

		CDXLNode *pdxlnIdent = child_parse_handler->CreateDXLNode();
		pdxlnIdent->AddRef();
		m_dxl_array->Append(pdxlnIdent);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
