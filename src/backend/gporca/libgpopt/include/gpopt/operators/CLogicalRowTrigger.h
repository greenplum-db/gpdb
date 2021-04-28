//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalRowTrigger.h
//
//	@doc:
//		Logical row-level trigger operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalRowTrigger_H
#define GPOPT_CLogicalRowTrigger_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalRowTrigger
//
//	@doc:
//		Logical row-level trigger operator
//
//---------------------------------------------------------------------------
class CLogicalRowTrigger : public CLogical
{
private:
	// relation id on which triggers are to be executed
	IMDId *m_rel_mdid;

	// trigger type
	INT m_type;

	// old columns
	CColRefArray *m_pdrgpcrOld;

	// new columns
	CColRefArray *m_pdrgpcrNew;

	// stability
	IMDFunction::EFuncStbl m_efs{IMDFunction::EfsImmutable};

	// initialize function properties
	void InitFunctionProperties();

	// return the type of a given trigger as an integer
	static INT ITriggerType(const IMDTrigger *pmdtrigger);

public:
	CLogicalRowTrigger(const CLogicalRowTrigger &) = delete;

	// ctor
	explicit CLogicalRowTrigger(CMemoryPool *mp);

	// ctor
	CLogicalRowTrigger(CMemoryPool *mp, IMDId *rel_mdid, INT type,
					   CColRefArray *pdrgpcrOld, CColRefArray *pdrgpcrNew);

	// dtor
	~CLogicalRowTrigger() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalRowTrigger;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalRowTrigger";
	}

	// relation id
	IMDId *
	GetRelMdId() const
	{
		return m_rel_mdid;
	}

	// trigger type
	INT
	GetType() const
	{
		return m_type;
	}

	// old columns
	CColRefArray *
	PdrgpcrOld() const
	{
		return m_pdrgpcrOld;
	}

	// new columns
	CColRefArray *
	PdrgpcrNew() const
	{
		return m_pdrgpcrNew;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;


	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 // mp
							 CExpressionHandle &exprhdl) const override
	{
		return CLogical::PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *,	// mp,
						CExpressionHandle &exprhdl) const override
	{
		return PpartinfoPassThruOuter(exprhdl);
	}

	// compute required stats columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *pcrsInput,
			 ULONG	// child_index
	) const override
	{
		return PcrsStatsPassThru(pcrsInput);
	}

	// derive function properties
	CFunctionProp *DeriveFunctionProperties(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive key collections
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalRowTrigger *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalRowTrigger == pop->Eopid());

		return dynamic_cast<CLogicalRowTrigger *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalRowTrigger
}  // namespace gpopt

#endif	// !GPOPT_CLogicalRowTrigger_H

// EOF
