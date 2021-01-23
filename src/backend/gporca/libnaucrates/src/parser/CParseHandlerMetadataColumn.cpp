//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataColumn.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing column metadata.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMetadataColumn.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataColumns.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumn::CParseHandlerMetadataColumn
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataColumn::CParseHandlerMetadataColumn(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_mdcol(NULL),
	  m_mdname(NULL),
	  m_mdid_type(NULL),
	  m_dxl_default_val(NULL),
	  m_width(gpos::ulong_max)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumn::~CParseHandlerMetadataColumn
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadataColumn::~CParseHandlerMetadataColumn()
{
	CRefCount::SafeRelease(m_mdcol);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumn::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadataColumn::StartElement(const XMLCh *const,  // element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const,  // element_qname
										  const Attributes &attrs)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumn),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse column name
	const XMLCh *column_name_xml = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenMetadataColumn);

	CWStringDynamic *col_name = CDXLUtils::CreateDynamicStringFromXMLChArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), column_name_xml);

	// create a copy of the string in the CMDName constructor
	m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, col_name);

	GPOS_DELETE(col_name);

	// parse attribute number
	m_attno = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenAttno,
		EdxltokenMetadataColumn);

	m_mdid_type = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenMetadataColumn);

	// parse optional type modifier
	m_type_modifier = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeMod,
		EdxltokenColDescr, true, default_type_modifier);

	// parse attribute number
	m_is_nullable = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenColumnNullable, EdxltokenMetadataColumn);

	// parse column length from attributes
	const XMLCh *col_len_xml =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColWidth));

	if (NULL != col_len_xml)
	{
		m_width = CDXLOperatorFactory::ConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), col_len_xml,
			EdxltokenColWidth, EdxltokenColDescr);
	}

	m_is_dropped = false;
	const XMLCh *xmlszDropped =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColDropped));

	if (NULL != xmlszDropped)
	{
		m_is_dropped = CDXLOperatorFactory::ConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), xmlszDropped,
			EdxltokenColDropped, EdxltokenMetadataColumn);
	}

	// install a parse handler for the default m_bytearray_value
	CParseHandlerBase *pph = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenColumnDefaultValue),
		m_parse_handler_mgr, this);

	// activate and store parse handler
	m_parse_handler_mgr->ActivateParseHandler(pph);
	this->Append(pph);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumn::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadataColumn::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumn),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(1 == this->Length());

	// get node for default value expression from child parse handler
	CParseHandlerScalarOp *op_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);

	m_dxl_default_val = op_parse_handler->CreateDXLNode();

	if (NULL != m_dxl_default_val)
	{
		m_dxl_default_val->AddRef();
	}

	m_mdcol = GPOS_NEW(m_mp)
		CMDColumn(m_mdname, m_attno, m_mdid_type, m_type_modifier,
				  m_is_nullable, m_is_dropped, m_dxl_default_val, m_width);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadataColumn::GetMdCol
//
//	@doc:
//		Return the constructed list of metadata columns
//
//---------------------------------------------------------------------------
CMDColumn *
CParseHandlerMetadataColumn::GetMdCol()
{
	return m_mdcol;
}

// EOF
