//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware, Inc. or its affiliates.
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
	// private copy ctor
	CScalarValuesList(const CScalarValuesList &);

public:
	// ctor
	CScalarValuesList(CMemoryPool *mp);

	// ident accessors
	EOperatorId
	Eopid() const
	{
		return EopScalarValuesList;
	}

	// operator name
	const CHAR *
	SzId() const
	{
		return "CScalarValuesList";
	}

	// match function
	BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}

	// type of expression's result
	IMDId *
	MdidType() const
	{
		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

		return md_accessor->PtMDType<IMDTypeBool>()->MDId();
	}

	// conversion function
	static CScalarValuesList *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarValuesList == pop->Eopid());

		return dynamic_cast<CScalarValuesList *>(pop);
	}

};	// class CScalarValuesList
}  // namespace gpopt

#endif	// !GPOPT_CScalarValuesList_H

// EOF
