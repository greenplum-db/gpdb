//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalAssert.h
//
//	@doc:
//		Assert operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalAssert_H
#define GPOS_CLogicalAssert_H

#include "gpos/base.h"

#include "naucrates/dxl/errorcodes.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnary.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalAssert
//
//	@doc:
//		Assert operator
//
//---------------------------------------------------------------------------
class CLogicalAssert : public CLogicalUnary
{
private:
	// exception
	CException *m_pexc;

	// private copy ctor
	CLogicalAssert(const CLogicalAssert &);

public:
	// ctors
	explicit CLogicalAssert(CMemoryPool *mp);

	CLogicalAssert(CMemoryPool *mp, CException *pexc);

	// dtor
	virtual ~CLogicalAssert()
	{
		GPOS_DELETE(m_pexc);
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalAssert;
	}

	// name of operator
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalAssert";
	}

	// exception
	CException *
	Pexc() const
	{
		return m_pexc;
	}

	// match function;
	virtual BOOL Matches(COperator *pop) const;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *DeriveOutputColumns(CMemoryPool *, CExpressionHandle &);

	// dervive keys
	virtual CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp, CExpressionHandle &exprhdl) const
	{
		return PpcDeriveConstraintFromPredicates(mp, exprhdl);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalAssert *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalAssert == pop->Eopid());

		return reinterpret_cast<CLogicalAssert *>(pop);
	}

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	// debug print
	IOstream &OsPrint(IOstream &os) const;

};	// class CLogicalAssert

}  // namespace gpopt

#endif	// !GPOS_CLogicalAssert_H

// EOF
