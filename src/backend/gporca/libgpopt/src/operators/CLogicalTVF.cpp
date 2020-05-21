//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CLogicalTVF.cpp
//
//	@doc:
//		Implementation of table functions
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalTVF.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/COptCtxt.h"

#include "naucrates/statistics/CStatistics.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::CLogicalTVF
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalTVF::CLogicalTVF
	(
	CMemoryPool *mp
	)
	:
	CLogical(mp),
	m_func_mdid(NULL),
	m_return_type_mdid(NULL),
	m_pstr(NULL),
	m_pdrgpcoldesc(NULL),
	m_pdrgpcrOutput(NULL),
	m_efs(IMDFunction::EfsImmutable),
	m_efda(IMDFunction::EfdaNoSQL),
	m_returns_set(true)
{
	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::CLogicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalTVF::CLogicalTVF
	(
	CMemoryPool *mp,
	IMDId *mdid_func,
	IMDId *mdid_return_type,
	CWStringConst *str,
	CColumnDescriptorArray *pdrgpcoldesc
	)
	:
	CLogical(mp),
	m_func_mdid(mdid_func),
	m_return_type_mdid(mdid_return_type),
	m_pstr(str),
	m_pdrgpcoldesc(pdrgpcoldesc),
	m_pdrgpcrOutput(NULL)
{
	GPOS_ASSERT(mdid_func->IsValid());
	GPOS_ASSERT(mdid_return_type->IsValid());
	GPOS_ASSERT(NULL != str);
	GPOS_ASSERT(NULL != pdrgpcoldesc);

	// generate a default column set for the list of column descriptors
	m_pdrgpcrOutput = PdrgpcrCreateMapping(mp, pdrgpcoldesc, UlOpId());

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDFunction *pmdfunc = md_accessor->RetrieveFunc(m_func_mdid);

	m_efs = pmdfunc->GetFuncStability();
	m_efda = pmdfunc->GetFuncDataAccess();
	m_returns_set = pmdfunc->ReturnsSet();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::CLogicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalTVF::CLogicalTVF
	(
	CMemoryPool *mp,
	IMDId *mdid_func,
	IMDId *mdid_return_type,
	CWStringConst *str,
	CColumnDescriptorArray *pdrgpcoldesc,
	CColRefArray *pdrgpcrOutput
	)
	:
	CLogical(mp),
	m_func_mdid(mdid_func),
	m_return_type_mdid(mdid_return_type),
	m_pstr(str),
	m_pdrgpcoldesc(pdrgpcoldesc),
	m_pdrgpcrOutput(pdrgpcrOutput)
{
	GPOS_ASSERT(mdid_func->IsValid());
	GPOS_ASSERT(mdid_return_type->IsValid());
	GPOS_ASSERT(NULL != str);
	GPOS_ASSERT(NULL != pdrgpcoldesc);
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDFunction *pmdfunc = md_accessor->RetrieveFunc(m_func_mdid);

	m_efs = pmdfunc->GetFuncStability();
	m_efda = pmdfunc->GetFuncDataAccess();
	m_returns_set = pmdfunc->ReturnsSet();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::~CLogicalTVF
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalTVF::~CLogicalTVF()
{
	CRefCount::SafeRelease(m_func_mdid);
	CRefCount::SafeRelease(m_return_type_mdid);
	CRefCount::SafeRelease(m_pdrgpcoldesc);
	CRefCount::SafeRelease(m_pdrgpcrOutput);
	GPOS_DELETE(m_pstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalTVF::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(
								COperator::HashValue(),
								gpos::CombineHashes(
										m_func_mdid->HashValue(),
										gpos::CombineHashes(
												m_return_type_mdid->HashValue(),
												gpos::HashPtr<CColumnDescriptorArray>(m_pdrgpcoldesc))));
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));
	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalTVF::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalTVF *popTVF = CLogicalTVF::PopConvert(pop);
		
	return m_func_mdid->Equals(popTVF->FuncMdId()) &&
			m_return_type_mdid->Equals(popTVF->ReturnTypeMdId()) &&
			m_pdrgpcoldesc->Equals(popTVF->Pdrgpcoldesc()) &&
			m_pdrgpcrOutput->Equals(popTVF->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalTVF::PopCopyWithRemappedColumns
	(
	CMemoryPool *mp,
	UlongToColRefMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRefArray *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemapAndCreate(mp, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput, colref_mapping, must_exist);
	}

	CWStringConst *str = GPOS_NEW(mp) CWStringConst(m_pstr->GetBuffer());
	m_func_mdid->AddRef();
	m_return_type_mdid->AddRef();
	m_pdrgpcoldesc->AddRef();

	return GPOS_NEW(mp) CLogicalTVF(mp, m_func_mdid, m_return_type_mdid, str, m_pdrgpcoldesc, pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalTVF::DeriveOutputColumns
	(
	CMemoryPool *mp,
	CExpressionHandle & // exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	pcrs->Include(m_pdrgpcrOutput);

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::DeriveFunctionProperties
//
//	@doc:
//		Derive function properties
//
//---------------------------------------------------------------------------
CFunctionProp *
CLogicalTVF::DeriveFunctionProperties
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
	const
{
	BOOL fVolatileScan = (IMDFunction::EfsVolatile == m_efs);
	return PfpDeriveFromChildren(mp, exprhdl, m_efs, m_efda, fVolatileScan, true /*fScan*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::FInputOrderSensitive
//
//	@doc:
//		Sensitivity to input order
//
//---------------------------------------------------------------------------
BOOL
CLogicalTVF::FInputOrderSensitive() const
{
	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalTVF::PxfsCandidates
	(
	CMemoryPool *mp
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfUnnestTVF);
	(void) xform_set->ExchangeSet(CXform::ExfImplementTVF);
	(void) xform_set->ExchangeSet(CXform::ExfImplementTVFNoArgs);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalTVF::DeriveMaxCard
	(
	CMemoryPool *, // mp
	CExpressionHandle & // exprhdl
	)
	const
{
	if (m_returns_set)
	{
		// unbounded by default
		return CMaxCard();
	}

	return CMaxCard(1);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------

IStatistics *
CLogicalTVF::PstatsDerive
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // stats_ctxt
	)
	const
{
	CDouble rows(1.0);
	if (m_returns_set)
	{
		rows = CStatistics::DefaultRelationRows;
	}

	return PstatsDeriveDummy(mp, exprhdl, rows);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalTVF::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalTVF::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}
	os << SzId() << " (" << Pstr()->GetBuffer() << ") ";
	os << "Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "] ";
		
	return os;
}

// EOF

