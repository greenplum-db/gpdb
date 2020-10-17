//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CColConstraintsArrayMapper_H
#define GPOPT_CColConstraintsArrayMapper_H

#include "gpos/memory/CMemoryPool.h"

#include "gpopt/base/CConstraint.h"
#include "gpopt/base/IColConstraintsMapper.h"

namespace gpopt
{
class CColConstraintsArrayMapper : public IColConstraintsMapper
{
public:
	CColConstraintsArrayMapper(gpos::CMemoryPool *mp,
							   CConstraintArray *pdrgpcnstr);
	virtual CConstraintArray *PdrgPcnstrLookup(CColRef *colref);

	virtual ~CColConstraintsArrayMapper();

private:
	gpos::CMemoryPool *m_mp;
	CConstraintArray *m_pdrgpcnstr;
};
}  // namespace gpopt

#endif	//GPOPT_CColConstraintsArrayMapper_H
