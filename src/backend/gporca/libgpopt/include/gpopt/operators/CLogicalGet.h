//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalGet.h
//
//	@doc:
//		Basic table accessor
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalGet_H
#define GPOPT_CLogicalGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalGet
//
//	@doc:
//		Basic table accessor
//
//---------------------------------------------------------------------------
class CLogicalGet : public CLogical
{
private:
	// alias for table
	const CName *m_pnameAlias;

	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// output columns
	CColRefArray *m_pdrgpcrOutput;

	// partition keys
	CColRef2dArray *m_pdrgpdrgpcrPart;

	// distribution columns (empty for master only tables)
	CColRefSet *m_pcrsDist;

	void CreatePartCols(CMemoryPool *mp, const ULongPtrArray *pdrgpulPart);

	// private copy ctor
	CLogicalGet(const CLogicalGet &);

public:
	// ctors
	explicit CLogicalGet(CMemoryPool *mp);

	CLogicalGet(CMemoryPool *mp, const CName *pnameAlias,
				CTableDescriptor *ptabdesc);

	CLogicalGet(CMemoryPool *mp, const CName *pnameAlias,
				CTableDescriptor *ptabdesc, CColRefArray *pdrgpcrOutput);

	// dtor
	~CLogicalGet() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalGet;
	}

	// distribution columns
	virtual const CColRefSet *
	PcrsDist() const
	{
		return m_pcrsDist;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalGet";
	}

	// accessors
	CColRefArray *
	PdrgpcrOutput() const
	{
		return m_pdrgpcrOutput;
	}

	// return table's name
	const CName &
	Name() const
	{
		return *m_pnameAlias;
	}

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// partition columns
	CColRef2dArray *
	PdrgpdrgpcrPartColumns() const
	{
		return m_pdrgpdrgpcrPart;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

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

	// derive not nullable output columns
	CColRefSet *DeriveNotNullColumns(CMemoryPool *mp,
									 CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *mp,
						CExpressionHandle &	 // exprhdl
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
		return PpcDeriveConstraintFromTable(mp, m_ptabdesc, m_pdrgpcrOutput);
	}

	// derive join depth
	ULONG
	DeriveJoinDepth(CMemoryPool *,		 // mp
					CExpressionHandle &	 // exprhdl
	) const override
	{
		return 1;
	}

	// derive table descriptor
	CTableDescriptor *
	DeriveTableDescriptor(CMemoryPool *,	   // mp
						  CExpressionHandle &  // exprhdl
	) const override
	{
		return m_ptabdesc;
	}

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp,
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   // pcrsInput
			 ULONG				   // child_index
	) const override
	{
		GPOS_ASSERT(!"CLogicalGet has no children");
		return nullptr;
	}

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
	static CLogicalGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalGet == pop->Eopid() ||
					EopLogicalForeignGet == pop->Eopid());

		return dynamic_cast<CLogicalGet *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalGet

}  // namespace gpopt


#endif	// !GPOPT_CLogicalGet_H

// EOF
