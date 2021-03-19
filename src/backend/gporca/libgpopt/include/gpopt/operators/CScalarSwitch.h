//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSwitch.h
//
//	@doc:
//		Scalar switch operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSwitch_H
#define GPOPT_CScalarSwitch_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSwitch
//
//	@doc:
//		Scalar switch operator. This corresponds to SQL case statments in the form
//		(case expr when expr1 then ret1 when expr2 then ret2 ... else retdef end)
//		The switch operator is represented as follows:
//		Switch
//		|-- expr1
//		|-- SwitchCase
//		|	|-- expr1
//		|	+-- ret1
//		|-- SwitchCase
//		|	|-- expr2
//		|	+-- ret2
//		:
//		+-- retdef
//
//---------------------------------------------------------------------------
class CScalarSwitch : public CScalar
{
private:
	// return type
	IMDId *m_mdid_type;

	// is operator return type BOOL?
	BOOL m_fBoolReturnType;

public:
	CScalarSwitch(const CScalarSwitch &) = delete;

	// ctor
	CScalarSwitch(CMemoryPool *mp, IMDId *mdid_type);

	// dtor
	~CScalarSwitch() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarSwitch;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarSwitch";
	}

	// the type of the scalar expression
	IMDId *
	MdidType() const override
	{
		return m_mdid_type;
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

	// boolean expression evaluation
	EBoolEvalResult
	Eber(ULongPtrArray *pdrgpulChildren) const override
	{
		return EberNullOnAllNullChildren(pdrgpulChildren);
	}

	// conversion function
	static CScalarSwitch *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSwitch == pop->Eopid());

		return dynamic_cast<CScalarSwitch *>(pop);
	}

};	// class CScalarSwitch

}  // namespace gpopt


#endif	// !GPOPT_CScalarSwitch_H

// EOF
