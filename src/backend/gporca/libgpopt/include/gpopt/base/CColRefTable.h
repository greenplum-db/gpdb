//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CColRefTable.h
//
//	@doc:
//		Column reference implementation for base table columns
//---------------------------------------------------------------------------
#ifndef GPOS_CColRefTable_H
#define GPOS_CColRefTable_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CList.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "gpopt/metadata/CName.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CColRefTable
//
//	@doc:
//		Column reference for base table columns
//
//---------------------------------------------------------------------------
class CColRefTable : public CColRef
{
private:
	// attno from catalog
	INT m_iAttno;

	// does column allow null values
	BOOL m_is_nullable;

	// id of the operator which is the source of this column reference
	// not owned
	ULONG m_ulSourceOpId;

	// is the column a distribution key
	BOOL m_is_dist_col;

	// width of the column, for instance  char(10) column has width 10
	ULONG m_width;

public:
	CColRefTable(const CColRefTable &) = delete;

	// ctors
	CColRefTable(const CColumnDescriptor *pcd, ULONG id, const CName *pname,
				 ULONG ulOpSource);

	CColRefTable(const IMDType *pmdtype, INT type_modifier, INT attno,
				 BOOL is_nullable, ULONG id, const CName *pname,
				 ULONG ulOpSource, BOOL is_dist_col,
				 ULONG ulWidth = gpos::ulong_max);

	// dtor
	~CColRefTable() override;

	// accessor of column reference type
	CColRef::Ecolreftype
	Ecrt() const override
	{
		return CColRef::EcrtTable;
	}

	// accessor of attribute number
	INT
	AttrNum() const
	{
		return m_iAttno;
	}

	// does column allow null values?
	BOOL
	IsNullable() const
	{
		return m_is_nullable;
	}

	// is column a system column?
	BOOL
	IsSystemCol() const override
	{
		// TODO-  04/13/2012, make this check system independent
		// using MDAccessor
		return 0 >= m_iAttno;
	}

	// is column a distribution column?
	BOOL
	IsDistCol() const override
	{
		return m_is_dist_col;
	}

	// width of the column
	ULONG
	Width() const
	{
		return m_width;
	}

	// id of source operator
	ULONG
	UlSourceOpId() const
	{
		return m_ulSourceOpId;
	}

	// conversion
	static CColRefTable *
	PcrConvert(CColRef *cr)
	{
		GPOS_ASSERT(cr->Ecrt() == CColRef::EcrtTable);
		return dynamic_cast<CColRefTable *>(cr);
	}


};	// class CColRefTable
}  // namespace gpopt

#endif	// !GPOS_CColRefTable_H

// EOF
