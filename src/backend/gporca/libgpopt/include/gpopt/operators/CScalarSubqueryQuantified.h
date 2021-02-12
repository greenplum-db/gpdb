//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryQuantified.h
//
//	@doc:
//		Parent class for quantified subquery operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubqueryQuantified_H
#define GPOPT_CScalarSubqueryQuantified_H

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"
#include "gpopt/xforms/CSubqueryHandler.h"

namespace gpopt
{
using namespace gpos;

// fwd declarations
class CExpressionHandle;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSubqueryQuantified
//
//	@doc:
//		Parent class for quantified subquery operators (ALL/ANY subqueries);
//		A quantified subquery expression has two children:
//		- Logical child: the inner logical expression
//		- Scalar child:	the scalar expression in the outer expression that
//		is used in quantified comparison;
//
//		Example: SELECT * from R where a+b = ANY (SELECT c from S);
//		- logical child: (SELECT c from S)
//		- scalar child : (a+b)
//
//---------------------------------------------------------------------------
class CScalarSubqueryQuantified : public CScalar
{
private:
	// id of comparison operator
	IMDId *m_scalar_op_mdid;

	// name of comparison operator
	const CWStringConst *m_pstrScalarOp;

	// column reference used in comparison
	const CColRef *m_pcr;

protected:
	// ctor
	CScalarSubqueryQuantified(CMemoryPool *mp, IMDId *scalar_op_mdid,
							  const CWStringConst *pstrScalarOp,
							  const CColRef *colref);

	// dtor
	~CScalarSubqueryQuantified() override;

public:
	CScalarSubqueryQuantified(const CScalarSubqueryQuantified &) = delete;

	// operator mdid accessor
	IMDId *MdIdOp() const;

	// operator name accessor
	const CWStringConst *PstrOp() const;

	// column reference accessor
	const CColRef *
	Pcr() const
	{
		return m_pcr;
	}

	// return the type of the scalar expression
	IMDId *MdidType() const override;

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

	// return locally used columns
	CColRefSet *PcrsUsed(CMemoryPool *mp, CExpressionHandle &exprhdl) override;

	// derive partition consumer info
	CPartInfo *PpartinfoDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const override;

	// conversion function
	static CScalarSubqueryQuantified *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSubqueryAny == pop->Eopid() ||
					EopScalarSubqueryAll == pop->Eopid());

		return dynamic_cast<CScalarSubqueryQuantified *>(pop);
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CScalarSubqueryQuantified
}  // namespace gpopt

#endif	// !GPOPT_CScalarSubqueryQuantified_H

// EOF
