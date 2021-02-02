//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CScalarCmp.h
//
//	@doc:
//		Base class for all ScalarCmp operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarCmp_H
#define GPOPT_CScalarCmp_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDType.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarCmp
//
//	@doc:
//		scalar comparison operator
//
//---------------------------------------------------------------------------
class CScalarCmp : public CScalar
{
private:
	// metadata id in the catalog
	IMDId *m_mdid_op;

	// comparison operator name
	const CWStringConst *m_pstrOp;

	// comparison type
	IMDType::ECmpType m_comparision_type;

	// does operator return NULL on NULL input?
	BOOL m_returns_null_on_null_input;

	// is comparison commutative
	BOOL m_fCommutative;

	// private copy ctor
	CScalarCmp(const CScalarCmp &);

public:
	// ctor
	CScalarCmp(CMemoryPool *mp, IMDId *mdid_op, const CWStringConst *pstrOp,
			   IMDType::ECmpType cmp_type);

	// dtor
	~CScalarCmp() override
	{
		m_mdid_op->Release();
		GPOS_DELETE(m_pstrOp);
	}


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarCmp;
	}

	// comparison type
	IMDType::ECmpType
	ParseCmpType() const
	{
		return m_comparision_type;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarCmp";
	}


	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// is operator commutative
	BOOL FCommutative() const;

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// name of the comparison operator
	const CWStringConst *Pstr() const;

	// metadata id
	IMDId *MdIdOp() const;

	// the type of the scalar expression
	IMDId *MdidType() const override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CScalarCmp *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarCmp == pop->Eopid());

		return dynamic_cast<CScalarCmp *>(pop);
	}

	// get commuted scalar comparision operator
	virtual CScalarCmp *PopCommutedOp(CMemoryPool *mp, COperator *pop);

	// get the string representation of a metadata object
	static CWStringConst *Pstr(CMemoryPool *mp, CMDAccessor *md_accessor,
							   IMDId *mdid);

	// get metadata id of the commuted operator
	static IMDId *PmdidCommuteOp(CMDAccessor *md_accessor, COperator *pop);



};	// class CScalarCmp

}  // namespace gpopt

#endif	// !GPOPT_CScalarCmp_H

// EOF
