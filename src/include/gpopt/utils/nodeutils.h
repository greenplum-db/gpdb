//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		nodeutils.h
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef NodeUtils_H
#define NodeUtils_H

#include "c.h"

extern struct List *extract_nodes(struct Node *ps, int nodeTag);

// String related functionality.

extern char *textToString(text *pt);
extern text *stringToText(const char *sz);
extern struct Const *stringToConst(char *sz);
extern char *constToString(Const *pconst);

#endif // NodeUtils_H

// EOF
