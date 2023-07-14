%{
#include "postgres.h"
#include "io_limit_gram.h"

#define YYSTYPE IO_LIMIT_YYSTYPE
%}

%option noinput nounput noyywrap
%option noyyalloc noyyfree noyyrealloc
%option prefix="io_limit_yy"
%option reentrant bison-bridge
%option warn never-interactive nodefault

id  [a-zA-Z_][a-zA-Z0-9_]*
wildcard \*

%%
[wr](b|io)ps {
                yylval->str = pstrdup(yytext);
                return IO_KEY;
             }

max          {
                yylval->str = pstrdup("max");
                return VALUE_MAX;
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

{wildcard}   {
                return STAR;
             }

[[:space:]]  ;

.           {
                return *yytext;
            }

%%


void *
io_limit_yyalloc(yy_size_t  size , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;
	return palloc(size);
}

void *
io_limit_yyrealloc(void * ptr, yy_size_t  size , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;

        if (ptr)
	    return repalloc(ptr, size);
        else
            return palloc(size);
}

void
io_limit_yyfree(void * ptr , yyscan_t yyscanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*)yyscanner;
	(void)yyg;

        if (ptr != NULL)
	    pfree( ptr );
}


void
io_limit_scanner_begin(IOLimitScannerState *state, const char *limit_str)
{
    yyscan_t scanner;
    yylex_init(&scanner);

    state->state = (void *)yy_scan_string(limit_str, scanner);
    state->scanner = scanner;
}


void
io_limit_scanner_finish(IOLimitScannerState *state)
{
    yy_delete_buffer((YY_BUFFER_STATE)state->state, (yyscan_t)state->scanner);
}
