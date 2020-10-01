//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSerializablePlan.cpp
//
//	@doc:
//		Serializable plan object
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/task/CTask.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "gpopt/minidump/CSerializablePlan.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CSerializablePlan::CSerializablePlan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializablePlan::CSerializablePlan(CMemoryPool *mp, const CDXLNode *pdxlnPlan,
									 ULLONG plan_id, ULLONG plan_space_size)
	: CSerializable(),
	  m_mp(mp),
	  m_plan_dxl_root(pdxlnPlan),
	  m_pstrPlan(NULL),
	  m_plan_id(plan_id),
	  m_plan_space_size(plan_space_size)
{
	GPOS_ASSERT(NULL != pdxlnPlan);
}


//---------------------------------------------------------------------------
//	@function:
//		CSerializablePlan::~CSerializablePlan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSerializablePlan::~CSerializablePlan()
{
	GPOS_DELETE(m_pstrPlan);
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializablePlan::Serialize
//
//	@doc:
//		Serialize contents into provided stream
//
//---------------------------------------------------------------------------
void
CSerializablePlan::Serialize(COstream &oos)
{
	CDXLUtils::SerializePlan(m_mp, oos, m_plan_dxl_root, m_plan_id,
							 m_plan_space_size, false /*fSerializeHeaders*/,
							 false /*indentation*/
	);
}

// EOF
