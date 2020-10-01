//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSerializablePlan.h
//
//	@doc:
//		Serializable plan object used to dump the query plan
//---------------------------------------------------------------------------
#ifndef GPOPT_CSerializablePlan_H
#define GPOPT_CSerializablePlan_H

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
//		CSerializablePlan
//
//	@doc:
//		Serializable plan object
//
//---------------------------------------------------------------------------
class CSerializablePlan : public CSerializable
{
private:
	CMemoryPool *m_mp;

	// plan DXL node
	const CDXLNode *m_plan_dxl_root;

	// serialized plan
	CWStringDynamic *m_pstrPlan;

	// plan Id
	ULLONG m_plan_id;

	// plan space size
	ULLONG m_plan_space_size;


	// private copy ctor
	CSerializablePlan(const CSerializablePlan &);

public:
	// ctor
	CSerializablePlan(CMemoryPool *mp, const CDXLNode *pdxlnPlan,
					  ULLONG plan_id, ULLONG plan_space_size);

	// dtor
	virtual ~CSerializablePlan();

	// serialize object to passed buffer
	virtual void Serialize(COstream &oos);

};	// class CSerializablePlan
}  // namespace gpopt

#endif	// !GPOPT_CSerializablePlan_H

// EOF
