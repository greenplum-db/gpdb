//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarAggFunc.h
//
//	@doc:
//		Class for scalar aggregate function calls
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarAggFunc_H
#define GPOPT_CScalarAggFunc_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

enum EAggfuncStage
{
	EaggfuncstageGlobal,
	EaggfuncstageIntermediate,	// Intermediate stage of a 3-stage aggregation
	EaggfuncstageLocal,	 // First (lower, earlier) stage of 2-stage aggregation

	EaggfuncstageSentinel
};

enum EAggfuncKind
{
	EaggfunckindNormal = 0,
	EaggfunckindOrderedSet,
	EaggfunckindHypothetical
};

enum EAggfuncChildIndices
{
	EaggfuncIndexArgs = 0,
	EaggfuncIndexDirectArgs,
	EaggfuncIndexOrder,
	EaggfuncIndexDistinct,
	EaggfuncIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CScalarAggFunc
//
//	@doc:
//		scalar aggregate function
//
//---------------------------------------------------------------------------
class CScalarAggFunc : public CScalar
{
private:
	// aggregate func id
	IMDId *m_pmdidAggFunc;

	// resolved return type refers to a non-ambiguous type that was resolved during query
	// parsing if the actual return type of Agg is ambiguous (e.g., AnyElement in GPDB)
	// if resolved return type is NULL, then we can get Agg return type by looking up MD cache
	// using Agg MDId
	IMDId *m_pmdidResolvedRetType;

	// return type obtained by looking up MD cache
	IMDId *m_return_type_mdid;

	// aggregate function name
	const CWStringConst *m_pstrAggFunc;

	// distinct aggregate computation
	BOOL m_is_distinct;

	EAggfuncKind m_aggkind;

	ULongPtrArray *m_argtypes;

	// stage of the aggregate function
	EAggfuncStage m_eaggfuncstage;

	// is result of splitting aggregates
	BOOL m_fSplit;

	// is aggregate replicate slice execution safe
	BOOL m_fRepSafe;

public:
	CScalarAggFunc(const CScalarAggFunc &) = delete;

	// ctor
	CScalarAggFunc(CMemoryPool *mp, IMDId *pmdidAggFunc,
				   IMDId *resolved_rettype, const CWStringConst *pstrAggFunc,
				   BOOL is_distinct, EAggfuncStage eaggfuncstage, BOOL fSplit,
				   EAggfuncKind aggkind, ULongPtrArray *argtypes,
				   BOOL fRepSafe);

	// dtor
	~CScalarAggFunc() override
	{
		m_pmdidAggFunc->Release();
		CRefCount::SafeRelease(m_pmdidResolvedRetType);
		CRefCount::SafeRelease(m_return_type_mdid);
		GPOS_DELETE(m_pstrAggFunc);
		CRefCount::SafeRelease(m_argtypes);
	}


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarAggFunc;
	}

	// return a string for aggregate function
	const CHAR *
	SzId() const override
	{
		return "CScalarAggFunc";
	}


	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// conversion function
	static CScalarAggFunc *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarAggFunc == pop->Eopid());

		return dynamic_cast<CScalarAggFunc *>(pop);
	}


	// aggregate function name
	const CWStringConst *PstrAggFunc() const;

	// aggregate func id
	IMDId *MDId() const;

	// ident accessors
	BOOL
	IsDistinct() const
	{
		return m_is_distinct;
	}

	void
	SetIsDistinct(BOOL val)
	{
		m_is_distinct = val;
	}

	EAggfuncKind
	AggKind() const
	{
		return m_aggkind;
	}

	ULongPtrArray *
	GetArgTypes() const
	{
		return m_argtypes;
	}

	// stage of the aggregate function
	EAggfuncStage
	Eaggfuncstage() const
	{
		return m_eaggfuncstage;
	}

	// global or local aggregate function
	BOOL
	FGlobal() const
	{
		return (EaggfuncstageGlobal == m_eaggfuncstage);
	}

	// is result of splitting aggregates
	BOOL
	FSplit() const
	{
		return m_fSplit;
	}

	// is aggregate replicate slice execution safe
	BOOL
	FRepSafe() const
	{
		return m_fRepSafe;
	}

	// type of expression's result
	IMDId *
	MdidType() const override
	{
		if (nullptr == m_pmdidResolvedRetType)
		{
			return m_return_type_mdid;
		}

		return m_pmdidResolvedRetType;
	}

	// is return type of Agg ambiguous?
	BOOL
	FHasAmbiguousReturnType() const
	{
		return (nullptr != m_pmdidResolvedRetType);
	}

	// is function count(*)?
	BOOL FCountStar() const;

	// is function count(Any)?
	BOOL FCountAny() const;

	// is function either min() or max()?
	BOOL IsMinMax(const IMDType *mdtype) const;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// lookup mdid of return type for given Agg function
	static IMDId *PmdidLookupReturnType(IMDId *pmdidAggFunc, BOOL fGlobal,
										CMDAccessor *pmdaInput = nullptr);

};	// class CScalarAggFunc

}  // namespace gpopt


#endif	// !GPOPT_CScalarAggFunc_H

// EOF
