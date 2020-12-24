//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarArrayRefIndexList.h
//
//	@doc:
//		Class for scalar arrayref index list
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarArrayRefIndexList_H
#define GPOPT_CScalarArrayRefIndexList_H

#include "gpos/base.h"
#include "naucrates/md/IMDId.h"

#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarArrayRefIndexList
//
//	@doc:
//		Scalar arrayref index list
//
//---------------------------------------------------------------------------
class CScalarArrayRefIndexList : public CScalar
{
public:
	enum EIndexListType
	{
		EiltLower,	// lower index
		EiltUpper,	// upper index
		EiltSentinel
	};

private:
	// index list type
	EIndexListType m_eilt;

public:
	CScalarArrayRefIndexList(const CScalarArrayRefIndexList &) = delete;

	// ctor
	CScalarArrayRefIndexList(CMemoryPool *mp, EIndexListType eilt);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarArrayRefIndexList;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarArrayRefIndexList";
	}

	// index list type
	EIndexListType
	Eilt() const
	{
		return m_eilt;
	}

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

	// type of expression's result
	IMDId *
	MdidType() const override
	{
		GPOS_ASSERT(
			!"Invalid function call: CScalarArrayRefIndexList::MdidType()");
		return NULL;
	}

	// conversion function
	static CScalarArrayRefIndexList *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarArrayRefIndexList == pop->Eopid());

		return dynamic_cast<CScalarArrayRefIndexList *>(pop);
	}

};	// class CScalarArrayRefIndexList
}  // namespace gpopt

#endif	// !GPOPT_CScalarArrayRefIndexList_H

// EOF
