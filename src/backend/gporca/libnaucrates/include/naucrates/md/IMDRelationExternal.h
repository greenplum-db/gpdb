//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		IMDRelationExternal.h
//
//	@doc:
//		Interface for external relations in the metadata cache
//---------------------------------------------------------------------------

#ifndef GPMD_IMDRelationExternal_H
#define GPMD_IMDRelationExternal_H

#include "gpos/base.h"

#include "naucrates/md/IMDRelation.h"

namespace gpmd
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		IMDRelationExternal
//
//	@doc:
//		Interface for external relations in the metadata cache
//
//---------------------------------------------------------------------------
class IMDRelationExternal : public IMDRelation
{
public:
	// storage type
	Erelstoragetype
	RetrieveRelStorageType() const override
	{
		return ErelstorageExternal;
	}

	// is this a temp relation
	BOOL
	IsTemporary() const override
	{
		return false;
	}

	// is this a partitioned table
	BOOL
	IsPartitioned() const override
	{
		return false;
	}

	// return true if a hash distributed table needs to be considered as random
	BOOL ConvertHashToRandom() const override = 0;

	// does this table have oids
	BOOL
	HasOids() const override
	{
		return false;
	}

	// number of partition columns
	ULONG
	PartColumnCount() const override
	{
		return 0;
	}

	// number of partitions
	ULONG
	PartitionCount() const override
	{
		return 0;
	}

	// retrieve the partition column at the given position
	const IMDColumn *PartColAt(ULONG /*pos*/) const override
	{
		GPOS_ASSERT(!"External tables have no partition columns");
		return nullptr;
	}

	// retrieve list of partition types
	CharPtrArray *
	GetPartitionTypes() const override
	{
		GPOS_ASSERT(!"External tables have no partition types");
		return nullptr;
	}

	// retrieve the partition type at the given position
	CHAR PartTypeAtLevel(ULONG /*pos*/) const override
	{
		GPOS_ASSERT(!"External tables have no partition types");
		return (CHAR) 0;
	}

	// part constraint
	CDXLNode *
	MDPartConstraint() const override
	{
		return nullptr;
	}

	// reject limit
	virtual INT RejectLimit() const = 0;

	// reject limit in rows?
	virtual BOOL IsRejectLimitInRows() const = 0;

	// format error table mdid
	virtual IMDId *GetFormatErrTableMdid() const = 0;
};
}  // namespace gpmd

#endif	// !GPMD_IMDRelationExternal_H

// EOF
