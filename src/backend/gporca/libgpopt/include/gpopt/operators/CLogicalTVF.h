//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalTVF.h
//
//	@doc:
//		Table-valued function
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalTVF_H
#define GPOPT_CLogicalTVF_H

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalTVF
//
//	@doc:
//		Table-valued function
//
//---------------------------------------------------------------------------
class CLogicalTVF : public CLogical
{
private:
	// function mdid
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	// function name
	CWStringConst *m_pstr;

	// array of column descriptors: the schema of the function result
	CColumnDescriptorArray *m_pdrgpcoldesc;

	// output columns
	CColRefArray *m_pdrgpcrOutput;

	// function stability
	IMDFunction::EFuncStbl m_efs;

	// function data access
	IMDFunction::EFuncDataAcc m_efda;

	// does this function return a set of rows
	BOOL m_returns_set;

	// private copy ctor
	CLogicalTVF(const CLogicalTVF &);

public:
	// ctors
	explicit CLogicalTVF(CMemoryPool *mp);

	CLogicalTVF(CMemoryPool *mp, IMDId *mdid_func, IMDId *mdid_return_type,
				CWStringConst *str, CColumnDescriptorArray *pdrgpcoldesc);

	CLogicalTVF(CMemoryPool *mp, IMDId *mdid_func, IMDId *mdid_return_type,
				CWStringConst *str, CColumnDescriptorArray *pdrgpcoldesc,
				CColRefArray *pdrgpcrOutput);

	// dtor
	virtual ~CLogicalTVF();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalTVF;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalTVF";
	}

	// function mdid
	IMDId *
	FuncMdId() const
	{
		return m_func_mdid;
	}

	// return type
	IMDId *
	ReturnTypeMdId() const
	{
		return m_return_type_mdid;
	}

	// function name
	const CWStringConst *
	Pstr() const
	{
		return m_pstr;
	}

	// col descr accessor
	CColumnDescriptorArray *
	Pdrgpcoldesc() const
	{
		return m_pdrgpcoldesc;
	}

	// accessors
	CColRefArray *
	PdrgpcrOutput() const
	{
		return m_pdrgpcrOutput;
	}

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const;

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *DeriveOutputColumns(CMemoryPool *, CExpressionHandle &);

	// derive partition consumer info
	virtual CPartInfo *
	DerivePartitionInfo(CMemoryPool *mp,
						CExpressionHandle &	 //exprhdl
	) const
	{
		return GPOS_NEW(mp) CPartInfo(mp);
	}

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &  //exprhdl
	) const
	{
		return GPOS_NEW(mp) CPropConstraint(
			mp, GPOS_NEW(mp) CColRefSetArray(mp), NULL /*pcnstr*/);
	}

	// derive function properties
	virtual CFunctionProp *DeriveFunctionProperties(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	virtual CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   // pcrsInput
			 ULONG				   // child_index
	) const
	{
		return NULL;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	// stat promise
	virtual EStatPromise
	Esp(CExpressionHandle &) const
	{
		return CLogical::EspLow;
	}

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalTVF *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalTVF == pop->Eopid());

		return dynamic_cast<CLogicalTVF *>(pop);
	}


	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CLogicalTVF

}  // namespace gpopt


#endif	// !GPOPT_CLogicalTVF_H

// EOF
