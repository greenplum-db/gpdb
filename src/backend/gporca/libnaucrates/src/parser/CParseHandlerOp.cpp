//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerOp.cpp
//
//	@doc:
//		Implementation of the base SAX parse handler class for parsing DXL operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerOp.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOp::CParseHandlerOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerOp::CParseHandlerOp(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_node(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOp::~CParseHandlerOp
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerOp::~CParseHandlerOp()
{
	CRefCount::SafeRelease(m_dxl_node);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOp::CreateDXLNode
//
//	@doc:
//		Returns the constructed DXL node and passes ownership over it.
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerOp::CreateDXLNode() const
{
	return m_dxl_node;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOp::AddChildFromParseHandler
//
//	@doc:
//		Extracts the node constructed from the given parse handler and adds it
//		to children array of the current node. Child nodes are ref-counted before
//		being added to the array.
//
//---------------------------------------------------------------------------
void
CParseHandlerOp::AddChildFromParseHandler(
	const CParseHandlerOp *parse_handler_op)
{
	GPOS_ASSERT(NULL != m_dxl_node);
	GPOS_ASSERT(NULL != parse_handler_op);

	// extract constructed element
	CDXLNode *child_dxlnode = parse_handler_op->CreateDXLNode();
	GPOS_ASSERT(NULL != child_dxlnode);

	child_dxlnode->AddRef();
	m_dxl_node->AddChild(child_dxlnode);
}


// EOF
