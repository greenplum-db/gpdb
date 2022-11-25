//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDrvdPropScalar.h
//
//	@doc:
//		Derived scalar properties
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropScalar_H
#define GPOPT_CDrvdPropScalar_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CFunctionProp.h"
#include "gpopt/base/CPartInfo.h"

namespace gpopt
{
using namespace gpos;

// forward declaration
class CExpressionHandle;

//---------------------------------------------------------------------------
//	@class:
//		CDrvdPropScalar
//
//	@doc:
//		Derived scalar properties container.
//
//		These are properties specific to scalar expressions like predicates and
//		project list. This includes used and defined columns.
//
//---------------------------------------------------------------------------
class CDrvdPropScalar : public CDrvdProp
{
	friend class CExpression;
	enum EDrvdPropType
	{
		EdptPcrsDefined = 0,
		EdptPcrsUsed,
		EdptPcrsSetReturningFunction,
		EdptFHasSubquery,
		EdptPPartInfo,
		EdptPfp,
		EdptFHasNonScalarFunction,
		EdptUlDistinctAggs,
		EdptUlOrderedAggs,
		EdptFHasMultipleDistinctAggs,
		EdptFHasScalarArrayCmp,
		EdptFHasScalarFuncProject,
		EdptFContainsOnlyReplicationSafeAggFuncs,
		EdptSentinel
	};

private:
	CMemoryPool *m_mp;

	CBitSet *m_is_prop_derived;

	// defined columns
	CColRefSet *m_pcrsDefined;

	// columns generated by set-returning-function like 'unnest'
	CColRefSet *m_pcrsSetReturningFunction;

	// used columns
	CColRefSet *m_pcrsUsed;

	// do subqueries appear in the operator's tree?
	BOOL m_fHasSubquery;

	// partition table consumers in subqueries
	CPartInfo *m_ppartinfo;

	// function properties
	CFunctionProp *m_pfp;

	// scalar expression contains non-scalar function?
	BOOL m_fHasNonScalarFunction;

	// total number of Distinct Aggs (e.g., {count(distinct a), sum(distinct a), count(distinct b)}, the value is 3),
	// only applicable to project lists
	ULONG m_ulDistinctAggs;

	// only applicable to project lists
	BOOL m_fHasScalarFunc;

	// does operator define Distinct Aggs on different arguments (e.g., count(distinct a), sum(distinct b)),
	// only applicable to project lists
	BOOL m_fHasMultipleDistinctAggs;
	ULONG m_ulOrderedAggs;

	// does expression contain ScalarArrayCmp generated for "scalar op ANY/ALL (array)" construct
	BOOL m_fHasScalarArrayCmp;

	// does expression contain only replication safe agg funcs
	BOOL m_fContainsOnlyReplicationSafeAggFuncs;

	// Have all the properties been derived?
	//
	// NOTE1: This is set ONLY when Derive() is called. If all the properties
	// are independently derived, m_is_complete will remain false. In that
	// case, even though Derive() would attempt to derive all the properties
	// once again, it should be quick, since each individual member has been
	// cached.
	// NOTE2: Once these properties are detached from the
	// corresponding expression used to derive it, this MUST be set to true,
	// since after the detachment, there will be no way to derive the
	// properties once again.
	BOOL m_is_complete;

protected:
	CColRefSet *DeriveDefinedColumns(CExpressionHandle &);

	CColRefSet *DeriveUsedColumns(CExpressionHandle &);

	CColRefSet *DeriveSetReturningFunctionColumns(CExpressionHandle &);

	BOOL DeriveHasSubquery(CExpressionHandle &);

	CPartInfo *DerivePartitionInfo(CExpressionHandle &);

	CFunctionProp *DeriveFunctionProperties(CExpressionHandle &);

	BOOL DeriveHasNonScalarFunction(CExpressionHandle &);

	ULONG DeriveTotalDistinctAggs(CExpressionHandle &);

	BOOL DeriveHasScalarFuncProject(CExpressionHandle &);

	BOOL DeriveHasMultipleDistinctAggs(CExpressionHandle &);

	BOOL DeriveContainsOnlyReplicationSafeAggFuncs(CExpressionHandle &);

	BOOL DeriveHasScalarArrayCmp(CExpressionHandle &);
	ULONG DeriveTotalOrderedAggs(CExpressionHandle &);

public:
	CDrvdPropScalar(const CDrvdPropScalar &) = delete;

	// ctor
	CDrvdPropScalar(CMemoryPool *mp);

	// dtor
	~CDrvdPropScalar() override;

	// type of properties
	EPropType
	Ept() override
	{
		return EptScalar;
	}

	BOOL
	IsComplete() const override
	{
		return m_is_complete;
	}

	// derivation function
	void Derive(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CDrvdPropCtxt *pdpctxt) override;

	// check for satisfying required plan properties
	BOOL FSatisfies(const CReqdPropPlan *prpp) const override;

	// defined columns
	CColRefSet *GetDefinedColumns() const;

	// used columns
	CColRefSet *GetUsedColumns() const;

	// columns containing set-returning function
	CColRefSet *GetSetReturningFunctionColumns() const;

	// do subqueries appear in the operator's tree?
	BOOL HasSubquery() const;

	// derived partition consumers
	CPartInfo *GetPartitionInfo() const;

	// function properties
	CFunctionProp *GetFunctionProperties() const;

	// scalar expression contains non-scalar function?
	virtual BOOL HasNonScalarFunction() const;

	// return total number of Distinct Aggs, only applicable to project list
	ULONG GetTotalDistinctAggs() const;

	BOOL HasScalarFuncProject() const;

	// does operator define Distinct Aggs on different arguments, only applicable to project lists
	BOOL HasMultipleDistinctAggs() const;

	BOOL HasScalarArrayCmp() const;

	BOOL ContainsOnlyReplicationSafeAggFuncs() const;

	// short hand for conversion
	static CDrvdPropScalar *GetDrvdScalarProps(CDrvdProp *pdp);

	// print function
	IOstream &OsPrint(IOstream &os) const override;

};	// class CDrvdPropScalar

}  // namespace gpopt


#endif	// !GPOPT_CDrvdPropScalar_H

// EOF
