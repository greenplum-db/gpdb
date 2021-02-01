//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalConstTable.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		const tables.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalConstTable.h"

#include "naucrates/dxl/operators/CDXLLogicalConstTable.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalConstTable::CParseHandlerLogicalConstTable
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalConstTable::CParseHandlerLogicalConstTable(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_const_tuples_datum_array(NULL),
	  m_dxl_datum_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalConstTable::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalConstTable::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname,
	const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalConstTable),
				 element_local_name))
	{
		// start of a const table operator node
		GPOS_ASSERT(0 == this->Length());
		GPOS_ASSERT(NULL == m_const_tuples_datum_array);

		// initialize the array of const tuples (datum arrays)
		m_const_tuples_datum_array = GPOS_NEW(m_mp) CDXLDatum2dArray(m_mp);

		// install a parse handler for the columns
		CParseHandlerBase *col_desc_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(col_desc_parse_handler);

		// store parse handler
		this->Append(col_desc_parse_handler);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenConstTuple),
					  element_local_name))
	{
		GPOS_ASSERT(
			NULL !=
			m_const_tuples_datum_array);  // we must have already seen a logical const table
		GPOS_ASSERT(NULL == m_dxl_datum_array);

		// initialize the array of datums (const tuple)
		m_dxl_datum_array = GPOS_NEW(m_mp) CDXLDatumArray(m_mp);
	}
	else if (0 ==
			 XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenDatum),
									  element_local_name))
	{
		// we must have already seen a logical const table and a const tuple
		GPOS_ASSERT(NULL != m_const_tuples_datum_array);
		GPOS_ASSERT(NULL != m_dxl_datum_array);

		// translate the datum and add it to the datum array
		CDXLDatum *dxl_datum = CDXLOperatorFactory::GetDatumVal(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenScalarConstValue);
		m_dxl_datum_array->Append(dxl_datum);
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
//		CParseHandlerLogicalConstTable::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalConstTable::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalConstTable),
				 element_local_name))
	{
		GPOS_ASSERT(1 == this->Length());

		CParseHandlerColDescr *col_descr_parse_handler =
			dynamic_cast<CParseHandlerColDescr *>((*this)[0]);
		GPOS_ASSERT(NULL != col_descr_parse_handler->GetDXLColumnDescrArray());

		CDXLColDescrArray *dxl_col_descr_array =
			col_descr_parse_handler->GetDXLColumnDescrArray();
		dxl_col_descr_array->AddRef();

		CDXLLogicalConstTable *lg_const_table_get_dxl_op =
			GPOS_NEW(m_mp) CDXLLogicalConstTable(m_mp, dxl_col_descr_array,
												 m_const_tuples_datum_array);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, lg_const_table_get_dxl_op);

#ifdef GPOS_DEBUG
		lg_const_table_get_dxl_op->AssertValid(m_dxl_node,
											   false /* validate_children */);
#endif	// GPOS_DEBUG

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenConstTuple),
					  element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_datum_array);
		m_const_tuples_datum_array->Append(m_dxl_datum_array);

		m_dxl_datum_array =
			NULL;  // intialize for the parsing the next const tuple (if needed)
	}
	else if (0 !=
			 XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenDatum),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}
// EOF
