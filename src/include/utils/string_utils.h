/*-------------------------------------------------------------------------
 *
 * string_utils.h
 *
 * Copyright (c) 2000-2016, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/string_utils.h
 *	    (copied from src/bin/psql/stringutils.h)
 *
 *-------------------------------------------------------------------------
 */

#ifndef GP_STRINGUTILS_H
#define GP_STRINGUTILS_H

extern char *
strtokx2(const char *s,
		 const char *whitespace,
		 const char *delim,
		 const char *quote,
		 char escape,
		 bool e_strings,
		 bool del_quotes,
		 int encoding);

extern void strip_quotes(char *source, char quote, char escape, int encoding);

#endif   /* GP_STRINGUTILS_H */
