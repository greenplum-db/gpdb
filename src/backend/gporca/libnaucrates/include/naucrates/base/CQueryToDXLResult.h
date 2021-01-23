//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CQueryToDXLResult.h
//
//	@doc:
//		Class representing the result of the Query to DXL translation
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorQueryToDXLOutput_H
#define GPDXL_CTranslatorQueryToDXLOutput_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CQueryToDXLResult
//
//	@doc:
//		Class representing the result of the Query to DXL translation
//
//---------------------------------------------------------------------------
class CQueryToDXLResult
{
private:
	// DXL representing the Query
	CDXLNode *m_query_dxl;

	// array of DXL nodes that represent the query output
	CDXLNodeArray *m_query_output;

	// CTE list
	CDXLNodeArray *m_cte_producers;

public:
	// ctor
	CQueryToDXLResult(CDXLNode *query, CDXLNodeArray *query_output,
					  CDXLNodeArray *cte_producers);

	// dtor
	~CQueryToDXLResult();

	// return the DXL representation of the query
	const CDXLNode *CreateDXLNode() const;

	// return the array of output columns
	const CDXLNodeArray *GetOutputColumnsDXLArray() const;

	// return the array of CTEs
	const CDXLNodeArray *GetCTEProducerDXLArray() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CTranslatorQueryToDXLOutput_H

// EOF
