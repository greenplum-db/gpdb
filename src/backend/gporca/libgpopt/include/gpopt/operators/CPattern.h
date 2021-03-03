//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPattern.h
//
//	@doc:
//		Base class for all pattern operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPattern_H
#define GPOPT_CPattern_H

#include "gpos/base.h"

#include "gpopt/operators/COperator.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPattern
//
//	@doc:
//		base class for all pattern operators
//
//---------------------------------------------------------------------------
class CPattern : public COperator
{
private:
public:
	CPattern(const CPattern &) = delete;

	// ctor
	explicit CPattern(CMemoryPool *mp) : COperator(mp)
	{
	}

	// dtor
	~CPattern() override = default;

	// type of operator
	BOOL
	FPattern() const override
	{
		GPOS_ASSERT(!FPhysical() && !FScalar() && !FLogical());
		return true;
	}

	// create derived properties container
	CDrvdProp *PdpCreate(CMemoryPool *mp) const override;

	// create required properties container
	CReqdProp *PrpCreate(CMemoryPool *mp) const override;

	// match function
	BOOL Matches(COperator *) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// check if operator is a pattern leaf
	virtual BOOL FLeaf() const = 0;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// conversion function
	static CPattern *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(pop->FPattern());

		return dynamic_cast<CPattern *>(pop);
	}

	// helper to check multi-node pattern
	static BOOL
	FMultiNode(COperator *pop)
	{
		return COperator::EopPatternMultiLeaf == pop->Eopid() ||
			   COperator::EopPatternMultiTree == pop->Eopid();
	}

};	// class CPattern

}  // namespace gpopt


#endif	// !GPOPT_CPattern_H

// EOF
