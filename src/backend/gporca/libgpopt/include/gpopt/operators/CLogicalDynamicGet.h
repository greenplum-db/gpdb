//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDynamicGet.h
//
//	@doc:
//		Dynamic table accessor for partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDynamicGet_H
#define GPOPT_CLogicalDynamicGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGetBase.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDynamicGet
//
//	@doc:
//		Dynamic table accessor
//
//---------------------------------------------------------------------------
class CLogicalDynamicGet : public CLogicalDynamicGetBase
{
protected:
	// Disjunction of selected child partition's constraints after static pruning
	CConstraint *m_partition_cnstrs_disj{nullptr};

	// Has done static pruning
	BOOL m_static_pruned{false};

	// Indexes correspond to partitions
	IMdIdArray *m_foreign_server_mdids{nullptr};

public:
	CLogicalDynamicGet(const CLogicalDynamicGet &) = delete;

	// ctors
	explicit CLogicalDynamicGet(CMemoryPool *mp);

	CLogicalDynamicGet(CMemoryPool *mp, const CName *pnameAlias,
					   CTableDescriptor *ptabdesc, ULONG ulPartIndex,
					   CColRefArray *pdrgpcrOutput,
					   CColRef2dArray *pdrgpdrgpcrPart,
					   IMdIdArray *partition_mdids,
					   CConstraint *partition_cnstrs_disj, BOOL static_pruned,
					   IMdIdArray *foreign_server_mdids);
	CLogicalDynamicGet(CMemoryPool *mp, const CName *pnameAlias,
					   CTableDescriptor *ptabdesc, ULONG ulPartIndex,
					   IMdIdArray *partition_mdids,
					   IMdIdArray *foreign_server_mdids);

	// dtor
	~CLogicalDynamicGet() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDynamicGet;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDynamicGet";
	}

	// return disjunctive constraint of selected partitions
	CConstraint *
	GetPartitionConstraintsDisj() const
	{
		return m_partition_cnstrs_disj;
	}

	// return whether static pruning is performed
	BOOL
	FStaticPruned() const
	{
		return m_static_pruned;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// returns whether table contains foreign partitions
	BOOL ContainsForeignParts() const;

	// returns mdid list containing foreign server mdids corresponding to partititons in m_partition_mdids.
	// Mdid is marked as invalid (0) if not a foreign partition
	IMdIdArray *
	ForeignServerMdIds() const
	{
		return m_foreign_server_mdids;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------


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

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;


	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp,
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   //pcrsInput
			 ULONG				   // child_index
	) const override
	{
		GPOS_ASSERT(!"CLogicalDynamicGet has no children");
		return nullptr;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	// Statistics
	//-------------------------------------------------------------------------------------

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// derive stats from base table using filters on partition and/or index columns
	IStatistics *PstatsDeriveFilter(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CExpression *pexprFilter) const;

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
	static CLogicalDynamicGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDynamicGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicGet *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalDynamicGet

}  // namespace gpopt


#endif	// !GPOPT_CLogicalDynamicGet_H

// EOF
