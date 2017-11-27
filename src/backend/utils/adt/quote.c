/*-------------------------------------------------------------------------
 *
 * quote.c
 *	  Functions for quoting identifiers and literals
 *
 * Portions Copyright (c) 2000-2009, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/adt/quote.c,v 1.26 2009/01/01 17:23:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/builtins.h"


/*
 * quote_ident -
 *	  returns a properly quoted identifier
 */
Datum
quote_ident(PG_FUNCTION_ARGS)
{
	text	   *t = PG_GETARG_TEXT_PP(0);
	const char *qstr;
	char	   *str;

	str = text_to_cstring(t);
	qstr = quote_identifier(str);
	PG_RETURN_TEXT_P(cstring_to_text(qstr));
}

/*
 * quote_literal -
 *	  returns a properly quoted literal
 *
 * NOTE: think not to make this function's behavior change with
 * standard_conforming_strings.  We don't know where the result
 * literal will be used, and so we must generate a result that
 * will work with either setting.  Take a look at what dblink
 * uses this for before thinking you know better.
 */
Datum
quote_literal(PG_FUNCTION_ARGS)
{
	text	   *t = PG_GETARG_TEXT_P(0);
	text	   *result;
	const char *qstr;
	char	   *str;
	int			len;

	/* We have to convert to a C string to use quote_literal_internal */
	len = VARSIZE(t) - VARHDRSZ;
	str = (char *) palloc(len + 1);
	memcpy(str, VARDATA(t), len);
	str[len] = '\0';

	qstr = quote_literal_internal(str);

	len = strlen(qstr);
	result = (text *) palloc(len + VARHDRSZ);
	SET_VARSIZE(result, len + VARHDRSZ);
	memcpy(VARDATA(result), qstr, len);

	PG_RETURN_TEXT_P(result);
}

/*
 * quote_nullable -
 *    Returns a properly quoted literal, with null values returned
 *    as the text string 'NULL'.
 */
Datum
quote_nullable(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0))
		PG_RETURN_TEXT_P(cstring_to_text("NULL"));
	else
		PG_RETURN_DATUM(DirectFunctionCall1(quote_literal,
											PG_GETARG_DATUM(0)));
}
