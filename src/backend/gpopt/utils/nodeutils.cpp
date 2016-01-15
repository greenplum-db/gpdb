//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@filename:
//		nodeutils.cpp
//
//	@doc:
//
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/utils/gpdbdefs.h"
#include "gpopt/utils/nodeutils.h"
#include "optimizer/walkers.h"

//---------------------------------------------------------------------------
//	@function:
//		textToString
//
//	@doc:
//		Convert text item to string. Caller is responsible for freeing memory.
//---------------------------------------------------------------------------

char *textToString(text *pt)
{
	char *sz = VARDATA(pt);
	int iLen = VARSIZE(pt) - VARHDRSZ;
	char *szResult = (char *) palloc(iLen + 1);
	memcpy(szResult, sz, iLen);
	szResult[iLen] = '\0';
	return szResult;
}

//---------------------------------------------------------------------------
//	@function:
//		stringToText
//
//	@doc:
//		String to text type. Caller is responsible for freeing memory.
//---------------------------------------------------------------------------

text *stringToText(const char *sz)
{
	int len = (int) (strlen(sz) + VARHDRSZ);
	text *ptResult = (text *) palloc(len + VARHDRSZ);
	SET_VARSIZE(ptResult, len);
	memcpy(VARDATA(ptResult), sz, len);
	return ptResult;
}

// EOF
