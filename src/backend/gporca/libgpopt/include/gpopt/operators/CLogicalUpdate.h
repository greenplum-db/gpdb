//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUpdate.h
//
//	@doc:
//		Logical Update operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalUpdate_H
#define GPOPT_CLogicalUpdate_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalUpdate
//
//	@doc:
//		Logical Update operator
//
//---------------------------------------------------------------------------
class CLogicalUpdate : public CLogical
{
private:
	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// columns to delete
	CColRefArray *m_pdrgpcrDelete;

	// columns to insert
	CColRefArray *m_pdrgpcrInsert;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

	// tuple oid column
	CColRef *m_pcrTupleOid;

	// private copy ctor
	CLogicalUpdate(const CLogicalUpdate &);

public:
	// ctor
	explicit CLogicalUpdate(CMemoryPool *mp);

	// ctor
	CLogicalUpdate(CMemoryPool *mp, CTableDescriptor *ptabdesc,
				   CColRefArray *pdrgpcrDelete, CColRefArray *pdrgpcrInsert,
				   CColRef *pcrCtid, CColRef *pcrSegmentId,
				   CColRef *pcrTupleOid);

	// dtor
	virtual ~CLogicalUpdate();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalUpdate;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalUpdate";
	}

	// columns to delete
	CColRefArray *
	PdrgpcrDelete() const
	{
		return m_pdrgpcrDelete;
	}

	// columns to insert
	CColRefArray *
	PdrgpcrInsert() const
	{
		return m_pdrgpcrInsert;
	}

	// ctid column
	CColRef *
	PcrCtid() const
	{
		return m_pcrCtid;
	}

	// segmentId column
	CColRef *
	PcrSegmentId() const
	{
		return m_pcrSegmentId;
	}

	// tuple oid column
	CColRef *
	PcrTupleOid() const
	{
		return m_pcrTupleOid;
	}

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
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
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
											CExpressionHandle &exprhdl);


	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 // mp
							 CExpressionHandle &exprhdl) const
	{
		return CLogical::PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive partition consumer info
	virtual CPartInfo *
	DerivePartitionInfo(CMemoryPool *,	// mp,
						CExpressionHandle &exprhdl) const
	{
		return PpartinfoPassThruOuter(exprhdl);
	}

	// compute required stats columns of the n-th child
	virtual CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *pcrsInput,
			 ULONG	// child_index
	) const
	{
		return PcrsStatsPassThru(pcrsInput);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	// derive key collections
	virtual CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	// stat promise
	virtual EStatPromise
	Esp(CExpressionHandle &) const
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalUpdate *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalUpdate == pop->Eopid());

		return dynamic_cast<CLogicalUpdate *>(pop);
	}

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CLogicalUpdate
}  // namespace gpopt

#endif	// !GPOPT_CLogicalUpdate_H

// EOF
