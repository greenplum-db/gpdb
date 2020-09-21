/*
 *	string.h
 *		string handling helpers
 *
 *	Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 *	Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/include/common/string.h
 */
#ifndef COMMON_STRING_H
#define COMMON_STRING_H

struct StringInfoData;			/* avoid including stringinfo.h here */

extern bool pg_str_endswith(const char *str, const char *end);

/* functions in src/common/pg_get_line.c */
extern bool pg_get_line_append(FILE *stream, struct StringInfoData *buf);

#endif   /* COMMON_STRING_H */
