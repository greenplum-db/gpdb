//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CRandom.h
//
//	@doc:
//		Random number generator.
//
//	@owner:
//		Siva
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPOS_CRandom_H
#define GPOS_CRandom_H

#include "gpos/types.h"

namespace gpos
{
class CRandom
{
private:
	// no copy c'tor
	CRandom(const CRandom &);

	// seed
	ULONG m_seed;

public:
	// no seed
	CRandom();

	// c'tor with seed
	CRandom(ULONG seed);

	// next random number
	ULONG Next();

	// d'tor
	~CRandom();
};	//class CRandom
}  // namespace gpos
#endif /* CRANDOM_H_ */

// EOF
