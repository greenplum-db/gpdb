//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CXMLSerializer.cpp
//
//	@doc:
//		Implementation of the class for creating XML documents.
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpdxl;

#define GPDXL_SERIALIZE_CFA_FREQUENCY 30

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::~CXMLSerializer
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CXMLSerializer::~CXMLSerializer()
{
	GPOS_DELETE(m_strstackElems);
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::StartDocument
//
//	@doc:
//		Write the opening tags for the XML document
//
//---------------------------------------------------------------------------
void
CXMLSerializer::StartDocument()
{
	GPOS_ASSERT(m_strstackElems->IsEmpty());
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenXMLDocHeader)->GetBuffer();
	if (m_indentation)
	{
		m_os << std::endl;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::OpenElement
//
//	@doc:
//		Write an opening tag for the specified element
//
//---------------------------------------------------------------------------
void
CXMLSerializer::OpenElement(const CWStringBase *pstrNamespace,
							const CWStringBase *elem_str)
{
	GPOS_ASSERT(NULL != elem_str);

	m_iteration_since_last_abortcheck++;

	if (GPDXL_SERIALIZE_CFA_FREQUENCY < m_iteration_since_last_abortcheck)
	{
		GPOS_CHECK_ABORT;
		m_iteration_since_last_abortcheck = 0;
	}

	// put element on the stack
	m_strstackElems->Push(elem_str);

	// write the closing bracket for the previous element if necessary and add indentation
	if (m_fOpenTag)
	{
		m_os << CDXLTokens::GetDXLTokenStr(EdxltokenBracketCloseTag)
					->GetBuffer();	// >
		if (m_indentation)
		{
			m_os << std::endl;
		}
	}

	Indent();

	// write element to stream
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenBracketOpenTag)
				->GetBuffer();	// <

	if (NULL != pstrNamespace)
	{
		m_os << pstrNamespace->GetBuffer()
			 << CDXLTokens::GetDXLTokenStr(EdxltokenColon)
					->GetBuffer();	// "namespace:"
	}
	m_os << elem_str->GetBuffer();

	m_fOpenTag = true;
	m_ulLevel++;
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::CloseElement
//
//	@doc:
//		Write a closing tag for the specified element
//
//---------------------------------------------------------------------------
void
CXMLSerializer::CloseElement(const CWStringBase *pstrNamespace,
							 const CWStringBase *elem_str)
{
	GPOS_ASSERT(NULL != elem_str);
	GPOS_ASSERT(0 < m_ulLevel);

	m_ulLevel--;

	// assert element is on top of the stack
#ifdef GPOS_DEBUG
	const CWStringBase *strOpenElem =
#endif
		m_strstackElems->Pop();

	GPOS_ASSERT(strOpenElem->Equals(elem_str));

	if (m_fOpenTag)
	{
		// singleton element with no children - close the element with "/>"
		m_os << CDXLTokens::GetDXLTokenStr(EdxltokenBracketCloseSingletonTag)
					->GetBuffer();	// />
		if (m_indentation)
		{
			m_os << std::endl;
		}
		m_fOpenTag = false;
	}
	else
	{
		// add indentation
		Indent();

		// write closing tag for element to stream
		m_os << CDXLTokens::GetDXLTokenStr(EdxltokenBracketOpenEndTag)
					->GetBuffer();	// </
		if (NULL != pstrNamespace)
		{
			m_os << pstrNamespace->GetBuffer()
				 << CDXLTokens::GetDXLTokenStr(EdxltokenColon)
						->GetBuffer();	// "namespace:"
		}
		m_os << elem_str->GetBuffer()
			 << CDXLTokens::GetDXLTokenStr(EdxltokenBracketCloseTag)
					->GetBuffer();	// >
		if (m_indentation)
		{
			m_os << std::endl;
		}
	}

	GPOS_CHECK_ABORT;
}


//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr,
							 const CWStringBase *str_value)
{
	GPOS_ASSERT(NULL != pstrAttr);
	GPOS_ASSERT(NULL != str_value);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	  // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // "
	WriteEscaped(m_os, str_value);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // "
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, const CHAR *szValue)
{
	GPOS_ASSERT(NULL != pstrAttr);
	GPOS_ASSERT(NULL != szValue);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // "
		 << szValue
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // "
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with a ULONG value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, ULONG ulValue)
{
	GPOS_ASSERT(NULL != pstrAttr);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // \"
		 << ulValue
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // \"
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with a ULLONG value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, ULLONG ullValue)
{
	GPOS_ASSERT(NULL != pstrAttr);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // \"
		 << ullValue
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // \"
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with an INT value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, INT iValue)
{
	GPOS_ASSERT(NULL != pstrAttr);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // \"
		 << iValue
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // \"
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with an LINT value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, LINT value)
{
	GPOS_ASSERT(NULL != pstrAttr);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // \"
		 << value
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // \"
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with a CDouble value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, CDouble value)
{
	GPOS_ASSERT(NULL != pstrAttr);

	GPOS_ASSERT(m_fOpenTag);
	m_os << CDXLTokens::GetDXLTokenStr(EdxltokenSpace)->GetBuffer()
		 << pstrAttr->GetBuffer()
		 << CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()	 // =
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer()	 // \"
		 << value
		 << CDXLTokens::GetDXLTokenStr(EdxltokenQuote)->GetBuffer();  // \"
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with a BOOL value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, BOOL fValue)
{
	const CWStringConst *str_value = NULL;

	if (fValue)
	{
		str_value = CDXLTokens::GetDXLTokenStr(EdxltokenTrue);
	}
	else
	{
		str_value = CDXLTokens::GetDXLTokenStr(EdxltokenFalse);
	}

	AddAttribute(pstrAttr, str_value);
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::Indent
//
//	@doc:
//		Adds indentation to the output document according to the current nesting
//		level.
//
//---------------------------------------------------------------------------
void
CXMLSerializer::Indent()
{
	if (!m_indentation)
	{
		return;
	}

	for (ULONG ul = 0; ul < m_ulLevel; ul++)
	{
		m_os << CDXLTokens::GetDXLTokenStr(EdxltokenIndent)->GetBuffer();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::WriteEscaped
//
//	@doc:
//		Write the given string to the output stream by escaping it first
//
//---------------------------------------------------------------------------
void
CXMLSerializer::WriteEscaped(IOstream &os, const CWStringBase *str)
{
	GPOS_ASSERT(NULL != str);

	const ULONG length = str->Length();
	const WCHAR *wsz = str->GetBuffer();

	for (ULONG ulA = 0; ulA < length; ulA++)
	{
		const WCHAR wc = wsz[ulA];

		switch (wc)
		{
			case GPOS_WSZ_LIT('\"'):
				os << GPOS_WSZ_LIT("&quot;");
				break;
			case GPOS_WSZ_LIT('\''):
				os << GPOS_WSZ_LIT("&apos;");
				break;
			case GPOS_WSZ_LIT('<'):
				os << GPOS_WSZ_LIT("&lt;");
				break;
			case GPOS_WSZ_LIT('>'):
				os << GPOS_WSZ_LIT("&gt;");
				break;
			case GPOS_WSZ_LIT('&'):
				os << GPOS_WSZ_LIT("&amp;");
				break;
			case GPOS_WSZ_LIT('\t'):
				os << GPOS_WSZ_LIT("&#x9;");
				break;
			case GPOS_WSZ_LIT('\n'):
				os << GPOS_WSZ_LIT("&#xA;");
				break;
			case GPOS_WSZ_LIT('\r'):
				os << GPOS_WSZ_LIT("&#xD;");
				break;
			default:
				os << wc;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXMLSerializer::AddAttribute
//
//	@doc:
//		Adds an attribute-value pair to the currently open XML tag.
//		Same as above but with an LINT value
//
//---------------------------------------------------------------------------
void
CXMLSerializer::AddAttribute(const CWStringBase *pstrAttr, BOOL is_null,
							 const BYTE *data, ULONG length)
{
	if (!is_null)
	{
		CWStringDynamic *str =
			CDXLUtils::EncodeByteArrayToString(m_mp, data, length);
		AddAttribute(pstrAttr, str);
		GPOS_DELETE(str);
	}
}

// EOF
