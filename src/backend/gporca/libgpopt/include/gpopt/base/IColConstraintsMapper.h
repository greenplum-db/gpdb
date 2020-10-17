//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_IColConstraintsMapper_H
#define GPOPT_IColConstraintsMapper_H

#include "gpos/common/CRefCount.h"
#include "gpopt/base/CConstraint.h"

namespace gpopt
{
class IColConstraintsMapper : public CRefCount
{
public:
	virtual CConstraintArray *PdrgPcnstrLookup(CColRef *colref) = 0;

	virtual ~IColConstraintsMapper() = 0;
};
}  // namespace gpopt

#endif	//GPOPT_IColConstraintsMapper_H
