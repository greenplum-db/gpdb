//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		exception.cpp
//
//	@doc:
//		Initialization of DXL-specific exception messages
//---------------------------------------------------------------------------

#include "naucrates/exception.h"

#include "gpos/error/CMessage.h"
#include "gpos/error/CMessageRepository.h"

using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		EresExceptionInit
//
//	@doc:
//		Message initialization for DXL exceptions
//
//---------------------------------------------------------------------------
void
gpdxl::EresExceptionInit(CMemoryPool *mp)
{
	//---------------------------------------------------------------------------
	// Basic DXL messages in English
	//---------------------------------------------------------------------------
	CMessage rgmsg[ExmiDXLSentinel] = {
		// DXL-parsing related messages
		CMessage(CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag),
				 CException::ExsevError, GPOS_WSZ_WSZLEN("Unexpected tag: %ls"),
				 1,	 // elemname
				 GPOS_WSZ_WSZLEN("Unexpected tag encountered during parsing;")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLMissingAttribute),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Missing attribute value for attribute %ls in element %ls"),
			2,	// # params: attrname, elemname
			GPOS_WSZ_WSZLEN("Missing attribute value")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN("Invalid value for attribute %ls in element %ls"),
			2,	// # params: attrname, elemname
			GPOS_WSZ_WSZLEN("Invalid attribute value")),

		CMessage(CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedOperator),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("DXL: Unrecognized operator: %ls"),
				 1,	 // elemname
				 GPOS_WSZ_WSZLEN("Unrecognized operator name")),

		CMessage(CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedType),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("Unrecognized type name"),
				 0,	 // TODO:  - Sep 30, 2010; add params: typename
				 GPOS_WSZ_WSZLEN("Unrecognized type name")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedCompOperator),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN("Unrecognized comparison operator"),
			1,	// comp op
			GPOS_WSZ_WSZLEN("Unrecognized comparison operator")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLValidationError),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"XML validation exception: XML document does not conform to the DXL schema. Details: %s"),
			1,	// exception details
			GPOS_WSZ_WSZLEN("XML validation exception")),

		CMessage(CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLXercesParseError),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("Xerces parse exception"),
				 0,	 //
				 GPOS_WSZ_WSZLEN("Xerces parse exception")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXLIncorrectNumberOfChildren),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN("Incorrect Number of children"),
			0,	//
			GPOS_WSZ_WSZLEN("Incorrect Number of children")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support the following feature: %ls"),
			1,	//
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support this feature")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLAttributeNotFound),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Query-to-DXL Translation: Attribute number %d not found in project list"),
			1,	//
			GPOS_WSZ_WSZLEN(
				"Query-to-DXL Translation: Attribute number not found in project list")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support the following feature: %ls"),
			1,	//
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support this feature.")),

		// MD related messages
		CMessage(
			CException(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryDuplicate),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Entry for metadata cache object %ls already exists"),
			1,	// mdid
			GPOS_WSZ_WSZLEN("Entry for metadata cache object already exists")),

		CMessage(CException(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("Lookup of object %ls in cache failed"),
				 1,	 // mdid
				 GPOS_WSZ_WSZLEN("Lookup of object in cache failed")),

		CMessage(
			CException(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support the following feature: %ls"),
			1,	// md obj
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support this feature")),

		CMessage(CException(gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError),
				 CException::ExsevError, GPOS_WSZ_WSZLEN("PG exception raised"),
				 1,	 // type
				 GPOS_WSZ_WSZLEN("PG exception converted to GPOS exception")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLError),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Query-to-DXL Translation: %ls entry found due to incorrect normalization of query"),
			1,	// entry name
			GPOS_WSZ_WSZLEN(
				"Query-to-DXL Translation: Entry not found due to incorrect normalization of query")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiExpr2DXLUnsupportedFeature),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support the following feature: %ls"),
			1,	// feature name
			GPOS_WSZ_WSZLEN(
				"Falling back to Postgres-based planner because GPORCA does not support this feature")),

		CMessage(
			CException(gpdxl::ExmaConstExprEval,
					   gpdxl::ExmiConstExprEvalNonConst),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Illegal column reference \"%ls\" in constant expression"),
			1,	// column name
			GPOS_WSZ_WSZLEN(
				"Illegal column reference \"%ls\" in constant expression")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiExpr2DXLAttributeNotFound),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Expr-to-DXL Translation: Attribute number %d not found in project list"),
			1,	// attno
			GPOS_WSZ_WSZLEN(
				"Expr-to-DXL Translation: Attribute number not found in project list")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"DXL-to-PlStmt Translation: Attribute number %d not found in project list"),
			1,	// attno
			GPOS_WSZ_WSZLEN(
				"DXL-to-PlStmt Translation: Attribute number not found in project list")),

		CMessage(
			CException(gpdxl::ExmaDXL, gpdxl::ExmiDXL2ExprAttributeNotFound),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"DXL-to-Expr Translation: Attribute number %d not found in project list"),
			1,	// attno
			GPOS_WSZ_WSZLEN(
				"DXL-to-Expr Translation: Attribute number not found in project list")),

	};

	// copy exception array into heap
	CMessage *rgpmsg[ExmiDXLSentinel];
	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgpmsg); i++)
	{
		rgpmsg[i] = GPOS_NEW(mp) CMessage(rgmsg[i]);
	}

	CMessageRepository *pmr = CMessageRepository::GetMessageRepository();

	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgmsg); i++)
	{
		pmr->AddMessage(ElocEnUS_Utf8, rgpmsg[i]);
	}
}


// EOF
