//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CTableDescriptor.h
//
//	@doc:
//		Abstraction of metadata for tables; represents metadata as stored
//		in the catalog -- not as used in queries, e.g. no aliasing etc.
//---------------------------------------------------------------------------
#ifndef GPOPT_CTableDescriptor_H
#define GPOPT_CTableDescriptor_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

// dynamic array of columns -- array owns columns
using CColumnDescriptorArray =
	CDynamicPtrArray<CColumnDescriptor, CleanupRelease>;

// dynamic array of bitsets
using CBitSetArray = CDynamicPtrArray<CBitSet, CleanupRelease>;

//---------------------------------------------------------------------------
//	@class:
//		CTableDescriptor
//
//	@doc:
//		metadata abstraction for tables
//
//---------------------------------------------------------------------------
class CTableDescriptor : public CRefCount,
						 public DbgPrintMixin<CTableDescriptor>
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// mdid of the table
	IMDId *m_mdid;

	// name of table
	CName m_name;

	// array of columns
	CColumnDescriptorArray *m_pdrgpcoldesc;

	// distribution policy
	IMDRelation::Ereldistrpolicy m_rel_distr_policy;

	// storage type
	IMDRelation::Erelstoragetype m_erelstoragetype;

	// distribution columns for hash distribution
	CColumnDescriptorArray *m_pdrgpcoldescDist;

	// Opfamily used for hash distribution
	IMdIdArray *m_distr_opfamilies;

	// if true, we need to consider a hash distributed table as random
	// there are two possible scenarios:
	// 1. in hawq 2.0, some hash distributed tables need to be considered as random,
	//	  depending on its bucket number
	// 2. for a partitioned table, it may contain a part with a different distribution
	BOOL m_convert_hash_to_random;

	// indexes of partition columns for partitioned tables
	ULongPtrArray *m_pdrgpulPart;

	// key sets
	CBitSetArray *m_pdrgpbsKeys;

	// id of user the table needs to be accessed with
	ULONG m_execute_as_user_id;

	// lockmode from the parser
	INT m_lockmode;

	// identifier of query to which current table belongs.
	// This field is used for assigning current table entry with
	// target one within DML operation. If descriptor doesn't point
	// to the target (result) relation it has value UNASSIGNED_QUERYID
	ULONG m_assigned_query_id_for_target_rel;

public:
	CTableDescriptor(const CTableDescriptor &) = delete;

	// ctor
	CTableDescriptor(CMemoryPool *, IMDId *mdid, const CName &,
					 BOOL convert_hash_to_random,
					 IMDRelation::Ereldistrpolicy rel_distr_policy,
					 IMDRelation::Erelstoragetype erelstoragetype,
					 ULONG ulExecuteAsUser, INT lockmode,
					 ULONG assigned_query_id_for_target_rel);

	// dtor
	~CTableDescriptor() override;

	// add a column to the table descriptor
	void AddColumn(CColumnDescriptor *);

	// add the column at the specified position to the list of distribution columns
	void AddDistributionColumn(ULONG ulPos, IMDId *opfamily);

	// add the column at the specified position to the list of partition columns
	void AddPartitionColumn(ULONG ulPos);

	// add a keyset
	BOOL FAddKeySet(CBitSet *pbs);

	// accessors
	ULONG ColumnCount() const;
	const CColumnDescriptor *Pcoldesc(ULONG) const;

	// mdid accessor
	IMDId *
	MDId() const
	{
		return m_mdid;
	}

	// name accessor
	const CName &
	Name() const
	{
		return m_name;
	}

	// execute as user accessor
	ULONG
	GetExecuteAsUserId() const
	{
		return m_execute_as_user_id;
	}

	INT
	LockMode() const
	{
		return m_lockmode;
	}

	// return the position of a particular attribute (identified by attno)
	ULONG GetAttributePosition(INT attno) const;

	// column descriptor accessor
	CColumnDescriptorArray *
	Pdrgpcoldesc() const
	{
		return m_pdrgpcoldesc;
	}

	// distribution column descriptors accessor
	const CColumnDescriptorArray *
	PdrgpcoldescDist() const
	{
		return m_pdrgpcoldescDist;
	}

	// distribution column descriptors accessor
	const IMdIdArray *
	DistrOpfamilies() const
	{
		return m_distr_opfamilies;
	}

	// partition column indexes accessor
	const ULongPtrArray *
	PdrgpulPart() const
	{
		return m_pdrgpulPart;
	}

	// array of key sets
	const CBitSetArray *
	PdrgpbsKeys() const
	{
		return m_pdrgpbsKeys;
	}


	// distribution policy
	IMDRelation::Ereldistrpolicy
	GetRelDistribution() const
	{
		return m_rel_distr_policy;
	}

	// storage type
	IMDRelation::Erelstoragetype
	RetrieveRelStorageType() const
	{
		return m_erelstoragetype;
	}

	BOOL
	IsPartitioned() const
	{
		return 0 < m_pdrgpulPart->Size();
	}

	// true iff a hash distributed table needs to be considered as random;
	// this happens for when we are in phase 1 of a gpexpand or (for GPDB 5X)
	// when we have a mix of hash-distributed and random distributed partitions
	BOOL
	ConvertHashToRandom() const
	{
		return m_convert_hash_to_random;
	}

	// helper function for finding the index of a column descriptor in
	// an array of column descriptors
	static ULONG UlPos(const CColumnDescriptor *,
					   const CColumnDescriptorArray *);

	IOstream &OsPrint(IOstream &os) const;

	// returns number of indices
	ULONG IndexCount();

	BOOL
	IsAORowOrColTable() const
	{
		return m_erelstoragetype == IMDRelation::ErelstorageAppendOnlyCols ||
			   m_erelstoragetype == IMDRelation::ErelstorageAppendOnlyRows;
	}

	ULONG
	GetAssignedQueryIdForTargetRel() const
	{
		return m_assigned_query_id_for_target_rel;
	}

};	// class CTableDescriptor
}  // namespace gpopt

#endif	// !GPOPT_CTableDescriptor_H

// EOF
