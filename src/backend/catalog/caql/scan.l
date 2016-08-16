%{
/*-------------------------------------------------------------------------
 *
 * scan.l
 *	  lexical yyscanner for CaQL
 *
 *
 * The rules are designed so that the yyscanner never has to backtrack,
 * in the sense that there is always a rule that can match the input
 * consumed so far (the rule action may internally throw back some input
 * with yyless(), however).  As explained in the flex manual, this makes
 * for a useful speed increase --- about a third faster than a plain -CF
 * lexer, in simple testing.  The extra complexity is mostly in the rules
 * for handling float numbers and continued string literals.  If you change
 * the lexical rules, verify that you haven't broken the no-backtrack
 * property by running flex with the "-b" option and checking that the
 * resulting "lex.backup" file says that no backing up is needed.
 *
 * We employ reentrant lexer and parser.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <unistd.h>

#include "catalog/caqlparse.h"
#include "parser/keywords.h"
#include "parser/scansup.h"

#include "gram.h"

/*
 * user data for caql lexer
 *
 * literalbuf is used to accumulate literal values when multiple rules
 * are needed to parse a single literal.  Call startlit to reset buffer
 * to empty, addlit to add text.  Note that the buffer is palloc'd and
 * starts life afresh on every parse cycle.
 */
typedef struct caql_yyextra_t
{
	char	   *literalbuf;
	int			literallen;
	int			literalalloc;

	const char *file;
	int			line;
	char	   *scanbuf;
} caql_yyextra_t;

/*
 * Declare here instead of separate header file, as the set is small enough.
 * All CaQL keywords are reserved ones.
 */
#define CAQL_KEYWORD(a, b) {a, b, RESERVED_KEYWORD},
static const ScanKeyword CaQLScanKeywords[] = {
	CAQL_KEYWORD("and", AND)
	CAQL_KEYWORD("by", BY)
	CAQL_KEYWORD("for", FOR)
	CAQL_KEYWORD("from", FROM)
	CAQL_KEYWORD("into", INTO)
	CAQL_KEYWORD("is", IS)
	CAQL_KEYWORD("order", ORDER)
	CAQL_KEYWORD("select", SELECT)
	CAQL_KEYWORD("update", UPDATE)
	CAQL_KEYWORD("where", WHERE)
};
static const ScanKeyword *CaQLLastScanKeyword = endof(CaQLScanKeywords);

/* Avoid exit() on fatal yyscanner errors (a bit ugly -- see yy_fatal_error) */
#undef fprintf
#define fprintf(file, fmt, msg)  ereport(ERROR, (errmsg_internal("%s", msg)))

static int		xcdepth = 0;	/* depth of nesting in slash-star comments */


static void addlit(char *ytext, int yleng, caql_scanner_t yyscanner);
static void addlitchar(unsigned char ychar, caql_scanner_t yyscanner);
static char *litbufdup(caql_scanner_t yyscanner);

/*
 * Each call to yylex must set yylloc to the location of the found token
 * (expressed as a byte offset from the start of the input text).
 * When we parse a token that requires multiple lexer rules to process,
 * this should be done in the first such rule, else yylloc will point
 * into the middle of the token.
 */
#define SET_YYLLOC()  (*(yylloc) = yytext - yyextra->scanbuf)

/*
 * Work around a bug in flex 2.5.35: it emits a couple of functions that
 * it forgets to emit declarations for.  Since we use -Wmissing-prototypes,
 * this would cause warnings.  Providing our own declarations should be
 * harmless even when the bug gets fixed.
 */
extern int	caql_yyget_column(yyscan_t yyscanner);
extern void caql_yyset_column(int column_no, yyscan_t yyscanner);
%}

%option reentrant
%option 8bit
%option never-interactive
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option noyyalloc
%option noyyrealloc
%option noyyfree
%option prefix="caql_yy"
%option extra-type="caql_yyextra_t *"
%option bison-bridge
%option bison-locations

/*
 * OK, here is a short description of lex/flex rules behavior.
 * The longest pattern which matches an input string is always chosen.
 * For equal-length patterns, the first occurring in the rules list is chosen.
 * INITIAL is the starting state, to which all non-conditional rules apply.
 * Exclusive states change parsing rules while the state is active.  When in
 * an exclusive state, only those rules defined for that state apply.
 *
 * We use exclusive states for quoted strings, extended comments,
 * and to eliminate parsing troubles for numeric strings.
 * Exclusive states:
 *  <xc> extended C-style comments
 *  <xq> standard quoted strings
 */

%x xc
%x xq

/*
 * In order to make the world safe for Windows and Mac clients as well as
 * Unix ones, we accept either \n or \r as a newline.  A DOS-style \r\n
 * sequence will be seen as two successive newlines, but that doesn't cause
 * any problems.  Comments that start with -- and extend to the next
 * newline are treated as equivalent to a single whitespace character.
 *
 * NOTE a fine point: if there is no newline following --, we will absorb
 * everything to the end of the input as a comment.  This is correct.  Older
 * versions of Postgres failed to recognize -- as a comment if the input
 * did not end with a newline.
 *
 * XXX perhaps \f (formfeed) should be treated as a newline as well?
 */

space			[ \t\n\r\f]
horiz_space		[ \t\f]
newline			[\n\r]
non_newline		[^\n\r]

comment			("--"{non_newline}*)

whitespace		({space}+|{comment})

/*
 * CaQL, as SQL does, requires at least one newline in the whitespace
 * separating string literals that are to be concatenated.  Note that
 * {whitespace_with_newline} should not have * after it, whereas {whitespace}
 * should generally have a * after it...
 */

special_whitespace		({space}+|{comment}{newline})
horiz_whitespace		({horiz_space}|{comment})
whitespace_with_newline	({horiz_whitespace}*{newline}{special_whitespace}*)

/*
 * To ensure that {quotecontinue} can be scanned without having to back up
 * if the full pattern isn't matched, we include trailing whitespace in
 * {quotestop}.  This matches all cases where {quotecontinue} fails to match,
 * except for {quote} followed by whitespace and just one "-" (not two,
 * which would start a {comment}).  To cover that we have {quotefail}.
 * The actions for {quotestop} and {quotefail} must throw back characters
 * beyond the quote proper.
 */
quote			'
quotestop		{quote}{whitespace}*
quotecontinue	{quote}{whitespace_with_newline}{quote}
quotefail		{quote}{whitespace}*"-"

/* Extended quote
 * xqdouble implements embedded quote, ''''
 */
xqstart			{quote}
xqdouble		{quote}{quote}
xqinside		[^']+

/*
 * C-style comments
 *
 * The "extended comment" syntax closely resembles allowable operator syntax.
 * The tricky part here is to get lex to recognize a string starting with
 * slash-star as a comment, when interpreting it as an operator would produce
 * a longer match --- remember lex will prefer a longer match!
 */
xcstart			\/\*
xcstop			\*+\/
xcinside		[^*/]+

digit			[0-9]
ident_start		[A-Za-z\200-\377_]
ident_cont		[A-Za-z\200-\377_0-9\$]

identifier		{ident_start}{ident_cont}*

equal			=
lt				<
le				<=
ge				>=
gt				>

/*
 * "self" is the set of chars that should be returned as single-character
 * tokens.  "op_chars" is the set of chars that can make up "Op" tokens,
 * which can be one or more characters long (but if a single-char token
 * appears in the "self" set, it is not to be returned as an Op).  Note
 * that the sets overlap, but each has some chars that are not in the other.
 *
 * If you change either set, adjust the character lists appearing in the
 * rule for "operator"!
 */
self			[,()\[\].;\:\+\-\*\/\%\^\<\>\=]

/*
 * We do not allow unary minus in numbers. Instead we pass it separately
 * to parser.
 *
 * {realfail1} and {realfail2} are added to prevent the need for yyscanner
 * backup when the {real} rule fails to match completely.
 */

integer			{digit}+
decimal			(({digit}*\.{digit}+)|({digit}+\.{digit}*))
real			({integer}|{decimal})[Ee][-+]?{digit}+
realfail1		({integer}|{decimal})[Ee]
realfail2		({integer}|{decimal})[Ee][-+]

param			\:{integer}

other			.

%%

{whitespace}	{
					/* ignore */
				}

{xcstart}		{
					/* Set location in case of syntax error in comment */
					SET_YYLLOC();
					xcdepth = 0;
					BEGIN(xc);
					/* Put back any characters past slash-star; see above */
					yyless(2);
				}

<xc>{xcstart}	{
					xcdepth++;
					/* Put back any characters past slash-star; see above */
					yyless(2);
				}

<xc>{xcstop}	{
					if (xcdepth <= 0)
						BEGIN(INITIAL);
					else
						xcdepth--;
				}

<xc>{xcinside}	{
					/* ignore */
				}

<xc>\*+			{
					/* ignore */
				}

<xc>\/+			{
					/* ignore */
				}

<xc><<EOF>>		{ caql_scanner_yyerror("unterminated /* comment", yyscanner); }

<xq>{quotestop}	|
<xq>{quotefail} {
					yyless(1);
					BEGIN(INITIAL);
					yylval->str = litbufdup(yyscanner);
					return SCONST;
				}
<xq>{xqdouble} {
					addlitchar('\'', yyscanner);
				}
<xq>{xqinside}  {
					addlit(yytext, yyleng, yyscanner);
				}
<xq>{quotecontinue} {
					/* ignore */
				}
<xq><<EOF>>		{ caql_scanner_yyerror("unterminated quoted string", yyscanner); }

{equal}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return OP_EQUAL;
				}

{lt}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return OP_LT;
				}

{le}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return OP_LE;
				}

{ge}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return OP_GE;
				}

{gt}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return OP_GT;
				}

{self}			{
					SET_YYLLOC();
					return yytext[0];
				}

{param}			{
					SET_YYLLOC();
					yylval->ival = atol(yytext + 1);
					return PARAM;
				}

{integer}		{
					long val;
					char* endptr;

					SET_YYLLOC();
					errno = 0;
					val = strtol(yytext, &endptr, 10);
					if (*endptr != '\0' || errno == ERANGE
#ifdef HAVE_LONG_INT_64
						/* if long > 32 bits, check for overflow of int4 */
						|| val != (long) ((int32) val)
#endif
						)
					{
						/* integer too large, treat it as a float */
						yylval->str = pstrdup(yytext);
						return FCONST;
					}
					yylval->ival = val;
					return ICONST;
				}
{decimal}		{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return FCONST;
				}
{real}			{
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return FCONST;
				}
{realfail1}		{
					/*
					 * throw back the [Ee], and treat as {decimal}.  Note
					 * that it is possible the input is actually {integer},
					 * but since this case will almost certainly lead to a
					 * syntax error anyway, we don't bother to distinguish.
					 */
					yyless(yyleng-1);
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return FCONST;
				}
{realfail2}		{
					/* throw back the [Ee][+-], and proceed as above */
					yyless(yyleng-2);
					SET_YYLLOC();
					yylval->str = pstrdup(yytext);
					return FCONST;
				}


{identifier}	{
					const ScanKeyword *keyword;
					char		   *ident;

					SET_YYLLOC();

					/* Is it a keyword? */
					keyword = ScanKeywordLookupExt(yytext, CaQLScanKeywords,
												   CaQLLastScanKeyword);
					if (keyword != NULL)
					{
						yylval->keyword = keyword->name;
						return keyword->value;
					}

					/*
					 * No.  Convert the identifier to lower case, and truncate
					 * if necessary.
					 */
					ident = downcase_truncate_identifier(yytext, yyleng, true);
					yylval->str = ident;
					return IDENT;
				}

{other}			{
					SET_YYLLOC();
					return yytext[0];
				}

<<EOF>>			{
					SET_YYLLOC();
					yyterminate();
				}

%%

/*
 * caql_scanner_yyerror
 *		Report a lexer or grammar error.
 *
 * The message's cursor position identifies the most recently lexed token.
 * This is OK for syntax error messages from the Bison parser, because Bison
 * parsers report error as soon as the first unparsable token is reached.
 * Beware of using yyerror for other purposes, as the cursor position might
 * be misleading!
 */
static void
caql_scanner_yyerror(const char *message, caql_scanner_t yyscanner)
{
	caql_yyextra_t *extra = yyget_extra(yyscanner);
	const char *loc = extra->scanbuf + *yyget_lloc(yyscanner);

	if (*loc == YY_END_OF_BUFFER_CHAR)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 /* translator: %s is typically the translation of "syntax error" */
				 errmsg("%s at end of input", _(message)),
				 errhint("%s (%s:%d)",
					 extra->scanbuf, extra->file, extra->line),
				 *yyget_lloc(yyscanner)));
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 /* translator: first %s is typically the translation of "syntax error" */
				 errmsg("%s at or near \"%s\"", _(message), loc),
				 errhint("%s (%s:%d)",
					 extra->scanbuf, extra->file, extra->line),
				 *yyget_lloc(yyscanner)));
	}
}


/*
 * Called before any actual scanning is done
 */
static caql_scanner_t
caql_scanner_init(const char *str, const char *file, int line)
{
	Size		slen = strlen(str);
	yyscan_t	scanner;
	caql_yyextra_t *extra;

	if (caql_yylex_init(&scanner) != 0)
		elog(ERROR, "caql_yylex_init() failed: %m");

	extra = (caql_yyextra_t *) palloc(sizeof(caql_yyextra_t));
	extra->file = file;
	extra->line = line;
	caql_yyset_extra(extra, scanner);

	/*
	 * Make a scan buffer with special termination needed by flex.
	 */
	extra->scanbuf = (char *) palloc(slen + 2);
	memcpy(extra->scanbuf, str, slen);
	extra->scanbuf[slen] = extra->scanbuf[slen + 1] = YY_END_OF_BUFFER_CHAR;
	caql_yy_scan_buffer(extra->scanbuf, slen + 2, scanner);

	/* initialize literal buffer to a reasonable but expansible size */
	extra->literalalloc = 1024;
	extra->literalbuf = (char *) palloc(extra->literalalloc);
	extra->literallen = 0;

	return scanner;
}


/*
 * Called after scanning is done to clean up after caql_scanner_init()
 */
static void
caql_scanner_finish(caql_scanner_t yyscanner)
{
	pfree(yyget_extra(yyscanner)->scanbuf);
	pfree(yyget_extra(yyscanner)->literalbuf);
	pfree(yyget_extra(yyscanner));

	caql_yyfree(yyscanner, yyscanner);
}

static void
addlit(char *ytext, int yleng, caql_scanner_t yyscanner)
{
	caql_yyextra_t *extra = yyget_extra(yyscanner);

	/* enlarge buffer if needed */
	if ((extra->literallen + yleng) >= extra->literalalloc)
	{
		do
		{
			extra->literalalloc *= 2;
		} while ((extra->literallen + yleng) >= extra->literalalloc);
		extra->literalbuf = (char *) repalloc(extra->literalbuf, extra->literalalloc);
	}
	/* append new data, add trailing null */
	memcpy(extra->literalbuf + extra->literallen, ytext, yleng);
	extra->literallen += yleng;
	extra->literalbuf[extra->literallen] = '\0';
}


static void
addlitchar(unsigned char ychar, caql_scanner_t yyscanner)
{
	caql_yyextra_t *extra = yyget_extra(yyscanner);

	/* enlarge buffer if needed */
	if ((extra->literallen + 1) >= extra->literalalloc)
	{
		extra->literalalloc *= 2;
		extra->literalbuf = (char *) repalloc(extra->literalbuf, extra->literalalloc);
	}
	/* append new data, add trailing null */
	extra->literalbuf[extra->literallen] = ychar;
	extra->literallen += 1;
	extra->literalbuf[extra->literallen] = '\0';
}


/*
 * One might be tempted to write pstrdup(literalbuf) instead of this,
 * but for long literals this is much faster because the length is
 * already known.
 */
static char *
litbufdup(caql_scanner_t yyscanner)
{
	char *new;
	caql_yyextra_t *extra = yyget_extra(yyscanner);

	new = palloc(extra->literallen + 1);
	memcpy(new, extra->literalbuf, extra->literallen + 1);
	return new;
}

/*
 * Interface functions to make flex use palloc() instead of malloc().
 * It'd be better to make these static, but flex insists otherwise.
 */

void *
caql_yyalloc(yy_size_t bytes, yyscan_t yyscanner)
{
	return palloc(bytes);
}

void *
caql_yyrealloc(void *ptr, yy_size_t bytes, yyscan_t yyscanner)
{
	if (ptr)
		return repalloc(ptr, bytes);
	else
		return palloc(bytes);
}

void
caql_yyfree(void *ptr, yyscan_t yyscanner)
{
	if (ptr)
		pfree(ptr);
}
