%output="yaml_parse.c"
%name-prefix="yaml_yy"
%pure-parser                             /* Because global variables are bad */
%error-verbose                           /* A little extra debugging info */

%{
  #include <parser.h>
  #include <yaml_parse.h>
  #include <yaml.h>

/*
 * Ancient flex versions, like the ones on our build machines don't support
 * flex .h file generation.
 */
#if USE_FLEX_HFILE
# include <yaml_scan.h>
#endif

  #include <stdio.h>

  int  yaml_yylex(YYSTYPE *lvalp, mapred_parser_t *parser);
  void yaml_yyerror(mapred_parser_t *parser, char const *);
%}
%parse-param {mapred_parser_t *parser}   /* So we can pass it to the lexer */
%lex-param   {mapred_parser_t *parser}

%union {
  int                    keyword;
  char                  *token;
}

/* Keyword tokens - be sure to keep in sync with keyword rules below */
%token <keyword>
  _COLUMNS_     _CONSOLIDATE_
  _DATABASE_    _DEFINE_      _DELIMITER_
  _ENCODING_    _ERROR_LIMIT_ _ESCAPE_      _EXEC_        _EXECUTE_
  _FILE_        _FINALIZE_    _FORMAT_      _FUNCTION_
  _GPFDIST_
  _HOST_
  _INITIALIZE_  _INPUT_
  _KEYS_
  _LANGUAGE_    _LIBRARY_
  _MAP_         _MODE_
  _NAME_        _NULL_
  _OPTIMIZE_    _ORDERING_    _OUTPUT_
  _PARAMETERS_  _PORT_ 
  _QUERY_       _QUOTE_
  _REDUCE_      _RETURNS_     _RUN_
  _SOURCE_
  _TABLE_       _TARGET_      _TASK_        _TRANSITION_ 
  _USER_
  _VERSION_


 /* Non-keyword scalar tokens */
%token <token> _INTEGER_
%token <token> _VERSION_STRING_
%token <token> _ID_
%token <token> _STRING_
%type  <token>        scalar

/* YAML State tokens */
%token START_STREAM   END_STREAM
%token START_DOCUMENT END_DOCUMENT
%token START_LIST     END_LIST
%token START_MAP      END_MAP

/* and the special "ERROR" token */
%token ERROR

%% /* Grammar rules and actions follow */


stream:
    START_STREAM 
    document_list 
    END_STREAM       
    { 
      if (parser->current_doc &&
		  parser->current_doc->u.document.flags & mapred_document_error)
	  {
		  YYABORT;  /* If we found an error, return error */
	  }
	}
    ;

/* 
 * For error recovery we often need all the keywords except 'foo', to help
 * facilitate this I break the keywords into logical groupings.
 */
doc_keywords:
    _DATABASE_|_DEFINE_|_EXECUTE_|_HOST_|_PORT_|_USER_|_VERSION_ 
    ;

obj_keywords:
    def_keywords|exec_keywords
    ;

def_keywords:
    _CONSOLIDATE_|_FINALIZE_|_INPUT_|_MAP_|_OUTPUT_|_REDUCE_|
    _TASK_|_TRANSITION_ 
    ;

exec_keywords:
    _RUN_
    ;

func_keywords:
    _FUNCTION_|_LANGUAGE_|_OPTIMIZE_|_PARAMETERS_|_RETURNS_|_MODE_|_LIBRARY_
    ;

io_keywords:
    _COLUMNS_|_DELIMITER_|_ENCODING_|_ERROR_LIMIT_|_ESCAPE_|_EXEC_|
    _FILE_|_FORMAT_|_GPFDIST_|_NULL_|_QUERY_|_QUOTE_|_TABLE_
    ;

misc_keywords:
    _INITIALIZE_|_KEYS_|_ORDERING_|_SOURCE_|_TARGET_
    ;

keyword:
    _NAME_|doc_keywords|obj_keywords|func_keywords|io_keywords|misc_keywords ;

document_list: 
    | document_list document
    ;

document:  
    START_DOCUMENT                 { parser_begin_document(parser); }
    document_contents
    END_DOCUMENT
    | error END_DOCUMENT
	;

document_contents:
    START_MAP doc_item_list END_MAP

    | valid_yaml_list
      { yaml_yyerror(parser, "Greenplum MapReduce document must begin with a YAML MAPPING"); }
    | scalar
      { yaml_yyerror(parser, "Greenplum MapReduce document must begin with a YAML MAPPING"); }
    ;

doc_item_list:
    doc_item
    | doc_item_list doc_item
    ;

doc_item:
    _VERSION_     doc_version  
    | _DATABASE_  doc_database
    | _USER_      doc_user
    | _HOST_      doc_host
    | _PORT_      doc_port
    | _DEFINE_    doc_define
    | _EXECUTE_   doc_execute

	| _NAME_           { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| obj_keywords     { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| func_keywords    { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| io_keywords      { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| misc_keywords    { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| scalar           { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| valid_yaml_list  { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
	| valid_yaml_map   { yaml_yyerror(parser, "Invalid Document Attribute"); } valid_yaml
    ;

doc_version:
    _VERSION_STRING_              { parser_set_version(parser, $1); }

    | valid_yaml_list             { yaml_yyerror(parser, "VERSION must be a scalar value"); }
    | valid_yaml_map              { yaml_yyerror(parser, "VERSION must be a scalar value"); }
    | _STRING_                    { yaml_yyerror(parser, "Invalid VERSION format"); }
    | _INTEGER_                   { yaml_yyerror(parser, "Invalid VERSION format"); }
    | _ID_                        { yaml_yyerror(parser, "Invalid VERSION format"); }
    | error                       { yaml_yyerror(parser, "Invalid VERSION format"); }
    ;

doc_database:
      _ID_                        { parser_set_database(parser, $1); }

    | valid_yaml_list             { yaml_yyerror(parser, "DATABASE must be a scalar value"); }
    | valid_yaml_map              { yaml_yyerror(parser, "DATABASE must be a scalar value"); }
    | _STRING_                    { yaml_yyerror(parser, "Invalid DATABASE format"); }
    | _INTEGER_                   { yaml_yyerror(parser, "Invalid DATABASE format"); }
    | _VERSION_STRING_            { yaml_yyerror(parser, "Invalid DATABASE format"); }
    | error                       { yaml_yyerror(parser, "Invalid DATABASE format"); }
    ;

doc_user:
      _ID_                        { parser_set_user(parser, $1); }

    | valid_yaml_list             { yaml_yyerror(parser, "USER must be a scalar value"); }
    | valid_yaml_map              { yaml_yyerror(parser, "USER must be a scalar value"); }
    | _STRING_                    { yaml_yyerror(parser, "Invalid USER format"); }
    | _INTEGER_                   { yaml_yyerror(parser, "Invalid USER format"); }
    | _VERSION_STRING_            { yaml_yyerror(parser, "Invalid USER format"); }
    | error                       { yaml_yyerror(parser, "Invalid USER format"); }
    ;

doc_host:
      scalar                      { parser_set_host(parser, $1); }

    | valid_yaml_list             { yaml_yyerror(parser, "HOST must be a scalar value"); }
    | valid_yaml_map              { yaml_yyerror(parser, "HOST must be a scalar value"); }
    | error                       { yaml_yyerror(parser, "Invalid HOST format"); }
    ;

doc_port:
      _INTEGER_                   { parser_set_port(parser, $1); }

    | valid_yaml_list             { yaml_yyerror(parser, "PORT must be an integer value"); }
    | valid_yaml_map              { yaml_yyerror(parser, "PORT must be an integer value"); }
    | _ID_                        { yaml_yyerror(parser, "PORT must be an integer value"); }
    | _STRING_                    { yaml_yyerror(parser, "PORT must be an integer value"); }
    | _VERSION_STRING_            { yaml_yyerror(parser, "PORT must be an integer value"); }
    | error                       { yaml_yyerror(parser, "PORT must be an integer value"); }
    ;

doc_define:
      START_LIST                  { parser_begin_define(parser); }
      define_list  
      END_LIST

    | scalar                      { yaml_yyerror(parser, "DEFINE must be a YAML LIST"); }
    | keyword                     { yaml_yyerror(parser, "DEFINE must be a YAML LIST"); }
    | valid_yaml_map              { yaml_yyerror(parser, "DEFINE must be a YAML LIST"); }

doc_execute:
      START_LIST                  { parser_begin_execute(parser); }
      execute_list
      END_LIST

    | scalar                      { yaml_yyerror(parser, "EXECUTE must be a YAML LIST"); }
    | keyword                     { yaml_yyerror(parser, "EXECUTE must be a YAML LIST"); }
    | valid_yaml_map              { yaml_yyerror(parser, "EXECUTE must be a YAML LIST"); }

   
execute_list:
    /* empty */
    | execute_list  
      START_MAP 
      execute_item 
      END_MAP 

    | valid_yaml_list             { yaml_yyerror(parser, "List element found in EXECUTE"); }
    | scalar 
      {
		  char buffer[128];
		  snprintf(buffer, sizeof(buffer), "Scalar value '%s' found in EXECUTE", $1);
		  yaml_yyerror(parser, buffer);
	  }
    ;

execute_item:
        _RUN_        { parser_add_run(parser); }       
        run_map

      | _NAME_           { yaml_yyerror(parser, "Invalid EXECUTE Attribute"); } valid_yaml
      | def_keywords     { yaml_yyerror(parser, "Invalid EXECUTE Attribute"); } valid_yaml
	  | func_keywords    { yaml_yyerror(parser, "Invalid EXECUTE Attribute"); } valid_yaml
	  | io_keywords      { yaml_yyerror(parser, "Invalid EXECUTE Attribute"); } valid_yaml
	  | misc_keywords    { yaml_yyerror(parser, "Invalid EXECUTE Attribute"); } valid_yaml
	  | valid_yaml_list  { yaml_yyerror(parser, "YAML LIST element found in EXECUTE"); } valid_yaml
  	  | valid_yaml_map   { yaml_yyerror(parser, "YAML MAPPING element found in EXECUTE"); } valid_yaml
	  | scalar          
        {
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "'%s' is not a valid EXECUTE element", $1);
			yaml_yyerror(parser, buffer); 
		} 
        valid_yaml
      ;

define_list:
      /* empty */
      | define_list     
        START_MAP 
        define_item
  	    more_define_items           /* errors if there actually is something */
        END_MAP

      | valid_yaml_list             { yaml_yyerror(parser, "List element found in DEFINE"); }
      | scalar 
        {
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "Scalar value '%s' found in DEFINE", $1);
			yaml_yyerror(parser, buffer);
		}
      ;

more_define_items:
       /* only good second item is one that doesn't exist */
	  | keyword    { yaml_yyerror(parser, "Multiple objects in one list element"); } valid_yaml
	    more_define_items
      | valid_yaml { yaml_yyerror(parser, "Multiple objects in one list element"); } valid_yaml
	    more_define_items


define_item:
        _INPUT_      { parser_add_object(parser, MAPRED_INPUT); }       input_map
      | _OUTPUT_     { parser_add_object(parser, MAPRED_OUTPUT); }      output_map
      | _REDUCE_     { parser_add_object(parser, MAPRED_REDUCER); }     reduce_map 
      | _MAP_        { parser_add_object(parser, MAPRED_MAPPER); }      function_map
      | _TRANSITION_ { parser_add_object(parser, MAPRED_TRANSITION); }  function_map
      | _CONSOLIDATE_ { parser_add_object(parser, MAPRED_COMBINER); }   function_map
      | _FINALIZE_   { parser_add_object(parser, MAPRED_FINALIZER); }   function_map
      | _TASK_       { parser_add_object(parser, MAPRED_TASK); }        task_map


	  | _NAME_          { yaml_yyerror(parser, "Invalid DEFINE Attribute"); } valid_yaml
	  | exec_keywords   { yaml_yyerror(parser, "Invalid DEFINE Attribute"); } valid_yaml
      | func_keywords   { yaml_yyerror(parser, "Invalid DEFINE Attribute"); } valid_yaml
      | io_keywords     { yaml_yyerror(parser, "Invalid DEFINE Attribute"); } valid_yaml
      | misc_keywords   { yaml_yyerror(parser, "Invalid DEFINE Attribute"); } valid_yaml
  	  | valid_yaml_list { yaml_yyerror(parser, "YAML LIST element found in DEFINE"); } valid_yaml
  	  | valid_yaml_map  { yaml_yyerror(parser, "YAML MAPPING element found in DEFINE"); } valid_yaml
	  | scalar
        {
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "'%s' is not a valid DEFINE element", $1);
			yaml_yyerror(parser, buffer); 
		} valid_yaml
      ;

input_map:
      START_MAP input_item_map END_MAP

	  | valid_yaml_list  { yaml_yyerror(parser, "INPUT must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "INPUT must contain a YAML MAPPING"); }
      ;

output_map:       
      START_MAP output_item_map END_MAP 

	  | valid_yaml_list  { yaml_yyerror(parser, "OUTPUT must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "OUTPUT must contain a YAML MAPPING"); }
	  ;

reduce_map:       
      START_MAP reduce_item_map END_MAP

	  | valid_yaml_list  { yaml_yyerror(parser, "REDUCE must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "REDUCE must contain a YAML MAPPING"); }
	  ;

function_map:     
      START_MAP function_item_map END_MAP

	  /* FIXME: error should refer to MAP/TRANSITION/... not FUNCTION */
	  | valid_yaml_list  { yaml_yyerror(parser, "FUNCTION must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "FUNCTION must contain a YAML MAPPING"); }
	  ;

task_map:         
       START_MAP task_item_map END_MAP

	  | valid_yaml_list  { yaml_yyerror(parser, "TASK must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "TASK must contain a YAML MAPPING"); }
	  ;

run_map:
        START_MAP run_item_map END_MAP

	  | valid_yaml_list  { yaml_yyerror(parser, "RUN must contain a YAML MAPPING"); }
      | scalar           { yaml_yyerror(parser, "RUN must contain a YAML MAPPING"); }
	  ;


input_item_map:       input_item | input_item_map    input_item ;
output_item_map:     output_item | output_item_map   output_item ;
reduce_item_map:     reduce_item | reduce_item_map   reduce_item ;
function_item_map: function_item | function_item_map function_item ;
task_item_map:         task_item | task_item_map     task_item ;
run_item_map:           run_item | run_item_map      run_item ;

input_item:
        _NAME_      obj_name
      | _COLUMNS_               { parser_begin_columns(parser); }     column_list
      | _FILE_                  { parser_begin_files(parser); }       file_list
      | _GPFDIST_               { parser_begin_gpfdist(parser); }     file_list
      | _TABLE_     io_table
      | _QUERY_     input_query
      | _EXEC_      input_exec
      | _FORMAT_    io_format
      | _DELIMITER_ scalar      { parser_set_delimiter(parser, $2); }
      | _NULL_      scalar      { parser_set_null(parser, $2); }
      | _QUOTE_     scalar      { parser_set_quote(parser, $2); }
      | _ESCAPE_    scalar      { parser_set_escape(parser, $2); }
      | _ENCODING_  scalar      { parser_set_encoding(parser, $2); }
      | _ERROR_LIMIT_ _INTEGER_ { parser_set_error_limit(parser, $2); }

    /* Error recovery */
      | _DELIMITER_   valid_yaml_list { parser_set_delimiter(parser, 0); }
      | _DELIMITER_   valid_yaml_map  { parser_set_delimiter(parser, 0); }
      | _NULL_        valid_yaml_list { parser_set_null(parser, 0); }
      | _NULL_        valid_yaml_map  { parser_set_null(parser, 0); }
      | _QUOTE_       valid_yaml_list { parser_set_quote(parser, 0); }
      | _QUOTE_       valid_yaml_map  { parser_set_quote(parser, 0); }
      | _ESCAPE_      valid_yaml_list { parser_set_escape(parser, 0); }
      | _ESCAPE_      valid_yaml_map  { parser_set_escape(parser, 0); }
      | _ENCODING_    valid_yaml_list { parser_set_encoding(parser, 0); }
      | _ENCODING_    valid_yaml_map  { parser_set_encoding(parser, 0); }
      | _ERROR_LIMIT_ valid_yaml_list { parser_set_error_limit(parser, 0); }
      | _ERROR_LIMIT_ valid_yaml_map  { parser_set_error_limit(parser, 0); }
      | _ERROR_LIMIT_ _STRING_        { parser_set_error_limit(parser, 0); }
      | _ERROR_LIMIT_ _ID_            { parser_set_error_limit(parser, 0); }
      | _ERROR_LIMIT_ _VERSION_STRING_ { parser_set_error_limit(parser, 0); }
	  | doc_keywords      { yaml_yyerror(parser, "Invalid INPUT Attribute"); } valid_yaml
	  | obj_keywords      { yaml_yyerror(parser, "Invalid INPUT Attribute"); } valid_yaml
      | func_keywords     { yaml_yyerror(parser, "Invalid INPUT Attribute"); } valid_yaml
      | misc_keywords     { yaml_yyerror(parser, "Invalid INPUT Attribute"); } valid_yaml
  	  | valid_yaml_list   { yaml_yyerror(parser, "YAML LIST element found in INPUT"); } valid_yaml
  	  | valid_yaml_map    { yaml_yyerror(parser, "YAML MAPPING element found in INPUT"); } valid_yaml
	  | scalar
       {
		   char buffer[128];
		   snprintf(buffer, 128, "%s is not a valid INPUT attribute", $1);
		   yaml_yyerror(parser, buffer); 
	   } valid_yaml
      ;


output_item:
        _NAME_      obj_name
      | _TABLE_     io_table
      | _FILE_      scalar         { parser_set_file(parser, $2); }
      | _FORMAT_    io_format
      | _DELIMITER_ scalar    { parser_set_delimiter(parser, $2); }
      | _MODE_ scalar         { parser_set_mode(parser, $2); }

    /* Error recovery */
      | _FILE_      valid_yaml_list     { parser_set_file(parser, 0); }
      | _FILE_      valid_yaml_map      { parser_set_file(parser, 0); }
      | _DELIMITER_ valid_yaml_list     { parser_set_delimiter(parser, 0); }
      | _DELIMITER_ valid_yaml_map      { parser_set_delimiter(parser, 0); }
      | _MODE_      valid_yaml_list     { parser_set_mode(parser, 0); }
      | _MODE_      valid_yaml_map      { parser_set_mode(parser, 0); }
	  | doc_keywords          { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | obj_keywords          { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _FUNCTION_            { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _LIBRARY_             { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _LANGUAGE_            { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _OPTIMIZE_            { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _PARAMETERS_          { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
	  | _RETURNS_             { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
      | misc_keywords         { yaml_yyerror(parser, "Invalid OUTPUT Attribute"); } valid_yaml
  	  | valid_yaml_list       { yaml_yyerror(parser, "YAML LIST element found in OUTPUT"); } valid_yaml
  	  | valid_yaml_map        { yaml_yyerror(parser, "YAML MAPPING element found in OUTPUT"); } valid_yaml

	  | _STRING_            
       {
		   char buffer[128];
		   snprintf(buffer, 128, "%s is not a valid OUTPUT attribute", $1);
		   yaml_yyerror(parser, buffer);
	   } valid_yaml
      ;

reduce_item:
        _NAME_       obj_name
      | _TRANSITION_ _ID_     { parser_set_transition(parser, $2); }
      | _CONSOLIDATE_ _ID_    { parser_set_combiner(parser, $2); }
      | _FINALIZE_   _ID_     { parser_set_finalizer(parser, $2); }
      | _INITIALIZE_ scalar   { parser_set_initialize(parser, $2); }
      | _KEYS_                { parser_begin_keys(parser); }
           key_list
      | _ORDERING_            { parser_begin_ordering(parser); }
           ordering_list

    /* Error recovery */
	  | _INPUT_               { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _OUTPUT_              { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _MAP_                 { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _REDUCE_              { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _TASK_                { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _SOURCE_              { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | _TARGET_              { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | doc_keywords          { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | exec_keywords         { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
	  | func_keywords         { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
      | io_keywords           { yaml_yyerror(parser, "Invalid REDUCE Attribute"); } valid_yaml
  	  | valid_yaml_list       { yaml_yyerror(parser, "YAML LIST element found in REDUCE"); } valid_yaml
  	  | valid_yaml_map        { yaml_yyerror(parser, "YAML MAPPING element found in REDUCE"); } valid_yaml
	  | _STRING_            
        {
			char buffer[128];
			snprintf(buffer, 128, "%s is not a valid REDUCER attribute", $1);
			yaml_yyerror(parser, buffer); 
		} valid_yaml
      ;

function_item:
       _NAME_ obj_name
      | _LANGUAGE_ scalar     { parser_set_language(parser, $2); }
      | _FUNCTION_ scalar     { parser_set_function(parser, $2); }
      | _LIBRARY_ scalar      { parser_set_library(parser, $2); }
      | _MODE_ scalar         { parser_set_mode(parser, $2); }
      | _OPTIMIZE_ scalar     { parser_set_optimize(parser, $2); }
      | _PARAMETERS_          { parser_begin_parameters(parser); } 
           parameter_list
      | _RETURNS_             { parser_begin_returns(parser); }
           return_list

    /* Error recovery */
      | _LANGUAGE_  valid_yaml_map   { parser_set_language(parser, 0); }
      | _LANGUAGE_  valid_yaml_list  { parser_set_language(parser, 0); }
      | _FUNCTION_  valid_yaml_map   { parser_set_function(parser, 0); }
      | _FUNCTION_  valid_yaml_list  { parser_set_function(parser, 0); }
      | _LIBRARY_   valid_yaml_map   { parser_set_library(parser, 0); }
      | _LIBRARY_   valid_yaml_list  { parser_set_library(parser, 0); }
      | _MODE_      valid_yaml_map   { parser_set_mode(parser, 0); }
      | _MODE_      valid_yaml_list  { parser_set_mode(parser, 0); }
      | _OPTIMIZE_  valid_yaml_map   { parser_set_optimize(parser, 0); }
      | _OPTIMIZE_  valid_yaml_list  { parser_set_optimize(parser, 0); }
	  | doc_keywords   { yaml_yyerror(parser, "Invalid FUNCTION Attribute"); } valid_yaml
	  | obj_keywords   { yaml_yyerror(parser, "Invalid FUNCTION Attribute"); } valid_yaml
      | io_keywords    { yaml_yyerror(parser, "Invalid FUNCTION Attribute"); } valid_yaml
      | misc_keywords  { yaml_yyerror(parser, "Invalid FUNCTION Attribute"); } valid_yaml
  	  | valid_yaml_list  { yaml_yyerror(parser, "YAML LIST element found in FUNCTION"); } valid_yaml
  	  | valid_yaml_map   { yaml_yyerror(parser, "YAML MAPPING element found in FUNCTION"); } valid_yaml
	  | _STRING_            
        {
			char buffer[128];
			snprintf(buffer, 128, "%s is not a valid FUNCTION attribute", $1);
			yaml_yyerror(parser, buffer); 
		} valid_yaml
      ;

task_item:
      _NAME_      obj_name
      | _SOURCE_  _ID_        { parser_set_source(parser, $2); }
      | _MAP_     _ID_        { parser_set_mapper(parser, $2); }
      | _REDUCE_  _ID_        { parser_set_reducer(parser, $2); }

    /* Error recovery */
	  | doc_keywords          { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | exec_keywords         { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | _TRANSITION_          { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | _CONSOLIDATE_         { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | _FINALIZE_            { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | _INPUT_               { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
	  | _OUTPUT_              { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
      | io_keywords           { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
      | _INITIALIZE_          { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
      | _TARGET_              { yaml_yyerror(parser, "Invalid TASK Attribute"); } valid_yaml
  	  | valid_yaml_list       { yaml_yyerror(parser, "YAML LIST element found in TASK"); } valid_yaml
  	  | valid_yaml_map        { yaml_yyerror(parser, "YAML MAPPING element found in TASK"); } valid_yaml
	  | _STRING_            
        {
			char buffer[128];
			snprintf(buffer, 128, "%s is not a valid TASK attribute", $1);
			yaml_yyerror(parser, buffer); 
		} valid_yaml
      ;

run_item:
      _NAME_      obj_name
      | _SOURCE_  _ID_        { parser_set_source(parser, $2); }
      | _TARGET_  _ID_        { parser_set_target(parser, $2); }
      | _MAP_     _ID_        { parser_set_mapper(parser, $2); }
      | _REDUCE_  _ID_        { parser_set_reducer(parser, $2); }

    /* Error recovery */
      | _INITIALIZE_          { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | doc_keywords          { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | exec_keywords         { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | _TRANSITION_          { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | _CONSOLIDATE_         { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | _FINALIZE_            { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | _INPUT_               { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
	  | _OUTPUT_              { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
      | io_keywords           { yaml_yyerror(parser, "Invalid RUN Attribute"); } valid_yaml
  	  | valid_yaml_list       { yaml_yyerror(parser, "YAML LIST element found in RUN"); } valid_yaml
  	  | valid_yaml_map        { yaml_yyerror(parser, "YAML MAPPING element found in RUN"); } valid_yaml
	  | _STRING_             
        {
			char buffer[128];
			snprintf(buffer, 128, "%s is not a valid RUN attribute", $1);
			yaml_yyerror(parser, buffer); 
		} valid_yaml
      ;


/* rules primarily created for error handling */
obj_name:
      _ID_                        { parser_set_name(parser, $1); }

    /* Error recovery */
    | { parser_set_name(parser, 0); }  _STRING_
    | { parser_set_name(parser, 0); }  _INTEGER_
    | { parser_set_name(parser, 0); }  _VERSION_STRING_
    | { parser_set_name(parser, 0); }  valid_yaml_list
    | { parser_set_name(parser, 0); }  valid_yaml_map
    | { 
         parser_set_name(parser, 0); 
         YYERROR;    /* Can't recover from unknown error */
      }  error
    ;

io_table:
      _ID_                        { parser_set_table(parser, $1); }

    /* Error recovery */
    | { parser_set_table(parser, 0); }  valid_yaml_list             
    | { parser_set_table(parser, 0); }  valid_yaml_map
    | { parser_set_table(parser, 0); }  _STRING_
    | { parser_set_table(parser, 0); }  _INTEGER_
    | { parser_set_table(parser, 0); }  _VERSION_STRING_
    | {
		  parser_set_table(parser, 0);
		  YYERROR;   /* Can't recover from unknown error */
	  } error
    ;

input_query:
      scalar                      { parser_set_query(parser, $1); }

      /* Error recovery */
    | valid_yaml_list             { parser_set_query(parser, 0); }
    | valid_yaml_map              { parser_set_query(parser, 0); }
    | error           
      {
		  parser_set_query(parser, 0);
		  YYERROR;   /* Can't recover from unknown error */
	  }
    ;

input_exec:
      scalar                      { parser_set_exec(parser, $1); }

      /* Error recovery */
    | valid_yaml_list             { parser_set_exec(parser, 0); }
    | valid_yaml_map              { parser_set_exec(parser, 0); }
    | error           
      {
		  parser_set_exec(parser, 0);
		  YYERROR;   /* Can't recover from unknown error */
	  }
    ;

io_format:
      scalar                    { parser_set_format(parser, $1); }

      /* Error recovery */
    | valid_yaml_list           { parser_set_format(parser, 0); }
    | valid_yaml_map            { parser_set_format(parser, 0); }
    | error           
      {
		  parser_set_format(parser, 0);
		  YYERROR;   /* Can't recover from unknown error */
	  }
    ;



/* Could probably make these more generic */
file_list:
        scalar                { parser_add_file(parser, $1); }
      | START_LIST file_list_2 END_LIST
	  
      /* Error recovery */
	  | valid_yaml_map        { parser_add_file(parser, 0); }
      ;

file_list_2:
      /* empty */
      | file_list_2 scalar    { parser_add_file(parser, $2); }

      /* Error recovery */
	  | valid_yaml_map        { parser_add_file(parser, 0); }
	  | valid_yaml_list       { parser_add_file(parser, 0); }
      ;

column_list:
        scalar                { parser_add_column(parser, $1); }
      | START_LIST column_list_2 END_LIST

      /* Error recovery */
	  | valid_yaml_map        { parser_add_column(parser, 0); }
      ;

column_list_2:
      /* empty */
      | column_list_2 scalar  { parser_add_column(parser, $2); }

      /* Error recovery */
	  | valid_yaml_map        { parser_add_column(parser, 0); }
	  | valid_yaml_list       { parser_add_column(parser, 0); }
      ;

parameter_list:
        scalar                { parser_add_parameter(parser, $1); }
      | START_LIST parameter_list_2 END_LIST

      /* Error recovery */
	  | valid_yaml_map        { parser_add_parameter(parser, 0); }
      ;


parameter_list_2:
      /* empty */
      | parameter_list_2 scalar { parser_add_parameter(parser, $2); }

      /* Error recovery */
      | valid_yaml_map        { parser_add_parameter(parser, 0); }
      | valid_yaml_list       { parser_add_parameter(parser, 0); }
      ;


return_list:
        scalar                { parser_add_return(parser, $1); }
      | START_LIST return_list_2 END_LIST

	  | valid_yaml_map        { parser_add_return(parser, 0); }
	  ;

return_list_2:
      /* empty */
      | return_list_2 scalar    { parser_add_return(parser, $2); }

      /* Error recovery */
	  | valid_yaml_map          { parser_add_return(parser, 0); }
	  | valid_yaml_list         { parser_add_return(parser, 0); }
      ;

key_list:
        scalar                { parser_add_key(parser, $1); }
      | START_LIST key_list_2 END_LIST

      /* Error recovery */
	  | valid_yaml_map        { parser_add_key(parser, 0); }
	  ;

key_list_2:
      /* empty */
      | key_list_2 scalar     { parser_add_key(parser, $2); }

      /* Error recovery */
	  | valid_yaml_map        { parser_add_key(parser, 0); }
	  | valid_yaml_list       { parser_add_key(parser, 0); }
      ;


ordering_list:
        scalar                { parser_add_ordering(parser, $1); }
      | START_LIST ordering_list_2 END_LIST

      /* Error recovery */
	  | valid_yaml_map        { parser_add_ordering(parser, 0); }
	  ;

ordering_list_2:
      /* empty */
      | ordering_list_2 scalar { parser_add_ordering(parser, $2); }

      /* Error recovery */
	  | valid_yaml_map        { parser_add_ordering(parser, 0); }
	  | valid_yaml_list       { parser_add_ordering(parser, 0); }
      ;




scalar:
      _STRING_
	  | _INTEGER_
	  | _ID_
	  | _VERSION_STRING_
	  ;


/* These are used to handle error recovery */
valid_yaml:       
      scalar
    | valid_yaml_list
    | valid_yaml_map
    ;
valid_yaml_list:  
      START_LIST valid_yaml_list_item END_LIST
    ;
valid_yaml_map:   
      START_MAP  valid_yaml_map_item  END_MAP
    ;
valid_yaml_list_item:    
    | valid_yaml_list_item valid_yaml
    ;

valid_yaml_map_item:    
    | valid_yaml_map_item valid_yaml valid_yaml
    | valid_yaml_map_item keyword valid_yaml
	;


%%

static const char EMPTY_STRING[1] = "";


/* Called by yyparse on error.  */
void yaml_yyerror (mapred_parser_t *parser, char const *s)
{
	if (parser->current_doc)
	{
		if (global_verbose_flag)
			fprintf(stderr, "    - ");
		parser->current_doc->u.document.flags |= 
			mapred_document_error;
	}
	else
	{
		if (global_verbose_flag)
			fprintf(stderr, "  - ");
	}

	if (parser->yparser->error != YAML_NO_ERROR)
	{
		fprintf(stderr, "Error: YAML syntax error - %s %s, at line %d\n",
				NULL != parser->yparser->problem? parser->yparser->problem: EMPTY_STRING,
				NULL != parser->yparser->context? parser->yparser->context: EMPTY_STRING,
				(int) parser->yparser->problem_mark.line+1);
	}
	else 
	{
		fprintf(stderr, "Error: %s, at line %d\n", s,
				(int) parser->event.start_mark.line+1);
	}
}

#if 0
#define DEBUG_TOKEN(x) printf("%s\n", x)
#else
#define DEBUG_TOKEN(x) do { } while (0)
#endif
#if 0
#define DEBUG_YYTOKEN(x) printf("  YYTOKEN=%d\n", x)
#else
#define DEBUG_YYTOKEN(x) do { } while (0)
#endif


/* 
 * int yamllex(lval, parser)
 *
 * We use a crazy mix of the yaml parse library and flex/bison to build
 * our parser.  
 * 
 * The YAML library handles all of the whitespace and flow parsing very
 * cleanly, but it has no domain knowledge of the Greenplum Mapreduce
 * YAML Schema.
 *
 * Coding up the YAML flow stuff in flex/bison is a pain
 *
 * So... we put the parse first through the YAML library parser and use
 * that as our first pass tokenizer to handle all of the YAML document flow
 * proccessing.  
 *
 * If it was just that simple then we could feed these tokens into bison and 
 * be done, but we also want additional lexical analysis on the scalar values 
 * so we feed them back into a flex tokenizer.
 *
 */
int
yaml_yylex (YYSTYPE *lvalp, mapred_parser_t *parser)
{
	if (parser->state == STATE_DONE)
		return 0;

	if (parser->state == STATE_SCALAR_LEX)
	{
		int token;
#if USE_FLEX_REENTRANT
		token = yaml_scalar_yylex(parser->yscanner, parser);
#else
		token = yaml_scalar_yylex(parser);
#endif
		if (token)
		{
			DEBUG_YYTOKEN(token);
			return token;
		}
		else
		{
#if USE_FLEX_REENTRANT
			yaml_scalar_yy_delete_buffer(parser->yscan_buffer, parser->yscanner);
			parser->yscan_buffer = NULL;
#endif
			parser->state = STATE_YAML_PARSE;
		}
	}

	if (parser->state != STATE_YAML_PARSE)
	{
		parser->state = STATE_DONE;
		return ERROR;
	}

	if (!yaml_parser_parse(parser->yparser, &parser->event))
	{
		parser->state = STATE_DONE;
		return ERROR;
	}

#if 0
	if (parser->frame < 0)
		printf("no frame\n");
	else if (parser->frame >= MAX_CONTEXT_DEPTH) 
		printf("bad frame\n");
	else 
	{
		switch (parser->context[parser->frame]) 
		{
			case CONTEXT_NONE:
				printf("FRAME [NONE]\n");
				break;

			case CONTEXT_HASH_KEY:
				printf("FRAME [HASH_KEY]\n");
				break;
				
			case CONTEXT_HASH_VALUE:
				printf("FRAME [HASH_VALUE]\n");
				break;

			case CONTEXT_LIST:
				printf("FRAME [LIST]\n");
				break;
				
			default:
				printf("FRAME [BAD FRAME]\n");
				break;
		}
	}
#endif

	while (1) {
		switch (parser->event.type) {

			case YAML_NO_EVENT:
				parser->state = STATE_DONE;
				return 0;

			case YAML_STREAM_START_EVENT:
				DEBUG_TOKEN("YAML_STREAM_START");
				return START_STREAM;

			case YAML_DOCUMENT_START_EVENT:
				DEBUG_TOKEN("YAML_DOCUMENT_START");
				return START_DOCUMENT;

			case YAML_MAPPING_START_EVENT:
				if (++parser->frame >= MAX_CONTEXT_DEPTH) {
					fprintf(stderr, "Maximum context depth exceded");
					parser->state = STATE_DONE;
					return END_STREAM;
				}
				parser->context[parser->frame] = CONTEXT_HASH_KEY;

				DEBUG_TOKEN("YAML_MAPPING_START");
				return START_MAP;

			case YAML_SEQUENCE_START_EVENT:
				if (++parser->frame >= MAX_CONTEXT_DEPTH) {
					printf("Maximum context depth exceded");
					parser->state = STATE_DONE;
					return END_STREAM;
				}
				parser->context[parser->frame] = CONTEXT_LIST;

				DEBUG_TOKEN("YAML_SEQUENCE_START");
				return START_LIST;

			case YAML_SCALAR_EVENT:
			{
				int   token;
				char *value = (char*) parser->event.data.scalar.value;

				DEBUG_TOKEN("SCALAR:");
				lvalp->token = value;

				/* Switch to the scalar scanner and continue */
				if (value[0] == '\0')
					token = _STRING_;
				else
				{
					parser->state = STATE_SCALAR_LEX;

#if USE_FLEX_REENTRANT
					parser->yscan_buffer = 
						yaml_scalar_yy_scan_string(value, parser->yscanner);
#else
					yaml_scalar_yy_scan_string(value);
#endif
					token = yaml_yylex(lvalp, parser);
				}

				/* 
				 * If we are in a hash context then we switch between the
				 * key states and the value states.
				 */
				if (parser->frame >= 0 && parser->frame < MAX_CONTEXT_DEPTH)
				{
					switch (parser->context[parser->frame]) 
					{
						case CONTEXT_HASH_KEY:
							parser->context[parser->frame] = CONTEXT_HASH_VALUE;
							break;

						case CONTEXT_HASH_VALUE:
							parser->context[parser->frame] = CONTEXT_HASH_KEY;
							break;

						default:
							break;
					}
				}
				return token;
			}
			
			case YAML_SEQUENCE_END_EVENT:
				if (--parser->frame < -1)
					parser->frame = -1;

				/* 
				 * If the sequence was the value pair of a hash then the next
				 * scalar will be a key 
				 */
				if (parser->frame >= 0 && parser->frame < MAX_CONTEXT_DEPTH &&
					parser->context[parser->frame] == CONTEXT_HASH_VALUE)
					parser->context[parser->frame] = CONTEXT_HASH_KEY;

				DEBUG_TOKEN("YAML_SEQUENCE_END");
				return END_LIST;
				
			case YAML_MAPPING_END_EVENT:
			{
				/* Pop the parser frame stack */
				if (--parser->frame < -1)
					parser->frame = -1;

				/* 
				 * If the mapping was the value pair of a hash then the next
				 * scalar will be a key 
				 */
				if (parser->frame >= 0 && parser->frame < MAX_CONTEXT_DEPTH &&
					parser->context[parser->frame] == CONTEXT_HASH_VALUE)
					parser->context[parser->frame] = CONTEXT_HASH_KEY;

				DEBUG_TOKEN("YAML_MAPPING_END");
				return END_MAP;
			}
				
			case YAML_DOCUMENT_END_EVENT:
				DEBUG_TOKEN("YAML_DOCUMENT_END");
				return END_DOCUMENT;
				
			case YAML_STREAM_END_EVENT:
				DEBUG_TOKEN("YAML_STREAM_END");
				return END_STREAM;
				
			default:
				printf("WARNING: Unknown event %d\n", parser->event.type);
		}
		
		yaml_event_delete(&parser->event);
	}
}
