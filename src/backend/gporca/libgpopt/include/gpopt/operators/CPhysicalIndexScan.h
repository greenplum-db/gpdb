//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CPhysicalIndexScan.h
//
//	@doc:
//		Base class for physical index scan operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalIndexScan_H
#define GPOPT_CPhysicalIndexScan_H

#include "gpos/base.h"

#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CIndexDescriptor;
class CName;
class CDistributionSpecHashed;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalIndexScan
//
//	@doc:
//		Base class for physical index scan operators
//
//---------------------------------------------------------------------------
class CPhysicalIndexScan : public CPhysicalScan
{
private:
	// index descriptor
	CIndexDescriptor *m_pindexdesc;

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

	// order
	COrderSpec *m_pos;

	// private copy ctor
	CPhysicalIndexScan(const CPhysicalIndexScan &);

public:
	// ctors
	CPhysicalIndexScan(CMemoryPool *mp, CIndexDescriptor *pindexdesc,
					   CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
					   const CName *pnameAlias, CColRefArray *colref_array,
					   COrderSpec *pos);

	// dtor
	virtual ~CPhysicalIndexScan();


	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalIndexScan;
	}

	// operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalIndexScan";
	}

	// table alias name
	const CName &
	NameAlias() const
	{
		return *m_pnameAlias;
	}

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	BOOL Matches(COperator *pop) const;

	// index descriptor
	CIndexDescriptor *
	Pindexdesc() const
	{
		return m_pindexdesc;
	}

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	virtual COrderSpec *
	PosDerive(CMemoryPool *,	   //mp
			  CExpressionHandle &  //exprhdl
	) const
	{
		m_pos->AddRef();
		return m_pos;
	}

	// derive partition index map
	virtual CPartIndexMap *
	PpimDerive(CMemoryPool *mp,
			   CExpressionHandle &,	 // exprhdl
			   CDrvdPropCtxt *		 //pdpctxt
	) const
	{
		return GPOS_NEW(mp) CPartIndexMap(mp);
	}

	virtual CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const
	{
		// rewindability of output is always true
		return GPOS_NEW(mp)
			CRewindabilitySpec(CRewindabilitySpec::ErtMarkRestore,
							   CRewindabilitySpec::EmhtNoMotion);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const;

	// conversion function
	static CPhysicalIndexScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalIndexScan == pop->Eopid());

		return dynamic_cast<CPhysicalIndexScan *>(pop);
	}

	// statistics derivation during costing
	virtual IStatistics *
	PstatsDerive(CMemoryPool *,		   // mp
				 CExpressionHandle &,  // exprhdl
				 CReqdPropPlan *,	   // prpplan
				 IStatisticsArray *	   //stats_ctxt
	) const
	{
		GPOS_ASSERT(
			!"stats derivation during costing for index scan is invalid");

		return NULL;
	}

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CPhysicalIndexScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalIndexScan_H

// EOF
