//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalProject.h
//
//	@doc:
//		Project operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalProject_H
#define GPOS_CLogicalProject_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnary.h"

namespace gpopt
{
// fwd declaration
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalProject
//
//	@doc:
//		Project operator
//
//---------------------------------------------------------------------------
class CLogicalProject : public CLogicalUnary
{
private:
	// return equivalence class from scalar ident project element
	static CColRefSetArray *PdrgpcrsEquivClassFromScIdent(
		CMemoryPool *mp, CExpression *pexprPrEl, CColRefSet *not_null_columns);

	// extract constraint from scalar constant project element
	static void ExtractConstraintFromScConst(CMemoryPool *mp,
											 CExpression *pexprPrEl,
											 CConstraintArray *pdrgpcnstr,
											 CColRefSetArray *pdrgpcrs);

public:
	CLogicalProject(const CLogicalProject &) = delete;

	// ctor
	explicit CLogicalProject(CMemoryPool *mp);

	// dtor
	~CLogicalProject() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalProject;
	}

	const CHAR *
	SzId() const override
	{
		return "CLogicalProject";
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;

	// dervive keys
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *DerivePropertyConstraint(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalProject *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalProject == pop->Eopid());

		return dynamic_cast<CLogicalProject *>(pop);
	}

};	// class CLogicalProject

}  // namespace gpopt

#endif	// !GPOS_CLogicalProject_H

// EOF
