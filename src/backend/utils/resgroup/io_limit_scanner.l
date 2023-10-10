%{
#include "postgres.h"
#include "io_limit_gram.h"

#define YYSTYPE IO_LIMIT_YYSTYPE

#define SAVE_COLUMN io_limit_yycolumn = next_col; next_col += strlen(yytext)

static int next_col = 1;
int io_limit_yycolumn = 1;
%}

%option noinput nounput noyywrap
%option noyyalloc noyyfree noyyrealloc
%option prefix="io_limit_yy"
%option reentrant bison-bridge
%option warn never-interactive nodefault

id  [a-zA-Z_][a-zA-Z0-9_]*
wildcard \*

%s ts_param

%%
:           {
                SAVE_COLUMN;
                BEGIN ts_param;
                return ':';
            }

<ts_param>[wr](b|io)ps {
                          SAVE_COLUMN;
                          yylval->str = pstrdup(yytext);
                          return IO_KEY;
                       }

<ts_param>max {
                SAVE_COLUMN;
                yylval->str = pstrdup("max");
                return VALUE_MAX;
              }

<ts_param>;  {
                SAVE_COLUMN;
                BEGIN INITIAL;
                return ';';
             }

{id}         {
                SAVE_COLUMN;
                yylval->str = pstrdup(yytext);
                return ID;
             }

[[:digit:]]+ {
                SAVE_COLUMN;
                yylval->integer = strtoull(yytext, NULL, 10);
                return NUMBER;
             }

{wildcard}   {
                SAVE_COLUMN;
                return STAR;
             }

[[:space:]]  {  SAVE_COLUMN; }

.           {
                SAVE_COLUMN;
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

    next_col = 1;
    io_limit_yycolumn = 1;

    state->buffer = (void *)yy_scan_string(limit_str, scanner);
    state->scanner = scanner;
}


void
io_limit_scanner_finish(IOLimitScannerState *state)
{
    yy_delete_buffer(state->buffer, state->scanner);
}
