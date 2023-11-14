//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarProjectElement.h
//
//	@doc:
//		Scalar project element operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarProjectElement_H
#define GPOPT_CScalarProjectElement_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarProjectElement
//
//	@doc:
//		Scalar project element operator is used to define a column reference
//		as equivalent to a scalar expression
//
//---------------------------------------------------------------------------
class CScalarProjectElement : public CScalar
{
private:
	// defined column reference
	CColRef *m_pcr;


public:
	CScalarProjectElement(const CScalarProjectElement &) = delete;

	CScalarProjectElement(CMemoryPool *mp) : CScalar(mp)
	{
	}

	// ctor
	CScalarProjectElement(CMemoryPool *mp, CColRef *colref)
		: CScalar(mp), m_pcr(colref)
	{
		GPOS_ASSERT(nullptr != colref);
	}

	// dtor
	~CScalarProjectElement() override = default;

	// identity accessor
	EOperatorId
	Eopid() const override
	{
		return EopScalarProjectElement;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarProjectElement";
	}

	// defined column reference accessor
	CColRef *
	Pcr() const
	{
		return m_pcr;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// return locally defined columns
	CColRefSet *
	PcrsDefined(CMemoryPool *mp,
				CExpressionHandle &	 // exprhdl
				) override
	{
		CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
		pcrs->Include(m_pcr);

		return pcrs;
	}

	// conversion function
	static CScalarProjectElement *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarProjectElement == pop->Eopid());

		return dynamic_cast<CScalarProjectElement *>(pop);
	}

	IMDId *
	MdidType() const override
	{
		GPOS_ASSERT(!"Invalid function call: CScalarProjectElemet::MdidType()");
		return nullptr;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CScalarProjectElement

}  // namespace gpopt


#endif	// !GPOPT_CScalarProjectElement_H

// EOF
