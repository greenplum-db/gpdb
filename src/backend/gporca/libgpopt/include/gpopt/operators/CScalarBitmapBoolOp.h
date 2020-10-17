//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CScalarBitmapBoolOp.h
//
//	@doc:
//		Bitmap bool op scalar operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CScalarBitmapBoolOp_H
#define GPOPT_CScalarBitmapBoolOp_H

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CScalarBitmapBoolOp
//
//	@doc:
//		Bitmap bool op scalar operator
//
//---------------------------------------------------------------------------
class CScalarBitmapBoolOp : public CScalar
{
public:
	// type of bitmap bool operator
	enum EBitmapBoolOp
	{
		EbitmapboolAnd,
		EbitmapboolOr,
		EbitmapboolSentinel
	};

private:
	// bitmap boolean operator
	EBitmapBoolOp m_ebitmapboolop;

	// bitmap type id
	IMDId *m_pmdidBitmapType;

	// private copy ctor
	CScalarBitmapBoolOp(const CScalarBitmapBoolOp &);

	static const WCHAR m_rgwszBitmapOpType[EbitmapboolSentinel][30];

public:
	// ctor
	CScalarBitmapBoolOp(CMemoryPool *mp, EBitmapBoolOp ebitmapboolop,
						IMDId *pmdidBitmapType);


	// dtor
	virtual ~CScalarBitmapBoolOp();

	// bitmap bool op type
	EBitmapBoolOp
	Ebitmapboolop() const
	{
		return m_ebitmapboolop;
	}

	// bitmap type id
	virtual IMDId *
	MdidType() const
	{
		return m_pmdidBitmapType;
	}

	// identifier
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarBitmapBoolOp;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarBitmapBoolOp";
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return false;
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

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

	// conversion
	static CScalarBitmapBoolOp *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarBitmapBoolOp == pop->Eopid());

		return dynamic_cast<CScalarBitmapBoolOp *>(pop);
	}

};	// class CScalarBitmapBoolOp
}  // namespace gpopt

#endif	// !GPOPT_CScalarBitmapBoolOp_H

// EOF
