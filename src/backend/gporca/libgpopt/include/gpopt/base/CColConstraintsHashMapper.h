//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#ifndef GPOPT_CColConstraintsHashMapper_H
#define GPOPT_CColConstraintsHashMapper_H

#include "gpos/memory/CMemoryPool.h"

#include "gpopt/base/CConstraint.h"
#include "gpopt/base/IColConstraintsMapper.h"

namespace gpopt
{
class CColConstraintsHashMapper : public IColConstraintsMapper
{
public:
	CColConstraintsHashMapper(CMemoryPool *mp, CConstraintArray *pdrgPcnstr);

	CConstraintArray *PdrgPcnstrLookup(CColRef *colref) override;
	~CColConstraintsHashMapper() override;

private:
	ColRefToConstraintArrayMap *m_phmColConstr;
};
}  // namespace gpopt

#endif	//GPOPT_CColConstraintsHashMapper_H
