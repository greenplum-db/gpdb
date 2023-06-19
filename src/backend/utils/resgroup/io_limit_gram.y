%define api.pure true
%define api.prefix {io_limit_yy}

%code top {
	#include "postgres.h"
	#include "commands/tablespace.h"
	#include "catalog/pg_tablespace.h"
	#include "catalog/pg_tablespace_d.h"

	#define YYMALLOC palloc
	#define YYFREE	 pfree
	#define YYERROR_VERBOSE 1
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


%parse-param { IO_LIMIT_PARSER_CONTEXT *context }

%param { void *scanner }

%code {
	int io_limit_yylex(void *lval, void *scanner);
	int io_limit_yyerror(IO_LIMIT_PARSER_CONTEXT *parser_context, void *scanner, const char *message);
}

%token <str> ID IOLIMIT_CONFIG_DELIM TABLESPACE_IO_CONFIG_START IOCONFIG_DELIM VALUE_MAX IO_KEY
%token <integer> VALUE

%type <str> tablespace_name
%type <integer> io_value
%type <ioconfig> ioconfigs
%type <tblspciolimit> tablespace_io_config
%type <list> iolimit_config_string start
%type <ioconfigitem> ioconfig

%destructor { pfree($$); } <str> <integer> <ioconfig> <ioconfigitem>
%destructor {
	pfree($$->ioconfig);
	list_free_deep($$->bdi_list);
	pfree($$);
} <tblspciolimit>

%%

start: iolimit_config_string
	   {
			context->result = $$ = $1;
			return 0;
	   }

iolimit_config_string: tablespace_io_config
					   {
							List *l = NIL;

							$$ = lappend(l, $1);
					   }
					 | iolimit_config_string IOLIMIT_CONFIG_DELIM tablespace_io_config
					   {
							$$ = lappend($1, $3);
					   }

tablespace_name: ID  { $$ = $1; } | '*' { $$ = "*"; }

tablespace_io_config: tablespace_name TABLESPACE_IO_CONFIG_START ioconfigs
					  {
							TblSpcIOLimit *tblspciolimit = (TblSpcIOLimit *)palloc0(sizeof(TblSpcIOLimit));

							if (strcmp($1, "*") == 0)
							{
								if (context->normal_tablespce_cnt > 0)
									ereport(ERROR,
											(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
											errmsg("io limit: tablespace '*' cannot be used with other tablespaces")));
								tblspciolimit->tablespace_oid = InvalidOid;
								context->star_tablespace_cnt++;
							}
							else
							{
								if (context->star_tablespace_cnt > 0)
									ereport(ERROR,
											(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
											errmsg("io limit: tablespace '*' cannot be used with other tablespaces")));
								tblspciolimit->tablespace_oid = get_tablespace_oid($1, false);
								context->normal_tablespce_cnt++;
							}

							tblspciolimit->ioconfig = $3;

							$$ = tblspciolimit;
					  }

ioconfigs: ioconfig
		   {
				IOconfig *config = (IOconfig *)palloc0(sizeof(IOconfig));
				uint64 *config_var = (uint64 *)config;
				if (config == NULL)
					io_limit_yyerror(NIL, NULL, "io limit: cannot allocate memory");

				*(config_var + $1->offset) = $1->value;
				$$ = config;
		   }
		 | ioconfigs IOCONFIG_DELIM ioconfig
		   {
				uint64 *config_var = (uint64 *)$1;
				*(config_var + $3->offset) = $3->value;
				$$ = $1;
		   }

ioconfig: IO_KEY '=' io_value
		  {
			IOconfigItem *item = (IOconfigItem *)palloc0(sizeof(IOconfigItem));
			if (item == NULL)
				io_limit_yyerror(NIL, NULL, "io limit: cannot allocate memory");

			item->value = $3;
			for (int i = 0;i < IOconfigTotalFields; ++i)
				if (strcmp($1, IOconfigFields[i]) == 0)
					item->offset = i;

			$$ = item;
		  }

io_value: VALUE
		  {
			if ($1 < 2)
				io_limit_yyerror(NIL, NULL, "io limit: value cannot smaller than 2");

			$$ = $1;
		  }
		| VALUE_MAX { $$ = -1; }

%%

int io_limit_yyerror(IO_LIMIT_PARSER_CONTEXT *parser_context, void *scanner, const char *message)
{
	ereport(ERROR, \
		(errcode(ERRCODE_SYNTAX_ERROR), \
		errmsg("%s", message))); \
	return 0;
}

/*
 * Parse io limit string to list of TblSpcIOLimit.
 */
List *io_limit_parse(const char *limit_str)
{
	List *result = NIL;
	IO_LIMIT_PARSER_CONTEXT *context = (IO_LIMIT_PARSER_CONTEXT *)palloc0(sizeof(IO_LIMIT_PARSER_CONTEXT));

	IO_LIMIT_PARSER_STATE *state = io_limit_begin_scan(limit_str);
	if (io_limit_yyparse(context, state->scanner) != 0)
		io_limit_yyerror(context, state->scanner, "io limit: parse error");

	io_limit_end_scan(state);

	result = context->result;
	pfree(context);

	return result;
}
