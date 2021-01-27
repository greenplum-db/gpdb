//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CMDProviderMemory.h
//
//	@doc:
//		Memory-based provider of metadata objects.
//---------------------------------------------------------------------------



#ifndef GPMD_CMDProviderMemory_H
#define GPMD_CMDProviderMemory_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/IMDCacheObject.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDProvider.h"

namespace gpmd
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CMDProviderMemory
//
//	@doc:
//		Memory-based provider of metadata objects.
//
//---------------------------------------------------------------------------
class CMDProviderMemory : public IMDProvider
{
protected:
	// hash map of serialized MD objects indexed by their MD id
	typedef CHashMap<IMDId, CWStringDynamic, IMDId::MDIdHash,
					 IMDId::MDIdCompare, CleanupRelease, CleanupDelete>
		MDIdToSerializedMDIdMap;

	// metadata objects indexed by their metadata id
	MDIdToSerializedMDIdMap *m_mdmap;

	// load MD objects in the hash map
	void LoadMetadataObjectsFromArray(CMemoryPool *mp,
									  IMDCacheObjectArray *mdcache_obj_array);

	// private copy ctor
	CMDProviderMemory(const CMDProviderMemory &);

public:
	// ctor
	CMDProviderMemory(CMemoryPool *mp, IMDCacheObjectArray *mdcache_obj_array);

	// ctor
	CMDProviderMemory(CMemoryPool *mp, const CHAR *file_name);

	//dtor
	virtual ~CMDProviderMemory();

	// returns the DXL string of the requested metadata object
	virtual CWStringBase *GetMDObjDXLStr(CMemoryPool *mp,
										 CMDAccessor *md_accessor,
										 IMDId *mdid) const;

	// return the mdid for the specified system id and type
	virtual IMDId *MDId(CMemoryPool *mp, CSystemId sysid,
						IMDType::ETypeInfo type_info) const;
};
}  // namespace gpmd



#endif	// !GPMD_CMDProviderMemory_H

// EOF
