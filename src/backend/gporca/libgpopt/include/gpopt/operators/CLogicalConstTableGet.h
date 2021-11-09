//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalConstTableGet.h
//
//	@doc:
//		Constant table accessor
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalConstTableGet_H
#define GPOPT_CLogicalConstTableGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// dynamic array of datum arrays -- array owns elements
using IDatum2dArray = CDynamicPtrArray<IDatumArray, CleanupRelease>;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalConstTableGet
//
//	@doc:
//		Constant table accessor
//
//---------------------------------------------------------------------------
class CLogicalConstTableGet : public CLogical
{
private:
	// array of column descriptors: the schema of the const table
	CColumnDescriptorArray *m_pdrgpcoldesc;

	// array of datum arrays
	IDatum2dArray *m_pdrgpdrgpdatum;

	// output columns
	CColRefArray *m_pdrgpcrOutput;

	// construct column descriptors from column references
	static CColumnDescriptorArray *PdrgpcoldescMapping(
		CMemoryPool *mp, CColRefArray *colref_array);

public:
	CLogicalConstTableGet(const CLogicalConstTableGet &) = delete;

	// ctors
	explicit CLogicalConstTableGet(CMemoryPool *mp);

	CLogicalConstTableGet(CMemoryPool *mp, CColumnDescriptorArray *pdrgpcoldesc,
						  IDatum2dArray *pdrgpdrgpdatum);

	CLogicalConstTableGet(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
						  IDatum2dArray *pdrgpdrgpdatum);

	// dtor
	~CLogicalConstTableGet() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalConstTableGet;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalConstTableGet";
	}

	// col descr accessor
	CColumnDescriptorArray *
	Pdrgpcoldesc() const
	{
		return m_pdrgpcoldesc;
	}

	// const table values accessor
	IDatum2dArray *
	Pdrgpdrgpdatum() const
	{
		return m_pdrgpdrgpdatum;
	}

	// accessors
	CColRefArray *
	PdrgpcrOutput() const
	{
		return m_pdrgpcrOutput;
	}

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *,
									CExpressionHandle &) override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *mp,
						CExpressionHandle &	 //exprhdl
	) const override
	{
		return GPOS_NEW(mp) CPartInfo(mp);
	}

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &  // exprhdl
	) const override
	{
		// TODO:  - Jan 11, 2013; compute constraints based on the
		// datum values in this CTG
		return GPOS_NEW(mp) CPropConstraint(
			mp, GPOS_NEW(mp) CColRefSetArray(mp), nullptr /*pcnstr*/);
	}

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   // pcrsInput
			 ULONG				   // child_index
	) const override
	{
		GPOS_ASSERT(!"CLogicalConstTableGet has no children");
		return nullptr;
	}

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspLow;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalConstTableGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalConstTableGet == pop->Eopid());

		return dynamic_cast<CLogicalConstTableGet *>(pop);
	}


	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalConstTableGet

}  // namespace gpopt


#endif	// !GPOPT_CLogicalConstTableGet_H

// EOF
