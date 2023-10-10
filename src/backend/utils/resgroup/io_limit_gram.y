%define api.pure true
%define api.prefix {io_limit_yy}
%error-verbose

%code top {
	#include "postgres.h"
	#include <limits.h>
	#include "commands/tablespace.h"
	#include "catalog/pg_tablespace.h"
	#include "catalog/pg_tablespace_d.h"

	#define YYMALLOC palloc
	#define YYFREE	 pfree

	extern int io_limit_yycolumn;
	static char *line;
}

%code requires {
	#include "utils/cgroup_io_limit.h"
}

%union {
	char *str;
	uint64 integer;
	IOconfig *ioconfig;
	TblSpcIOLimit *tblspciolimit;
	List *list;
	IOconfigItem *ioconfigitem;
}


%parse-param { IOLimitParserContext *context }

%param { void *scanner }

%code {
	int io_limit_yylex(void *lval, void *scanner);
	void io_limit_yyerror(IOLimitParserContext *parser_context, void *scanner, const char *message);
}

%token STAR VALUE_MAX
%token <str> ID IO_KEY
%token <integer> NUMBER

%type <str> tablespace_name
%type <integer> io_value
%type <ioconfig> ioconfigs
%type <tblspciolimit> tablespace_io_config
%type <list> iolimit_config_string
%type <ioconfigitem> ioconfig

%destructor { pfree($$); } <str> <ioconfig> <ioconfigitem>
%destructor {
	pfree($$->ioconfig);
	list_free_deep($$->bdi_list);
	pfree($$);
} <tblspciolimit>

%%

iolimit_config_string: tablespace_io_config
					   {
							List *l = NIL;

							$$ = lappend(l, $1);

							context->result = $$;
					   }
					 | iolimit_config_string ';' tablespace_io_config
					   {
							$$ = lappend($1, $3);

							context->result = $$;
					   }
;

tablespace_name: ID  { $$ = $1; } ;

tablespace_io_config: tablespace_name ':' ioconfigs
					  {

							TblSpcIOLimit *tblspciolimit = (TblSpcIOLimit *)palloc0(sizeof(TblSpcIOLimit));

							if (context->star_tablespace_cnt > 0)
								yyerror(NULL, NULL, "tablespace '*' cannot be used with other tablespaces");
							tblspciolimit->tablespace_oid = get_tablespace_oid($1, false);
							context->normal_tablespce_cnt++;

							tblspciolimit->ioconfig = $3;

							$$ = tblspciolimit;
					  }
					| STAR ':' ioconfigs
					  {
							TblSpcIOLimit *tblspciolimit = (TblSpcIOLimit *)palloc0(sizeof(TblSpcIOLimit));

							if (context->normal_tablespce_cnt > 0 || context->star_tablespace_cnt > 0)
								yyerror(NULL, NULL, "tablespace '*' cannot be used with other tablespaces");

							tblspciolimit->tablespace_oid = InvalidOid;
							context->star_tablespace_cnt++;

							tblspciolimit->ioconfig = $3;

							$$ = tblspciolimit;
					  }
					| NUMBER ':' ioconfigs
					  {
							TblSpcIOLimit *tblspciolimit = (TblSpcIOLimit *)palloc0(sizeof(TblSpcIOLimit));

							if (context->star_tablespace_cnt > 0)
								yyerror(NULL, NULL, "tablespace '*' cannot be used with other tablespaces");
							if (!OidIsValid($1) || $1 > OID_MAX)
								yyerror(NULL, NULL, "tablespace oid exceeds the limit");

							tblspciolimit->tablespace_oid = $1;
							context->normal_tablespce_cnt++;

							tblspciolimit->ioconfig = $3;

							$$ = tblspciolimit;
					  }
;

ioconfigs: ioconfig
		   {
				IOconfig *config = (IOconfig *)palloc0(sizeof(IOconfig));
				uint64 *config_var = (uint64 *)config;
				if (config == NULL)
					yyerror(NULL, NULL, "cannot allocate memory");

				*(config_var + $1->offset) = $1->value;
				$$ = config;
		   }
		 | ioconfigs ',' ioconfig
		   {
				uint64 *config_var = (uint64 *)$1;

				if (*(config_var + $3->offset) != IO_LIMIT_EMPTY)
					yyerror(NULL, NULL, psprintf("duplicated IO_KEY: %s", IOconfigFields[$3->offset]));

				*(config_var + $3->offset) = $3->value;
				$$ = $1;
		   }
;

ioconfig: IO_KEY '=' io_value
		  {
			uint64 max;
			IOconfigItem *item = (IOconfigItem *)palloc(sizeof(IOconfigItem));
			item->value = IO_LIMIT_MAX;

			if (item == NULL)
				yyerror(NULL, NULL, "cannot allocate memory");

			item->value = $3;
			for (int i = 0; i < lengthof(IOconfigFields); ++i)
				if (strcmp($1, IOconfigFields[i]) == 0)
				{
					item->offset = i;
					if (!io_limit_value_validate(IOconfigFields[i], item->value, &max))
						yyerror(NULL, NULL,
								psprintf("value of '%s' must in range [2, %lu] or equal 'max'", IOconfigFields[i], max));
				}

			$$ = item;
		  }
;

io_value: NUMBER
		{
			$$ = $1;
		}
		| VALUE_MAX { $$ = IO_LIMIT_MAX; }
;

%%

void
io_limit_yyerror(IOLimitParserContext *parser_context, void *scanner, const char *message)
{
	ereport(ERROR,
		(errcode(ERRCODE_SYNTAX_ERROR),
		errmsg("io limit: %s", message),
		errhint(" %s\n        %*c", line, io_limit_yycolumn, '^')));
}

/*
 * Parse io limit string to list of TblSpcIOLimit.
 */
List *
io_limit_parse(const char *limit_str)
{
	List *result = NIL;
	IOLimitParserContext context;
	IOLimitScannerState state;

	line = (char *) limit_str;

	context.result = NIL;
	context.normal_tablespce_cnt = 0;
	context.star_tablespace_cnt = 0;
	io_limit_scanner_begin(&state, limit_str);
	if (yyparse(&context, state.scanner) != 0)
		yyerror(&context, state.scanner, "parse error");

	io_limit_scanner_finish(&state);

	result = context.result;

	return result;
}
