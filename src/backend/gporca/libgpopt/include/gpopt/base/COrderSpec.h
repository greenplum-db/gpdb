//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		COrderSpec.h
//
//	@doc:
//		Description of sort order;
//		Can be used as required or derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_COrderSpec_H
#define GPOPT_COrderSpec_H

#include "gpos/base.h"
#include "gpos/common/DbgPrintMixin.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CPropSpec.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
// type definition of corresponding dynamic pointer array
class COrderSpec;
using COrderSpecArray = CDynamicPtrArray<COrderSpec, CleanupRelease>;

using namespace gpos;

// fwd declaration
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		COrderSpec
//
//	@doc:
//		Array of Order Expressions
//
//---------------------------------------------------------------------------
class COrderSpec : public CPropSpec
{
public:
	enum ENullTreatment
	{
		EntAuto,  // default behavior, as implemented by operator

		EntFirst,
		EntLast,

		EntSentinel
	};

private:
	//---------------------------------------------------------------------------
	//	@class:
	//		COrderExpression
	//
	//	@doc:
	//		Spec of sort order component consisting of
	//
	//			1. sort operator's mdid
	//			2. column reference
	//			3. definition of NULL treatment
	//
	//---------------------------------------------------------------------------
	class COrderExpression : public gpos::DbgPrintMixin<COrderExpression>
	{
	private:
		// MD id of sort operator
		gpmd::IMDId *m_mdid;

		// sort column
		const CColRef *m_pcr;

		// null treatment
		ENullTreatment m_ent;

	public:
		COrderExpression(const COrderExpression &) = delete;

		// ctor
		COrderExpression(gpmd::IMDId *mdid, const CColRef *colref,
						 ENullTreatment ent);

		// dtor
		virtual ~COrderExpression();

		// accessor of sort operator midid
		gpmd::IMDId *
		GetMdIdSortOp() const
		{
			return m_mdid;
		}

		// accessor of sort column
		const CColRef *
		Pcr() const
		{
			return m_pcr;
		}

		// accessor of null treatment
		ENullTreatment
		Ent() const
		{
			return m_ent;
		}

		// check if order specs match
		BOOL Matches(const COrderExpression *poe) const;

		// print
		IOstream &OsPrint(IOstream &os) const;

	};	// class COrderExpression

	// array of order expressions
	using COrderExpressionArray =
		CDynamicPtrArray<COrderExpression, CleanupDelete>;


	// memory pool
	CMemoryPool *m_mp;

	// components of order spec
	COrderExpressionArray *m_pdrgpoe;

	// extract columns from order spec into the given column set
	void ExtractCols(CColRefSet *pcrs) const;

public:
	COrderSpec(const COrderSpec &) = delete;

	// ctor
	explicit COrderSpec(CMemoryPool *mp);

	// dtor
	~COrderSpec() override;

	// number of sort expressions
	ULONG
	UlSortColumns() const
	{
		return m_pdrgpoe->Size();
	}

	// accessor of sort operator of the n-th component
	IMDId *
	GetMdIdSortOp(ULONG ul) const
	{
		COrderExpression *poe = (*m_pdrgpoe)[ul];
		return poe->GetMdIdSortOp();
	}

	// accessor of sort column of the n-th component
	const CColRef *
	Pcr(ULONG ul) const
	{
		COrderExpression *poe = (*m_pdrgpoe)[ul];
		return poe->Pcr();
	}

	// accessor of null treatment of the n-th component
	ENullTreatment
	Ent(ULONG ul) const
	{
		COrderExpression *poe = (*m_pdrgpoe)[ul];
		return poe->Ent();
	}

	// check if order spec has no columns
	BOOL
	IsEmpty() const
	{
		return UlSortColumns() == 0;
	}

	// append new component
	void Append(gpmd::IMDId *mdid, const CColRef *colref, ENullTreatment ent);

	// extract colref set of order columns
	CColRefSet *PcrsUsed(CMemoryPool *mp) const override;

	// property type
	EPropSpecType
	Epst() const override
	{
		return EpstOrder;
	}

	// check if order specs match
	BOOL Matches(const COrderSpec *pos) const;

	// check if order specs satisfies req'd spec
	BOOL FSatisfies(const COrderSpec *pos) const;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// hash function
	ULONG HashValue() const override;

	// return a copy of the order spec with remapped columns
	virtual COrderSpec *PosCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// return a copy of the order spec after excluding the given columns
	virtual COrderSpec *PosExcludeColumns(CMemoryPool *mp, CColRefSet *pcrs);

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// matching function over order spec arrays
	static BOOL Equals(const COrderSpecArray *pdrgposFirst,
					   const COrderSpecArray *pdrgposSecond);

	// combine hash values of a maximum number of entries
	static ULONG HashValue(const COrderSpecArray *pdrgpos, ULONG ulMaxSize);

	// print array of order spec objects
	static IOstream &OsPrint(IOstream &os, const COrderSpecArray *pdrgpos);

	// extract colref set of order columns used by elements of order spec array
	static CColRefSet *GetColRefSet(CMemoryPool *mp, COrderSpecArray *pdrgpos);

	// filter out array of order specs from order expressions using the passed columns
	static COrderSpecArray *PdrgposExclude(CMemoryPool *mp,
										   COrderSpecArray *pdrgpos,
										   CColRefSet *pcrsToExclude);


};	// class COrderSpec

}  // namespace gpopt

#endif	// !GPOPT_COrderSpec_H

// EOF
