//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CScalarArrayRef.h
//
//	@doc:
//		Class for scalar arrayref
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarArrayRef_H
#define GPOPT_CScalarArrayRef_H

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarArrayRef
//
//	@doc:
//		Scalar arrayref
//
//		Arrayrefs are used to reference array elements or subarrays
//		e.g. select a[1], b[1][2][3], c[3:5], d[1:2][4:9] from arrtest;
//
//---------------------------------------------------------------------------
class CScalarArrayRef : public CScalar
{
private:
	// element type id
	IMDId *m_pmdidElem;

	// element type modifier
	INT m_type_modifier;

	// array type id
	IMDId *m_pmdidArray;

	// return type id
	IMDId *m_mdid_type;

	// private copy ctor
	CScalarArrayRef(const CScalarArrayRef &);

public:
	// ctor
	CScalarArrayRef(CMemoryPool *mp, IMDId *elem_type_mdid, INT type_modifier,
					IMDId *array_type_mdid, IMDId *return_type_mdid);

	// dtor
	virtual ~CScalarArrayRef();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarArrayRef;
	}

	// operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarArrayRef";
	}

	// element type id
	IMDId *
	PmdidElem() const
	{
		return m_pmdidElem;
	}

	// element type modifier
	virtual INT TypeModifier() const;

	// array type id
	IMDId *
	PmdidArray() const
	{
		return m_pmdidArray;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}

	// type of expression's result
	virtual IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// conversion function
	static CScalarArrayRef *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarArrayRef == pop->Eopid());

		return dynamic_cast<CScalarArrayRef *>(pop);
	}

};	// class CScalarArrayRef
}  // namespace gpopt

#endif	// !GPOPT_CScalarArrayRef_H

// EOF
