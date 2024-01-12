//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CScanHint.h
//
//	@doc:
//		CScanHint represents an ORCA optimizer hint that favors a specific scan
//		type (e.g. index scan, bitmap scan, seq scan) on a specific relation
//		(identified by table name or alias) in a query.
//---------------------------------------------------------------------------
#ifndef GPOS_CScanHint_H
#define GPOS_CScanHint_H

#include <vector>

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CEnumSet.h"
#include "gpos/common/CEnumSetIter.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/hints/IHint.h"
#include "gpopt/operators/COperator.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpopt
{
class CScanHint : public IHint, public DbgPrintMixin<CScanHint>
{
public:
	enum EType
	{
		SeqScan,
		NoSeqScan,
		IndexScan,
		NoIndexScan,
		IndexOnlyScan,
		NoIndexOnlyScan,
		BitmapScan,
		NoBitmapScan,

		Sentinal
	};

	using CEHintTypeSet = CEnumSet<CScanHint::EType, CScanHint::Sentinal>;
	using CEHintTypeSetSetIter =
		CEnumSetIter<CScanHint::EType, CScanHint::Sentinal>;

protected:
	CMemoryPool *m_mp;

private:
	// relation or alias used in the query
	const CWStringBase *m_name;

	// specfied index names (if any) for index and bitmap scans
	StringPtrArray *m_indexnames;

	// all scan hint associated with this relation
	CEHintTypeSet *m_types;

public:
	CScanHint(CMemoryPool *mp, const CWStringBase *name,
			  StringPtrArray *indexnames)
		: m_mp(mp),
		  m_name(name),
		  m_indexnames(indexnames),
		  m_types(GPOS_NEW(mp) CEHintTypeSet(mp))
	{
	}

	~CScanHint() override
	{
		GPOS_DELETE(m_name);
		m_indexnames->Release();
		m_types->Release();
	}

	const CWStringBase *
	GetName() const
	{
		return m_name;
	}

	const StringPtrArray *
	GetIndexNames() const
	{
		return m_indexnames;
	}

	void
	AddType(CScanHint::EType type)
	{
		m_types->ExchangeSet(type);
	}

	virtual BOOL SatisfiesOperator(COperator *op) const;

	virtual IOstream &OsPrint(IOstream &os) const;

	virtual void Serialize(CXMLSerializer *xml_serializer) const;
};

using ScanHintList = CDynamicPtrArray<CScanHint, CleanupRelease>;

}  // namespace gpopt

#endif	// !GPOS_CScanHint_H

// EOF
