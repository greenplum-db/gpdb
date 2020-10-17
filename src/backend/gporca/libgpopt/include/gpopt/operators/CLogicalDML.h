//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDML.h
//
//	@doc:
//		Logical DML operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDML_H
#define GPOPT_CLogicalDML_H

#include "gpos/base.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDML
//
//	@doc:
//		Logical DML operator
//
//---------------------------------------------------------------------------
class CLogicalDML : public CLogical
{
public:
	// enum of DML operators
	enum EDMLOperator
	{
		EdmlInsert,
		EdmlDelete,
		EdmlUpdate,
		EdmlSentinel
	};

	static const WCHAR m_rgwszDml[EdmlSentinel][10];

private:
	// dml operator
	EDMLOperator m_edmlop;

	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// source columns
	CColRefArray *m_pdrgpcrSource;

	// set of modified columns from the target table
	CBitSet *m_pbsModified;

	// action column
	CColRef *m_pcrAction;

	// table oid column
	CColRef *m_pcrTableOid;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

	// tuple oid column if one exists
	CColRef *m_pcrTupleOid;

	// private copy ctor
	CLogicalDML(const CLogicalDML &);

public:
	// ctor
	explicit CLogicalDML(CMemoryPool *mp);

	// ctor
	CLogicalDML(CMemoryPool *mp, EDMLOperator edmlop,
				CTableDescriptor *ptabdesc, CColRefArray *colref_array,
				CBitSet *pbsModified, CColRef *pcrAction, CColRef *pcrTableOid,
				CColRef *pcrCtid, CColRef *pcrSegmentId, CColRef *pcrTupleOid);

	// dtor
	virtual ~CLogicalDML();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalDML;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalDML";
	}

	// dml operator
	EDMLOperator
	Edmlop() const
	{
		return m_edmlop;
	}

	// source columns
	CColRefArray *
	PdrgpcrSource() const
	{
		return m_pdrgpcrSource;
	}

	// modified columns set
	CBitSet *
	PbsModified() const
	{
		return m_pbsModified;
	}

	// action column
	CColRef *
	PcrAction() const
	{
		return m_pcrAction;
	}

	// table oid column
	CColRef *
	PcrTableOid() const
	{
		return m_pcrTableOid;
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

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// tuple oid column
	CColRef *
	PcrTupleOid() const
	{
		return m_pcrTupleOid;
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
	virtual CPropConstraint *DerivePropertyConstraint(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

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
	static CLogicalDML *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalDML == pop->Eopid());

		return dynamic_cast<CLogicalDML *>(pop);
	}

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CLogicalDML
}  // namespace gpopt

#endif	// !GPOPT_CLogicalDML_H

// EOF
