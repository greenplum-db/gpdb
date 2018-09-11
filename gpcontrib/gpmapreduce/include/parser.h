#ifndef PARSER_H
#define PARSER_H

#ifndef MAPRED_H
#include <mapred.h>
#endif
#ifndef YAML_H
#include <yaml.h>
#endif


typedef enum {
	STATE_BOOTSTRAP = 0, 
	STATE_YAML_PARSE,
	STATE_SCALAR_LEX,
	STATE_DONE,
} mapred_parser_state_t;

typedef enum {
	CONTEXT_NONE = 0,
	CONTEXT_HASH_KEY,
	CONTEXT_HASH_VALUE,
	CONTEXT_LIST,
} mapred_parser_context_t;

/*
 * Current max context depth = 7
 *   -DOCUMENT HASH:
 *     - DEFINE LIST:
 *       - ITEM HASH:
 *         - ATTR HASH:
 *           - VALUE LIST
 *             - KEY/VALUE hash
 *               - extra level to promote good error messages
 */
#define MAX_CONTEXT_DEPTH 7

typedef struct mapred_parser_ {
	mapred_parser_state_t     state;
	mapred_parser_context_t   context[MAX_CONTEXT_DEPTH];
	int                       frame;
	yaml_event_t              event;
	yaml_parser_t            *yparser;
	mapred_olist_t           *doclist;
	mapred_object_t          *current_doc;
	mapred_object_t          *current_obj;
	int                       doc_number;

#if USE_FLEX_REENTRANT
	void                     *yscanner;
	void                     *yscan_buffer;
#endif
} mapred_parser_t;

/*
 * A couple flex options that would be nice, but aren't supported by the
 * ancient flex on the build machines.
 */
#define USE_FLEX_HFILE     0
#define USE_FLEX_REENTRANT 0

#if USE_FLEX_REENTRANT
# define YY_DECL \
	int yaml_scalar_yylex (yyscan_t yyscanner, mapred_parser_t *parser)
#else
# define YY_DECL \
	int yaml_scalar_yylex (mapred_parser_t *parser)
#endif
YY_DECL;
struct yy_buffer_state* yaml_scalar_yy_scan_string (const char *yy_str);

/* Why bison doesn't put this in the header file I do not know... */
int yaml_yyparse(mapred_parser_t *parser);

/* Given a yaml parser return a list of parsed documents */
mapred_olist_t* mapred_parse_yaml(yaml_parser_t *parser);

/* object functions */
void parser_begin_document(mapred_parser_t *parser);
void   parser_begin_define(mapred_parser_t *parser);
void  parser_begin_execute(mapred_parser_t *parser);

void   parser_add_document(mapred_parser_t *parser);
void     parser_add_object(mapred_parser_t *parser, mapred_kind_t kind);
void        parser_add_run(mapred_parser_t *parser);

/* list functions */
void      parser_begin_files(mapred_parser_t *parser);
void    parser_begin_gpfdist(mapred_parser_t *parser);
void    parser_begin_columns(mapred_parser_t *parser);
void parser_begin_parameters(mapred_parser_t *parser);
void    parser_begin_returns(mapred_parser_t *parser);
void       parser_begin_keys(mapred_parser_t *parser);
void   parser_begin_ordering(mapred_parser_t *parser);

void       parser_add_file(mapred_parser_t *parser, char *value);
void     parser_add_column(mapred_parser_t *parser, char *value);
void  parser_add_parameter(mapred_parser_t *parser, char *value);
void     parser_add_return(mapred_parser_t *parser, char *value);
void        parser_add_key(mapred_parser_t *parser, char *value);
void   parser_add_ordering(mapred_parser_t *parser, char *value);

/* scalar functions */
void    parser_set_version(mapred_parser_t *parser, char *value);
void   parser_set_database(mapred_parser_t *parser, char *value);
void       parser_set_user(mapred_parser_t *parser, char *value);
void       parser_set_host(mapred_parser_t *parser, char *value);
void       parser_set_port(mapred_parser_t *parser, char *value);
void       parser_set_name(mapred_parser_t *parser, char *value);
void      parser_set_table(mapred_parser_t *parser, char *value);
void      parser_set_query(mapred_parser_t *parser, char *value);
void    parser_set_gpfdist(mapred_parser_t *parser, char *value);
void       parser_set_exec(mapred_parser_t *parser, char *value);
void     parser_set_format(mapred_parser_t *parser, char *value);
void       parser_set_null(mapred_parser_t *parser, char *value);
void      parser_set_quote(mapred_parser_t *parser, char *value);
void  parser_set_delimiter(mapred_parser_t *parser, char *value);
void     parser_set_escape(mapred_parser_t *parser, char *value);
void   parser_set_encoding(mapred_parser_t *parser, char *value);
void parser_set_error_limit(mapred_parser_t *parser, char *value);
void       parser_set_mode(mapred_parser_t *parser, char *value);
void       parser_set_file(mapred_parser_t *parser, char *value);
void parser_set_transition(mapred_parser_t *parser, char *value);
void   parser_set_combiner(mapred_parser_t *parser, char *value);
void  parser_set_finalizer(mapred_parser_t *parser, char *value);
void parser_set_initialize(mapred_parser_t *parser, char *value);
void   parser_set_language(mapred_parser_t *parser, char *value);
void   parser_set_function(mapred_parser_t *parser, char *value);
void    parser_set_library(mapred_parser_t *parser, char *value);
void   parser_set_optimize(mapred_parser_t *parser, char *value);
void     parser_set_source(mapred_parser_t *parser, char *value);
void     parser_set_target(mapred_parser_t *parser, char *value);
void     parser_set_mapper(mapred_parser_t *parser, char *value);
void    parser_set_reducer(mapred_parser_t *parser, char *value);



#endif
