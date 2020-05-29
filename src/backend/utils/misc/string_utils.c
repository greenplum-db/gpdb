/*
 * string_utils.c
 *
 * Copyright (c) 2000-2016, PostgreSQL Global Development Group
 *
 * (copied from src/bin/psql/stringutils.c)
 */
#include "postgres.h"
#include "mb/pg_wchar.h"
#include "utils/string_utils.h"

/*
 * strip_quotes
 *
 * (copied from bin/psql/stringutils.c - TODO: place to share FE and BE code?).
 *
 * Remove quotes from the string at *source.  Leading and trailing occurrences
 * of 'quote' are removed; embedded double occurrences of 'quote' are reduced
 * to single occurrences; if 'escape' is not 0 then 'escape' removes special
 * significance of next character.
 *
 * Note that the source string is overwritten in-place.
 */
extern void
strip_quotes(char *source, char quote, char escape, int encoding)
{
	char	   *src;
	char	   *dst;

	Assert(source);
	Assert(quote);

	src = dst = source;

	if (*src && *src == quote)
		src++;					/* skip leading quote */

	while (*src)
	{
		char		c = *src;
		int			i;

		if (c == quote && src[1] == '\0')
			break;				/* skip trailing quote */
		else if (c == quote && src[1] == quote)
			src++;				/* process doubled quote */
		else if (c == escape && src[1] != '\0')
			src++;				/* process escaped character */

		i = pg_encoding_mblen(encoding, src);
		while (i--)
			*dst++ = *src++;
	}

	*dst = '\0';
}

/*
 * strtokx2
 *
 * strtokx2 is a replica of psql's strtokx (bin/psql/stringutils.c), fitted
 * to be used in the backend for the same purpose - parsing an sql string of
 * literals. Information follows (right now identical to strtokx, except for
 * a small hack - see below comment about MPP-6698):
 *
 * Replacement for strtok() (a.k.a. poor man's flex)
 *
 * Splits a string into tokens, returning one token per call, then NULL
 * when no more tokens exist in the given string.
 *
 * The calling convention is similar to that of strtok, but with more
 * frammishes.
 *
 * s -			string to parse, if NULL continue parsing the last string
 * whitespace - set of whitespace characters that separate tokens
 * delim -		set of non-whitespace separator characters (or NULL)
 * quote -		set of characters that can quote a token (NULL if none)
 * escape -		character that can quote quotes (0 if none)
 * e_strings -	if TRUE, treat E'...' syntax as a valid token
 * del_quotes - if TRUE, strip quotes from the returned token, else return
 *				it exactly as found in the string
 * encoding -	the active character-set encoding
 *
 * Characters in 'delim', if any, will be returned as single-character
 * tokens unless part of a quoted token.
 *
 * Double occurrences of the quoting character are always taken to represent
 * a single quote character in the data.  If escape isn't 0, then escape
 * followed by anything (except \0) is a data character too.
 *
 * The combination of e_strings and del_quotes both TRUE is not currently
 * handled.  This could be fixed but it's not needed anywhere at the moment.
 *
 * Note that the string s is _not_ overwritten in this implementation.
 *
 * NB: it's okay to vary delim, quote, and escape from one call to the
 * next on a single source string, but changing whitespace is a bad idea
 * since you might lose data.
 */
extern char *
strtokx2(const char *s,
		 const char *whitespace,
		 const char *delim,
		 const char *quote,
		 char escape,
		 bool e_strings,
		 bool del_quotes,
		 int encoding)
{
	static char *storage = NULL;/* store the local copy of the users string
								 * here */
	static char *string = NULL; /* pointer into storage where to continue on
								 * next call */

	/* variously abused variables: */
	unsigned int offset;
	char	   *start;
	char	   *p;

	if (s)
	{
		/*
		 * We may need extra space to insert delimiter nulls for adjacent
		 * tokens.  2X the space is a gross overestimate, but it's unlikely
		 * that this code will be used on huge strings anyway.
		 */
		storage = palloc(2 * strlen(s) + 1);
		strcpy(storage, s);
		string = storage;
	}

	if (!storage)
		return NULL;

	/* skip leading whitespace */
	offset = strspn(string, whitespace);
	start = &string[offset];

	/* end of string reached? */
	if (*start == '\0')
	{
		/* technically we don't need to free here, but we're nice */
		pfree(storage);
		storage = NULL;
		string = NULL;
		return NULL;
	}

	/* test if delimiter character */
	if (delim && strchr(delim, *start))
	{
		/*
		 * If not at end of string, we need to insert a null to terminate the
		 * returned token.  We can just overwrite the next character if it
		 * happens to be in the whitespace set ... otherwise move over the
		 * rest of the string to make room.  (This is why we allocated extra
		 * space above).
		 */
		p = start + 1;
		if (*p != '\0')
		{
			if (!strchr(whitespace, *p))
				memmove(p + 1, p, strlen(p) + 1);
			*p = '\0';
			string = p + 1;
		}
		else
		{
			/* at end of string, so no extra work */
			string = p;
		}

		return start;
	}

	/* check for E string */
	p = start;
	if (e_strings &&
		(*p == 'E' || *p == 'e') &&
		p[1] == '\'')
	{
		quote = "'";
		escape = '\\';			/* if std strings before, not any more */
		p++;
	}

	/* test if quoting character */
	if (quote && strchr(quote, *p))
	{
		/* okay, we have a quoted token, now scan for the closer */
		char		thisquote = *p++;

		/*
		 * MPP-6698 START
		 *
		 * unfortunately, it is possible for an external table format string
		 * to be represented in the catalog in a way which is problematic to
		 * parse: when using a single quote as a QUOTE or ESCAPE character the
		 * format string will show [quote ''']. since we do not want to change
		 * how this is stored at this point (as it will affect previous
		 * versions of the software already in production) the following code
		 * block will detect this scenario where 3 quote characters follow
		 * each other, with no fourth one. in that case, we will skip the
		 * second one (the first is skipped just above) and the last trailing
		 * quote will be skipped below. the result will be the actual token
		 * (''') and after stripping it due to del_quotes we'll end up with
		 * ('). very ugly, but will do the job...
		 */
		char		qt = quote[0];

		if (strlen(p) >= 3 && p[0] == qt && p[1] == qt && p[2] != qt)
			p++;
		/* MPP-6698 END */

		for (; *p; p += pg_encoding_mblen(encoding, p))
		{
			if (*p == escape && p[1] != '\0')
				p++;			/* process escaped anything */
			else if (*p == thisquote && p[1] == thisquote)
				p++;			/* process doubled quote */
			else if (*p == thisquote)
			{
				p++;			/* skip trailing quote */
				break;
			}
		}

		/*
		 * If not at end of string, we need to insert a null to terminate the
		 * returned token.  See notes above.
		 */
		if (*p != '\0')
		{
			if (!strchr(whitespace, *p))
				memmove(p + 1, p, strlen(p) + 1);
			*p = '\0';
			string = p + 1;
		}
		else
		{
			/* at end of string, so no extra work */
			string = p;
		}

		/* Clean up the token if caller wants that */
		if (del_quotes)
			strip_quotes(start, thisquote, escape, encoding);

		return start;
	}

	/*
	 * Otherwise no quoting character.  Scan till next whitespace, delimiter
	 * or quote.  NB: at this point, *start is known not to be '\0',
	 * whitespace, delim, or quote, so we will consume at least one character.
	 */
	offset = strcspn(start, whitespace);

	if (delim)
	{
		unsigned int offset2 = strcspn(start, delim);

		if (offset > offset2)
			offset = offset2;
	}

	if (quote)
	{
		unsigned int offset2 = strcspn(start, quote);

		if (offset > offset2)
			offset = offset2;
	}

	p = start + offset;

	/*
	 * If not at end of string, we need to insert a null to terminate the
	 * returned token.  See notes above.
	 */
	if (*p != '\0')
	{
		if (!strchr(whitespace, *p))
			memmove(p + 1, p, strlen(p) + 1);
		*p = '\0';
		string = p + 1;
	}
	else
	{
		/* at end of string, so no extra work */
		string = p;
	}

	return start;
}
