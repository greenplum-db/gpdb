%option noinput nounput noyywrap
%option noyyalloc noyyfree noyyrealloc
%option prefix="io_limit_yy"
%option reentrant bison-bridge

%{
#include "postgres.h"
#include "io_limit_gram.h"

#define YYSTYPE IO_LIMIT_YYSTYPE
%}

id  [a-zA-Z_][a-zA-Z0-9_]*

%%
[wr](b|io)ps {
                    yylval->str = pstrdup(yytext);
                    return IO_KEY;
             }

{id}         {
                yylval->str = pstrdup(yytext);
                return ID;
             }

\;           { return IOLIMIT_CONFIG_DELIM; }

:            { return TABLESPACE_IO_CONFIG_START; };

,            { return IOCONFIG_DELIM; }

[[:digit:]]+ {
                yylval->integer = strtoull(yytext, NULL, 10);
                return VALUE;
             }

max          {
                yylval->str = pstrdup("max");
                return VALUE_MAX;
             }

[[:space:]]  ;

.           {
                return *yytext;
            }

%%


void *yyalloc (yy_size_t  size , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;
	return palloc(size);
}

void *yyrealloc  (void * ptr, yy_size_t  size , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;

	/* The cast to (char *) in the following accommodates both
	 * implementations that use char* generic pointers, and those
	 * that use void* generic pointers.  It works with the latter
	 * because both ANSI C and C++ allow castless assignment from
	 * any pointer type to void*, and deal with argument conversions
	 * as though doing an assignment.
	 */
	return repalloc(ptr, size);
}

void yyfree (void * ptr , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;
	pfree( (char *) ptr );	/* see yyrealloc() for (char *) cast */
}


IO_LIMIT_PARSER_STATE *io_limit_begin_scan(const char *limit_str)
{
    IO_LIMIT_PARSER_STATE *state = (IO_LIMIT_PARSER_STATE *)palloc0(sizeof(IO_LIMIT_PARSER_STATE));
    yyscan_t scanner;
    io_limit_yylex_init(&scanner);

    state->state = (void *)io_limit_yy_scan_string(limit_str, scanner);
    state->scanner = scanner;

    return state;
}


void io_limit_end_scan(IO_LIMIT_PARSER_STATE *state)
{
    io_limit_yy_delete_buffer((YY_BUFFER_STATE)state->state, (yyscan_t)state->scanner);
    pfree(state);
}
