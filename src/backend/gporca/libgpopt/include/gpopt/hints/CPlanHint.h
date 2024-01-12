//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CPlanHint.h
//
//	@doc:
//		CPlanHint is a container for all hints (IHint) used by a query.
//---------------------------------------------------------------------------
#ifndef GPOS_CPlanHint_H
#define GPOS_CPlanHint_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/hints/CScanHint.h"
#include "gpopt/hints/IHint.h"
#include "gpopt/operators/COperator.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPlanHint
//---------------------------------------------------------------------------
class CPlanHint : public CRefCount, public DbgPrintMixin<CPlanHint>
{
private:
	CMemoryPool *m_mp{nullptr};

	ScanHintList *m_scan_hints{nullptr};

public:
	CPlanHint(CMemoryPool *mp);

	~CPlanHint() override;

	// Add a scan hint
	void AddHint(CScanHint *hint);

	// Get a scan hint that matches a name (table or alias)
	CScanHint *GetScanHint(const char *name);
	CScanHint *GetScanHint(const CWStringBase *name);

	IOstream &OsPrint(IOstream &os) const;

	void Serialize(CXMLSerializer *xml_serializer) const;
};	// class CPlanHint

}  // namespace gpopt


#endif	// !GPOS_CPlanHint_H

// EOF
