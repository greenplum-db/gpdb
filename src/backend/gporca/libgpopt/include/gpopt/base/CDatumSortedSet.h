//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CDatumSortedSet_H
#define GPOPT_CDatumSortedSet_H

#include "gpos/memory/CMemoryPool.h"

#include "gpopt/base/IComparator.h"
#include "gpopt/operators/CExpression.h"
#include "naucrates/base/IDatum.h"

namespace gpopt
{
// A sorted and uniq'd array of pointers to datums
// It facilitates the construction of CConstraintInterval
class CDatumSortedSet : public IDatumArray
{
private:
	BOOL m_fIncludesNull;

public:
	CDatumSortedSet(CMemoryPool *mp, CExpression *pexprArray,
					const IComparator *pcomp);

	BOOL FIncludesNull() const;
};
}  // namespace gpopt

#endif
