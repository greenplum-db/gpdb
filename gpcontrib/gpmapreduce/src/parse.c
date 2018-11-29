#include <parser.h>
#include <except.h>
#include <mapred_errors.h>

#include <stdio.h>
#include <yaml_parse.h>
#include <yaml.h>

#include <stdarg.h>

int mapred_parse_error(mapred_parser_t *parser, char *fmt, ...)
	__attribute__((format(PG_PRINTF_ATTRIBUTE, 2, 3)));
int mapred_verify_object(mapred_parser_t *parser, mapred_object_t *obj);

/* -------------------------------------------------------------------------- */
int mapred_parse_error(mapred_parser_t *parser, char *fmt, ...)
{
	mapred_object_t *obj = parser->current_obj;
	va_list arg;

	if (parser && parser->current_doc)
	{
		if (global_verbose_flag)
			fprintf(stderr, "    - ");
		parser->current_doc->u.document.flags |= mapred_document_error;
	}
	else if (global_verbose_flag)
		fprintf(stderr, "  - ");

	fprintf(stderr, "Error: ");
	if (obj && obj->name)
		fprintf(stderr, "%s '%s': ", mapred_kind_name[obj->kind], obj->name);
	if (obj && !obj->name)
		fprintf(stderr, "%s: ", mapred_kind_name[obj->kind]);

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	if (parser && parser->event.start_mark.line)
		fprintf(stderr, ", at line %d", (int) parser->event.start_mark.line+1);
	fprintf(stderr, "\n");
	
	return MAPRED_PARSE_ERROR;
}


#define copyscalar(s)							\
	strcpy(malloc(strlen(s)+1), s)

mapred_olist_t* mapred_parse_string(unsigned char *yaml)
{
	mapred_olist_t  *documents;
	yaml_parser_t    parser;
	
	XASSERT(yaml);
	if (!yaml_parser_initialize(&parser))
		XRAISE(MAPRED_PARSE_INTERNAL,
			   "YAML parser initialization failed");

	yaml_parser_set_input_string(&parser, yaml, strlen((char*) yaml));
	documents = mapred_parse_yaml(&parser);
	yaml_parser_delete(&parser);
	return documents;
}

mapred_olist_t* mapred_parse_file(FILE *file)
{
	mapred_olist_t  *documents;
	yaml_parser_t parser;

	XASSERT(file);
	if (!yaml_parser_initialize(&parser))
		XRAISE(MAPRED_PARSE_INTERNAL,
			   "YAML parser initialization failed");

	yaml_parser_set_input_file(&parser, file);
	documents = mapred_parse_yaml(&parser);
	yaml_parser_delete(&parser);
	return documents;
}

mapred_olist_t* mapred_parse_yaml(yaml_parser_t *yparser)
{
	mapred_parser_t		 parser;
	int					 i;
	int					 error = 0;
	mapred_olist_t		*doc_item;

	/* Give us a clean slate */
	memset(&parser, 0, sizeof(parser));

	/* Initialize what must be initialized */
#if USE_FLEX_REENTRANT
	yaml_scalar_yylex_init (&parser.yscanner);
#endif
	parser.yparser = yparser;
	parser.state   = STATE_YAML_PARSE;
	parser.frame   = -1;
	for (i = 0; i < MAX_CONTEXT_DEPTH; i++)
		parser.context[i] = CONTEXT_NONE;

	/* Call into the parser, detects grammar errors */
	error = yaml_yyparse(&parser);

	/* finalize final document */
	parser_add_document(&parser);

	/* Cleanup and return */
#if USE_FLEX_REENTRANT
	yaml_scalar_yylex_destroy(parser.yscanner);
#endif

	/* Check for errors within documents */
	for (doc_item = parser.doclist;
		 doc_item && !error;
		 doc_item = doc_item->next)
	{
		if (doc_item->object->u.document.flags & mapred_document_error)
			error = true;
	}

	/* Cleanup and return */	
	if (error)
	{
		mapred_destroy_olist(&parser.doclist);
		XRAISE(MAPRED_PARSE_ERROR, "parse failure");
	}
	
	return parser.doclist;
}


void parser_add_document(mapred_parser_t *parser)
{
	mapred_olist_t     *newitem;
	mapred_olist_t     *doclist;
	int                 error;

	if (!parser->current_doc)
		return;

	/* Add the last of the documents objects into the document */
	parser_add_object(parser, MAPRED_NO_KIND);

	/* Verify the completed document */
	error = mapred_verify_object(parser, parser->current_doc);
	if (error != NO_ERROR)
		parser->current_doc->u.document.flags |= mapred_document_error;

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_olist_t));
	newitem->object = parser->current_doc;
	newitem->next   = (mapred_olist_t *) NULL;

	/* Insert it into the last slot of the existing list */
	doclist = parser->doclist;
	while (doclist && doclist->next)
		doclist = doclist->next;
	if (doclist)
		doclist->next = newitem;
	else
		parser->doclist = newitem;
}

void parser_begin_document(mapred_parser_t *parser)
{
	/* If there is a current document add it first */
	parser_add_document(parser);

	/* Allocate an object for the new document and return */
	parser->current_doc = malloc(sizeof(mapred_object_t));
	memset(parser->current_doc, 0, sizeof(mapred_object_t));
	parser->current_doc->kind = MAPRED_DOCUMENT;
	parser->current_doc->u.document.id = ++parser->doc_number;
	parser->current_doc->line = (int) parser->event.start_mark.line+1;

	if (global_verbose_flag)
		fprintf(stderr, "  - Parsing YAML Document %d:\n", parser->doc_number);
}

void parser_begin_define(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);

	/*
	 * The only thing we have to do is ensure that this isn't a duplicate
	 * define list.
	 */
	if (parser->current_doc->u.document.flags & mapred_document_defines)
	{
		mapred_parse_error(parser, "Duplicate DEFINE list in DOCUMENT");
		return;
	}

	parser->current_doc->u.document.flags |= mapred_document_defines;
}

void parser_begin_execute(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);

	/*
	 * The only thing we have to do is ensure that this isn't a duplicate
	 * execution list.
	 */
	if (parser->current_doc->u.document.flags & mapred_document_executes)
	{
		mapred_parse_error(parser, "Duplicate EXECUTE list in DOCUMENT");
		return;
	}

	parser->current_doc->u.document.flags |= mapred_document_executes;
}

void parser_set_version(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);

	if (parser->current_doc->u.document.version)
	{
		mapred_parse_error(parser, "Duplicate Version: %s", value);
		return;
	}

	/*
	 * We have already assured that the value matches a good regex,
	 * but we must still validate that the version itself is supported.
	 */
	if (strcmp(value, "1.0.0.1") < 0 || strcmp(value, "1.0.0.3") > 0)
	{
		mapred_parse_error(parser, "Unrecognized VERSION");
	}

	parser->current_doc->u.document.version = copyscalar(value);
}

void parser_set_database(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	if (parser->current_doc->u.document.database)
	{
		mapred_parse_error(parser, "Duplicate Database: %s", value);
		return;
	}
	parser->current_doc->u.document.database = copyscalar(value);
}

void parser_set_user(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	if (parser->current_doc->u.document.user)
	{
		mapred_parse_error(parser, "Duplicate User: %s", value);
		return;
	}
	parser->current_doc->u.document.user = copyscalar(value);
}

void parser_set_host(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	if (parser->current_doc->u.document.host)
	{
		mapred_parse_error(parser, "Duplicate Host: %s", value);
		return;
	}
	parser->current_doc->u.document.host = copyscalar(value);
}

void parser_set_port(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	if (parser->current_doc->u.document.port > 0)
	{
		mapred_parse_error(parser, "Duplicate Port: %s", value);
		return;
	}

	/*
	 * The parse has already assured that the value consists of a sequence
	 * of digits, so strtol should convert successfully.
	 */
	parser->current_doc->u.document.port = (int) strtol(value, NULL, 10);
}


/*
 * parser_add_object - Create a new empty object for the current document.
 */
void parser_add_object(mapred_parser_t *parser, mapred_kind_t kind)
{
	int error;

	XASSERT(parser->current_doc);

	/*
	 * If we have a current object then verify it and add it into the
	 * document's object list.
	 */
	if (parser->current_obj)
	{
		mapred_olist_t *newitem;
		mapred_olist_t *objlist;

		/* Validate the finished object */
		error = mapred_verify_object(parser, parser->current_obj);
		if (error != NO_ERROR)
		{
			mapred_destroy_object(&parser->current_obj);
			parser->current_doc->u.document.flags |=
				mapred_document_error;
		}
		else
		{

			/* Allocate the new list item */
			newitem = malloc(sizeof(mapred_olist_t));
			newitem->object = parser->current_obj;
			newitem->next   = (mapred_olist_t *) NULL;

			/* Insert it into the last slot of the existing list */
			objlist = parser->current_doc->u.document.objects;
			while (objlist && objlist->next)
				objlist = objlist->next;
			if (objlist)
				objlist->next = newitem;
			else
				parser->current_doc->u.document.objects = newitem;

			if (global_verbose_flag)
			{
				const char *type, *name;
				XASSERT (newitem->object->kind > 0 &&
						 newitem->object->kind <= MAPRED_MAXKIND);


				type = mapred_kind_name[newitem->object->kind];
				name = newitem->object->name;
				if (name)
					fprintf(stderr, "    - %s: %s\n", type, name);
				else
					fprintf(stderr, "    - %s\n", type);
			}
		}
	}

	/*
	 * If 'kind' is 'NO_KIND' then we just add in the current object
	 * (above) and do not create a new one.  We call it this way once
	 * at the end to add the last object into the current document.
	 */
	if (kind == MAPRED_NO_KIND)
	{
		parser->current_obj = (mapred_object_t *) NULL;
		return;
	}

	/* Allocate a new empyt object of the correct kind and return. */
	parser->current_obj = malloc(sizeof(mapred_object_t));
	memset(parser->current_obj, 0, sizeof(mapred_object_t));
	parser->current_obj->kind = kind;
	parser->current_obj->line = (int) parser->event.start_mark.line+1;
}



void parser_add_run(mapred_parser_t *parser)
{
	/*
	 * Execution objects just re-use the 'task' structure.  The only
	 * differences are that:
	 *    Execution objects get RUN
	 *    Execution objects do not require (or support) a NAME
	 */
	parser_add_object(parser, MAPRED_EXECUTION);
	parser->current_obj->u.task.execute = true;
}



void parser_set_name(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT      ||
		   parser->current_obj->kind == MAPRED_OUTPUT     ||
		   parser->current_obj->kind == MAPRED_MAPPER     ||
		   parser->current_obj->kind == MAPRED_TRANSITION ||
		   parser->current_obj->kind == MAPRED_COMBINER   ||
		   parser->current_obj->kind == MAPRED_FINALIZER  ||
		   parser->current_obj->kind == MAPRED_REDUCER    ||
		   parser->current_obj->kind == MAPRED_TASK);

	/* If this is an invalid name => throw an error */
	if (!value || strlen(value) == 0)
	{
		value = "?";
		mapred_parse_error(parser, "Invalid NAME: %s", value);
	}

	/* If the object already has a name => throw an error */
	if (parser->current_obj->name)
	{
		mapred_parse_error(parser, "Duplicate NAME: %s", value);
		return;
	}

	parser->current_obj->name = copyscalar(value);
}

void parser_set_table(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT ||
		   parser->current_obj->kind == MAPRED_OUTPUT);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid TABLE");
	}

	if (parser->current_obj->kind == MAPRED_INPUT)
	{
		if (!value || strlen(value) == 0)
		{
			if (parser->current_obj->u.input.type == MAPRED_INPUT_NONE)
				parser->current_obj->u.input.type = MAPRED_INPUT_TABLE;
			mapred_parse_error(parser, "Invalid TABLE");
			return;
		}

		if (parser->current_obj->u.input.type != MAPRED_INPUT_NONE)
		{
			switch (parser->current_obj->u.input.type)
			{
				case MAPRED_INPUT_TABLE:
					mapred_parse_error(parser,
									   "Duplicate TABLE");
					return;
				case MAPRED_INPUT_FILE:
					mapred_parse_error(parser,
									   "FILE is incompatible with TABLE");
					return;
				case MAPRED_INPUT_GPFDIST:
					mapred_parse_error(parser,
									   "GPFDIST is incompatible with TABLE");
					return;
				case MAPRED_INPUT_QUERY:
					mapred_parse_error(parser,
									   "QUERY is incompatible with TABLE");
					return;
				case MAPRED_INPUT_EXEC:
					mapred_parse_error(parser,
									   "GPFDIST is incompatible with TABLE");
					return;
				default:
					XASSERT(false);
			}
		}
		parser->current_obj->u.input.type = MAPRED_INPUT_TABLE;
		parser->current_obj->u.input.desc = copyscalar(value);
	}
	else
	{
		if (!value || strlen(value) == 0)
		{
			if (parser->current_obj->u.output.type == MAPRED_OUTPUT_NONE)
				parser->current_obj->u.output.type = MAPRED_OUTPUT_TABLE;
			mapred_parse_error(parser, "Invalid TABLE");
			return;
		}

		if (parser->current_obj->u.output.type != MAPRED_OUTPUT_NONE)
		{
			switch (parser->current_obj->u.output.type)
			{
				case MAPRED_OUTPUT_TABLE:
					mapred_parse_error(parser,
									   "Duplicate TABLE");
					return;
				case MAPRED_OUTPUT_FILE:
					mapred_parse_error(parser,
									   "FILE is incompatible with TABLE");
					return;
				default:
					XASSERT(false);
			}
		}
		parser->current_obj->u.output.type = MAPRED_OUTPUT_TABLE;
		parser->current_obj->u.output.desc = copyscalar(value);
	}
}

void parser_set_query(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid QUERY");
	}

	if (parser->current_obj->u.input.type != MAPRED_INPUT_NONE)
	{
		if (parser->current_obj->u.input.type == MAPRED_INPUT_QUERY)
		{
			mapred_parse_error(parser, "Duplicate QUERY for INPUT");
			return;
		}
		else
		{
			mapred_parse_error(parser, "INPUT may only specify one of "
							   "FILE, GPFDIST, TABLE, QUERY, EXEC");
			return;
		}
	}
	parser->current_obj->u.input.type = MAPRED_INPUT_QUERY;
	parser->current_obj->u.input.desc = copyscalar(value);
}

void parser_set_exec(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid EXEC");
	}

	if (parser->current_obj->u.input.type != MAPRED_INPUT_NONE)
	{
		if (parser->current_obj->u.input.type == MAPRED_INPUT_EXEC)
		{
			mapred_parse_error(parser, "Duplicate EXEC for INPUT");
			return;
		}
		else
		{
			mapred_parse_error(parser, "INPUT may only specify one of "
							   "FILE, GPFDIST, TABLE, QUERY, EXEC");
			return;
		}
	}
	parser->current_obj->u.input.type = MAPRED_INPUT_EXEC;
	parser->current_obj->u.input.desc = copyscalar(value);
}

void parser_set_format(mapred_parser_t *parser, char *value)
{
	mapred_format_t format;
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	if (value && !strcasecmp(value, "text"))
		format = MAPRED_FORMAT_TEXT;
	else if (value && !strcasecmp(value, "csv"))
		format = MAPRED_FORMAT_CSV;
	else
		format = MAPRED_FORMAT_INVALID;

	switch (parser->current_obj->kind)
	{
		case MAPRED_INPUT:
			if (format == MAPRED_FORMAT_INVALID)
				mapred_parse_error(parser, "Duplicate FORMAT");
			if (parser->current_obj->u.input.format != MAPRED_FORMAT_NONE)
			{
				format = MAPRED_FORMAT_INVALID;
				mapred_parse_error(parser, "Duplicate FORMAT");
			}
			parser->current_obj->u.input.format = format;
			return;

		case MAPRED_OUTPUT:
			if (format == MAPRED_FORMAT_INVALID)
				mapred_parse_error(parser, "Duplicate FORMAT");
			if (parser->current_obj->u.output.format != MAPRED_FORMAT_NONE)
			{
				format = MAPRED_FORMAT_INVALID;
				mapred_parse_error(parser, "Duplicate FORMAT");
			}
			parser->current_obj->u.output.format = format;
			return;

		default:
			XASSERT(false);
	}
}

void parser_set_delimiter(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid DELIMITER");
	}

	switch (parser->current_obj->kind)
	{
		case MAPRED_INPUT:
			if (parser->current_obj->u.input.delimiter)
			{
				mapred_parse_error(parser, "Duplicate DELIMITER");
				return;
			}
			parser->current_obj->u.input.delimiter = copyscalar(value);
			return;

		case MAPRED_OUTPUT:
			if (parser->current_obj->u.output.delimiter)
			{
				mapred_parse_error(parser, "Duplicate DELIMITER");
				return;
			}
			parser->current_obj->u.output.delimiter = copyscalar(value);
			return;
			
		default:
			XASSERT(false);
	}
}

void parser_set_escape(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid ESCAPE");
	}
	if (parser->current_obj->u.input.escape)
	{
		mapred_parse_error(parser, "Duplicate ESCAPE");
		return;
	}
	parser->current_obj->u.input.escape = copyscalar(value);
}


void parser_set_null(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		parser->current_obj->u.input.null = copyscalar("");
		mapred_parse_error(parser, "Invalid NULL");
		return;
	}
	if (parser->current_obj->u.input.null)
	{
		mapred_parse_error(parser, "Duplicate NULL");
		return;
	}
	parser->current_obj->u.input.null = copyscalar(value);
}

void parser_set_quote(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		parser->current_obj->u.input.quote = copyscalar("");
		mapred_parse_error(parser, "Invalid QUOTE");
		return;
	}
	if (parser->current_obj->u.input.quote)
	{
		mapred_parse_error(parser, "Duplicate QUOTE");
		return;
	}
	parser->current_obj->u.input.quote = copyscalar(value);
}


void parser_set_encoding(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		parser->current_obj->u.input.encoding = copyscalar("");
		mapred_parse_error(parser, "Invalid ENCODING");
		return;
	}
	if (parser->current_obj->u.input.encoding)
	{
		mapred_parse_error(parser, "Duplicate ENCODING");
		return;
	}
	parser->current_obj->u.input.encoding = copyscalar(value);
}

void parser_set_error_limit(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (!value || strlen(value) == 0)
	{
		parser->current_obj->u.input.error_limit = -1;
		mapred_parse_error(parser, "Invalid ERROR_LIMIT");
		return;
	}
	if (parser->current_obj->u.input.error_limit > 0)
	{
		mapred_parse_error(parser, "Duplicate ERROR_LIMIT");
		return;
	}

	/*
	 * The parse has already assured that the value consists of a sequence
	 * of digits, so strtol should convert successfully.
	 */
	parser->current_obj->u.input.error_limit = (int) strtol(value, NULL, 10);
}


void parser_set_mode(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	switch (parser->current_obj->kind)
	{
		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:
		{
			mapred_mode_t mode;

			/* Convert input string into a valid mode */
			if (value && !strcasecmp(value, "single"))
				mode = MAPRED_MODE_SINGLE;
			else if (value && !strcasecmp(value, "multi"))
				mode = MAPRED_MODE_MULTI;
			else
				mode = MAPRED_MODE_INVALID;

			/* Only MAP and FINALIZE support MULTI mode */
			if (mode == MAPRED_MODE_MULTI &&
				parser->current_obj->kind != MAPRED_MAPPER &&
				parser->current_obj->kind != MAPRED_FINALIZER)
			{
				mode = MAPRED_MODE_INVALID;
			}

			/* Error for invalid or duplicate modes */
			if (mode == MAPRED_MODE_INVALID)
			{
				mapred_parse_error(parser, "Invalid MODE");
			}
			if (parser->current_obj->u.function.mode != MAPRED_MODE_NONE)
			{
				mode = MAPRED_MODE_INVALID;
				mapred_parse_error(parser, "Duplicate MODE");
			}

			/* Set mode and return */
			parser->current_obj->u.function.mode = mode;
			return;
		}

		case MAPRED_OUTPUT:
		{
			mapred_output_mode_t mode;

			/* Convert input string into a valid mode */
			if (value && !strcasecmp(value, "replace"))
				mode = MAPRED_OUTPUT_MODE_REPLACE;
			else if (value && !strcasecmp(value, "append"))
				mode = MAPRED_OUTPUT_MODE_APPEND;
			else
				mode = MAPRED_OUTPUT_MODE_INVALID;

			/* Error for invalid or duplicate modes */
			if (mode == MAPRED_OUTPUT_MODE_INVALID)
			{
				mapred_parse_error(parser, "Invalid MODE");
			}
			if (parser->current_obj->u.output.mode != MAPRED_OUTPUT_MODE_NONE)
			{
				mode = MAPRED_OUTPUT_MODE_INVALID;
				mapred_parse_error(parser, "Duplicate MODE");
			}

			/* Set mode and return */
			parser->current_obj->u.output.mode = mode;
			return;
		}

		default:
			XASSERT(false);  /* ONLY functions and OUTPUTS have modes */
	}
}

void parser_set_file(mapred_parser_t *parser, char *value)
{
	/*
	 * Only applies to OUTPUTS which have a single file.
	 * INPUTS use parser_begin_files, parser_add_file ...
	 */
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_OUTPUT);

	switch (parser->current_obj->u.output.type)
	{
		case MAPRED_OUTPUT_NONE:
			parser->current_obj->u.output.type = MAPRED_OUTPUT_FILE;
			if (!value || strlen(value) == 0)
			{
				mapred_parse_error(parser, "Invalid FILE");
				return;
			}
			parser->current_obj->u.output.desc = copyscalar(value);
			break;

		case MAPRED_OUTPUT_FILE:
		{
			mapred_parse_error(parser, "Duplicate FILE");
			return;
		}

		case MAPRED_OUTPUT_TABLE:
		{
			mapred_parse_error(parser, "TABLE is incompatible with FILE");
			return;
		}
			
		default:
			XASSERT(false);
	}
}

void parser_set_transition(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	if (parser->current_obj->u.reducer.transition.name)
	{
		mapred_parse_error(parser, "Duplicate TRANSITION for REDUCE");
		return;
	}
	parser->current_obj->u.reducer.transition.name = copyscalar(value);
}

void parser_set_combiner(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	if (parser->current_obj->u.reducer.combiner.name)
	{
		mapred_parse_error(parser, "Duplicate CONSOLIDATE for REDUCE");
		return;
	}
	parser->current_obj->u.reducer.combiner.name = copyscalar(value);
}

void parser_set_finalizer(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	if (parser->current_obj->u.reducer.finalizer.name)
	{
		mapred_parse_error(parser, "Duplicate FINALIZE for REDUCE");
		return;
	}
	parser->current_obj->u.reducer.finalizer.name = copyscalar(value);
}

void parser_set_initialize(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	if (parser->current_obj->u.reducer.initialize)
	{
		mapred_parse_error(parser, "Duplicate INITIALIZE for REDUCE");
		return;
	}
	parser->current_obj->u.reducer.initialize = copyscalar(value);
}


void parser_set_language(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_MAPPER     ||
			parser->current_obj->kind == MAPRED_TRANSITION ||
			parser->current_obj->kind == MAPRED_COMBINER   ||
			parser->current_obj->kind == MAPRED_FINALIZER);
			
	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid LANGUAGE");
	}
	if (parser->current_obj->u.function.language)
	{
		mapred_parse_error(parser, "Duplicate LANGUAGE");
		return;
	}
	parser->current_obj->u.function.language = copyscalar(value);
}

void parser_set_function(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_MAPPER     ||
			parser->current_obj->kind == MAPRED_TRANSITION ||
			parser->current_obj->kind == MAPRED_COMBINER   ||
			parser->current_obj->kind == MAPRED_FINALIZER);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid FUNCTION");
	}
	if (parser->current_obj->u.function.body)
	{
		mapred_parse_error(parser, "Duplicate FUNCTION");
		return;
	}
	parser->current_obj->u.function.body = copyscalar(value);


	/*
	 * The "start_mark" of function body has a line number, but what that line
	 * number refers to is a bit finicky depending on the nature of the YAML.
	 * So we take it and adjust it accordingly.
	 */
	parser->current_obj->u.function.lineno = parser->event.start_mark.line;
	switch (parser->event.data.scalar.style)
	{
		case YAML_LITERAL_SCALAR_STYLE:
		case YAML_FOLDED_SCALAR_STYLE:
			parser->current_obj->u.function.lineno += 2;
			break;

		case YAML_PLAIN_SCALAR_STYLE:
		case YAML_SINGLE_QUOTED_SCALAR_STYLE:
		case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
			parser->current_obj->u.function.lineno += 1;
			break;			
		default:
			break;
	}
}

/*
 * parser_set_library was added to support the "LIBRARY" option in mapreduce
 * yaml schema version 1.0.0.2.  This is used by C language functions to
 * specify which code library the C function is defined in.
 *
 * - MAP:
 *     ...
 *     LIBRARY:  $libdir/libfoo
 *     FUNCTION: myFunc
 */
void parser_set_library(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_MAPPER     ||
			parser->current_obj->kind == MAPRED_TRANSITION ||
			parser->current_obj->kind == MAPRED_COMBINER   ||
			parser->current_obj->kind == MAPRED_FINALIZER);

	if (!value || strlen(value) == 0)
	{
		value = "";
		mapred_parse_error(parser, "Invalid LIBRARY");
	}
	if (parser->current_obj->u.function.library)
	{
		mapred_parse_error(parser, "Duplicate LIBRARY");
		return;
	}
	parser->current_obj->u.function.library = copyscalar(value);

	/*
	 * We will validate that the document version is >= 1.0.0.2
	 * durring object verification.
	 */
}

void parser_set_optimize(mapred_parser_t *parser, char *value)
{
   /* FIXME */
}


void parser_set_source(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	switch (parser->current_obj->kind)
	{
		case MAPRED_TASK:
			if (parser->current_obj->u.task.input.name)
			{
				mapred_parse_error(parser, "Duplicate SOURCE for TASK");
				return;
			}
			parser->current_obj->u.task.input.name = copyscalar(value);
			break;

		case MAPRED_EXECUTION:
			if (parser->current_obj->u.task.input.name)
			{
				mapred_parse_error(parser, "Duplicate SOURCE for RUN");
				return;
			}
			parser->current_obj->u.task.input.name = copyscalar(value);
			break;

		default:
			XASSERT(false);
	}
}

void parser_set_target(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_EXECUTION);

	if (parser->current_obj->u.task.output.name)
	{
		mapred_parse_error(parser, "Duplicate TARGET for RUN");
		return;
	}
	parser->current_obj->u.task.output.name = copyscalar(value);
}

void parser_set_mapper(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	switch (parser->current_obj->kind)
	{
		case MAPRED_TASK:
			if (parser->current_obj->u.task.mapper.name)
			{
				mapred_parse_error(parser, "Duplicate MAP for TASK");
				return;
			}
			parser->current_obj->u.task.mapper.name = copyscalar(value);
			break;

		case MAPRED_EXECUTION:
			if (parser->current_obj->u.task.mapper.name)
			{
				mapred_parse_error(parser, "Duplicate MAP for RUN");
				return;
			}
			parser->current_obj->u.task.mapper.name = copyscalar(value);
			break;

		default:
			XASSERT(false);
	}
}

void parser_set_reducer(mapred_parser_t *parser, char *value)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);

	switch (parser->current_obj->kind)
	{
		case MAPRED_TASK:
			if (parser->current_obj->u.task.reducer.name)
			{
				mapred_parse_error(parser, "Duplicate REDUCE for TASK");
				return;
			}
			parser->current_obj->u.task.reducer.name = copyscalar(value);
			break;

		case MAPRED_EXECUTION:
			if (parser->current_obj->u.task.reducer.name)
			{
				mapred_parse_error(parser, "Duplicate REDUCE for RUN");
				return;
			}
			parser->current_obj->u.task.reducer.name = copyscalar(value);
			break;

		default:
			XASSERT(false);
	}
}

void parser_begin_ordering(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	/*
	 * We will validate that the document version is >= 1.0.0.3
	 * durring object verification.
	 */
	if (parser->current_obj->u.reducer.ordering)
	{
		mapred_parse_error(parser, "Duplicate ORDERING for REDUCER");
		return;
	}
}

void parser_add_ordering(mapred_parser_t *parser, char *value)
{
	mapred_clist_t *newitem;
	mapred_clist_t *clist;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	/*
	 * Validate ordering:
	 *   In general ordering can be an arbitrary expression so it is
	 *   difficult to verify easily.  If we need more verification it
	 *   makes sense to push that verification into the grammar.
	 */
	if (!value || strlen(value) == 0)
	{
		mapred_parse_error(parser, "Invalid ORDERING");
		return;
	}

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_clist_t));
	newitem->value = copyscalar(value);
	newitem->next = (mapred_clist_t *) NULL;

	/* Add the new item into the last slot of the list */
	clist = parser->current_obj->u.reducer.ordering;
	if (clist == NULL)
		parser->current_obj->u.reducer.ordering = newitem;
	else
	{
		while (clist && clist->next)
			clist = clist->next;
		clist->next = newitem;
	}
}


/* List functions */
void parser_begin_files(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (parser->current_obj->u.input.type != MAPRED_INPUT_NONE)
	{
		if (parser->current_obj->u.input.type == MAPRED_INPUT_FILE)
		{
			mapred_parse_error(parser, "Duplicate FILE for INPUT");
			return;
		}
		else
		{
			mapred_parse_error(parser, "INPUT may only specify one of "
							   "FILE, GPFDIST, TABLE, QUERY, EXEC");
			return;
		}
	}

	/* files will be added individually */
	parser->current_obj->u.input.type = MAPRED_INPUT_FILE;
}

void parser_begin_gpfdist(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	if (parser->current_obj->u.input.type != MAPRED_INPUT_NONE)
	{
		if (parser->current_obj->u.input.type == MAPRED_INPUT_GPFDIST)
		{
			mapred_parse_error(parser, "Duplicate GPFDIST for INPUT");
			return;
		}
		else
		{
			mapred_parse_error(parser, "INPUT may only specify one of "
							   "FILE, GPFDIST, TABLE, QUERY, EXEC");
			return;
		}
	}
	parser->current_obj->u.input.type = MAPRED_INPUT_GPFDIST;
}

void parser_begin_columns(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);
	if (parser->current_obj->u.input.columns)
	{
		mapred_parse_error(parser, "Duplicate COLUMNS for INPUT");
		return;
	}
}

void parser_begin_parameters(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	switch (parser->current_obj->kind)
	{
		case MAPRED_MAPPER:
			if (parser->current_obj->u.function.parameters)
			{
				mapred_parse_error(parser, "Duplicate PARAMETERS for MAP");
				return;
			}
			break;

		case MAPRED_TRANSITION:
			if (parser->current_obj->u.function.parameters)
			{
				mapred_parse_error(parser, "Duplicate PARAMETERS for TRANSITION");
				return;
			}
			break;

		case MAPRED_COMBINER:
			if (parser->current_obj->u.function.parameters)
			{
				mapred_parse_error(parser, "Duplicate PARAMETERS for CONSOLIDATE");
				return;
			}
			break;

		case MAPRED_FINALIZER:
			if (parser->current_obj->u.function.parameters)
			{
				mapred_parse_error(parser, "Duplicate PARAMETERS for FINALIZE");
				return;
			}
			break;

		default:
			XASSERT(false);
	}
}

void parser_begin_returns(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	switch (parser->current_obj->kind)
	{
		case MAPRED_MAPPER:
			if (parser->current_obj->u.function.returns)
			{
				mapred_parse_error(parser, "Duplicate RETURNS for MAP");
				return;
			}
			break;

		case MAPRED_TRANSITION:
			if (parser->current_obj->u.function.returns)
			{
				mapred_parse_error(parser, "Duplicate RETURNS for TRANSITION");
				return;
			}
			break;

		case MAPRED_COMBINER:
			if (parser->current_obj->u.function.returns)
			{
				mapred_parse_error(parser, "Duplicate RETURNS for CONSOLIDATE");
				return;
			}
			break;

		case MAPRED_FINALIZER:
			if (parser->current_obj->u.function.returns)
			{
				mapred_parse_error(parser, "Duplicate RETURNS for FINALIZE");
				return;
			}
			break;

		default:
			XASSERT(false);
	}
}

void parser_begin_keys(mapred_parser_t *parser)
{
	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);
	if (parser->current_obj->u.reducer.keys)
	{
		mapred_parse_error(parser, "Duplicate KEYS for REDUCER");
		return;
	}
}

void parser_add_file(mapred_parser_t *parser, char *value)
{
	mapred_clist_t *newitem;
	mapred_clist_t *clist;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);
	XASSERT(parser->current_obj->u.input.type == MAPRED_INPUT_FILE ||
			parser->current_obj->u.input.type == MAPRED_INPUT_GPFDIST);

	/* Verify the new file */
	if (!value || strlen(value) == 0)
	{
		switch (parser->current_obj->u.input.type)
		{
			case MAPRED_INPUT_FILE:
				mapred_parse_error(parser, "Invalid FILE");
				return;
			case MAPRED_INPUT_GPFDIST:
				mapred_parse_error(parser, "Invalid GPFDIST");
				return;
			default:
				XASSERT(false);
		}
	}
	/* Todo: improved regex checking on files */

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_clist_t));
	newitem->value = copyscalar(value);
	newitem->next  = (mapred_clist_t *) NULL;

	/* Add the new item into the last slot of the list */
	clist = parser->current_obj->u.input.files;
	while (clist && clist->next)
		clist = clist->next;
	if (clist)
		clist->next = newitem;
	else
		parser->current_obj->u.input.files = newitem;
}

void parser_add_column(mapred_parser_t *parser, char *value)
{
	mapred_plist_t *newitem;
	mapred_plist_t *plist;
	char           *name, *type, *tokenizer;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_INPUT);

	/*
	 * Verify the new column
	 * It should be in one of two forms:
	 *    1)   <name>
	 *    2)   <name> <datatype>
	 */
	if (!value || strlen(value) == 0)
	{
		mapred_parse_error(parser, "Invalid COLUMNS");
		return;
	}
	name = strtok_r(value, " \t\r", &tokenizer);
	type = strtok_r(NULL, " \t\r", &tokenizer);
	if (!type)
		type = "text";  /* type defaults to 'text' */
	
    /* double check that there's nothing else */
	if (strtok_r(NULL, " \t\r", &tokenizer))
	{
		mapred_parse_error(parser, "Invalid COLUMNS");
		return;
	}

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_plist_t));
	newitem->name = copyscalar(name);
	newitem->type = copyscalar(type);
	newitem->next  = (mapred_plist_t *) NULL;

	/* Add the new item into the last slot of the list */
	plist = parser->current_obj->u.input.columns;
	while (plist && plist->next)
		plist = plist->next;
	if (plist)
		plist->next = newitem;
	else
		parser->current_obj->u.input.columns = newitem;
}

void parser_add_parameter(mapred_parser_t *parser, char *value)
{
	mapred_plist_t *newitem;
	mapred_plist_t *plist;
	char           *name, *type, *tokenizer;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_MAPPER     ||
			parser->current_obj->kind == MAPRED_TRANSITION ||
			parser->current_obj->kind == MAPRED_COMBINER   ||
			parser->current_obj->kind == MAPRED_FINALIZER);

	/*
	 * Verify the new parameter
	 * It should be in one of two forms:
	 *    1)   <name>
	 *    2)   <name> <datatype>
	 */
	if (!value || strlen(value) == 0)
	{
		mapred_parse_error(parser, "Invalid PARAMETERS");
		return;
	}
	name = strtok_r(value, " \t\r", &tokenizer);
	type = strtok_r(NULL, " \t\r", &tokenizer);
	if (!type)
		type = "text";  /* type defaults to 'text' */
	
    /* double check that there's nothing else */
	if (strtok_r(NULL, " \t\r", &tokenizer))
	{
		mapred_parse_error(parser, "Invalid PARAMETERS");
		return;
	}

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_plist_t));
	newitem->name = copyscalar(name);
	newitem->type = copyscalar(type);
	newitem->next  = (mapred_plist_t *) NULL;

	/* Add the new item into the last slot of the list */
	plist = parser->current_obj->u.function.parameters;
	while (plist && plist->next)
		plist = plist->next;
	if (plist)
		plist->next = newitem;
	else
		parser->current_obj->u.function.parameters = newitem;
}

void parser_add_return(mapred_parser_t *parser, char *value)
{
	mapred_plist_t *newitem;
	mapred_plist_t *plist;
	char           *name, *type, *tokenizer;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_MAPPER     ||
			parser->current_obj->kind == MAPRED_TRANSITION ||
			parser->current_obj->kind == MAPRED_COMBINER   ||
			parser->current_obj->kind == MAPRED_FINALIZER);

	/*
	 * Verify the new return
	 * It should be in one of two forms:
	 *    1)   <name>
	 *    2)   <name> <datatype>
	 */
	if (!value || strlen(value) == 0)
	{
		mapred_parse_error(parser, "Invalid RETURNS");
		return;
	}
	name = strtok_r(value, " \t\r", &tokenizer);
	type = strtok_r(NULL, " \t\r", &tokenizer);
	if (!type)
		type = "text";  /* type defaults to 'text' */
	
    /* double check that there's nothing else */
	if (strtok_r(NULL, " \t\r", &tokenizer))
	{
		mapred_parse_error(parser, "Invalid RETURNS");
		return;
	}

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_plist_t));
	newitem->name = copyscalar(name);
	newitem->type = copyscalar(type);
	newitem->next  = (mapred_plist_t *) NULL;

	/* Add the new item into the last slot of the list */
	plist = parser->current_obj->u.function.returns;
	while (plist && plist->next)
		plist = plist->next;
	if (plist)
		plist->next = newitem;
	else
		parser->current_obj->u.function.returns = newitem;
}


void parser_add_key(mapred_parser_t *parser, char *value)
{
	mapred_clist_t *newitem;
	mapred_clist_t *clist;

	XASSERT(parser->current_doc);
	XASSERT(parser->current_obj);
	XASSERT(parser->current_obj->kind == MAPRED_REDUCER);

	/* Validate key */
	if (!value || strlen(value) == 0)
	{
		mapred_parse_error(parser, "Invalid KEYS");
		return;
	}

	/* Allocate the new list item */
	newitem = malloc(sizeof(mapred_clist_t));
	newitem->value = copyscalar(value);
	newitem->next  = (mapred_clist_t *) NULL;

	/* Add the new item into the last slot of the list */
	clist = parser->current_obj->u.reducer.keys;
	while (clist && clist->next)
		clist = clist->next;
	if (clist)
		clist->next = newitem;
	else
		parser->current_obj->u.reducer.keys = newitem;
}



/*
 * mapred_dump_yaml - Given an object, dump it's YAML representation.
 *   This is the inverse of parsing
 *
 *   (*) Could be re-written to avoid code duplication issues.
 */
void mapred_dump_yaml(mapred_object_t *obj)
{
	char *ckind = NULL;

	if (!obj)
		return;

	switch (obj->kind)
	{
		case MAPRED_DOCUMENT:
			printf("---\n");
			/* Dumping the current version */
			printf("VERSION:          1.0.0.3\n");
			if (obj->u.document.database)
				printf("DATABASE:         %s\n", obj->u.document.database);
			if (obj->u.document.user)
				printf("USER:             %s\n", obj->u.document.user);
			if (obj->u.document.host)
				printf("HOST:             %s\n", obj->u.document.host);
			if (obj->u.document.port > 0)
				printf("PORT:             %d\n", obj->u.document.port);
			if (obj->u.document.flags & mapred_document_defines)
			{
				mapred_olist_t *sub;
				printf("DEFINE:\n");
				for (sub = obj->u.document.objects; sub; sub = sub->next)
					if (sub->object->kind != MAPRED_EXECUTION)
						mapred_dump_yaml(sub->object);
			}
			if (obj->u.document.flags & mapred_document_executes)
			{
				mapred_olist_t *sub;
				printf("EXECUTE:\n");
				for (sub = obj->u.document.objects; sub; sub = sub->next)
					if (sub->object->kind == MAPRED_EXECUTION)
						mapred_dump_yaml(sub->object);
			}
			break;

		case MAPRED_INPUT:
			printf("  - INPUT:\n");
			if (obj->name)
				printf("      NAME:       %s\n", obj->name);
			if (obj->u.input.columns)
			{
				mapred_plist_t *plist;
				printf("      COLUMNS:\n");
				for (plist = obj->u.input.columns; plist; plist = plist->next)
					printf("        - %s %s\n", plist->name, plist->type);
			}
			if (obj->u.input.delimiter)
				printf("      DELIMITER:  %s\n", obj->u.input.delimiter);
			if (obj->u.input.encoding)
				printf("      ENCODING:   %s\n", obj->u.input.encoding);
			switch (obj->u.input.format)
			{
				case MAPRED_FORMAT_NONE:
					break;
				case MAPRED_FORMAT_TEXT:
					printf("      FORMAT:     TEXT\n");
					break;
				case MAPRED_FORMAT_CSV:
					printf("      FORMAT:     CSV\n");
					break;
				default:
					XASSERT(false);
			}
			switch (obj->u.input.type)
			{
				case MAPRED_INPUT_NONE:
					break;
				case MAPRED_INPUT_FILE:
				{
					mapred_clist_t *clist;
					printf("      FILE:\n");
					for (clist = obj->u.input.files; clist; clist = clist->next)
						printf("        - %s\n", clist->value);
					break;
				}
				case MAPRED_INPUT_GPFDIST:
					printf("      GPFDIST:    %s\n", obj->u.input.desc);
					break;
				case MAPRED_INPUT_TABLE:
					printf("      TABLE:      %s\n", obj->u.input.desc);
					break;
				case MAPRED_INPUT_QUERY:
					printf("      QUERY: |\n");
					printf("         %s\n", obj->u.input.desc);
					break;
				case MAPRED_INPUT_EXEC:
					printf("      EXEC:       %s\n", obj->u.input.desc);
					break;
				default:
					XASSERT(false);
			}
			break;

		case MAPRED_OUTPUT:
			printf("  - OUTPUT:\n");
			if (obj->name)
				printf("      NAME:       %s\n", obj->name);
			switch (obj->u.output.mode)
			{
				case MAPRED_OUTPUT_MODE_NONE:
					break;
				case MAPRED_OUTPUT_MODE_REPLACE:
					printf("      MODE:       REPLACE\n");
					break;
				case MAPRED_OUTPUT_MODE_APPEND:
					printf("      MODE:       APPEND\n");
					break;
				default:
					XASSERT(false);			
			}
			switch (obj->u.output.type)
			{
				case MAPRED_OUTPUT_NONE:
					break;
				case MAPRED_OUTPUT_FILE:
					printf("      FILE:       %s\n", obj->u.output.desc);
					break;
				case MAPRED_OUTPUT_TABLE:
					printf("      TABLE:      %s\n", obj->u.output.desc);
					break;
				default:
					XASSERT(false);
			}
			break;

		case MAPRED_MAPPER:
			ckind = "MAP";
			/* fallthrough */

		case MAPRED_TRANSITION:
			if (!ckind)
				ckind = "TRANSITION";
			/* fallthrough */

		case MAPRED_COMBINER:
			if (!ckind)
				ckind = "CONSOLIDATE";
			/* fallthrough */

		case MAPRED_FINALIZER:
			if (!ckind)
				ckind = "FINALIZE";

			printf("  - %s:\n", ckind);
			if (obj->name)
				printf("      NAME:       %s\n", obj->name);
			if (obj->u.function.parameters)
			{
				mapred_plist_t *plist;
				printf("      PARAMETERS:\n");
				for (plist = obj->u.function.parameters; plist;
					 plist = plist->next)
					printf("        - %s %s\n", plist->name, plist->type);
			}
			if (obj->u.function.returns)
			{
				mapred_plist_t *plist;
				printf("      RETURNS:\n");
				for (plist = obj->u.function.returns; plist;
					 plist = plist->next)
					printf("        - %s %s\n", plist->name, plist->type);
			}
			switch (obj->u.function.mode)
			{
				case MAPRED_MODE_NONE:
					break;
				case MAPRED_MODE_SINGLE:
					printf("      MODE:       SINGLE\n");
					break;
				case MAPRED_MODE_MULTI:
					printf("      MODE:       MULTI\n");
					break;
				case MAPRED_MODE_ACCUMULATED:
					printf("      MODE:       ACCUMULATED\n");
					break;
				case MAPRED_MODE_WINDOWED:
					printf("      MODE:       WINDOWED\n");
					break;
				default:
					printf("      MODE:       UNKNOWN\n");
					break;
			}
			if (obj->u.function.flags)
			{
				printf("      OPTIMIZE:   ");
				if (obj->u.function.flags & mapred_function_strict)
					printf("STRICT ");
				if (obj->u.function.flags & mapred_function_immutable)
					printf("IMMUTABLE ");
				if (obj->u.function.flags & mapred_function_unordered)
					printf("UNORDERED ");
				printf("\n");
			}
			if (obj->u.function.language)
				printf("      LANGUAGE:   %s\n", obj->u.function.language);
			if (obj->u.function.body)
			{
				printf("      FUNCTION: |\n");
				printf("         %s\n", obj->u.function.body);
			}
			break;

		case MAPRED_REDUCER:
			printf("  - REDUCE:\n");
			if (obj->name)
				printf("      NAME:       %s\n", obj->name);
			if (obj->u.reducer.transition.name)
				printf("      TRANSITION: %s\n",
					   obj->u.reducer.transition.name);
			if (obj->u.reducer.combiner.name)
				printf("      CONSOLIDATE:   %s\n",
					   obj->u.reducer.combiner.name);
			if (obj->u.reducer.finalizer.name)
				printf("      FINALIZE:  %s\n",
					   obj->u.reducer.finalizer.name);
			if (obj->u.reducer.initialize)
				printf("      INITIALIZE: %s\n",
					   obj->u.reducer.initialize);
			if (obj->u.reducer.keys)
			{
				mapred_clist_t *clist;
				printf("      KEYS: |\n");
				for (clist = obj->u.reducer.keys; clist; clist = clist->next)
					printf("        - %s\n", clist->value);
			}
			if (obj->u.reducer.ordering)
			{
				mapred_clist_t *clist;
				printf("      ORDERING: |\n");
				for (clist = obj->u.reducer.ordering; clist; clist = clist->next)
					printf("        - %s\n", clist->value);
			}
			break;

		case MAPRED_TASK:
		case MAPRED_EXECUTION:
			if (obj->u.task.execute)
				printf("  - RUN:\n");
			else
				printf("  - TASK:\n");
			if (obj->name)
				printf("      NAME:       %s\n", obj->name);
			if (obj->u.task.input.name)
				printf("      SOURCE:     %s\n", obj->u.task.input.name);
			if (obj->u.task.mapper.name)
				printf("      MAP:     %s\n", obj->u.task.mapper.name);
			if (obj->u.task.reducer.name)
				printf("      REDUCE:    %s\n", obj->u.task.reducer.name);
			if (obj->u.task.output.name)
				printf("      TARGET:     %s\n", obj->u.task.output.name);
			break;

		case MAPRED_NO_KIND:
		default:
			XRAISE(MAPRED_PARSE_INTERNAL,
				   "Unknown object type");
	}
}


	
int mapred_verify_object(mapred_parser_t *parser, mapred_object_t *obj)
{
	char *name;
	int error = NO_ERROR;

	XASSERT(obj);

	/* Verify that all required fields are present and valid */
	name = obj->name ? obj->name : "unnamed";
	switch (obj->kind)
	{
		case MAPRED_DOCUMENT:
			
			/*
			 * If there is a version on the document then it should have
			 * been validated by parser_set_version()
			 */
			if (!obj->u.document.version)
			{
				error = mapred_obj_error(obj, "Missing VERSION",
										 parser->doc_number);
			}

			break;

		case MAPRED_INPUT:

			/* Validate required fields */
			if (!obj->name)
				error = mapred_obj_error(obj, "Missing NAME");
			if (obj->u.input.type == MAPRED_INPUT_NONE)
				error = mapred_obj_error(obj,
						  "Missing FILE, GPFDIST, TABLE, QUERY, or EXEC");

			/* set default values */
			if (error == NO_ERROR)
			{
				if (!obj->u.input.columns)
				{
					obj->u.input.columns = malloc(sizeof(mapred_plist_t));
					obj->u.input.columns->name = copyscalar("value");
					obj->u.input.columns->type = copyscalar("text");
					obj->u.input.columns->next = NULL;
				}
				if (!obj->u.input.columns->next &&
					!obj->u.input.delimiter)
				{
					obj->u.input.delimiter = copyscalar("off");
				}
			}
			break;

		case MAPRED_OUTPUT:

			if (!obj->name)
				error = mapred_obj_error(obj, "Missing NAME");
			if (obj->u.output.type == MAPRED_OUTPUT_NONE)
				error = mapred_obj_error(obj, "Missing FILE or TABLE");
			break;

		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:

			if (!obj->name)
				error = mapred_obj_error(obj, "Missing NAME");

			/*
			 * We now support "builtin" functions, which are specified by a lack
			 * of an implementation language.  If a language is specified then
			 * a function body is still required.  If a language is not specified
			 * then the function body just defaults to the name of the function.
			 */
			if (obj->name && !obj->u.function.language && !obj->u.function.body)
				obj->u.function.body = copyscalar(obj->name);

			if (obj->u.function.language && !obj->u.function.body)
				error = mapred_obj_error(obj, "Missing FUNCTION");

			/*
			 * LIBRARY is required for "C" language functions.
			 * LIBRARY is invalid for any other language.
			 *
			 * It would be good to verify that LIBRARY is not used in
			 * older YAML formats, but that is difficult given the current
			 * structure of the code.
			 */
			if (obj->u.function.language)
			{
				if (obj->u.function.library)
				{
					if (strcasecmp("C", obj->u.function.language))
					{
						error = mapred_obj_error(obj, "LIBRARY is invalid for "
												 "%s LANGUAGE functions",
												 obj->u.function.language);
					}
				}
				else if (!strcasecmp("C", obj->u.function.language))
				{
					error = mapred_obj_error(obj, "Missing LIBRARY");
				}

				/*
				 * Don't bother filling in default arguments if we already have
				 * an error.
				 */
				if (error)
					break;

				/*
				 * Set default values.
				 *   For builtin functions we delay this so that we can lookup the
				 *   function in the catalog to determine the defaults.
				 */
				if (!obj->u.function.parameters)
				{
					const char *name = default_parameter_names[obj->kind][0];
					name = default_parameter_names[obj->kind][0];
					obj->u.function.parameters = malloc(sizeof(mapred_plist_t));
					obj->u.function.parameters->type = copyscalar("text");
					obj->u.function.parameters->name = copyscalar(name);
					obj->u.function.parameters->next = NULL;

					name = default_parameter_names[obj->kind][1];
					if (name)
					{
						obj->u.function.parameters->next = malloc(sizeof(mapred_plist_t));
						obj->u.function.parameters->next->type = copyscalar("text");
						obj->u.function.parameters->next->name = copyscalar(name);
						obj->u.function.parameters->next->next = NULL;						
					}
				}
				else
				{
					switch (obj->kind)
					{
						case MAPRED_TRANSITION:
							if (!obj->u.function.parameters->next)
							{
								error = mapred_obj_error(
									obj,
									"requires at least 2 input parameters [state, arg1, ...]"
									);
							}
							break;

						case MAPRED_COMBINER:
							if (!obj->u.function.parameters->next ||
								obj->u.function.parameters->next->next)
							{
								error = mapred_obj_error(
									obj,
									"requires exactly 2 input parameters [state1, state2]"
									);
							}
							break;

						case MAPRED_FINALIZER:
							if (obj->u.function.parameters->next)
							{
								error = mapred_obj_error(
									obj,
									"requires exactly 1 input parameter [state]"
									);
							}
							break;

						case MAPRED_MAPPER:
						default:
							break;
					}
				}

				if (!obj->u.function.returns)
				{
					const char *name = default_return_names[obj->kind][0];
					obj->u.function.returns = malloc(sizeof(mapred_plist_t));
					obj->u.function.returns->type = copyscalar("text");
					obj->u.function.returns->name = copyscalar(name);
					obj->u.function.returns->next = NULL;

					name = default_return_names[obj->kind][1];
					if (name)
					{
						obj->u.function.returns->next = malloc(sizeof(mapred_plist_t));
						obj->u.function.returns->next->type = copyscalar("text");
						obj->u.function.returns->next->name = copyscalar(name);
						obj->u.function.returns->next->next = NULL;						
					}
				}
				else if (obj->kind == MAPRED_TRANSITION ||
						 obj->kind == MAPRED_COMBINER)
				{
					if (obj->u.function.returns->next)
					{
						error = mapred_obj_error(
							obj,
							"requires exactly one output parameter [state]"
							);
					}
				}

				/* Set default mode: depends on type of function */
				if (obj->u.function.mode == MAPRED_MODE_NONE)
				{
					if (obj->kind == MAPRED_TRANSITION ||
						obj->kind == MAPRED_COMBINER)
					{
						obj->u.function.mode = MAPRED_MODE_SINGLE;
					}
					else
					{
						obj->u.function.mode = MAPRED_MODE_MULTI;
					}
				}
			}
			break;

		case MAPRED_REDUCER:

			if (!obj->name)
				error = mapred_obj_error(obj, "Missing NAME");
			if (!obj->u.reducer.transition.name)
				error = mapred_obj_error(obj, "Missing TRANSITION");
			/*
			 * Will verify that functions are valid for reducer input after we
			 * have resolved the pointers.
			 */

			/*
			 * It would be good to verify that ORDERING is not used in
			 * older YAML formats, but that is difficult given the current
			 * structure of the code.
			 */

			/*
			 * ORDERING and COMBINER are incompatible
			 */
			if (obj->u.reducer.ordering != NULL &&
				obj->u.reducer.combiner.name)
			{
				error = mapred_obj_error(obj,
										 "REDUCERS cannot specify both a COMBINER "
										 "function and an ORDERING specification");
			}

			/* Setup default "keys" */
			if (!obj->u.reducer.keys)
			{
				obj->u.reducer.keys = malloc(sizeof(mapred_clist_t));
				obj->u.reducer.keys->value = copyscalar("key");
				obj->u.reducer.keys->next = malloc(sizeof(mapred_clist_t));				
				obj->u.reducer.keys->next->next = NULL;
				obj->u.reducer.keys->next->value = copyscalar("*");
			}

			break;

		case MAPRED_TASK:
			if (!obj->name)
				error = mapred_obj_error(obj, "Missing NAME");

			/* Fallthrough */

		case MAPRED_EXECUTION:
			
			if (!obj->u.task.input.name)
				error = mapred_obj_error(obj, "Missing SOURCE");
			
			/* IDENTITY Mappers and Reducers */
			if (obj->u.task.mapper.name &&
				!strcasecmp("IDENTITY", obj->u.task.mapper.name))
			{
				free(obj->u.task.mapper.name);
				obj->u.task.mapper.name = NULL;
			}
			if (obj->u.task.reducer.name &&
				!strcasecmp("IDENTITY", obj->u.task.reducer.name))
			{
				free(obj->u.task.reducer.name);
				obj->u.task.reducer.name = NULL;
			}

			/* STDOUT Output */
			if (obj->u.task.output.name &&
				!strcasecmp("STDOUT", obj->u.task.output.name))
			{
				free(obj->u.task.output.name);
				obj->u.task.output.name = NULL;
			}
			break;

		case MAPRED_NO_KIND:
		default:
			XASSERT(false);
	}

	return error;
}



