//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSerializableQuery.cpp
//
//	@doc:
//		Serializable query object
//---------------------------------------------------------------------------

#include "gpopt/minidump/CSerializableQuery.h"

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/task/CTask.h"

#include "naucrates/dxl/CDXLUtils.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::CSerializableQuery
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializableQuery::CSerializableQuery(
	CMemoryPool *mp, const CDXLNode *query,
	const CDXLNodeArray *query_output_dxlnode_array,
	const CDXLNodeArray *cte_producers)
	: CSerializable(),
	  m_mp(mp),
	  m_query_dxl_root(query),
	  m_query_output(query_output_dxlnode_array),
	  m_cte_producers(cte_producers)
{
	GPOS_ASSERT(NULL != query);
	GPOS_ASSERT(NULL != query_output_dxlnode_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::~CSerializableQuery
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSerializableQuery::~CSerializableQuery()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::Serialize
//
//	@doc:
//		Serialize contents into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableQuery::Serialize(COstream &oos)
{
	CDXLUtils::SerializeQuery(m_mp, oos, m_query_dxl_root, m_query_output,
							  m_cte_producers, false /*fSerializeHeaders*/,
							  false /*indentation*/
	);
}

// EOF
