//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDynamicIndexGet.h
//
//	@doc:
//		Dynamic index get operator for partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDynamicIndexGet_H
#define GPOPT_CLogicalDynamicIndexGet_H

#include "gpos/base.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/operators/CLogicalDynamicGetBase.h"


namespace gpopt
{
// fwd declarations
class CName;
class CColRefSet;
class CPartConstraint;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDynamicIndexGet
//
//	@doc:
//		Dynamic index accessor for partitioned tables
//
//---------------------------------------------------------------------------
class CLogicalDynamicIndexGet : public CLogicalDynamicGetBase
{
private:
	// index descriptor
	CIndexDescriptor *m_pindexdesc;

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

	// order spec
	COrderSpec *m_pos;

	// Number of predicate not applicable on the index
	ULONG m_ulUnindexedPredColCount;

public:
	CLogicalDynamicIndexGet(const CLogicalDynamicIndexGet &) = delete;

	// ctors
	explicit CLogicalDynamicIndexGet(CMemoryPool *mp);

	CLogicalDynamicIndexGet(CMemoryPool *mp, const IMDIndex *pmdindex,
							CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
							const CName *pnameAlias, ULONG ulPartIndex,
							CColRefArray *pdrgpcrOutput,
							CColRef2dArray *pdrgpdrgpcrPart,
							IMdIdArray *partition_mdids,
							ULONG ulUnindexedPredColCount);

	// dtor
	~CLogicalDynamicIndexGet() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDynamicIndexGet;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDynamicIndexGet";
	}

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// index name
	const CName &
	Name() const override
	{
		return m_pindexdesc->Name();
	}

	// table alias name
	const CName &
	NameAlias() const
	{
		return *m_pnameAlias;
	}

	// index descriptor
	CIndexDescriptor *
	Pindexdesc() const
	{
		return m_pindexdesc;
	}

	// order spec
	COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// derive outer references
	CColRefSet *DeriveOuterReferences(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// number of predicate not applicable on the index
	ULONG
	ResidualPredicateSize() const
	{
		return m_ulUnindexedPredColCount;
	}

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   //mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   //pcrsInput
			 ULONG				   // child_index
	) const override
	{
		GPOS_ASSERT(!"CLogicalDynamicIndexGet has no children");
		return nullptr;
	}

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
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	// conversion function
	//-------------------------------------------------------------------------------------

	static CLogicalDynamicIndexGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDynamicIndexGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicIndexGet *>(pop);
	}


	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalDynamicIndexGet

}  // namespace gpopt

#endif	// !GPOPT_CLogicalDynamicIndexGet_H

// EOF
