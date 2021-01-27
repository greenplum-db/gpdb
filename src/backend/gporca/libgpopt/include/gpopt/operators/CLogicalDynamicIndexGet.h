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

	// private copy ctor
	CLogicalDynamicIndexGet(const CLogicalDynamicIndexGet &);

public:
	// ctors
	explicit CLogicalDynamicIndexGet(CMemoryPool *mp);

	CLogicalDynamicIndexGet(CMemoryPool *mp, const IMDIndex *pmdindex,
							CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
							const CName *pnameAlias, ULONG ulPartIndex,
							CColRefArray *pdrgpcrOutput,
							CColRef2dArray *pdrgpdrgpcrPart,
							ULONG ulSecondaryPartIndexId,
							CPartConstraint *ppartcnstr,
							CPartConstraint *ppartcnstrRel);

	// dtor
	virtual ~CLogicalDynamicIndexGet();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalDynamicIndexGet;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
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
	Name() const
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

	// check if index is partial given the table descriptor and the index mdid
	static BOOL IsPartialIndex(CTableDescriptor *ptabdesc,
							   const IMDIndex *pmdindex);

	// order spec
	COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// derive outer references
	virtual CColRefSet *DeriveOuterReferences(CMemoryPool *mp,
											  CExpressionHandle &exprhdl);

	// sensitivity to order of inputs
	virtual BOOL FInputOrderSensitive() const;

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	virtual CColRefSet *
	PcrsStat(CMemoryPool *,		   //mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   //pcrsInput
			 ULONG				   // child_index
	) const
	{
		GPOS_ASSERT(!"CLogicalDynamicIndexGet has no children");
		return NULL;
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
	// conversion function
	//-------------------------------------------------------------------------------------

	static CLogicalDynamicIndexGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalDynamicIndexGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicIndexGet *>(pop);
	}


	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CLogicalDynamicIndexGet

}  // namespace gpopt

#endif	// !GPOPT_CLogicalDynamicIndexGet_H

// EOF
