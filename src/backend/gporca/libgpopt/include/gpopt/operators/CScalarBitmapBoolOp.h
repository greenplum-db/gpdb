//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
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

	static const WCHAR m_rgwszBitmapOpType[EbitmapboolSentinel][30];

public:
	CScalarBitmapBoolOp(const CScalarBitmapBoolOp &) = delete;

	// ctor
	CScalarBitmapBoolOp(CMemoryPool *mp, EBitmapBoolOp ebitmapboolop,
						IMDId *pmdidBitmapType);


	// dtor
	~CScalarBitmapBoolOp() override;

	// bitmap bool op type
	EBitmapBoolOp
	Ebitmapboolop() const
	{
		return m_ebitmapboolop;
	}

	// bitmap type id
	IMDId *
	MdidType() const override
	{
		return m_pmdidBitmapType;
	}

	// identifier
	EOperatorId
	Eopid() const override
	{
		return EopScalarBitmapBoolOp;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarBitmapBoolOp";
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
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

	// debug print
	IOstream &OsPrint(IOstream &) const override;

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
