//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CLogicalMultiExternalGet.h
//
//	@doc:
//  	Logical external get operator for multiple tables sharing a common
//  	column layout
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalMultiExternalGet_H
#define GPOPT_CLogicalMultiExternalGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGetBase.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;
class CColRefSet;

// Logical external get operator for multiple tables sharing a common column layout
// Currently only used for external leaf partitions in a partitioned table.
class CLogicalMultiExternalGet : public CLogicalDynamicGetBase
{
private:
	// private copy ctor
	CLogicalMultiExternalGet(const CLogicalMultiExternalGet &);

	// partition mdids to scan
	IMdIdArray *m_part_mdids;

public:
	// ctors
	explicit CLogicalMultiExternalGet(CMemoryPool *mp);

	CLogicalMultiExternalGet(CMemoryPool *mp, IMdIdArray *part_mdids,
							 const CName *pnameAlias,
							 CTableDescriptor *ptabdesc, ULONG scan_id,
							 CColRefArray *pdrgpcrOutput);

	~CLogicalMultiExternalGet();

	// ident accessors

	IMdIdArray *
	GetScanPartitionMdids() const
	{
		return m_part_mdids;
	}

	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalMultiExternalGet;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalMultiExternalGet";
	}

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	virtual CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp,
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   // pcrsInput
			 ULONG				   // child_index
	) const
	{
		GPOS_ASSERT(!"CLogicalMultiExternalGet has no children");
		return NULL;
	}

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
		return false;
	}

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	// stat promise
	virtual EStatPromise
	Esp(CExpressionHandle &) const
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalMultiExternalGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalMultiExternalGet == pop->Eopid());

		return dynamic_cast<CLogicalMultiExternalGet *>(pop);
	}

};	// class CLogicalMultiExternalGet
}  // namespace gpopt

#endif	// !GPOPT_CLogicalMultiExternalGet_H

// EOF
