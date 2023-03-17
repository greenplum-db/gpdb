//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CLogicalDynamicForeignGet.h
//
//	@doc:
//  	Logical foreign get operator for partitioned table with only foreign tables
//      with the same foreign server
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDynamicForeignGet_H
#define GPOPT_CLogicalDynamicForeignGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGetBase.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;
class CColRefSet;

class CLogicalDynamicForeignGet : public CLogicalDynamicGetBase
{
private:
	OID m_foreign_server_oid;

	BOOL m_is_master_only;

public:
	CLogicalDynamicForeignGet(const CLogicalDynamicForeignGet &) = delete;

	// ctors
	explicit CLogicalDynamicForeignGet(CMemoryPool *mp);

	CLogicalDynamicForeignGet(CMemoryPool *mp, const CName *pnameAlias,
							  CTableDescriptor *ptabdesc, ULONG ulPartIndex,
							  CColRefArray *pdrgpcrOutput,
							  CColRef2dArray *pdrgpdrgpcrPart,
							  IMdIdArray *partition_mdids,
							  OID foreign_server_oid, BOOL is_master_only);

	// ident accessors

	EOperatorId
	Eopid() const override
	{
		return EopLogicalDynamicForeignGet;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDynamicForeignGet";
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	OID
	GetForeignServerOid() const
	{
		return m_foreign_server_oid;
	}

	BOOL
	IsMasterOnly() const
	{
		return m_is_master_only;
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
		GPOS_ASSERT(!"CLogicalDynamicForeignGet has no children");
		return nullptr;
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
		return false;
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
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalDynamicForeignGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDynamicForeignGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicForeignGet *>(pop);
	}

};	// class CLogicalDynamicForeignGet
}  // namespace gpopt

#endif	// !GPOPT_CLogicalDynamicForeignGet_H

// EOF
