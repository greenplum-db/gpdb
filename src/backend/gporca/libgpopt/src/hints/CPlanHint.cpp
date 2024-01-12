//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CPlanHint.cpp
//
//	@doc:
//		Container of plan hint objects
//---------------------------------------------------------------------------

#include "gpopt/hints/CPlanHint.h"

#include "gpos/base.h"

using namespace gpopt;

FORCE_GENERATE_DBGSTR(CPlanHint);

//---------------------------------------------------------------------------
//	@function:
//		CPlanHint::CPlanHint
//---------------------------------------------------------------------------
CPlanHint::CPlanHint(CMemoryPool *mp)
	: m_mp(mp), m_scan_hints(GPOS_NEW(mp) ScanHintList(mp))
{
}

CPlanHint::~CPlanHint()
{
	m_scan_hints->Release();
}

void
CPlanHint::AddHint(CScanHint *hint)
{
	m_scan_hints->Append(hint);
}

CScanHint *
CPlanHint::GetScanHint(const char *relname)
{
	CWStringConst *name = GPOS_NEW(m_mp) CWStringConst(m_mp, relname);
	CScanHint *hint = GetScanHint(name);
	GPOS_DELETE(name);
	return hint;
}

CScanHint *
CPlanHint::GetScanHint(const CWStringBase *name)
{
	for (ULONG ul = 0; ul < m_scan_hints->Size(); ul++)
	{
		CScanHint *hint = (*m_scan_hints)[ul];
		if (name->Equals(hint->GetName()))
		{
			return hint;
		}
	}
	return nullptr;
}

IOstream &
CPlanHint::OsPrint(IOstream &os) const
{
	os << "PlanHint: [";
	if (nullptr == m_scan_hints)
	{
		os << "]";
		return os;
	}

	os << "\n";
	for (ULONG ul = 0; ul < m_scan_hints->Size(); ul++)
	{
		os << "  ";
		(*m_scan_hints)[ul]->OsPrint(os) << "\n";
	}
	os << "]";
	return os;
}

void
CPlanHint::Serialize(CXMLSerializer *xml_serializer) const
{
	for (ULONG ul = 0; ul < m_scan_hints->Size(); ul++)
	{
		(*m_scan_hints)[ul]->Serialize(xml_serializer);
	}
}


// EOF
