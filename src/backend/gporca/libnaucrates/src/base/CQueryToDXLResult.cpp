//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CQueryToDXLResult.cpp
//
//	@doc:
//		Implementation of the methods for accessing the result of the translation
//---------------------------------------------------------------------------


#include "naucrates/base/CQueryToDXLResult.h"


using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CQueryToDXLResult::CQueryToDXLResult
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CQueryToDXLResult::CQueryToDXLResult(CDXLNode *query,
									 CDXLNodeArray *query_output,
									 CDXLNodeArray *cte_producers)
	: m_query_dxl(query),
	  m_query_output(query_output),
	  m_cte_producers(cte_producers)
{
	GPOS_ASSERT(NULL != query);
}

//---------------------------------------------------------------------------
//	@function:
//		CQueryToDXLResult::~CQueryToDXLResult
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CQueryToDXLResult::~CQueryToDXLResult()
{
	m_query_dxl->Release();
	CRefCount::SafeRelease(m_query_output);
	CRefCount::SafeRelease(m_cte_producers);
}

//---------------------------------------------------------------------------
//	@function:
//		CQueryToDXLResult::GetOutputColumnsDXLArray
//
//	@doc:
//		Return the array of dxl nodes representing the query output
//
//---------------------------------------------------------------------------
const CDXLNodeArray *
CQueryToDXLResult::GetOutputColumnsDXLArray() const
{
	return m_query_output;
}

//---------------------------------------------------------------------------
//	@function:
//		CQueryToDXLResult::GetCTEProducerDXLArray
//
//	@doc:
//		Return the array of CTEs
//
//---------------------------------------------------------------------------
const CDXLNodeArray *
CQueryToDXLResult::GetCTEProducerDXLArray() const
{
	return m_cte_producers;
}

//---------------------------------------------------------------------------
//	@function:
//		CQueryToDXLResult::CreateDXLNode
//
//	@doc:
//		Return the DXL node representing the query
//
//---------------------------------------------------------------------------
const CDXLNode *
CQueryToDXLResult::CreateDXLNode() const
{
	return m_query_dxl;
}



// EOF
