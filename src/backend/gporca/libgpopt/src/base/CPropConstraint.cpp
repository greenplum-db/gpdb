//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPropConstraint.cpp
//
//	@doc:
//		Implementation of constraint property
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CPropConstraint.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/COptCtxt.h"
#include "gpos/error/CAutoTrace.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::CPropConstraint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPropConstraint::CPropConstraint
	(
	CMemoryPool *mp,
	CColRefSetArray *pdrgpcrs,
	CConstraint *pcnstr
	)
	:
	m_pdrgpcrs(pdrgpcrs),
	m_phmcrcrs(NULL),
	m_pcnstr(pcnstr)
{
	GPOS_ASSERT(NULL != pdrgpcrs);
	InitHashMap(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::~CPropConstraint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPropConstraint::~CPropConstraint()
{
	m_pdrgpcrs->Release();
	CRefCount::SafeRelease(m_phmcrcrs);
	CRefCount::SafeRelease(m_pcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::InitHashMap
//
//	@doc:
//		Initialize mapping between columns and equivalence classes
//
//---------------------------------------------------------------------------
void
CPropConstraint::InitHashMap
	(
	CMemoryPool *mp
	)
{
	GPOS_ASSERT(NULL == m_phmcrcrs);
	const ULONG ulEquiv = m_pdrgpcrs->Size();

	// m_phmcrcrs is only needed when storing equivalent columns
	if (0 != ulEquiv)
	{
		m_phmcrcrs = GPOS_NEW(mp) ColRefToColRefSetMap(mp);
	}
	for (ULONG ul = 0; ul < ulEquiv; ul++)
	{
		CColRefSet *pcrs = (*m_pdrgpcrs)[ul];

		CColRefSetIter crsi(*pcrs);
		while (crsi.Advance())
		{
			pcrs->AddRef();
#ifdef GPOS_DEBUG
			BOOL fres =
#endif //GPOS_DEBUG
			m_phmcrcrs->Insert(crsi.Pcr(), pcrs);
			GPOS_ASSERT(fres);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::FContradiction
//
//	@doc:
//		Is this a contradiction
//
//---------------------------------------------------------------------------
BOOL
CPropConstraint::FContradiction()
const
{
	return (NULL != m_pcnstr && m_pcnstr->FContradiction());
}

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::PexprScalarMappedFromEquivCols
//
//	@doc:
//		Return scalar expression on the given column mapped from all constraints
//		on its equivalent columns
//
//---------------------------------------------------------------------------
CExpression *
CPropConstraint::PexprScalarMappedFromEquivCols
	(
	CMemoryPool *mp,
	CColRef *colref
	)
	const
{
	if(NULL == m_pcnstr || NULL == m_phmcrcrs)
	{
		return NULL;
	}
	CColRefSet *pcrs = m_phmcrcrs->Find(colref);
	if (NULL == pcrs || 1 == pcrs->Size())
	{
		return NULL;
	}

	// get constraints for all other columns in this equivalence class
	// except the current column
	CColRefSet *pcrsEquiv = GPOS_NEW(mp) CColRefSet(mp);
	pcrsEquiv->Include(pcrs);
	pcrsEquiv->Exclude(colref);

	CConstraint *pcnstr = m_pcnstr->Pcnstr(mp, pcrsEquiv);
	pcrsEquiv->Release();
	if (NULL == pcnstr)
	{
		return NULL;
	}

	// generate a copy of all these constraints for the current column
	CConstraint *pcnstrCol = pcnstr->PcnstrRemapForColumn(mp, colref);
	CExpression *pexprScalar = pcnstrCol->PexprScalar(mp);
	pexprScalar->AddRef();

	pcnstr->Release();
	pcnstrCol->Release();

	return pexprScalar;
}

//---------------------------------------------------------------------------
//	@function:
//		CPropConstraint::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CPropConstraint::OsPrint
	(
	IOstream &os
	)
	const
{
	const ULONG length = m_pdrgpcrs->Size();
	if (0 < length)
	{
		os << "Equivalence Classes: { ";

		for (ULONG ul = 0; ul < length; ul++)
		{
			CColRefSet *pcrs = (*m_pdrgpcrs)[ul];
			os << "(" << *pcrs << ") ";
		}

		os << "} ";
	}

	if (NULL != m_pcnstr)
	{
		os << "Constraint:" << *m_pcnstr;
	}

	return os;
}

// EOF

