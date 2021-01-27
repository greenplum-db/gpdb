//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDRequest.h
//
//	@doc:
//		Class for a representing MD requests
//---------------------------------------------------------------------------



#ifndef GPMD_CMDRequest_H
#define GPMD_CMDRequest_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "naucrates/md/CSystemId.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDType.h"

namespace gpmd
{
using namespace gpos;


//--------------------------------------------------------------------------
//	@class:
//		CMDRequest
//
//	@doc:
//		Class for representing MD requests
//
//--------------------------------------------------------------------------
class CMDRequest : public CRefCount
{
public:
	// fwd decl
	struct SMDTypeRequest;
	struct SMDFuncRequest;

	// array of type requests
	typedef CDynamicPtrArray<SMDTypeRequest, CleanupDelete> SMDTypeRequestArray;

	//---------------------------------------------------------------------------
	//	@class:
	//		SMDTypeRequest
	//
	//	@doc:
	//		Struct for representing requests for types metadata
	//
	//---------------------------------------------------------------------------
	struct SMDTypeRequest
	{
		// system id
		CSystemId m_sysid;

		// type info
		IMDType::ETypeInfo m_type_info;

		// ctor
		SMDTypeRequest(CSystemId sysid, IMDType::ETypeInfo type_info)
			: m_sysid(sysid), m_type_info(type_info)
		{
		}
	};

private:
	// memory pool
	CMemoryPool *m_mp;

	// array of mdids
	IMdIdArray *m_mdid_array;

	// type info requests
	SMDTypeRequestArray *m_mdtype_request_array;

	// serialize system id
	CWStringDynamic *GetStrRepr(CSystemId sysid);

	// private copy ctor
	CMDRequest(const CMDRequest &);

public:
	// ctor
	CMDRequest(CMemoryPool *mp, IMdIdArray *mdid_array,
			   SMDTypeRequestArray *mdtype_request_array);

	// ctor: type request only
	CMDRequest(CMemoryPool *mp, SMDTypeRequest *md_type_request);

	// dtor
	virtual ~CMDRequest();

	// accessors

	// array of mdids
	IMdIdArray *
	GetMdIdArray() const
	{
		return m_mdid_array;
	}

	// array of type info requests
	SMDTypeRequestArray *
	GetMDTypeRequestArray() const
	{
		return m_mdtype_request_array;
	}

	// serialize request in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer);
};
}  // namespace gpmd

#endif	// !GPMD_CMDRequest_H

// EOF
