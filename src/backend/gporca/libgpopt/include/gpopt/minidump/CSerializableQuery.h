//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSerializableQuery.h
//
//	@doc:
//		Serializable query object used to dump the query when an exception is raised;
//---------------------------------------------------------------------------
#ifndef GPOS_CSerializableQuery_H
#define GPOS_CSerializableQuery_H

#include "gpos/base.h"
#include "gpos/error/CSerializable.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLNode.h"

using namespace gpos;
using namespace gpdxl;

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CSerializableQuery
//
//	@doc:
//		Serializable query object
//
//---------------------------------------------------------------------------
class CSerializableQuery : public CSerializable
{
private:
	CMemoryPool *m_mp;

	// query DXL node;
	const CDXLNode *m_query_dxl_root;

	// query output
	const CDXLNodeArray *m_query_output;

	// CTE DXL nodes
	const CDXLNodeArray *m_cte_producers;


	// private copy ctor
	CSerializableQuery(const CSerializableQuery &);

public:
	// ctor
	CSerializableQuery(CMemoryPool *mp, const CDXLNode *query,
					   const CDXLNodeArray *query_output_dxlnode_array,
					   const CDXLNodeArray *cte_producers);

	// dtor
	virtual ~CSerializableQuery();

	// serialize object to passed stream
	virtual void Serialize(COstream &oos);

};	// class CSerializableQuery
}  // namespace gpopt

#endif	// !GPOS_CSerializableQuery_H

// EOF
