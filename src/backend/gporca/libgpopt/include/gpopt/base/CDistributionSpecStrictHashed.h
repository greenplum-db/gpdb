//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CDistributionSpecStrictHashed_H
#define GPOPT_CDistributionSpecStrictHashed_H

#include "gpopt/base/CDistributionSpecHashed.h"

namespace gpopt
{
// Class to represent a "forced" hashed distribution. Introduced to support
// parallel append, this distribution is meant to be incompatible with
// (or unsatisfiable by) most other distributions to force a motion.
class CDistributionSpecStrictHashed : public CDistributionSpecHashed
{
public:
	CDistributionSpecStrictHashed(CExpressionArray *pdrgpexpr,
								  BOOL fNullsColocated);

	virtual EDistributionType Edt() const;

	virtual const CHAR *
	SzId() const
	{
		return "STRICT HASHED";
	}
};
}  // namespace gpopt

#endif
