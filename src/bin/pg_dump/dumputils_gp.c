/*-------------------------------------------------------------------------
 *
 * GPDB-specific utility routines for SQL dumping
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include "dumputils.h"

/*
 * Escape any backslashes in given string (from initdb.c)
 */
char *
escape_backslashes(const char *src, bool quotes_too)
{
	int			len = strlen(src),
				i,
				j;
	char	   *result = malloc(len * 2 + 1);

	for (i = 0, j = 0; i < len; i++)
	{
		if ((src[i]) == '\\' || ((src[i]) == '\'' && quotes_too))
			result[j++] = '\\';
		result[j++] = src[i];
	}
	result[j] = '\0';
	return result;
}

/*
 * Escape backslashes and apostrophes in EXTERNAL TABLE format strings.
 *
 * The fmtopts field of a pg_exttable tuple has an odd encoding -- it is
 * partially parsed and contains "string" values that aren't legal SQL.
 * Each string value is delimited by apostrophes and is usually, but not
 * always, a single character.	The fmtopts field is typically something
 * like {delimiter '\x09' null '\N' escape '\'} or
 * {delimiter ',' null '' escape '\' quote '''}.  Each backslash and
 * apostrophe in a string must be escaped and each string must be
 * prepended with an 'E' denoting an "escape syntax" string.
 *
 * Usage note: A field value containing an apostrophe followed by a space
 * will throw this algorithm off -- it presumes no embedded spaces.
 */
char *
escape_fmtopts_string(const char *src)
{
	int			len = strlen(src);
	int			i;
	int			j;
	char	   *result = pg_malloc(len * 2 + 1);
	bool		inString = false;

	for (i = 0, j = 0; i < len; i++)
	{
		switch (src[i])
		{
			case '\'':
				if (inString)
				{
					/*
					 * Escape apostrophes *within* the string.	If the
					 * apostrophe is at the end of the source string or is
					 * followed by a space, it is presumed to be a closing
					 * apostrophe and is not escaped.
					 */
					if ((i + 1) == len || src[i + 1] == ' ')
						inString = false;
					else
						result[j++] = '\\';
				}
				else
				{
					result[j++] = 'E';
					inString = true;
				}
				break;
			case '\\':
				result[j++] = '\\';
				break;
		}

		result[j++] = src[i];
	}

	result[j] = '\0';
	return result;
}

/*
 * Tokenize a fmtopts string (for use with 'custom' formatters)
 * i.e. convert it to: a = b, format.
 * (e.g.:  formatter E'fixedwidth_in null E' ' preserve_blanks E'on')
 */
char *
custom_fmtopts_string(const char *src)
{
	PQExpBufferData result;
	char *srcdup;
	char *to_free;
	char *find_res;
	char *srcdup_end;
	int last;

	if (!src)
		return NULL;

	to_free = srcdup = pg_strdup(src);
	srcdup_end = srcdup + strlen(srcdup);
	initPQExpBuffer(&result);

	while (srcdup)
	{
		/* find first word (a) */
		find_res = strchr(srcdup, ' ');
		if (!find_res)
			break;
		*find_res = '\0';
		appendPQExpBufferStr(&result, srcdup);
		/* skip space */
		srcdup = find_res + 1;
		/* remove E if E' */
		if ((strlen(srcdup) > 2) && (srcdup[0] == 'E') && (srcdup[1] == '\''))
			srcdup++;
		/* add " = " */
		appendPQExpBuffer(&result, " = ");
		/* find second word (b) until second '
		   find \' combinations and ignore them */
		find_res = strchr(srcdup + 1, '\'');
		while (find_res && (*(find_res - 1) == '\\') /* ignore \' */)
		{
			find_res = strchr(find_res + 1, '\'');
		}
		if (!find_res)
			break;
		find_res++;
		*find_res = '\0';
		appendPQExpBufferStr(&result, srcdup);
		srcdup = find_res;
		/* move to the next token if exists and add ',' */
		if (find_res < srcdup_end - 1)
		{
			srcdup = find_res + 1;
			appendPQExpBuffer(&result, ",");
		}
	}

	/* fix string - remove trailing ',' or '=' */
	last = strlen(result.data) - 1;
	if (last >= 0 && (result.data[last] == ',' || result.data[last] == '='))
		result.data[last] = '\0';

	pg_free(to_free);
	return result.data;
}
