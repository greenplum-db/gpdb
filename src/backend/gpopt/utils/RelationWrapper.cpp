//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//---------------------------------------------------------------------------
#include "gpopt/utils/RelationWrapper.h"

#include "gpopt/gpdbwrappers.h"	 // for CloseRelation

namespace gpdb
{
RelationWrapper::~RelationWrapper() noexcept(false)
{
	if (m_relation)
	{
		CloseRelation(m_relation);
	}
}

void
RelationWrapper::Close()
{
	if (m_relation)
	{
		CloseRelation(m_relation);
		m_relation = nullptr;
	}
}
}  // namespace gpdb
