//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerQuery.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing queries.
//
//
//	@owner:
//
//
//	@test:
//private:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerQuery.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerQueryOutput.h"
#include "naucrates/dxl/parser/CParseHandlerCTEList.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::CParseHandlerQuery
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerQuery::CParseHandlerQuery(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_node(NULL),
	  m_output_colums_dxl_array(NULL),
	  m_cte_producers(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::~CParseHandlerQuery
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerQuery::~CParseHandlerQuery()
{
	CRefCount::SafeRelease(m_dxl_node);
	CRefCount::SafeRelease(m_output_colums_dxl_array);
	CRefCount::SafeRelease(m_cte_producers);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::CreateDXLNode
//
//	@doc:
//		Root of constructed DXL plan
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerQuery::CreateDXLNode() const
{
	return m_dxl_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::GetOutputColumnsDXLArray
//
//	@doc:
//		Returns the list of query output columns
//
//---------------------------------------------------------------------------
CDXLNodeArray *
CParseHandlerQuery::GetOutputColumnsDXLArray() const
{
	return m_output_colums_dxl_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::GetCTEProducerDXLArray
//
//	@doc:
//		Returns the list of CTEs
//
//---------------------------------------------------------------------------
CDXLNodeArray *
CParseHandlerQuery::GetCTEProducerDXLArray() const
{
	return m_cte_producers;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::GetParseHandlerType
//
//	@doc:
//		Parse handler type
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerQuery::GetParseHandlerType() const
{
	return EdxlphQuery;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerQuery::StartElement(const XMLCh *const,  // element_uri,
								 const XMLCh *const element_local_name,
								 const XMLCh *const,  // element_qname
								 const Attributes &	  // attrs
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenQuery),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	GPOS_ASSERT(NULL != m_mp);

	// create parse handler for the query output node
	CParseHandlerBase *parse_handler_query_output =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenQueryOutput),
			m_parse_handler_mgr, this);

	// create parse handler for the CTE list
	CParseHandlerBase *parse_handler_cte =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenCTEList),
			m_parse_handler_mgr, this);

	// create a parse handler for logical nodes
	CParseHandlerBase *parse_handler_root =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical),
			m_parse_handler_mgr, this);

	m_parse_handler_mgr->ActivateParseHandler(parse_handler_root);
	m_parse_handler_mgr->ActivateParseHandler(parse_handler_cte);
	m_parse_handler_mgr->ActivateParseHandler(parse_handler_query_output);

	// store parse handlers
	this->Append(parse_handler_query_output);
	this->Append(parse_handler_cte);
	this->Append(parse_handler_root);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerQuery::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerQuery::EndElement(const XMLCh *const,	// element_uri,
							   const XMLCh *const element_local_name,
							   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenQuery),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerQueryOutput *parse_handler_query_output =
		dynamic_cast<CParseHandlerQueryOutput *>((*this)[0]);
	GPOS_ASSERT(NULL != parse_handler_query_output &&
				NULL != parse_handler_query_output->GetOutputColumnsDXLArray());

	// store constructed node
	m_output_colums_dxl_array =
		parse_handler_query_output->GetOutputColumnsDXLArray();
	m_output_colums_dxl_array->AddRef();

	CParseHandlerCTEList *parse_handler_cte =
		dynamic_cast<CParseHandlerCTEList *>((*this)[1]);
	GPOS_ASSERT(NULL != parse_handler_cte &&
				NULL != parse_handler_cte->GetDxlCteArray());

	m_cte_producers = parse_handler_cte->GetDxlCteArray();
	m_cte_producers->AddRef();

	CParseHandlerLogicalOp *parse_handler_logical_op =
		dynamic_cast<CParseHandlerLogicalOp *>((*this)[2]);
	GPOS_ASSERT(NULL != parse_handler_logical_op &&
				NULL != parse_handler_logical_op->CreateDXLNode());

	// store constructed node
	m_dxl_node = parse_handler_logical_op->CreateDXLNode();
	m_dxl_node->AddRef();

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
