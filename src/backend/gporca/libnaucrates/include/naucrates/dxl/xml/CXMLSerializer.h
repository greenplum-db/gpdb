//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CXMLSerializer.h
//
//	@doc:
//		Class for creating XML documents.
//---------------------------------------------------------------------------

#ifndef GPDXL_CXMLSerializer_H
#define GPDXL_CXMLSerializer_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/common/CStack.h"
#include "gpos/io/COstream.h"
#include "gpos/string/CWStringConst.h"

#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXMLSerializer
//
//	@doc:
//		Class for creating XML documents.
//
//---------------------------------------------------------------------------
class CXMLSerializer
{
	// stack of strings
	typedef CStack<const CWStringBase> StrStack;

private:
	// memory pool
	CMemoryPool *m_mp;

	// output stream for writing out the xml document
	IOstream &m_os;

	// should XML document be indented
	BOOL m_indentation;

	// stack of open elements
	StrStack *m_strstackElems;

	// denotes whether the last written tag is open and needs closing
	BOOL m_fOpenTag;

	// level of nesting in the XML document (i.e. number of open XML tags)
	ULONG m_ulLevel;

	// steps since last check for aborts
	ULONG m_iteration_since_last_abortcheck;

	// private copy ctor
	CXMLSerializer(const CXMLSerializer &);

	// add indentation
	void Indent();

	// escape the given string and write it to the given stream
	static void WriteEscaped(IOstream &os, const CWStringBase *str);

public:
	// ctor/dtor
	CXMLSerializer(CMemoryPool *mp, IOstream &os, BOOL indentation = true)
		: m_mp(mp),
		  m_os(os),
		  m_indentation(indentation),
		  m_strstackElems(NULL),
		  m_fOpenTag(false),
		  m_ulLevel(0),
		  m_iteration_since_last_abortcheck(0)
	{
		m_strstackElems = GPOS_NEW(m_mp) StrStack(m_mp);
	}

	~CXMLSerializer();

	// get underlying memory pool
	CMemoryPool *
	Pmp() const
	{
		return m_mp;
	}

	// starts an XML document
	void StartDocument();

	// opens a new element with the given name
	void OpenElement(const CWStringBase *pstrNamespace,
					 const CWStringBase *elem_str);

	// closes the element with the given name
	void CloseElement(const CWStringBase *pstrNamespace,
					  const CWStringBase *elem_str);

	// adds a string-valued attribute
	void AddAttribute(const CWStringBase *pstrAttr,
					  const CWStringBase *str_value);

	// adds a character string attribute
	void AddAttribute(const CWStringBase *pstrAttr, const CHAR *szValue);

	// adds an unsigned integer-valued attribute
	void AddAttribute(const CWStringBase *pstrAttr, ULONG ulValue);

	// adds an unsigned long integer attribute
	void AddAttribute(const CWStringBase *pstrAttr, ULLONG ullValue);

	// adds an integer-valued attribute
	void AddAttribute(const CWStringBase *pstrAttr, INT iValue);

	// adds an integer-valued attribute
	void AddAttribute(const CWStringBase *pstrAttr, LINT value);

	// adds a boolean attribute
	void AddAttribute(const CWStringBase *pstrAttr, BOOL fValue);

	// add a double-valued attribute
	void AddAttribute(const CWStringBase *pstrAttr, CDouble value);

	// add a byte array attribute
	void AddAttribute(const CWStringBase *pstrAttr, BOOL is_null,
					  const BYTE *data, ULONG length);
};

}  // namespace gpdxl

#endif	//!GPDXL_CXMLSerializer_H

// EOF
