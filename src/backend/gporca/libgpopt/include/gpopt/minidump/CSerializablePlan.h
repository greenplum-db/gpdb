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


public:
	CSerializablePlan(const CSerializablePlan &) = delete;

	// ctor
	CSerializablePlan(CMemoryPool *mp, const CDXLNode *pdxlnPlan,
					  ULLONG plan_id, ULLONG plan_space_size);

	// dtor
	~CSerializablePlan() override;

	// serialize object to passed buffer
	void Serialize(COstream &oos) override;

};	// class CSerializablePlan
}  // namespace gpopt

#endif	// !GPOPT_CSerializablePlan_H

// EOF
