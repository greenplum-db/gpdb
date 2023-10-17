//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLTranslateContext.cpp
//
//	@doc:
//		Implementation of the methods for accessing translation context
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/translate/CDXLTranslateContext.h"

using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::CDXLTranslateContext
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLTranslateContext::CDXLTranslateContext(CMemoryPool *mp,
										   BOOL is_child_agg_node,
										   const Query *query)
	: m_mp(mp), m_is_child_agg_node(is_child_agg_node), m_query(query)
{
	// initialize hash table
	m_colid_to_target_entry_map = GPOS_NEW(m_mp) ULongToTargetEntryMap(m_mp);
	m_colid_to_paramid_map = GPOS_NEW(m_mp) ULongToColParamMap(m_mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::CDXLTranslateContext
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLTranslateContext::CDXLTranslateContext(CMemoryPool *mp,
										   BOOL is_child_agg_node,
										   ULongToColParamMap *original)
	: m_mp(mp), m_is_child_agg_node(is_child_agg_node), m_query(NULL)
{
	m_colid_to_target_entry_map = GPOS_NEW(m_mp) ULongToTargetEntryMap(m_mp);
	m_colid_to_paramid_map = GPOS_NEW(m_mp) ULongToColParamMap(m_mp);
	CopyParamHashmap(original);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::~CDXLTranslateContext
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLTranslateContext::~CDXLTranslateContext()
{
	m_colid_to_target_entry_map->Release();
	m_colid_to_paramid_map->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::IsParentAggNode
//
//	@doc:
//		Is this translation context created by a parent Agg node
//
//---------------------------------------------------------------------------
BOOL
CDXLTranslateContext::IsParentAggNode() const
{
	return m_is_child_agg_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::CopyParamHashmap
//
//	@doc:
//		copy the params hashmap
//
//---------------------------------------------------------------------------
void
CDXLTranslateContext::CopyParamHashmap(ULongToColParamMap *original)
{
	// iterate over full map
	ULongToColParamMapIter hashmapiter(original);
	while (hashmapiter.Advance())
	{
		CMappingElementColIdParamId *colidparamid =
			const_cast<CMappingElementColIdParamId *>(hashmapiter.Value());

		const ULONG colid = colidparamid->GetColId();
		ULONG *key = GPOS_NEW(m_mp) ULONG(colid);
		colidparamid->AddRef();
		m_colid_to_paramid_map->Insert(key, colidparamid);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::GetTargetEntry
//
//	@doc:
//		Lookup target entry associated with a given col id
//
//---------------------------------------------------------------------------
const TargetEntry *
CDXLTranslateContext::GetTargetEntry(ULONG colid) const
{
	return m_colid_to_target_entry_map->Find(&colid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::GetParamIdMappingElement
//
//	@doc:
//		Lookup col->param mapping associated with a given col id
//
//---------------------------------------------------------------------------
const CMappingElementColIdParamId *
CDXLTranslateContext::GetParamIdMappingElement(ULONG colid) const
{
	return m_colid_to_paramid_map->Find(&colid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::InsertMapping
//
//	@doc:
//		Insert a (col id, target entry) mapping
//
//---------------------------------------------------------------------------
void
CDXLTranslateContext::InsertMapping(ULONG colid, TargetEntry *target_entry)
{
	// copy key
	ULONG *key = GPOS_NEW(m_mp) ULONG(colid);

	// insert colid->target entry mapping in the hash map
	BOOL result = m_colid_to_target_entry_map->Insert(key, target_entry);

	if (!result)
	{
		GPOS_DELETE(key);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContext::FInsertParamMapping
//
//	@doc:
//		Insert a (col id, param id) mapping
//
//---------------------------------------------------------------------------
BOOL
CDXLTranslateContext::FInsertParamMapping(
	ULONG colid, CMappingElementColIdParamId *colidparamid)
{
	// copy key
	ULONG *key = GPOS_NEW(m_mp) ULONG(colid);

	// insert colid->target entry mapping in the hash map
	return m_colid_to_paramid_map->Insert(key, colidparamid);
}

// EOF
