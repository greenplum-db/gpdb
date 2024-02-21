//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CLogicalDynamicIndexOnlyGet.h
//
//	@doc:
//		Dynamic index only get operator for partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDynamicIndexOnlyGet_H
#define GPOPT_CLogicalDynamicIndexOnlyGet_H

#include "gpos/base.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/operators/CLogicalDynamicIndexGet.h"


namespace gpopt
{
// fwd declarations
class CName;
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDynamicIndexOnlyGet
//
//	@doc:
//		Dynamic index accessor for partitioned tables
//
//---------------------------------------------------------------------------
class CLogicalDynamicIndexOnlyGet : public CLogicalDynamicIndexGet
{
public:
	CLogicalDynamicIndexOnlyGet(const CLogicalDynamicIndexOnlyGet &) = delete;

	// ctors
	explicit CLogicalDynamicIndexOnlyGet(CMemoryPool *mp)
		: CLogicalDynamicIndexGet(mp)
	{
	}

	CLogicalDynamicIndexOnlyGet(CMemoryPool *mp, const IMDIndex *pmdindex,
								CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
								const CName *pnameAlias, ULONG ulPartIndex,
								CColRefArray *pdrgpcrOutput,
								CColRef2dArray *pdrgpdrgpcrPart,
								IMdIdArray *partition_mdids,
								ULONG ulUnindexedPredColCount)
		: CLogicalDynamicIndexGet(mp, pmdindex, ptabdesc, ulOriginOpId,
								  pnameAlias, ulPartIndex, pdrgpcrOutput,
								  pdrgpdrgpcrPart, partition_mdids,
								  ulUnindexedPredColCount)
	{
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDynamicIndexOnlyGet;
	}

	const CHAR *
	SzId() const override
	{
		return "CLogicalDynamicIndexOnlyGet";
	}

	// match function
	BOOL
	Matches(COperator *pop) const override
	{
		return CUtils::FMatchDynamicIndex(this, pop);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *
	PxfsCandidates(CMemoryPool *mp) const override
	{
		CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

		(void) xform_set->ExchangeSet(
			CXform::ExfDynamicIndexOnlyGet2DynamicIndexOnlyScan);

		return xform_set;
	}

	//-------------------------------------------------------------------------------------
	// conversion function
	//-------------------------------------------------------------------------------------

	static CLogicalDynamicIndexOnlyGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDynamicIndexOnlyGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicIndexOnlyGet *>(pop);
	}

};	// class CLogicalDynamicIndexOnlyGet

}  // namespace gpopt

#endif	// !GPOPT_CLogicalDynamicIndexOnlyGet_H

// EOF
