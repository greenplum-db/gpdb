//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CScalarValuesList.h
//
//	@doc:
//		Class for scalar arrayref index list
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarValuesList_H
#define GPOPT_CScalarValuesList_H

#include "gpos/base.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CScalar.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDTypeBool.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarValuesList
//
//	@doc:
//		Scalar arrayref index list
//
//---------------------------------------------------------------------------
class CScalarValuesList : public CScalar
{
public:
	// private copy ctor
	CScalarValuesList(const CScalarValuesList &) = delete;

	// ctor
	CScalarValuesList(CMemoryPool *mp);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarValuesList;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarValuesList";
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
		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

		return md_accessor->PtMDType<IMDTypeBool>()->MDId();
	}

	// conversion function
	static CScalarValuesList *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarValuesList == pop->Eopid());

		return dynamic_cast<CScalarValuesList *>(pop);
	}

};	// class CScalarValuesList
}  // namespace gpopt

#endif	// !GPOPT_CScalarValuesList_H

// EOF
