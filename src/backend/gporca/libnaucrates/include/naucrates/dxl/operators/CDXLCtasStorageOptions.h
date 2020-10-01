//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLCtasStorageOptions.h
//
//	@doc:
//		Class for CTAS storage options
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLCTASStorageOptions_H
#define GPDXL_CDXLCTASStorageOptions_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"
#include "naucrates/md/CMDName.h"

using namespace gpmd;

namespace gpdxl
{
// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLCtasStorageOptions
//
//	@doc:
//		Class for representing CTAS storage options
//
//---------------------------------------------------------------------------
class CDXLCtasStorageOptions : public CRefCount
{
public:
	struct CDXLCtasOption
	{
		// the type of the Option encoded as an integer
		ULONG m_type;

		// option name
		CWStringBase *m_str_name;

		// option value
		CWStringBase *m_str_value;

		// does this represent a NULL value
		BOOL m_is_null;

		// ctor
		CDXLCtasOption(ULONG type, CWStringBase *str_name,
					   CWStringBase *str_value, BOOL is_null)
			: m_type(type),
			  m_str_name(str_name),
			  m_str_value(str_value),
			  m_is_null(is_null)
		{
			GPOS_ASSERT(NULL != str_name);
			GPOS_ASSERT(NULL != str_value);
		}

		// dtor
		~CDXLCtasOption()
		{
			GPOS_DELETE(m_str_name);
			GPOS_DELETE(m_str_value);
		}
	};

	typedef CDynamicPtrArray<CDXLCtasOption, CleanupDelete> CDXLCtasOptionArray;

	//-------------------------------------------------------------------
	//	@enum:
	//		ECtasOnCommitAction
	//
	//	@doc:
	//		On commit specification for temporary tables created with CTAS
	//		at the end of a transaction block
	//
	//-------------------------------------------------------------------
	enum ECtasOnCommitAction
	{
		EctascommitNOOP,	  // no action
		EctascommitPreserve,  // rows are preserved
		EctascommitDelete,	  // rows are deleted
		EctascommitDrop,	  // table is dropped
		EctascommitSentinel
	};

private:
	// tablespace name
	CMDName *m_mdname_tablespace;

	// on commit action spec
	ECtasOnCommitAction m_ctas_on_commit_action;

	// array of name-value pairs of storage options
	CDXLCtasOptionArray *m_ctas_storage_option_array;

	// private copy ctor
	CDXLCtasStorageOptions(const CDXLCtasStorageOptions &);

	// string representation of OnCommit action
	static const CWStringConst *GetOnCommitActionStr(
		ECtasOnCommitAction ctas_on_commit_action);

public:
	// ctor
	CDXLCtasStorageOptions(CMDName *mdname_tablespace,
						   ECtasOnCommitAction ctas_on_commit_action,
						   CDXLCtasOptionArray *ctas_storage_option_array);

	// dtor
	virtual ~CDXLCtasStorageOptions();

	// accessor to tablespace name
	CMDName *GetMdNameTableSpace() const;

	// on commit action
	CDXLCtasStorageOptions::ECtasOnCommitAction GetOnCommitAction() const;

	// accessor to options
	CDXLCtasOptionArray *GetDXLCtasOptionArray() const;

	// serialize to DXL
	void Serialize(CXMLSerializer *xml_serializer) const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLCTASStorageOptions_H

// EOF
