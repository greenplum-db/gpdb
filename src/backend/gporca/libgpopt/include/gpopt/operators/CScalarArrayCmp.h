//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarArrayCmp.h
//
//	@doc:
//		Class for scalar array compare operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarArrayCmp_H
#define GPOPT_CScalarArrayCmp_H

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
//		CScalarArrayCmp
//
//	@doc:
//		Class for scalar array compare operators
//
//---------------------------------------------------------------------------
class CScalarArrayCmp : public CScalar
{
public:
	// type of array comparison
	enum EArrCmpType
	{
		EarrcmpAny,
		EarrcmpAll,
		EarrcmpSentinel
	};

private:
	// compare operator mdid
	IMDId *m_mdid_op;

	// comparison operator name
	const CWStringConst *m_pscOp;

	// array compare type
	EArrCmpType m_earrccmpt;

	// does operator return NULL on NULL input?
	BOOL m_returns_null_on_null_input;

	// names of array compare types
	static const CHAR m_rgszCmpType[EarrcmpSentinel][10];

public:
	CScalarArrayCmp(const CScalarArrayCmp &) = delete;

	// ctor
	CScalarArrayCmp(CMemoryPool *mp, IMDId *mdid_op,
					const CWStringConst *pstrOp, EArrCmpType earrcmpt);

	// dtor
	~CScalarArrayCmp() override
	{
		m_mdid_op->Release();
		GPOS_DELETE(m_pscOp);
	}


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarArrayCmp;
	}

	// comparison type
	EArrCmpType
	Earrcmpt() const
	{
		return m_earrccmpt;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarArrayCmp";
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
	static CScalarArrayCmp *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarArrayCmp == pop->Eopid());

		return dynamic_cast<CScalarArrayCmp *>(pop);
	}

	// name of the comparison operator
	const CWStringConst *Pstr() const;

	// operator mdid
	IMDId *MdIdOp() const;

	// the type of the scalar expression
	IMDId *MdidType() const override;

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// expand array comparison expression into a conjunctive/disjunctive expression
	static CExpression *PexprExpand(CMemoryPool *mp,
									CExpression *pexprArrayCmp);

};	// class CScalarArrayCmp

}  // namespace gpopt

#endif	// !GPOPT_CScalarArrayCmp_H

// EOF
