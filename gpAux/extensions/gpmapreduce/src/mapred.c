#include <mapred.h>
#include <except.h>
#include <mapred_errors.h>

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>     /* for file "access" test */
#include <errno.h>

#define scalarfree(x)							\
	do {										\
		if (x) {								\
			free(x);							\
			x = NULL;							\
		}										\
	} while (0)


/* instantiate the extern from mapred.h */
const char *mapred_kind_name[MAPRED_MAXKIND+1] =
{
	"<NULL>",
	"DOCUMENT",
	"INPUT",
	"OUTPUT",
	"MAP",
	"TRANSITION",
	"CONSOLIDATE",
	"FINALIZE",
	"REDUCE",
	"TASK",
	"RUN",
	"INTERNAL"
};

/* instantiate default parameter names from mapred.h */
const char *default_parameter_names[MAPRED_MAXKIND+1][2] =
{
	{NULL, NULL},          /* MAPRED_NO_KIND */
	{NULL, NULL},          /* MAPRED_DOCUMENT */
	{NULL, NULL},          /* MAPRED_INPUT */
	{NULL, NULL},          /* MAPRED_OUTPUT */
	{"key", "value"},      /* MAPRED_MAPPER */
	{"state", "value"},    /* MAPRED_TRANSITION */
	{"state1", "state2"},  /* MAPRED_COMBINER */
	{"state", NULL},       /* MAPRED_FINALIZER */
	{NULL, NULL},          /* MAPRED_REDUCER */
	{NULL, NULL},          /* MAPRED_TASK */
	{NULL, NULL},          /* MAPRED_EXECUTION */
	{NULL, NULL}           /* MAPRED_ADT */
};

/* instantiate default parameter names from mapred.h */
const char *default_return_names[MAPRED_MAXKIND+1][2] =
{
	{NULL, NULL},          /* MAPRED_NO_KIND */
	{NULL, NULL},          /* MAPRED_DOCUMENT */
	{NULL, NULL},          /* MAPRED_INPUT */
	{NULL, NULL},          /* MAPRED_OUTPUT */
	{"key", "value"},      /* MAPRED_MAPPER */
	{"value", NULL},       /* MAPRED_TRANSITION */
	{"value", NULL},       /* MAPRED_COMBINER */
	{"value", NULL},       /* MAPRED_FINALIZER */
	{NULL, NULL},          /* MAPRED_REDUCER */
	{NULL, NULL},          /* MAPRED_TASK */
	{NULL, NULL},          /* MAPRED_EXECUTION */
	{NULL, NULL}           /* MAPRED_ADT */
};


/*
 * libpq Errors that we care about
 * (would be better to add <errcodes.h> to the include path)
 */
const char *IN_FAILED_SQL_TRANSACTION = "25P02";
const char *OBJ_DOES_NOT_EXIST        = "42P01";
const char *SCHEMA_DOES_NOT_EXIST     = "3F000";

const char *DISTRIBUTION_NOTICE = "NOTICE:  Table doesn't have 'DISTRIBUTED BY' clause";

/* local prototypes */
void *         mapred_malloc(int size);
void             mapred_free(void *ptr);

buffer_t *        makebuffer(size_t bufsize, size_t grow);
void                bufreset(buffer_t *b);
void                  bufcat(buffer_t **bufp, char* fmt);
void   ignore_notice_handler(void *arg, const PGresult *res);
void    print_notice_handler(void *arg, const PGresult *res);
void    mapred_setup_columns(PGconn *conn, mapred_object_t *obj);

boolean mapred_create_object(PGconn *conn, mapred_document_t *doc,
							 mapred_object_t *obj);
void    mapred_remove_object(PGconn *conn, mapred_document_t *doc,
							 mapred_object_t *obj);
void      mapred_run_queries(PGconn *conn, mapred_document_t *doc);

void mapred_resolve_dependencies(PGconn *conn, mapred_document_t *doc);
void mapred_resolve_ref(mapred_olist_t *olist, mapred_reference_t *ref);
void mapred_resolve_object(PGconn *conn, mapred_document_t *doc,
						   mapred_object_t *obj, int *exec_count);

void lookup_function_in_catalog(PGconn *conn, mapred_document_t *doc,
								mapred_object_t *obj);

/* Wrappers around malloc/free to handle error conditions more cleanly */
void *mapred_malloc(int size)
{
	void *m;
	XASSERT(size > 0);

#ifdef INTERNAL_BUILD
	if (global_debug_flag && global_verbose_flag)
		fprintf(stderr, "Allocating %d bytes: ", size);
#endif

	m = malloc(size);
	if (!m)
		XRAISE(MEMORY_ERROR, "Memory allocation failure");

#ifdef INTERNAL_BUILD
	if (global_debug_flag && global_verbose_flag)
		fprintf(stderr, "%p\n", m);
#endif

	return m;
}

#define copyscalar(s)							\
	strcpy(mapred_malloc(strlen(s)+1), s)

void mapred_free(void *ptr)
{
	XASSERT(ptr);

#ifdef INTERNAL_BUILD
	if (global_debug_flag && global_verbose_flag)
		fprintf(stderr, "Freeing memory: %p\n", ptr);
#endif

	free(ptr);
}

int mapred_obj_error(mapred_object_t *obj, char *fmt, ...)
{
	va_list arg;

	if (global_verbose_flag)
		fprintf(stderr, "    - ");

	fprintf(stderr, "Error: ");
	if (obj && obj->name)
		fprintf(stderr, "%s '%s': ", mapred_kind_name[obj->kind], obj->name);
	if (obj && !obj->name)
		fprintf(stderr, "%s: ", mapred_kind_name[obj->kind]);

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	if (obj->line > 0)
		fprintf(stderr, ", at line %d\n", obj->line);
	else
		fprintf(stderr, "\n");

	return MAPRED_PARSE_ERROR;
}

static void mapred_obj_debug(mapred_object_t *obj)
{
	mapred_plist_t *plist;

	if (!obj)
	{
		fprintf(stderr, "Object is NULL");
		return;
	}
	fprintf(stderr, "%s: \n", mapred_kind_name[obj->kind]);
	fprintf(stderr, "  NAME: '%s': \n", obj->name ? obj->name : "-");
	switch (obj->kind)
	{
		case MAPRED_NO_KIND:
		case MAPRED_DOCUMENT:
		case MAPRED_ADT:
		case MAPRED_INPUT:
		case MAPRED_OUTPUT:
		case MAPRED_TASK:
		case MAPRED_EXECUTION:
		case MAPRED_REDUCER:
		{
			fprintf(stderr, "  DEBUG: 'debug output not yet implemented'\n");
			break;
		}

		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:
		{
			fprintf(stderr, "  LANGUAGE: %s\n", obj->u.function.language ?
					obj->u.function.language : "-");
			fprintf(stderr, "  PARAMETERS: [");
			for (plist = obj->u.function.parameters; plist; plist = plist->next)
			{
				fprintf(stderr, "%s %s%s", plist->name, plist->type,
						plist->next ? ", " : "");
			}
			fprintf(stderr,"]\n");
			fprintf(stderr, "  RETURNS: [");
			for (plist = obj->u.function.returns; plist; plist = plist->next)
			{
				fprintf(stderr, "%s %s%s", plist->name, plist->type,
						plist->next ? ", " : "");
			}
			fprintf(stderr,"]\n");
			fprintf(stderr, "  LIBRARY: %s\n", obj->u.function.library ?
					obj->u.function.library : "-");
			fprintf(stderr, "  FUNCTION: %s\n", obj->u.function.body ?
					obj->u.function.body : "-");
			break;
		}
	}
}


/* -------------------------------------------------------------------------- */
/* Functions that play with buffers                                           */
/* -------------------------------------------------------------------------- */
buffer_t *makebuffer(size_t bufsize, size_t grow)
{
	buffer_t *b;

	XASSERT(bufsize > 0 && grow > 0);

	b = mapred_malloc(sizeof(buffer_t) + bufsize);
	b->buffer   = (char*)(b+1);
	b->bufsize  = bufsize;
	b->grow     = grow;
	b->position = 0;
	b->buffer[0] = '\0';
	return b;
}

/* to re-use a buffer just "reset" it */
void bufreset(buffer_t *b)
{
	XASSERT(b && b->bufsize > 0 && b->grow > 0);
	b->position = 0;
	b->buffer[0] = '\0';
}

/*
 * A simple wrapper around a strncpy that handles resizing an input buffer
 * when needed.
 */
void bufcat(buffer_t **bufp, char* str)
{
	buffer_t  *b;
	size_t     len;

	XASSERT(bufp && *bufp);
	XASSERT(str);

	b = *bufp;
	len = strlen(str);

	/* If the buffer is too small, grow it */
	if (b->bufsize <= b->position + len)
	{
		buffer_t *newbuf;

		/* use the minumum of "grow" and the new length for the grow amount */
		if (b->grow <= len)
			b->grow = len+1;

		newbuf = makebuffer(b->bufsize + b->grow, b->grow);
		memcpy(newbuf->buffer, b->buffer, b->position+1);
		newbuf->position = b->position;
		*bufp = newbuf;
		mapred_free(b);
		b = newbuf;
	}

	/* We are now guaranteed that we have enough space in the buffer */
	XASSERT( b->bufsize - b->position > len );
	strcpy(b->buffer+b->position, str);
	b->position += len;
	b->buffer[b->position] = '\0';
}


/*
 * Currently we just ignore all warnings, may eventually do something
 * smarter, but this is preferable to dumping them to libpq's default
 * of dumping them to stderr.
 */
void ignore_notice_handler(void *arg, const PGresult *res)
{
}

void print_notice_handler(void *arg, const PGresult *res)
{
	char *error = PQresultErrorMessage(res);
	if (!strncmp(error, DISTRIBUTION_NOTICE, strlen(DISTRIBUTION_NOTICE)-1))
		return;

	if (global_verbose_flag)
		fprintf(stderr, "   - ");
	fprintf(stderr, "%s", error);
}

/*
 * If a function is already defined in the database we need to be able to
 * lookup the function information directly from the catalog.  This is
 * fairly similar to func_get_detail in backend/parser/parse_func.c, but
 * the lookup from yaml is slightly different because we don't know the
 * context that the function is in, but we _might_ have been told some
 * of the parameter information already.
 */
void lookup_function_in_catalog(PGconn *conn, mapred_document_t *doc,
								mapred_object_t *obj)
{
	PGresult			*result	 = NULL;
	PGresult			*result2 = NULL;
	mapred_plist_t		*plist, *plist2;
	mapred_plist_t		*newitem = NULL;
	mapred_plist_t		*returns = NULL;
	buffer_t			*buffer	 = NULL;
	char				*tmp1	 = NULL;
	char				*tmp2	 = NULL;
	char				*tmp3	 = NULL;
	const int			STR_LEN  = 50;
	char				str[STR_LEN];
	int					i, nargs;

	XASSERT(doc);
	XASSERT(obj);
	XASSERT(obj->kind == MAPRED_MAPPER     ||
			obj->kind == MAPRED_TRANSITION ||
			obj->kind == MAPRED_COMBINER   ||
			obj->kind == MAPRED_FINALIZER);

	obj->internal = true;
	obj->u.function.internal_returns = NULL;

	XTRY
	{
		buffer = makebuffer(1024, 1024);

		/* Try to lookup the specified function */
		bufcat(&buffer,
			   "SELECT proretset, prorettype::regtype, pronargs,\n"
			   "       proargnames, proargmodes, \n"
			   "       (proargtypes::regtype[])[0:pronargs] as proargtypes,\n"
			   "       proallargtypes::regtype[],\n");

		/*
		 * If we have return types defined in the yaml then we want to resolve
		 * them to their authorative names for comparison purposes.
		 */
		if (obj->u.function.returns)
		{
			bufcat(&buffer, "       ARRAY[");
			for (plist = obj->u.function.returns; plist; plist = plist->next)
			{
				if (plist->type)
				{
					bufcat(&buffer, "'");
					bufcat(&buffer, plist->type);
					bufcat(&buffer, "'::regtype");
				}
				else
				{
					/* If we don't know the type, punt */
					bufcat(&buffer, "'-'::regtype");
				}
				if (plist->next)
					bufcat(&buffer, ", ");
			}
			bufcat(&buffer, "] as yaml_rettypes\n");
		}
		else
		{
			bufcat(&buffer, "       null::regtype[] as yaml_rettypes\n");
		}

		bufcat(&buffer,
			   "FROM   pg_proc\n"
			   "WHERE  not proisagg and not proiswindow\n"
			   "  AND  proname = lower('");
		bufcat(&buffer, obj->name);
		bufcat(&buffer, "')\n");

		/* Fill in the known parameter types */
		nargs = 0;
		if (obj->u.function.parameters)
		{
			bufcat(&buffer, "  AND  (proargtypes::regtype[])[0:pronargs] = ARRAY[");
			for (plist = obj->u.function.parameters; plist; plist = plist->next)
			{
				nargs++;
				bufcat(&buffer, "'");
				bufcat(&buffer, plist->type);
				bufcat(&buffer, "'::regtype");
				if (plist->next)
					bufcat(&buffer, ", ");
			}
			snprintf(str, STR_LEN, "]\n  AND pronargs=%d\n", nargs);
			bufcat(&buffer, str);
		}

		/* Run the SQL */
		if (global_print_flag || global_debug_flag)
			printf("%s", buffer->buffer);
		result = PQexec(conn, buffer->buffer);
		bufreset(buffer);

		if (PQresultStatus(result) != PGRES_TUPLES_OK)
		{
			/*
			 * The SQL statement failed:
			 * Most likely scenario is a bad datatype causing the regtype cast
			 * to fail.
			 */
			char *code  = PQresultErrorField(result, PG_DIAG_SQLSTATE);
			char *error = PQresultErrorMessage(result);

			printf("errcode=\"%s\"\n", code);  /* Todo: validate expected error code */

			mapred_obj_error(obj, "SQL Error resolving function: \n  %s",
							 error);
			XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
		}
		else if (PQntuples(result) == 0)
		{
			/* No such function */
			mapred_obj_error(obj, "No such function");
			XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
		}
		else if (PQntuples(result) > 1)
		{
			XASSERT(!obj->u.function.parameters);
			mapred_obj_error(obj, "Ambiguous function, supply a function "
							 "prototype for disambiguation");
			XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
		}
		else
		{
			char		*value;
			int          len;
			boolean		 retset;
			int          nargs;
			char		*argtypes	 = NULL;
			char		*argnames	 = "";
			char		*argmodes	 = "";
			char		*allargtypes = "";
			char		*rettype	 = NULL;
			char        *yaml_rettypes = "";
			char        *type, *typetokens = NULL;
			char        *name, *nametokens = NULL;
			char        *mode, *modetokens = NULL;
			boolean     name_end, mode_end;

			value = PQgetvalue(result, 0, 0);   /* Column 0: proretset */
			retset = (value[0] == 't');
			value = PQgetvalue(result, 0, 1);   /* Column 1: prorettype */
			rettype = value;
			value = PQgetvalue(result, 0, 2);   /* Column 2: pronargs */
			nargs = (int) strtol(value, (char **) NULL, 10);

			/*
			 * Arrays are formatted as:  "{value,value,...}"
			 * of which we only want "value,value, ..."
			 * so find the part of the string between the braces
			 */
			if (!PQgetisnull(result, 0, 3))   /* Column 3: proargnames */
			{
				value = PQgetvalue(result, 0, 3);
				argnames = value+1;
				len = strlen(argnames);
				if (len > 0)
					argnames[len-1] = '\0';
			}

			if (!PQgetisnull(result, 0, 4))   /* Column 4: proargmodes */
			{
				value = PQgetvalue(result, 0, 4);
				argmodes = value+1;
				len = strlen(argmodes);
				if (len > 0)
					argmodes[len-1] = '\0';
			}
			if (!PQgetisnull(result, 0, 5))   /* Column 5: proargtypes */
			{
				value = PQgetvalue(result, 0, 5);
				argtypes = value+1;
				len = strlen(argtypes);
				if (len > 0)
					argtypes[len-1] = '\0';
			}
			if (!PQgetisnull(result, 0, 6))   /* Column 6: proallargtypes */
			{
				value = PQgetvalue(result, 0, 6);
				allargtypes = value+1;
				len = strlen(allargtypes);
				if (len > 0)
					allargtypes[len-1] = '\0';
			}
			if (!PQgetisnull(result, 0, 7))   /* Column 7: yaml_rettypes */
			{
				value = PQgetvalue(result, 0, 7);
				yaml_rettypes = value+1;
				len = strlen(yaml_rettypes);
				if (len > 0)
					yaml_rettypes[len-1] = '\0';
			}

			/*
			 * These constraints should all be enforced in the catalog, so
			 * if something is wrong then it's a coding error above.
			 */
			XASSERT(rettype);
			XASSERT(argtypes);
			XASSERT(nargs >= 0);

			/*
			 * If we just derived the parameters from the catalog then we
			 * need complete our internal metadata.
			 */
			plist = NULL;
			if (!obj->u.function.parameters)
			{
				/* strtok is destructive and we need to preserve the original
				 * string, so we make some annoying copies prior to strtok.
				 */
				tmp1 = copyscalar(argtypes);
				tmp2 = copyscalar(argnames);
				tmp3 = copyscalar(argmodes);

				type = strtok_r(tmp1, ",", &typetokens);
				name = strtok_r(tmp2, ",", &nametokens);
				mode = strtok_r(tmp3, ",", &modetokens);

				/*
				 * Name and mode are used for IN/OUT parameters and may not be
				 * present.  In the event that they are we are looking for:
				 *   - the "i" (in) arguments
				 *   - the "b" (inout) arguments
				 * we skip over:
				 *   - the "o" (out) arguments.
				 *   - the "t" (table out) arguments.
				 *
				 * Further it is possible for some of the arguments to be named
				 * and others to be unnamed.  The unnamed arguments will show
				 * up as "" (two quotes, not an empty string) if there is an
				 * argnames defined.
				 *
				 * If argmodes is not defined then all names in proargnames
				 * refer to input arguments.
				 */

				while (mode && strcmp(mode, "i") && strcmp(mode, "b"))
				{
					name = strtok_r(NULL, ",", &nametokens);
					mode = strtok_r(NULL, ",", &modetokens);
				}
				name_end = (NULL == name);
				mode_end = (NULL == mode);

				i = 0;
				while (type)
				{
					/* Keep track of which parameter we are on */
					i++;
					XASSERT(i <= nargs);

					/*
					 * If a name was not specified by the user, and was not
					 * specified by the in/out parameters then we assign it a
					 * default name.
					 */
					if (!name)
					{
						/* single argument functions always default to "value" */
						if (i == 1 && nargs == 1)
							name = (char*) "value";

						/* Base name on default parameter names for the first
						 * two arguments */
						else if (i <= 2)
							name = (char*) default_parameter_names[obj->kind][i-1];

						/*
						 * If we still didn't decide on a name, make up
						 * something useless.
						 */
						if (!name)
						{
							snprintf(str, STR_LEN, "parameter%d", i);
							name = str;
						}
					}

					if (!plist)
					{
						plist = mapred_malloc(sizeof(mapred_plist_t));
						plist->name = copyscalar(name);
						plist->type = copyscalar(type);
						plist->next = (mapred_plist_t *) NULL;
						obj->u.function.parameters = plist;
					}
					else
					{
						plist->next = mapred_malloc(sizeof(mapred_plist_t));
						plist = plist->next;
						plist->name = copyscalar(name);
						plist->type = copyscalar(type);
						plist->next = (mapred_plist_t *) NULL;
					}

					/* Procede to the next parameter */
					type = strtok_r(NULL, ",", &typetokens);
					if (!name_end)
					{
						name = strtok_r(NULL, ",", &nametokens);
						name_end = (NULL == name);
					}
					if (!mode_end)
					{
						mode = strtok_r(NULL, ",", &modetokens);
						mode_end = (NULL == mode);
					}
					while (mode && strcmp(mode, "i") && strcmp(mode, "b"))
					{
						if (!name_end)
						{
							name = strtok_r(NULL, ",", &nametokens);
							name_end = (NULL == name);
						}
						if (!mode_end)
						{
							mode = strtok_r(NULL, ",", &modetokens);
							mode_end = (NULL == mode);
						}
					}
				}

				mapred_free(tmp1);
				mapred_free(tmp2);
				mapred_free(tmp3);
				tmp1 = NULL;
				tmp2 = NULL;
				tmp3 = NULL;
			}

			/*
			 * Check that the number of parameters received is appropriate.
			 * This would be better moved to a generalized validation routine.
			 */
			switch (obj->kind)
			{

				case MAPRED_MAPPER:
					/*
					 * It would probably be possible to start supporting zero
					 * argument mappers, but:
					 *   1) It would require more modifications
					 *   2) Doesn't currently have a known use case
					 *   3) Has easy workarounds
					 */
					if (nargs < 1)
					{
						mapred_obj_error(obj, "Transition functions require "
										 "two or more parameters");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					break;

				case MAPRED_TRANSITION:
					if (nargs < 2)
					{
						mapred_obj_error(obj, "Transition functions require "
										 "two or more parameters");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					if (retset)
					{
						mapred_obj_error(obj, "Transition functions cannot "
										 "be table functions");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					break;

				case MAPRED_COMBINER:
					if (nargs != 2)
					{
						mapred_obj_error(obj, "Consolidate functions require "
										 "exactly two parameters");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					if (retset)
					{
						mapred_obj_error(obj, "Consolidate functions cannot "
										 "be table functions");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					break;

				case MAPRED_FINALIZER:
					if (nargs != 1)
					{
						mapred_obj_error(obj, "Finalize functions require "
										 "exactly one parameter");
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
					break;

				default:
					XASSERT(false);
			}

			/* Fill in return type information */
			if (retset)
				obj->u.function.mode = MAPRED_MODE_MULTI;
			else
				obj->u.function.mode = MAPRED_MODE_SINGLE;

			/*
			 * Determine the return type information, there are 3 primary
			 * subcases:
			 *
			 *  1) Function is defined with OUT/TABLE parameters.
			 *  2) Function returns a simple type.
			 *  3) Function returns a complex type.
			 *  4) Return type is void [error]
			 */
			plist = returns = NULL;
			if (argmodes && strlen(argmodes) > 0)
			{

				/* strtok is destructive and we need to preserve the original
				 * string, so we make some annoying copies prior to strtok.
				 */
				tmp1 = copyscalar(allargtypes);
				tmp2 = copyscalar(argnames);
				tmp3 = copyscalar(argmodes);

				type = strtok_r(tmp1, ",", &typetokens);
				name = strtok_r(tmp2, ",", &nametokens);
				mode = strtok_r(tmp3, ",", &modetokens);

				i = 1;
				while (mode)
				{
					while (mode &&
						   strcmp(mode, "o") &&
						   strcmp(mode, "b") &&
						   strcmp(mode, "t"))
					{
						/* skip input parameters */
						type = strtok_r(NULL, ",", &typetokens);
						name = strtok_r(NULL, ",", &nametokens);
						mode = strtok_r(NULL, ",", &modetokens);
					}
					if (mode)
					{
						XASSERT(type);

						newitem = mapred_malloc(sizeof(mapred_plist_t));

						/*
						 * Note we haven't made local copies of these, we will
						 * do this after resolution when validating against any
						 * RETURNS defined in the yaml, if any.
						 */

						if( NULL != name &&
								0 != strcmp(name, "") &&
								0 != strcmp(name, "\"\"") )
						{
							/*if name defined in db, just use it*/
							newitem->name = copyscalar(name);
						}
						else
						{
							/*else just obey the default name in db*/
							snprintf( str, STR_LEN, "column%d", i);
							newitem->name = copyscalar(str);
						}

						newitem->type = copyscalar(type);

						newitem->next = NULL;

						if (plist)
							plist->next = newitem;
						else
							returns = newitem;
						plist = newitem;
						++i;
					}
					type = strtok_r(NULL, ",", &typetokens);
					name = strtok_r(NULL, ",", &nametokens);
					mode = strtok_r(NULL, ",", &modetokens);
				}

				mapred_free(tmp1);
				mapred_free(tmp2);
				mapred_free(tmp3);
				tmp1 = NULL;
				tmp2 = NULL;
				tmp3 = NULL;
			}

			/*
			 * If the arguments were not defined in the function definition then
			 * we check to see if this was a complex type by looking up the type
			 * information in pg_attribute.
			 */
			if (!returns)
			{
				bufcat(&buffer,
					   "SELECT attname, atttypid::regtype\n"
					   "FROM   pg_attribute a\n"
					   "JOIN   pg_class c on (a.attrelid = c.oid)\n"
					   "WHERE  not a.attisdropped\n"
					   "  AND  a.attnum > 0\n"
					   "  AND  c.reltype = '");
				bufcat(&buffer, rettype);
				bufcat(&buffer,
					   "'::regtype\n"
					   "ORDER BY -attnum");
				result2 = PQexec(conn, buffer->buffer);
				bufreset(buffer);

				if (PQresultStatus(result2) != PGRES_TUPLES_OK)
				{
					char *error = PQresultErrorMessage(result);

					mapred_obj_error(obj, "Error resolving function: %s", error);
					XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
				}
				else if (PQntuples(result2) > 0)
				{
					/* We have a complex type, build the return list */
					for (i = 0; i < PQntuples(result2); i++)
					{
						name = PQgetvalue(result2, i, 0);
						type = PQgetvalue(result2, i, 1);

						newitem = mapred_malloc(sizeof(mapred_plist_t));
						newitem->name = copyscalar(name);
						newitem->type = copyscalar(type);
						newitem->next = returns;
						returns = newitem;
					}
				}
			}

			/*
			 * If the return types were not defined in either the argument list
			 * nor the catalog then we assume it is a simple type.
			 */
			if (!returns)
			{
				/* Check against "void" which is a special return type that
				 * means there is no return value - which we don't support for
				 * mapreduce.
				 */
				if (!strcmp(rettype, "void"))
				{
					mapred_obj_error(obj, "Function returns void");
					XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
				}
				returns = mapred_malloc(sizeof(mapred_plist_t));
				returns->type = copyscalar(rettype);
				returns->name = NULL;
				returns->next = NULL;
			}

			/*
			 * We now should have a returns list, compare it against the RETURNS
			 * list given in the yaml.  The yaml overrides return names, but can
			 * not override return types.  If the return types are incompatible
			 * raise an error.
			 */
			obj->u.function.internal_returns = returns;
			if (obj->u.function.returns)
			{
				/*
				 * The first thing to do is normalize the given return types
				 * with their formal names.  This will, for example turn a type
				 * like "float8" => "double precision".  The input name might
				 * be correct (float8) but we need it represented as the formal
				 * name so that we can compare against the formal name we got
				 * when we looked up the function in the catalog.
				 */
				plist = obj->u.function.returns;
				type = strtok_r(yaml_rettypes, ",", &typetokens);
				while (plist)
				{
					XASSERT(type);  /* should be an equal number */

					/*
					 * If we have a type specified replace it with the one we
					 * resolved from the select stmt, otherwise just keep it
					 * as NULL and fill it in during the compare against what
					 * was in the catalog.
					 */
					if (plist->type)
					{
						mapred_free(plist->type);

						/*
						 * When in an array the typname may get wrapped in
						 * double quotes, if so we need to strip them back out.
						 */
						if (type[0] == '"')
						{
							plist->type = copyscalar(type+1);
							plist->type[strlen(plist->type)-1] = '\0';
						}
						else
						{
							plist->type = copyscalar(type);
						}
					}

					plist = plist->next;
					type = strtok_r(NULL, ",", &typetokens);
				}


				/* Compare against actual function return types */
				plist = obj->u.function.returns;
				plist2 = returns;
				while (plist && plist2)
				{
					XASSERT(plist->name);   /* always defined in YAML */
					XASSERT(plist2->type);  /* always defined in SQL */

					/*
					 * In the YAML it is possible to have a name without a type,
					 * if that is the case then simply take the SQL type.
					 */
					if (!plist->type)
						plist->type = copyscalar(plist2->type);
					else if (strcmp(plist->type, plist2->type))
						break;
					plist  = plist->next;
					plist2 = plist2->next;
				}
				if (plist || plist2)
				{
					mapred_obj_error(obj, "RETURN parameter '%s %s' != '%s %s'",
									 plist ? plist->name : "\"\"",
									 plist ? plist->type : "-",
									 plist2 ? (plist2->name ? plist2->name : plist->name) : "\"\"",
									 plist2 ? plist2->type : "-");
					XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
				}
			}
			else
			{
				obj->u.function.returns = returns;
				i = 0;
				for (plist = returns; plist; plist = plist->next)
				{
					XASSERT(plist->type);
					i++;
					plist->type = copyscalar(plist->type);

					/*
					 * if plist->name is not null and empty string,
					 * then use that name
					 */
					if (plist->name &&
							0 != strcmp(plist->name, "") &&
							0 != strcmp(plist->name, "\"\"") )
					{
						plist->name = copyscalar(plist->name);
					}
					/*
					 * else We need generate a name anyway
					 */
					else
					{
						/*
						 * Manufacture a name for a column based on default
						 * naming rules.
						 */
						name = (char*) NULL;
						if (i <= 2)
							name = (char*) default_return_names[obj->kind][i-1];
						if (!name)
						{
							snprintf(str, STR_LEN, "parameter%d", i);
							name = str;
						}
						plist->name = copyscalar(name);
					}
				}
			}
		}
	}
	XFINALLY
	{
		if (result)
			PQclear(result);
		if (result2)
			PQclear(result2);
		if (buffer)
			mapred_free(buffer);

		if (tmp1)
			mapred_free(tmp1);
		if (tmp2)
			mapred_free(tmp2);
		if (tmp3)
			mapred_free(tmp3);

	}
	XTRY_END;
}

void mapred_run_document(PGconn *conn, mapred_document_t *doc)
{
	PGresult       *result;
	mapred_olist_t *olist;
	boolean         done;
	boolean         executes;

	/* Ignore NOTICE messages from database */
	PQsetNoticeReceiver(conn, ignore_notice_handler, NULL);

	/* Establish a name-prefix for temporary objects */
	doc->prefix = mapred_malloc(64);
	snprintf(doc->prefix, 64, "mapreduce_%d_", PQbackendPID(conn));


	/*
	 * Resolution of dependecies was defered until now so that
	 * a database connection could be available to look up any
	 * dependencies that are not defined within the YAML document.
	 */
	if (global_verbose_flag)
		fprintf(stderr, "  - Resolving Dependencies:\n");
	mapred_resolve_dependencies(conn, doc);
	if (global_verbose_flag)
		fprintf(stderr, "    - DONE\n");

	XTRY
	{

		/*
		 * Setting gp_mapreduce_define will disable logging of sql
		 * statements.
		 */
#ifndef INTERNAL_BUILD
		result = PQexec(conn, "set gp_mapreduce_define=true");
		PQclear(result);
#endif

		/*
		 * By running things within a transaction we can effectively
		 * obscure the mapreduce sql definitions.  They could still
		 * be exposed by a savy user via mapreduce views that access
		 * the catalog tables, but it's a cleaner method of handling
		 * things.
		 */
		result = PQexec(conn, "BEGIN TRANSACTION");
		if (PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			PQclear(result);
			XRAISE(MAPRED_SQL_ERROR, NULL);
		}
		PQclear(result);

		/* With dependencies resolved we can now try creating objects */
		if (global_verbose_flag)
			fprintf(stderr, "  - Creating Objects\n");

		/*
		 * we don't try to create any executes until all non-executes are
		 * successfully created
		 */
		executes = false;
		do
		{
			boolean progress = false;

			/*
			 * We keep a buffer of errors during parsing, and display them at
			 * the end if they haven't been resolved.
			 */
			if (!doc->errors)
				doc->errors = makebuffer(1024, 1024);
			else
				bufreset(doc->errors);

			/*
			 * Loop through the objects, creating each in turn.
			 * If an object has dependencies that have not been created yet
			 * it will return false and we will make additional passes through
			 * the object list
			 */
			done  = true;
			for (olist = doc->objects; olist; olist = olist->next)
			{
				mapred_object_t *obj = olist->object;
				if (!obj->created &&
					(executes || obj->kind != MAPRED_EXECUTION))
				{
					if (global_verbose_flag && obj->kind != MAPRED_ADT)
					{
						fprintf(stderr, "    - %s:\n",
								mapred_kind_name[obj->kind]);
						fprintf(stderr, "       NAME: %s\n", obj->name);
					}

					if (!mapred_create_object(conn, doc, obj))
						done = false;
					else
						progress = true;
				}
			}

			/*
			 * If all non-execute objects have been created then switch over
			 * and start creating the execution jobs
			 */
			if (done && !executes)
			{
				executes = true;
				done = false;
			}

			/*
			 * If we looped through the list, we are not done, and no progress
			 * was made then we have an infinite cycle and should probably stop.
			 */
			if (!done && !progress)
			{
				if (doc->errors && doc->errors->position > 0)
					fprintf(stderr, "%s", doc->errors->buffer);
				XRAISE(MAPRED_PARSE_ERROR,
					   "Unable to make forward progress creating objects\n");
			}

		} while (!done);

		/*
		 * Re-enable statement logging before we try running queries
		 */
#ifndef INTERNAL_BUILD
		result = PQexec(conn, "set gp_mapreduce_define=false");
		PQclear(result);
#endif

		/* objects created, execute queries */
		mapred_run_queries(conn, doc);
	}
	XCATCH(MAPRED_SQL_ERROR)
	{
		if (global_verbose_flag)
			fprintf(stderr, "    - ");
		fprintf(stderr, "%s", PQerrorMessage(conn));
		XRERAISE();
	}
	XFINALLY
	{

		/*
		 * disable statement logging before deleting objects
		 */
#ifndef INTERNAL_BUILD
		result = PQexec(conn, "set gp_mapreduce_define=true");
		PQclear(result);
#endif

		/* Remove all the objects that we created */
		if (global_print_flag || global_debug_flag)
			printf("\n");
		for (olist = doc->objects; olist; olist = olist->next)
			mapred_remove_object(conn, doc, olist->object);
		if (global_print_flag || global_debug_flag)
			printf("\n");

		/*
		 * We always commit the transaction, even on failure since the failure
		 * may have occured after we generated some output tables and we want
		 * to keep the partial results.
		 */
		result = PQexec(conn, "COMMIT TRANSACTION");
		if (PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			if (global_verbose_flag)
				fprintf(stderr, "    - ");
			fprintf(stderr, "%s", PQerrorMessage(conn));
		}
	}
	XTRY_END;
}


void mapred_resolve_dependencies(PGconn *conn, mapred_document_t *doc)
{
	mapred_olist_t     *olist;
	int                 exec_count = 0;

	/* Walk the list of objects */
	for (olist = doc->objects; olist; olist = olist->next)
		mapred_resolve_object(conn, doc, olist->object, &exec_count);

}

void mapred_resolve_object(PGconn *conn, mapred_document_t *doc,
						   mapred_object_t *obj, int *exec_count)
{
	mapred_olist_t  *newlist;
	mapred_object_t *sub;   /* sub-object */
	size_t           len;

	switch (obj->kind)
	{
		/* Objects with no dependencies */
		case MAPRED_OUTPUT:
		case MAPRED_ADT:
			break;

		case MAPRED_INPUT:
			/*
			 * For FILE/GPFDIST/EXEC inputs we will create a name-prefixed
			 * version of the object to prevent name collisions, and then
			 * create a second temporary view over the external table to
			 * support access to the input by "name".  This involves creating
			 * a second copy of the input which we place directly after the
			 * original input in the document object list.
			 */
			if (obj->u.input.type == MAPRED_INPUT_GPFDIST ||
				obj->u.input.type == MAPRED_INPUT_FILE    ||
				obj->u.input.type == MAPRED_INPUT_EXEC)
			{
				mapred_object_t *newinput;
				mapred_olist_t  *parent;

				newinput = mapred_malloc(sizeof(mapred_object_t));
				memset(newinput, 0, sizeof(mapred_object_t));
				newinput->kind = MAPRED_INPUT;
				len = strlen(obj->name) + 1;
				newinput->name = mapred_malloc(len);
				sprintf(newinput->name, "%s", obj->name);
				newinput->u.input.type = MAPRED_INPUT_QUERY;
				len = strlen(doc->prefix) + strlen(obj->name) + 16;
				newinput->u.input.desc = mapred_malloc(len);
				snprintf(newinput->u.input.desc, len,
						 "select * from %s%s",
						 doc->prefix, obj->name);

				/*
				 * Find parent input in the doclist and add the new object
				 * immediately after it.
				 */
				for (parent = doc->objects;
					 parent && parent->object != obj;
					 parent = parent->next);
				XASSERT(parent);
				newlist = mapred_malloc(sizeof(mapred_olist_t));
				newlist->object = newinput;
				newlist->next = parent->next;
				parent->next = newlist;
			}
			break;


		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:


			/*
			 * If the function is an internal function then we try to resolve
			 * the function by looking it up in the catalog.
			 */
			obj->u.function.internal_returns = NULL;
			obj->internal = false;

			if (!obj->u.function.language)
			{
				obj->internal = true;
				lookup_function_in_catalog(conn, doc, obj);
			}
			/* ??? */
			else if (!obj->u.function.returns)
			{
				XASSERT(false);
			}

			/*
			 * The function types may manufacture a dependency on an adt,
			 * but have no other dependencies.
			 */
			else if (obj->u.function.returns->next)
			{
				sub = mapred_malloc(sizeof(mapred_object_t));
				memset(sub, 0, sizeof(mapred_object_t));
				sub->kind = MAPRED_ADT;
				len = strlen(doc->prefix) + strlen(obj->name) + 7;
				sub->name = mapred_malloc(len);
				snprintf(sub->name, len, "%s%s_rtype",
						 doc->prefix, obj->name);
				sub->u.adt.returns = obj->u.function.returns;

				obj->u.function.rtype.name = sub->name;
				obj->u.function.rtype.object = sub;

				/* Add the ADT to the list of document objects */
				newlist = mapred_malloc(sizeof(mapred_olist_t));
				newlist->object = sub;
				newlist->next = doc->objects;
				doc->objects = newlist;

				/* And resolve the sub-object */
				mapred_resolve_object(conn, doc, sub, exec_count);
			}
			else
			{
				obj->u.function.rtype.name = obj->u.function.returns->type;
				obj->u.function.rtype.object = NULL;
			}
			break;


		case MAPRED_REDUCER:
		{
			/*
			 * If we have a function, but no object then we assume that it is
			 * a database function.  Create a dummy object to handle this case.
			 */
			mapred_resolve_ref(doc->objects, &obj->u.reducer.transition);
			if (obj->u.reducer.transition.name &&
				!obj->u.reducer.transition.object)
			{
				len = strlen(obj->u.reducer.transition.name) + 1;
				sub = mapred_malloc(sizeof(mapred_object_t));
				memset(sub, 0, sizeof(mapred_object_t));
				sub->kind = MAPRED_TRANSITION;
				sub->name = mapred_malloc(len);
				sub->line = obj->line;
				strncpy(sub->name, obj->u.reducer.transition.name, len);

				newlist = mapred_malloc(sizeof(mapred_olist_t));
				newlist->object = sub;
				newlist->next = doc->objects;
				doc->objects = newlist;
				obj->u.reducer.transition.object = sub;

				/* And resolve the sub-object */
				mapred_resolve_object(conn, doc, sub, exec_count);
			}
			mapred_resolve_ref(doc->objects, &obj->u.reducer.combiner);
			if (obj->u.reducer.combiner.name &&
				!obj->u.reducer.combiner.object)
			{
				len = strlen(obj->u.reducer.combiner.name) + 1;
				sub = mapred_malloc(sizeof(mapred_object_t));
				memset(sub, 0, sizeof(mapred_object_t));
				sub->kind = MAPRED_COMBINER;
				sub->name = mapred_malloc(len);
				sub->line = obj->line;
				strncpy(sub->name, obj->u.reducer.combiner.name, len);

				newlist = mapred_malloc(sizeof(mapred_olist_t));
				newlist->object = sub;
				newlist->next = doc->objects;
				doc->objects = newlist;
				obj->u.reducer.combiner.object = sub;

				/* And resolve the sub-object */
				mapred_resolve_object(conn, doc, sub, exec_count);
			}
			mapred_resolve_ref(doc->objects, &obj->u.reducer.finalizer);
			if (obj->u.reducer.finalizer.name &&
				!obj->u.reducer.finalizer.object)
			{
				len = strlen(obj->u.reducer.finalizer.name) + 1;
				sub = mapred_malloc(sizeof(mapred_object_t));
				memset(sub, 0, sizeof(mapred_object_t));
				sub->kind = MAPRED_FINALIZER;
				sub->name = mapred_malloc(len);
				sub->line = obj->line;
				strncpy(sub->name, obj->u.reducer.finalizer.name, len);

				newlist = mapred_malloc(sizeof(mapred_olist_t));
				newlist->object = sub;
				newlist->next = doc->objects;
				doc->objects = newlist;
				obj->u.reducer.finalizer.object = sub;

				/* And resolve the sub-object */
				mapred_resolve_object(conn, doc, sub, exec_count);
			}

			break;
		}


		case MAPRED_TASK:
		case MAPRED_EXECUTION:
		{
			/*
			 * Resolving a task may require recursion to resolve other
			 * tasks to work out parameter lists.  We keep track of
			 * our resolution state in order to detect potential
			 * infinite recursion issues.
			 */
			if (obj->u.task.flags & mapred_task_resolved)
				return;

			/* Assign a name to anonymous executions */
			if (!obj->name)
			{
				size_t     len;

				XASSERT(obj->u.task.execute);

				/* 10 characters for max int digits, 4 for "run_" */
				len = strlen(doc->prefix) + 16;
				obj->name = mapred_malloc(len);
				snprintf(obj->name, len, "%srun_%d",
						 doc->prefix, ++(*exec_count));
			}

			/* Check for infinite recursion */
			if (obj->u.task.flags & mapred_task_resolving)
			{
				mapred_obj_error(obj, "Infinite recursion detected while "
								 "trying to resove TASK");
				XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
			}
			obj->u.task.flags |= mapred_task_resolving;

			/* Validate object types */
			if (obj->u.task.input.name)
			{
				mapred_resolve_ref(doc->objects, &obj->u.task.input);
				sub = obj->u.task.input.object;

				/* If we can't find the input, throw an error */
				if (!sub)
				{
					/* Can't find INPUT object */
					mapred_obj_error(obj, "SOURCE '%s' not found in document",
									 obj->u.task.input.name);
					XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
				}

				/*
				 * The input must either be an INPUT or a TASK
				 */
				switch (sub->kind)
				{
					case MAPRED_INPUT:
						break;

					case MAPRED_TASK:
						/* This objects input is the sub objects output */
						mapred_resolve_object(conn, doc, sub, exec_count);
						break;

						/* Otherwise generate an error */
					default:

						/* SOURCE wasn't an INPUT */
						mapred_obj_error(obj, "SOURCE '%s' is neither an INPUT nor a TASK",
										 obj->u.task.input.name);
						XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
				}
			}

			if (obj->u.task.mapper.name)
			{
				mapred_resolve_ref(doc->objects, &obj->u.task.mapper);
				sub = obj->u.task.mapper.object;

				if (!sub)
				{
					/* Create an internal map function */
					len = strlen(obj->u.task.mapper.name) + 1;
					sub = mapred_malloc(sizeof(mapred_object_t));
					memset(sub, 0, sizeof(mapred_object_t));
					sub->kind = MAPRED_MAPPER;
					sub->name = mapred_malloc(len);
					sub->line = obj->line;
					strncpy(sub->name, obj->u.task.mapper.name, len);

					newlist = mapred_malloc(sizeof(mapred_olist_t));
					newlist->object = sub;
					newlist->next = doc->objects;
					doc->objects = newlist;
					obj->u.task.mapper.object = sub;

					/* And resolve the sub-object */
					mapred_resolve_object(conn, doc, sub, exec_count);
				}
				else
				{
					/* Allow any function type */
					switch (sub->kind)
					{
						case MAPRED_MAPPER:
						case MAPRED_TRANSITION:
						case MAPRED_COMBINER:
						case MAPRED_FINALIZER:
							break;

						default:
							mapred_obj_error(obj, "MAP '%s' is not a MAP object",
											 obj->u.task.mapper.name);
							XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
					}
				}
			}

			if (obj->u.task.reducer.name)
			{
				mapred_resolve_ref(doc->objects, &obj->u.task.reducer);
				sub = obj->u.task.reducer.object;

				if (!sub)
				{
					/* FIXME: non-yaml reducers */
				}
				else if (sub->kind == MAPRED_REDUCER)
				{   /* Validate Reducer */
					mapred_resolve_object(conn, doc, sub, exec_count);
				}
				else
				{   /* It's an object, but not a REDUCER */
					mapred_obj_error(obj, "REDUCE '%s' is not a REDUCE object",
									 obj->u.task.reducer.name);
					XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
				}
			}

			if (obj->u.task.output.name)
			{
				mapred_resolve_ref(doc->objects, &obj->u.task.output);

				sub = obj->u.task.output.object;
				if (sub && sub->kind != MAPRED_OUTPUT)
				{
					mapred_obj_error(obj, "TARGET '%s' is not an OUTPUT object",
									 obj->u.task.output.name);
					XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
				}
				if (!sub && obj->u.task.output.name)
				{
					mapred_obj_error(obj, "TARGET '%s' is not defined in "
									 "document",
									 obj->u.task.output.name);
					XRAISE(MAPRED_PARSE_ERROR, "Object Resolution Failure");
				}
			}

			/* clear resolving bit and set resolved bit */
			obj->u.task.flags &= !mapred_task_resolving;
			obj->u.task.flags |= mapred_task_resolved;
			break;
		}

		default:
			XASSERT(false);
	}

	if (global_debug_flag)
		mapred_obj_debug(obj);
}

/*
 * mapred_setup_columns -
 *   setup column lists (input, output, grouping, etc)
 *
 *   This is usually able to be determined directly from the YAML,
 *   but for some things (defined in the database rather than in
 *   the YAML, eg QUERY INPUTS) we can not determine the columns
 *   until the object has been created.  Which can trickle down to
 *   any object that depends on it.
 *
 *   For this reason we don't setup the columns during the parse phase,
 *   but rather just before or just after we actually create the object
 *   once we know that all the dependencies have already been created.
 */
void mapred_setup_columns(PGconn *conn, mapred_object_t *obj)
{
	mapred_object_t *sub;
	PGresult        *result;

	/* switch based on object type */
	switch (obj->kind)
	{
		case MAPRED_ADT:
			break;

		case MAPRED_INPUT:

			/*
			 * Should be called after creation, otherwise catalog queries
			 * could fail.
			 */
			XASSERT(obj->created);

			/* setup the column list for database defined inputs */
			if (obj->u.input.type == MAPRED_INPUT_TABLE ||
				obj->u.input.type == MAPRED_INPUT_QUERY)
			{
				/*
				 * This gets the ordered list of columns for the first
				 * input of the given name in the user's search path.
				 */
				buffer_t *buffer = makebuffer(1024, 1024);
				bufcat(&buffer,
					   "SELECT  attname, "
					   "        pg_catalog.format_type(atttypid, atttypmod)\n"
					   "FROM    pg_catalog.pg_attribute\n"
					   "WHERE   attnum > 0 AND attrelid = lower('");
				if (obj->u.input.type == MAPRED_INPUT_TABLE)
					bufcat(&buffer, obj->u.input.desc);
				else
					bufcat(&buffer, obj->name);
				bufcat(&buffer,
					   "')::regclass\n"
					   "ORDER BY   -attnum;\n\n");

				if (global_debug_flag)
					printf("%s", buffer->buffer);

				result = PQexec(conn, buffer->buffer);
				mapred_free(buffer);

				if (PQresultStatus(result) == PGRES_TUPLES_OK &&
					PQntuples(result) > 0)
				{
					mapred_plist_t *newitem;
					int i;

					/* Destroy any previous default values we setup */
					mapred_destroy_plist(&obj->u.input.columns);

					/*
					 * The columns were sorted reverse order above so
					 * the list can be generated back -> front
					 */
					for (i = 0; i < PQntuples(result); i++)
					{
						char *name = PQgetvalue(result, i, 0);
						char *type = PQgetvalue(result, i, 1);

						/* Add the column to the list */
						newitem = mapred_malloc(sizeof(mapred_plist_t));
						newitem->name = mapred_malloc(strlen(name)+1);
						strncpy(newitem->name, name, strlen(name)+1);
						newitem->type = mapred_malloc(strlen(type)+1);
						strncpy(newitem->type, type, strlen(type)+1);
						newitem->next = obj->u.input.columns;
						obj->u.input.columns = newitem;
					}
				}
				else
				{
					char *error = PQresultErrorField(result, PG_DIAG_SQLSTATE);
					char *name;

					if (obj->u.input.type == MAPRED_INPUT_TABLE)
						name = obj->u.input.desc;
					else
						name = obj->name;

					if (PQresultStatus(result) == PGRES_TUPLES_OK)
					{
						mapred_obj_error(obj, "Table '%s' contains no rows", name);
					}
					else if (!strcmp(error, OBJ_DOES_NOT_EXIST) ||
							 !strcmp(error, SCHEMA_DOES_NOT_EXIST) )
					{
						mapred_obj_error(obj, "Table '%s' not found", name);
					}
					else
					{
						mapred_obj_error(obj, "Table '%s' unknown error: %s", name, error);
					}
					XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
				}
				PQclear(result);
			}
			break;


		case MAPRED_OUTPUT:
			break;

		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:
			XASSERT(obj->u.function.parameters);
			XASSERT(obj->u.function.returns);
			break;


		case MAPRED_REDUCER:
		{
			mapred_object_t *transition = obj->u.reducer.transition.object;

			XASSERT(transition);
			XASSERT(transition->u.function.parameters);
			obj->u.reducer.parameters =
				transition->u.function.parameters->next;

			/*
			 * Use the return result of:
			 *   1) The finalizer
			 *   2) The combiner, or
			 *   3) The transition
			 *
			 * in that order, if the return is not derivable then
			 * fall into the default value of a single text column
			 * named "value"
			 */
			if (obj->u.reducer.finalizer.name)
				sub = obj->u.reducer.finalizer.object;
			else if (obj->u.reducer.combiner.name)
				sub = obj->u.reducer.combiner.object;
			else
				sub = obj->u.reducer.transition.object;

			if (sub)
				obj->u.reducer.returns = sub->u.function.returns;

			if (!obj->u.reducer.returns)
			{
				/*
				 * If unable to determine the returns based on the reducer
				 * components (generally due to use of SQL functions) then
				 * use the default of a single text column named "value".
				 */
				obj->u.reducer.returns = mapred_malloc(sizeof(mapred_plist_t));
				obj->u.reducer.returns->name = "value";
				obj->u.reducer.returns->type = "text";
				obj->u.reducer.returns->next = NULL;
			}
			break;
		}


		case MAPRED_TASK:
		case MAPRED_EXECUTION:
		{
			mapred_plist_t *scan;
			mapred_plist_t *last = NULL;

			/*
			 * The input must either be an INPUT or a TASK
			 */
			sub = obj->u.task.input.object;
			switch (sub->kind)
			{
				case MAPRED_INPUT:
					obj->u.task.parameters = sub->u.input.columns;
					break;

				case MAPRED_TASK:
					/* union the input tasks returns and grouping */
					for (scan = sub->u.task.grouping;
						 scan;
						 scan = scan->next)
					{
						if (!last)
						{
							obj->u.task.parameters =
								mapred_malloc(sizeof(mapred_plist_t));
							last = obj->u.task.parameters;
						}
						else
						{
							last->next =
								mapred_malloc(sizeof(mapred_plist_t));
							last = last->next;
						}
						last->name = scan->name;
						last->type = scan->type;
						last->next = NULL;
					}
					for (scan = sub->u.task.returns;
						 scan;
						 scan = scan->next)
					{
						if (!last)
						{
							obj->u.task.parameters =
								mapred_malloc(sizeof(mapred_plist_t));
							last = obj->u.task.parameters;
						}
						else
						{
							last->next =
								mapred_malloc(sizeof(mapred_plist_t));
							last = last->next;
						}
						last->name = scan->name;
						last->type = scan->type;
						last->next = NULL;
					}
					break;

				default:
					/* Should have already been validated */
					XASSERT(false);
			}

			if (obj->u.task.mapper.name)
			{
				sub = obj->u.task.mapper.object;
				if (!sub)
				{
					/* FIXME: Lookup function in database */
					/* for now... do nothing */
				}
				else
				{
					/* Allow any function type */
					switch (sub->kind)
					{
						case MAPRED_MAPPER:
						case MAPRED_TRANSITION:
						case MAPRED_COMBINER:
						case MAPRED_FINALIZER:
							break;

						default:
							/* Should have already been validated */
							XASSERT(false);
					}
				}
			}

			if (obj->u.task.reducer.name)
			{
				mapred_clist_t *keys;
				mapred_plist_t *source;

				/*
				 * The grouping columns for a task are the columns produced
				 * by the input/mapper that are not consumed by the reducer.
				 *
				 * A special exception is made for a column named "key" which
				 * is always a grouping column.
				 *
				 * FIXME: deal with non-yaml map functions
				 *
				 * FIXME: deal with KEY specifications
				 */
				if (obj->u.task.mapper.object)
					source = obj->u.task.mapper.object->u.function.returns;
				else
					source = obj->u.task.parameters;

				sub = obj->u.task.reducer.object;
				if (!sub)
				{
					/*
					 * The output of a built in function is defined to be
					 * "value", with an input of "value", everything else
					 * is defined to be a grouping column.
					 */
					last = NULL;
					for (scan = source; scan; scan = scan->next)
					{
						if (strcasecmp(scan->name, "value"))
						{
							if (!last)
							{
								obj->u.task.grouping =
									mapred_malloc(sizeof(mapred_plist_t));
								last = obj->u.task.grouping;
							}
							else
							{
								last->next =
									mapred_malloc(sizeof(mapred_plist_t));
								last = last->next;
							}
							last->name = scan->name;
							last->type = scan->type;
							last->next = NULL;
						}
					}
				}
				else
				{
					/* Validate Reducer */
					XASSERT(sub->kind == MAPRED_REDUCER);

					/*
					 * source is the set of input columns that the reducer has
					 * to work with.
					 *
					 * Loop the reducer "keys" clause to determine what keys are
					 * present.
					 */
					last = NULL;
					for (keys = sub->u.reducer.keys; keys; keys = keys->next)
					{
						/*
						 * If there is a '*' in the keys then it catches all
						 * unreferenced columns.
						 */
						if (keys->value[0] == '*' && keys->value[1] == '\0')
						{
							/*
							 * Add all sources not found in either parameters,
							 * or explicitly mentioned in keys
							 */
							for (scan = source; scan; scan = scan->next)
							{
								mapred_plist_t *pscan;
								mapred_clist_t *kscan;

								for (pscan = sub->u.reducer.parameters;
									 pscan;
									 pscan = pscan->next)
								{
									if (!strcasecmp(scan->name, pscan->name))
										break;
								}
								if (pscan)
									continue;   /* found in parameters */
								for (kscan = sub->u.reducer.keys;
									 kscan;
									 kscan = kscan->next)
								{
									if (!strcasecmp(scan->name, kscan->value))
										break;
								}
								if (kscan)
									continue;   /* found in keys */

								/* we have an unmatched source, add to grouping */
								if (!last)
								{
									obj->u.task.grouping =
										mapred_malloc(sizeof(mapred_plist_t));
									last = obj->u.task.grouping;
								}
								else
								{
									last->next =
										mapred_malloc(sizeof(mapred_plist_t));
									last = last->next;
								}
								last->name = scan->name;
								last->type = scan->type;
								last->next = NULL;
							}
						}
						else
						{
							/* Look for the referenced key in the source list */
							for (scan = source; scan; scan = scan->next)
								if (!strcasecmp(keys->value, scan->name))
								{
									/* we have a match, add the key to grouping */
									if (!last)
									{
										obj->u.task.grouping =
											mapred_malloc(sizeof(mapred_plist_t));
										last = obj->u.task.grouping;
									}
									else
									{
										last->next =
											mapred_malloc(sizeof(mapred_plist_t));
										last = last->next;
									}
									last->name = scan->name;
									last->type = scan->type;
									last->next = NULL;
									break;
								}
						}
					}
				}
			}

			/*
			 * If there is a reducer then the "returns" columns are the
			 * output of the reducer, and must be unioned with the grouping
			 * columns for final output.
			 *
			 * If there is no reducer then the returns columns are the
			 * returns columns of the mapper or the input
			 */
			if (obj->u.task.reducer.name)
			{
				/*
				 * If it is a built in function then we'll just fall into the
				 * default of a single text column named "value".
				 */
				sub = obj->u.task.reducer.object;
				if (sub)
					obj->u.task.returns = sub->u.reducer.returns;

			}
			else if (obj->u.task.mapper.name)
			{
				sub = obj->u.task.mapper.object;
				if (sub)
					obj->u.task.returns = sub->u.function.returns;
			}
			else
			{
				obj->u.task.returns = obj->u.task.parameters;
			}

			if (!obj->u.task.returns)
			{
				/*
				 * If unable to determine the returns based on the reducer
				 * components (generally due to use of SQL functions) then
				 * use the default of a single text column named "value".
				 */
				obj->u.task.returns = mapred_malloc(sizeof(mapred_plist_t));
				obj->u.task.returns->name = "value";
				obj->u.task.returns->type = "text";
				obj->u.task.returns->next = NULL;
			}
			break;
		}

		default:
			XASSERT(false);
	}
}

void mapred_resolve_ref(mapred_olist_t *olist, mapred_reference_t *ref)
{
	XASSERT(ref);
	if (!ref->name)
		return;

	/* Scan the list of objects until we find one with a matching name */
	for (; olist; olist = olist->next)
	{
		if (olist->object->name && !strcasecmp(ref->name, olist->object->name))
		{
			ref->object = olist->object;
			return;
		}
	}
}

/* Some basic destructors */
void mapred_destroy_object(mapred_object_t **objh)
{
	mapred_object_t *obj;

	/*
	 * We are passed a handle to the object, get the actual pointer and point
	 * the handle to NULL so that it is not stale once we free the list below.
	 */
	if (!objh || !*objh)
		return;
	obj = *objh;
	*objh = (mapred_object_t *) NULL;

	/* What fields are valid is dependent on what kind of object it is */
	scalarfree(obj->name);
	switch (obj->kind)
	{
		case MAPRED_NO_KIND:
			break;

		case MAPRED_DOCUMENT:
			scalarfree(obj->u.document.version);
			scalarfree(obj->u.document.database);
			scalarfree(obj->u.document.user);
			scalarfree(obj->u.document.host);
			mapred_destroy_olist(&obj->u.document.objects);
			mapred_destroy_olist(&obj->u.document.execute);
			break;

		case MAPRED_INPUT:
			scalarfree(obj->u.input.desc);
			scalarfree(obj->u.input.delimiter);
			scalarfree(obj->u.input.encoding);
			mapred_destroy_clist(&obj->u.input.files);
			mapred_destroy_plist(&obj->u.input.columns);
			break;

		case MAPRED_OUTPUT:
			scalarfree(obj->u.output.desc);
			break;

		case MAPRED_MAPPER:
		case MAPRED_TRANSITION:
		case MAPRED_COMBINER:
		case MAPRED_FINALIZER:
			scalarfree(obj->u.function.body);
			scalarfree(obj->u.function.language);
			mapred_destroy_plist(&obj->u.function.parameters);

			if( obj->internal &&
					obj->u.function.internal_returns != obj->u.function.returns )
				mapred_destroy_plist(&obj->u.function.internal_returns);

			mapred_destroy_plist(&obj->u.function.returns);
			break;

		case MAPRED_REDUCER:
			scalarfree(obj->u.reducer.transition.name);
			scalarfree(obj->u.reducer.combiner.name);
			scalarfree(obj->u.reducer.finalizer.name);
			scalarfree(obj->u.reducer.initialize);
			break;

		case MAPRED_TASK:
		case MAPRED_EXECUTION:
			scalarfree(obj->u.task.input.name);
			scalarfree(obj->u.task.mapper.name);
			scalarfree(obj->u.task.reducer.name);
			scalarfree(obj->u.task.output.name);
			break;

		/*
		 * ADT just borrowed the parameter list from the owning function,
		 * so it has nothing else to delete.
		 */
		case MAPRED_ADT:
			break;

		default:
			XASSERT(false);
	}
}

void mapred_destroy_olist(mapred_olist_t **olisth)
{
	mapred_olist_t *olist;
	mapred_olist_t *next;

	/*
	 * We are passed a handle to the olist, get the actual pointer and point
	 * the handle to NULL so that it is not stale once we free the list below.
	 */
	if (!olisth || !*olisth)
		return;
	olist = *olisth;
	*olisth = (mapred_olist_t *) NULL;

	/* Walk the list destroying each item as we come to it. */
	while (olist)
	{
		mapred_destroy_object(&olist->object);
		next = olist->next;
		mapred_free(olist);
		olist = next;
	}
}


void mapred_destroy_clist(mapred_clist_t **clisth)
{
	mapred_clist_t *clist;
	mapred_clist_t *next;

	/*
	 * We are passed a handle to the olist, get the actual pointer and point
	 * the handle to NULL so that it is not stale once we free the list below.
	 */
	if (!clisth || !*clisth)
		return;
	clist = *clisth;
	*clisth = (mapred_clist_t *) NULL;

	/* Walk the list destroying each item as we come to it. */
	while (clist)
	{
		scalarfree(clist->value);
		next = clist->next;
		mapred_free(clist);
		clist = next;
	}
}

void mapred_destroy_plist(mapred_plist_t **plisth)
{
	mapred_plist_t *plist;
	mapred_plist_t *next;

	/*
	 * We are passed a handle to the olist, get the actual pointer and point
	 * the handle to NULL so that it is not stale once we free the list below.
	 */
	if (!plisth || !*plisth)
		return;
	plist = *plisth;
	*plisth = (mapred_plist_t *) NULL;

	/* Walk the list destroying each item as we come to it. */
	while (plist)
	{
		scalarfree(plist->name);
		scalarfree(plist->type);
		next = plist->next;
		mapred_free(plist);
		plist = next;
	}
}




/* -------------------------------------------------------------------------- */
/* Functions that get things done                                             */
/* -------------------------------------------------------------------------- */
void mapred_run_queries(PGconn *conn, mapred_document_t *doc)
{
	mapred_olist_t  *olist;
	mapred_plist_t  *columns;
	mapred_object_t *output;
	PGresult        *result  = NULL;
	FILE            *outfile = stdout;
	buffer_t        *buffer  = NULL;

	XTRY
	{
		/* allocates 512 bytes, extending by 512 bytes if we run out. */
		buffer = makebuffer(512, 512);

		/* Loop through all objects */
		for (olist = doc->objects; olist; olist = olist->next)
		{
			if (olist->object->kind == MAPRED_EXECUTION)
			{
				boolean exists = false;

				XASSERT(olist->object->name);

				/* Reset the buffer from any previous executions */
				bufreset(buffer);

				output = olist->object->u.task.output.object;

				/*
				 *  [CREATE TABLE <name> AS ]
				 *    SELECT * FROM <name>
				 *    ORDER BY <column-list>
				 */
				if (output && output->u.output.type == MAPRED_OUTPUT_TABLE)
				{
					/* does the table already exist? */
					bufcat(&buffer,
						   "SELECT n.nspname \n"
						   "FROM   pg_catalog.pg_class c JOIN \n"
						   "       pg_catalog.pg_namespace n on \n"
						   "       (c.relnamespace = n.oid) \n"
						   "WHERE  n.nspname = ANY(current_schemas(true)) \n"
						   "  AND  c.relname = lower('");
					bufcat(&buffer, output->u.output.desc);
					bufcat(&buffer, "')");
					result = PQexec(conn, buffer->buffer);
					if (PQresultStatus(result) == PGRES_TUPLES_OK &&
						PQntuples(result) > 0)
						exists = true;
					bufreset(buffer);

					if (exists && output->u.output.mode == MAPRED_OUTPUT_MODE_REPLACE)
					{
						bufcat(&buffer, "DROP TABLE ");
						bufcat(&buffer, output->u.output.desc);
						PQexec(conn, "SAVEPOINT mapreduce_save");
						result = PQexec(conn, buffer->buffer);
						if (PQresultStatus(result) == PGRES_COMMAND_OK)
						{
							PQexec(conn, "RELEASE SAVEPOINT mapreduce_save");
						}
						else
						{
							/* rollback to savepoint */
							PQexec(conn, "ROLLBACK TO SAVEPOINT mapreduce_save");
							PQexec(conn, "RELEASE SAVEPOINT mapreduce_save");

							if (global_verbose_flag)
								fprintf(stderr, "   - ");
							fprintf(stderr, "Error: %s\n",
									PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY));
							mapred_obj_error(output, "Table '%s' can't be replaced",
											 output->u.output.desc);
							XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
						}
						bufreset(buffer);
						exists = false;
					}

					/* Handle Explain for OUTPUT TABLE */
					if (global_explain_flag & global_analyze)
						bufcat(&buffer, "EXPLAIN ANALYZE ");
					else if (global_explain_flag)
						bufcat(&buffer, "EXPLAIN ");

					if (!exists)
					{
						bufcat(&buffer, "CREATE TABLE ");
						bufcat(&buffer, output->u.output.desc);
						bufcat(&buffer, " AS ");
					}
					else if (output->u.output.mode == MAPRED_OUTPUT_MODE_APPEND)
					{
						bufcat(&buffer, "INSERT INTO ");
						bufcat(&buffer, output->u.output.desc);
						bufcat(&buffer, " (");
					}
					else
					{
						/* exists, mode is neither replace or append => error */
						mapred_obj_error(output, "Table '%s' already exists",
										 output->u.output.desc);
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
					}
				}
				/* Handle Explain for non-table output */
				else if (global_explain_flag & global_analyze)
				{
					bufcat(&buffer, "EXPLAIN ANALYZE ");
				}
				else if (global_explain_flag)
				{
					bufcat(&buffer, "EXPLAIN ");
				}

				bufcat(&buffer, "SELECT * FROM ");
				bufcat(&buffer, olist->object->name);

				/*
				 * add the DISTRIBUTED BY clause for output tables
				 * OR, the ORDER BY clause for other output formats
				 */
				if (output && output->u.output.type == MAPRED_OUTPUT_TABLE)
				{
					/*
					 * If there are no key columns then leave off the
					 * distributed by clause and let the server choose.
					 */
					if (exists)
						bufcat(&buffer, ")");

					else if (olist->object->u.task.grouping)
					{
						bufcat(&buffer, " DISTRIBUTED BY (");
						columns = olist->object->u.task.grouping;
						while (columns)
						{
							bufcat(&buffer, columns->name);
							if (columns->next)
								bufcat(&buffer, ", ");
							columns = columns->next;
						}
						bufcat(&buffer, ")");
					}
					else
					{
						/*
						 * don't have any hints for what the distribution keys
						 * should be, so we do nothing and let the database
						 * decide
						 */
					}
				}
				else
				{
					if (olist->object->u.task.returns ||
						olist->object->u.task.grouping)
					{
						bufcat(&buffer, " ORDER BY ");
						columns = olist->object->u.task.grouping;
						while (columns)
						{
							bufcat(&buffer, columns->name);
							if (columns->next || olist->object->u.task.returns)
								bufcat(&buffer, ", ");
							columns = columns->next;
						}
						columns = olist->object->u.task.returns;
						while (columns)
						{
							bufcat(&buffer, columns->name);
							if (columns->next)
								bufcat(&buffer, ", ");
							columns = columns->next;
						}
					}
				}
				bufcat(&buffer, ";\n");

				/* Tell the user what job we are running */
				if (global_verbose_flag)
					fprintf(stderr, "  - RUN: ");
				if (global_print_flag || global_debug_flag)
					fprintf(stderr, "%s", buffer->buffer);
				else
					fprintf(stderr, "%s\n", olist->object->name);

				/* But we only execute it if we are not in "print-only" mode */
				if (!global_print_flag)
				{
					/* If we have an output file, open it for write now */
					if (output && output->u.output.type == MAPRED_OUTPUT_FILE)
					{
						switch (output->u.output.mode)
						{
							case MAPRED_OUTPUT_MODE_NONE:
								/* check if the file exists */
								if (access(output->u.output.desc, F_OK) == 0)
								{
									mapred_obj_error(output, "file '%s' already exists",
													 output->u.output.desc);
									XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
								}
								/* Fallthrough */

							case MAPRED_OUTPUT_MODE_REPLACE:
								outfile = fopen(output->u.output.desc, "wb");
								break;

							case MAPRED_OUTPUT_MODE_APPEND:
								outfile = fopen(output->u.output.desc, "ab");
								break;

							default:
								XASSERT(false);
						}

						if (!outfile)
						{
							mapred_obj_error(output, "could not open file '%s' for write",
											 output->u.output.desc);
							XRAISE(MAPRED_PARSE_ERROR, "Object creation Failure");
						}
					}
					else
					{
						outfile = stdout;
					}

					/*
					 * Enable notices for user queries since they may contain
					 * debugging info.
					 */
					PQsetNoticeReceiver(conn, print_notice_handler, NULL);
					result = PQexec(conn, buffer->buffer);
					PQsetNoticeReceiver(conn, ignore_notice_handler, NULL);
					switch (PQresultStatus(result))
					{
						/* Output is STDOUT or FILE */
						case  PGRES_TUPLES_OK:
						{
							PQprintOpt options;
							memset(&options, 0, sizeof(options));

							/*
							 * Formatting:
							 *   STDOUT = fancy formatting
							 *   FILE   = plain formatting
							 */
							if (outfile == stdout)
							{
								options.header = true;
								options.align  = true;
								options.fieldSep  = "|";
							}
							else if (output->u.output.delimiter)
							{
								options.fieldSep = output->u.output.delimiter;
							}
							else
							{
								/* "\t" is our default delimiter */
								options.fieldSep  = "\t";
							}

							PQprint(outfile, result, &options);
							break;
						}

						/* OUTPUT is a table */
						case PGRES_COMMAND_OK:
							fprintf(stderr, "DONE\n");
							break;

						/* An error of some kind */
						default:
							XRAISE(MAPRED_SQL_ERROR, "Execution Failure");
					}
					PQclear(result);
					result = NULL;

					if (NULL != outfile && outfile != stdout)
					{
						fclose(outfile);
						outfile = stdout;
					}
				}
			}
		}
	}
	XFINALLY
	{
		if (result)
			PQclear(result);

		if (NULL != outfile && outfile != stdout)
		{
			fclose(outfile);
			outfile = stdout;
		}

		if (buffer)
			mapred_free(buffer);
	}
	XTRY_END;
}


boolean mapred_create_object(PGconn *conn, mapred_document_t *doc,
							 mapred_object_t *obj)
{
	mapred_clist_t *clist    = NULL;
	mapred_plist_t *plist    = NULL;
	mapred_plist_t *plist2   = NULL;
	const char     *ckind    = NULL;
	buffer_t       *buffer   = NULL;
	buffer_t       *qbuffer  = NULL;
	PGresult       *result   = NULL;

	/* If the object was created in a prior pass, then do nothing */
	if (obj->created)
		return true;

	/* Otherwise attempt to create the object */
	XTRY
	{
		/* allocates 1024 bytes, extending by 1024 bytes if we run out */
		buffer = makebuffer(1024, 1024);

		switch (obj->kind)
		{
			case MAPRED_INPUT:
				XASSERT(obj->name);

				switch (obj->u.input.type)
				{
					case MAPRED_INPUT_TABLE:
						/* Nothing to actually create */
						obj->created = true;
						break;

					case MAPRED_INPUT_FILE:
					case MAPRED_INPUT_GPFDIST:
						XASSERT(obj->u.input.files);

						/* Allocate and produce buffer */
						bufcat(&buffer, "CREATE EXTERNAL TABLE ");
						bufcat(&buffer, doc->prefix);
						bufcat(&buffer, obj->name);
						bufcat(&buffer, "(");
						for (plist = obj->u.input.columns;
							 plist;
							 plist = plist->next)
						{
							bufcat(&buffer, plist->name);
							bufcat(&buffer, " ");
							bufcat(&buffer, plist->type);
							if (plist->next)
								bufcat(&buffer, ", ");
						}
						bufcat(&buffer, ")\n");
						bufcat(&buffer, "  LOCATION(");
						for (clist = obj->u.input.files;
							 clist;
							 clist = clist->next)
						{
							char *domain_port, *path, *p = NULL;

							if (obj->u.input.type == MAPRED_INPUT_GPFDIST)
								bufcat(&buffer, "'gpfdist://");
							else
								bufcat(&buffer, "'file://");

							/*
							 * The general syntax of a URL is    scheme://domain:port/path?query_string#fragment_id
							 * clist->value should contain just           domain:port/path?query_string#fragment_id
							 */
							p = strchr(clist->value, '/');
							if (p == NULL)
							{
								mapred_obj_error(obj, "Failed to find '/' indicating start of path (%s)",
												 clist->value);
								XRAISE(MAPRED_PARSE_ERROR,
									   "Invalid INPUT source specification");
							}
							if (p == clist->value)
							{
								mapred_obj_error(obj, "Missing domain and port before '/' indicating start of path (%s)",
												 clist->value);
								XRAISE(MAPRED_PARSE_ERROR,
									   "Invalid INPUT source specification");
							}
							domain_port = clist->value;
							path        = p+1;

							/*
							 * Overwrite the / separating the domain:port from the path
							 * with a nul and move back one byte to check for a trailing ':'.
							 * We put the / back in when copying into the destination buffer.
							 */
							*p-- = '\0';

							if (strlen(path) < 1)
							{
								mapred_obj_error(obj, "Missing path after '/' (%s)",
												 clist->value);
								XRAISE(MAPRED_PARSE_ERROR,
									   "Invalid INPUT source specification");
							}

							/*
							 * We allow a trailing ':'  (e.g. host:/filepath)
							 * but we must not copy it into the external table url.
							 */
							if (*p == ':')
								*p = '\0';

							if (strlen(domain_port) < 1)
							{
								mapred_obj_error(obj, "Missing host before '/' (%s)",
												 clist->value);
								XRAISE(MAPRED_PARSE_ERROR,
									   "Invalid INPUT source specification");
							}

							bufcat(&buffer, domain_port);
							bufcat(&buffer, "/");
							bufcat(&buffer, path);
							if (clist->next)
								bufcat(&buffer, "',\n           ");
						}
						bufcat(&buffer, "')\n");
						if (obj->u.input.format == MAPRED_FORMAT_CSV)
							bufcat(&buffer, "  FORMAT 'CSV'");
						else
							bufcat(&buffer, "  FORMAT 'TEXT'");
						if (obj->u.input.delimiter ||
							obj->u.input.escape    ||
							obj->u.input.quote     ||
							obj->u.input.null)
						{
							bufcat(&buffer, " ( ");
							if (obj->u.input.delimiter)
							{
								bufcat(&buffer, "DELIMITER '");
								bufcat(&buffer, obj->u.input.delimiter);
								bufcat(&buffer, "' ");
							}
							if (obj->u.input.escape)
							{
								bufcat(&buffer, "ESCAPE '");
								bufcat(&buffer, obj->u.input.escape);
								bufcat(&buffer, "' ");
							}
							if (obj->u.input.quote)
							{
								bufcat(&buffer, "QUOTE '");
								bufcat(&buffer, obj->u.input.quote);
								bufcat(&buffer, "' ");
							}
							if (obj->u.input.null)
							{
								bufcat(&buffer, "NULL '");
								bufcat(&buffer, obj->u.input.null);
								bufcat(&buffer, "' ");
							}
							bufcat(&buffer, ")");
						}

						if (obj->u.input.error_limit > 0)
						{
							char intbuf[11];
							snprintf(intbuf, 11, "%d",
									 obj->u.input.error_limit);

							bufcat(&buffer, "\n  SEGMENT REJECT LIMIT ");
							bufcat(&buffer, intbuf);
						}

						bufcat(&buffer, ";\n\n");
						break;

					case MAPRED_INPUT_EXEC:
						XASSERT(obj->u.input.desc);

						bufcat(&buffer, "CREATE EXTERNAL WEB TABLE ");
						bufcat(&buffer, doc->prefix);
						bufcat(&buffer, obj->name);
						bufcat(&buffer, "(");
						for (plist = obj->u.input.columns;
							 plist;
							 plist = plist->next)
						{
							bufcat(&buffer, plist->name);
							bufcat(&buffer, " ");
							bufcat(&buffer, plist->type);
							if (plist->next)
								bufcat(&buffer, ", ");
						}
						bufcat(&buffer, ")\n");
						bufcat(&buffer, "EXECUTE '");
						bufcat(&buffer, obj->u.input.desc);
						bufcat(&buffer, "'\n");
						if (obj->u.input.format == MAPRED_FORMAT_CSV)
							bufcat(&buffer, "  FORMAT 'CSV'");
						else
							bufcat(&buffer, "  FORMAT 'TEXT'");
						if (obj->u.input.delimiter ||
							obj->u.input.quote     ||
							obj->u.input.null)
						{
							bufcat(&buffer, " ( ");
							if (obj->u.input.delimiter)
							{
								bufcat(&buffer, "DELIMITER '");
								bufcat(&buffer, obj->u.input.delimiter);
								bufcat(&buffer, "' ");
							}
							if (obj->u.input.quote)
							{
								bufcat(&buffer, "QUOTE '");
								bufcat(&buffer, obj->u.input.quote);
								bufcat(&buffer, "' ");
							}
							if (obj->u.input.null)
							{
								bufcat(&buffer, "NULL '");
								bufcat(&buffer, obj->u.input.null);
								bufcat(&buffer, "' ");
							}
							bufcat(&buffer, ")");
						}

						if (obj->u.input.error_limit > 0)
						{
							char intbuf[11];
							snprintf(intbuf, 11, "%d",
									 obj->u.input.error_limit);

							bufcat(&buffer, "\n  SEGMENT REJECT LIMIT ");
							bufcat(&buffer, intbuf);
						}
						bufcat(&buffer, ";\n\n");
						break;

					case MAPRED_INPUT_QUERY:
						XASSERT(obj->u.input.desc);

						/*
						 *  CREATE TEMPORARY VIEW <name> AS
						 *  <desc>;
						 */
						bufcat(&buffer, "CREATE TEMPORARY VIEW ");
						bufcat(&buffer, obj->name);
						bufcat(&buffer, " AS\n");
						bufcat(&buffer, obj->u.input.desc);
						bufcat(&buffer, ";\n\n");
						break;

					case MAPRED_INPUT_NONE:
					default:
						XASSERT(false);
				}
				if (global_print_flag || global_debug_flag)
					printf("-- INPUT %s\n", obj->name);
				break;

			case MAPRED_OUTPUT:
				/*
				 * Outputs have no backend objects created directly.
				 * For output tables we may issue a create table as
				 * select, but that occurs at run-time.
				 */
				obj->created = true;
				mapred_setup_columns(conn, obj);
				break;

				/*
				 * The function types have different defaults and generate
				 * slightly different error messages, but basically do the
				 * same thing.
				 */
			case MAPRED_MAPPER:
			case MAPRED_TRANSITION:
			case MAPRED_COMBINER:
			case MAPRED_FINALIZER:
				ckind = mapred_kind_name[obj->kind];

				XASSERT(obj->name);

				/*
				 * 'kind' specific initialization accomplished above, now handle
				 * the generic function creation.
				 */
				if (global_print_flag || global_debug_flag)
					printf("-- %s %s\n", ckind, obj->name);

				/*
				 * Nothing to do if we already looked up the function in the
				 * catalog.
				 */
				if (obj->internal)
				{
					obj->created = true;
					break;
				}

				/* Non-internal functions should have these defined */
				XASSERT(obj->u.function.body);
				XASSERT(obj->u.function.language);

				mapred_setup_columns(conn, obj);
				XASSERT(obj->u.function.parameters);
				XASSERT(obj->u.function.rtype.name);
				XASSERT(NULL == obj->u.function.internal_returns);

				/*
				 * fill in the buffer:
				 *
				 *    CREATE FUNCTION <name>(<parameters>)
				 *    RETURNS [SETOF] <rtype> LANGUAGE <lang> AS
				 *    $$
				 *    <body>
				 *    $$ [STRICT] [IMMUTABLE];
				 *
				 */
				bufcat(&buffer, "CREATE FUNCTION ");
				bufcat(&buffer, doc->prefix);
				bufcat(&buffer, obj->name);
				bufcat(&buffer, "(");

				/* Handle parameter list */
				for (plist = obj->u.function.parameters;
					 plist;
					 plist = plist->next)
				{
					bufcat(&buffer, plist->name);
					bufcat(&buffer, " ");
					bufcat(&buffer, plist->type);
					if (plist->next)
						bufcat(&buffer, ", ");
				}

				/* Handle Return clause */
				bufcat(&buffer, ")\nRETURNS ");
				if (obj->u.function.mode == MAPRED_MODE_MULTI)
					bufcat(&buffer, "SETOF ");
				bufcat(&buffer, obj->u.function.rtype.name);

				/*
				 * Handle LANGUAGE clause, every langauge but 'C' and 'SQL'
				 * has 'pl' prefixing it
				 */
				if (!strcasecmp("C", obj->u.function.language) ||
					!strcasecmp("SQL", obj->u.function.language) ||
					!strncasecmp("PL", obj->u.function.language, 2))
				{
					bufcat(&buffer, " LANGUAGE ");
					bufcat(&buffer, obj->u.function.language);
				}
				else
				{
					bufcat(&buffer, " LANGUAGE pl");
					bufcat(&buffer, obj->u.function.language);
				}

				/* python only has an untrusted form */
				if (!strcasecmp("python", obj->u.function.language))
					bufcat(&buffer, "u");


				bufcat(&buffer, " AS ");

				/*
				 * Handle procedural language specific formatting for the
				 * function definition.
				 *
				 * C language functions are defined using the two parameter
				 * form:  AS "library", "function".
				 *
				 * Perl functions append the yaml file line number via a
				 *  #line declaration.
				 *
				 * Python functions try to append the yaml file line number
				 * by inserting a bunch of newlines.  (only works for runtime
				 * errors, not compiletime errors).
				 */
				if (!strcasecmp("C", obj->u.function.language))
				{
					bufcat(&buffer, "$$");
					bufcat(&buffer, obj->u.function.library);
					bufcat(&buffer, "$$, $$");
					bufcat(&buffer, obj->u.function.body);
					bufcat(&buffer, "$$");
				}
				else if (!strncasecmp("plperl", obj->u.function.language, 6) ||
						 !strncasecmp("perl",   obj->u.function.language, 4))
				{
					char lineno[10];
					snprintf(lineno, sizeof(lineno), "%d", obj->u.function.lineno);
					bufcat(&buffer, "$$\n#line ");
					bufcat(&buffer, lineno);
					bufcat(&buffer, "\n");
					bufcat(&buffer, obj->u.function.body);
					if (buffer->buffer[buffer->position-1] != '\n')
						bufcat(&buffer, "\n");
					bufcat(&buffer, "$$");
				}
				else if (!strncasecmp("plpython", obj->u.function.language, 8) ||
						 !strncasecmp("python",   obj->u.function.language, 6))
				{
					/*
					 * Python very stubborn about not letting you manually
					 * adjust line number.  So instead we take the stupid route
					 * and just insert N newlines.
					*/
					int i;
					bufcat(&buffer, "$$\n");
					for (i = 1; i < obj->u.function.lineno-2; i++)
						bufcat(&buffer, "\n");
					bufcat(&buffer, obj->u.function.body);
					if (buffer->buffer[buffer->position-1] != '\n')
						bufcat(&buffer, "\n");
					bufcat(&buffer, "$$");
				}
				else
				{
					/* Some generic other language, take our best guess */
					bufcat(&buffer, "$$");
					bufcat(&buffer, obj->u.function.body);
					bufcat(&buffer, "$$");
				}

				/* Handle options */
				if (obj->u.function.flags & mapred_function_strict)
					bufcat(&buffer, " STRICT");
				if (obj->u.function.flags & mapred_function_immutable)
					bufcat(&buffer, " IMMUTABLE");

				/* All done */
				bufcat(&buffer, ";\n\n");
				break;

			case MAPRED_REDUCER:
			{
				mapred_object_t *transition = obj->u.reducer.transition.object;
				mapred_object_t *combiner   = obj->u.reducer.combiner.object;
				mapred_object_t *finalizer  = obj->u.reducer.finalizer.object;
				char *state;

				XASSERT(obj->name);
				XASSERT(transition);
				XASSERT(transition->name);

				/*
				 * If the reducer depends on an object that hasn't been created
				 * then return false, it will be resolved during a second pass
				 */
				if ((transition && !transition->created) ||
					(combiner   && !combiner->created)   ||
					(finalizer  && !finalizer->created))
				{
					if (global_print_flag && global_debug_flag)
						printf("-- deferring REDUCE %s\n", obj->name);
					break;
				}
				if (global_print_flag || global_debug_flag)
					printf("-- REDUCE %s\n", obj->name);

				/* Now, set things up to create the thing */
				mapred_setup_columns(conn, obj);
				plist = transition->u.function.parameters;
				XASSERT(plist);          /* state */
				XASSERT(plist->next);    /* parameters */

				if (obj->u.reducer.ordering)
					bufcat(&buffer, "CREATE ORDERED AGGREGATE ");
				else
					bufcat(&buffer, "CREATE AGGREGATE ");
				bufcat(&buffer, doc->prefix);
				bufcat(&buffer, obj->name);
				bufcat(&buffer, " (");

				/*
				 * Get the state type, and write out the aggregate parameters
				 * based on the parameter list of the transition function.
				 */
				plist = transition->u.function.parameters;
				state = plist->type;
				for (plist = plist->next; plist; plist = plist->next)
				{
					bufcat(&buffer, plist->type);
					if (plist->next)
						bufcat(&buffer, ", ");
				}
				bufcat(&buffer, ") (\n");
				bufcat(&buffer, "  stype = ");
				bufcat(&buffer, state);
				if (obj->u.reducer.initialize)
				{
					bufcat(&buffer, ",\n  initcond = '");
					bufcat(&buffer, obj->u.reducer.initialize);
					bufcat(&buffer, "'");
				}
				bufcat(&buffer, ",\n  sfunc = ");
				if (!transition->internal)
					bufcat(&buffer, doc->prefix);
				bufcat(&buffer, transition->name);
				if (combiner)
				{
					bufcat(&buffer, ",\n  prefunc = ");
					if (!combiner->internal)
						bufcat(&buffer, doc->prefix);
					bufcat(&buffer, combiner->name);
				}

				/*
				 * To handle set returning finalizers the finalizer is pushed
				 * into the task definition rather than being placed in the
				 * uda where it belongs.
				 */
				/*
				 if (obj->u.reducer.finalizer.name)
				 {
				    bufcat(&buffer, ",\n  finalfunc = ");
				    bufcat(&buffer, obj->u.reducer.finalizer.name);
				 }
				*/

				bufcat(&buffer, "\n);\n\n");
				break;
			}

			case MAPRED_TASK:
			case MAPRED_EXECUTION:
			{
				mapred_object_t *input    = obj->u.task.input.object;
				mapred_object_t *mapper   = obj->u.task.mapper.object;
				mapred_object_t *reducer  = obj->u.task.reducer.object;
				mapred_plist_t  *columns  = NULL;
				mapred_plist_t  *ingrouping = NULL;
				mapred_plist_t  *newitem  = NULL;
				mapred_plist_t  *grouping = NULL;
				mapred_plist_t  *last     = NULL;
				mapred_plist_t  *scan     = NULL;
				buffer_t        *swap;

				if (!obj->u.task.execute)
					XASSERT(obj->name);
				XASSERT(obj->u.task.input.name);

				if (!qbuffer)
					qbuffer = makebuffer(1024, 1024);
				else
					bufreset(qbuffer);

				/*
				 * If the task depends on an object that hasn't been created then
				 * return false, it will be resolved during a second pass
				 */
				if ((input   && !input->created)  ||
					(mapper  && !mapper->created) ||
					(reducer && !reducer->created))
				{
					if (global_print_flag && global_debug_flag)
					{
						if (obj->u.task.execute)
							printf("-- deferring EXECUTION\n");
						else
							printf("-- deferring TASK %s\n", obj->name);
					}
					break;
				}

				if (global_print_flag || global_debug_flag)
				{
					if (obj->u.task.execute)
						printf("-- EXECUTION\n");
					else
						printf("-- TASK %s\n", obj->name);
				}

				/*
				 * 1) Handle the INPUT, two cases:
				 *   1a) There is no MAP/REDUCE:  "SELECT * FROM <input>"
				 *   1b) There is a MAP and/or REDUCE:  "<input>"
				 */
				mapred_setup_columns(conn, obj);
				if (!obj->u.task.mapper.name && !obj->u.task.reducer.name)
				{
					/* Allocate the buffer for the input. */
					if (input->u.input.type == MAPRED_INPUT_TABLE)
					{
						bufcat(&qbuffer, "SELECT * FROM ");
						bufcat(&qbuffer, input->u.input.desc);
					}
					else
					{
						bufcat(&qbuffer, "SELECT * FROM ");
						bufcat(&qbuffer, input->name);
					}
				}
				else
				{
					/* Input is just the name or description of the input */
					if (input->u.input.type == MAPRED_INPUT_TABLE)
						bufcat(&qbuffer, input->u.input.desc);
					else
						bufcat(&qbuffer, input->name);
				}

				/*
				 * How we get the columns depends a bit on the input.
				 * Is the input actually an "MAPRED_INPUT" object, or is it
				 * a "MAPRED_TASK" object?
				 */
				switch (input->kind)
				{
					case MAPRED_INPUT:
						columns = input->u.input.columns;
						break;

					case MAPRED_TASK:
						columns    = input->u.task.returns;
						ingrouping = input->u.task.grouping;

						if (!columns)
						{
							mapred_obj_error(obj, "Unable to determine return "
											 "columns for TASK '%s'",
											 obj->u.task.input.name);
							XRAISE(MAPRED_PARSE_INTERNAL, NULL);
						}
						break;

					default:
						mapred_obj_error(obj, "SOURCE '%s' is not an INPUT or "
										 "TASK object",
										 obj->u.task.input.name);
						XRAISE(MAPRED_PARSE_ERROR, "Object creation Error");
						break;
				}
				XASSERT(columns);

				/*
				 * 2) Handle the MAPPER, two cases
				 *  2a) The Mapper returns an generated ADT that needs extraction
				 *           "SELECT key(m), ...
				 *            FROM (SELECT <map(...) as m FROM <input>) mapsubq
				 *  2b) The Mapper returns a single column:
				 *           "SELECT <map>(...) FROM <input>"
				 */
				XASSERT(mapper || !obj->u.task.mapper.name);
				if (mapper)
				{
					plist = mapper->u.function.returns;
					plist2 = mapper->u.function.internal_returns;
					XASSERT(plist);

					if (plist->next)
					{ /* 2a */
						bufcat(&buffer, "SELECT ");
						for (; plist; plist = plist->next )
						{
							if( obj->internal )
							{
								XASSERT(plist2 != NULL);
								bufcat(&buffer, plist2->name);
								plist2 = plist2->next;
							}
							else
							{
								bufcat(&buffer, plist->name);
							}
							bufcat(&buffer, "(m)");
							bufcat(&buffer, " as ");
							bufcat(&buffer, plist->name);
							if (plist->next)
								bufcat(&buffer, ", ");
						}
						bufcat(&buffer, "\nFROM (");
					}

					/* shared code */
					bufcat(&buffer, "SELECT ");
					if (!mapper->internal)
						bufcat(&buffer, doc->prefix);
					bufcat(&buffer, mapper->name);
					bufcat(&buffer, "(");
					plist = mapper->u.function.parameters;
					for (; plist; plist = plist->next)
					{
						/* Check if this parameter is one of the input columns */
						for (scan = columns; scan; scan = scan->next)
							if (!strcasecmp(plist->name, scan->name))
							{
								bufcat(&buffer, plist->name);
								break;
							}

						/* Task inputs also need to scan the grouping columns */
						if (!scan)
							for (scan = ingrouping; scan; scan = scan->next)
								if (!strcasecmp(plist->name, scan->name))
								{
									bufcat(&buffer, plist->name);
									break;
								}

						/* Check if this parameter is in global_plist */
						if (!scan)
							for (scan = global_plist; scan; scan = scan->next)
								if (!strcasecmp(plist->name, scan->name))
								{
									/*
									 * (HACK)
									 * Note that global_plist overloads the
									 * plist structure using the "type" field
									 * to store "value".
									 * (HACK)
									 */
									bufcat(&buffer, "'");
									bufcat(&buffer, scan->type);
									bufcat(&buffer, "'::");
									bufcat(&buffer, plist->type);
									break;
								}


						/*
						 * If we couldn't find it issue a warning and
						 * set to NULL
						 */
						if (!scan)
						{
							if (global_verbose_flag)
								fprintf(stderr, "       ");
							fprintf(stderr,
									"WARNING: unset parameter - "
									"%s(%s => NULL)\n",
									mapper->name, plist->name);
							bufcat(&buffer, "NULL::");
							bufcat(&buffer, plist->type);
						}

						/* Add a comma if there is another parameter */
						if (plist->next)
							bufcat(&buffer, ", ");
					}
					bufcat(&buffer, ") as ");

					/* break into cases again */
					plist = mapper->u.function.returns;
					if (plist->next)
					{  /* 2a */
						bufcat(&buffer, "m FROM ");
						bufcat(&buffer, qbuffer->buffer);
						bufcat(&buffer, ") mapxq\n");
						/*
						 * Need to work this through, it seems that m is true
						 * whenever a column is null, which is not the desired
						 * behavior.
						 *
						 * Look more closely at "grep" code for why we want it,
						 * and "oreilly" code for why we don't.
						 *
						 * For the moment the compromise is that we do it only
						 * for SINGLE mode functions, since MULTI mode can
						 * control it's own filtering without this.
						 */
						if (mapper->u.function.mode != MAPRED_MODE_MULTI)
							bufcat(&buffer, "WHERE m is not null");
					}
					else
					{
						bufcat(&buffer, plist->name);
						bufcat(&buffer, " FROM ");
						bufcat(&buffer, qbuffer->buffer);
					}

					/*
					 * Swap the buffer into the qbuffer for input as the next
					 * stage of the query pipeline.
					 */
					swap = qbuffer;
					qbuffer = buffer;
					buffer = swap;
					bufreset(buffer);

					/* Columns are now the output of the mapper */
					columns  = mapper->u.function.returns;
					ingrouping = NULL;
				}

				/*
				 * 3) Handle the Reducer, several sub-cases:
				 */
				if (obj->u.task.reducer.name)
				{
					/*
					 * Step 1:   Determine grouping columns
					 *    Find which columns are returned from the previous
					 *    stage that are NOT parameters to the reducer.
					 */
					grouping = last = NULL;
					if (!reducer)
					{
						/*
						 * We have a reducer, but it isn't listed in the YAML.
						 * How to work out parameter handling still needs to
						 * be worked out.  For now we just assume that this
						 * sort of function always takes a single "value" column
						 * and returns a "value" column.
						 */
						for (plist = columns; plist; plist = plist->next)
						{
							if (strcasecmp(plist->name, "value"))
							{
								if (grouping)
								{
									last->next =
										mapred_malloc(sizeof(mapred_plist_t));
									last = last->next;
								}
								else
								{
									grouping =
										mapred_malloc(sizeof(mapred_plist_t));
									last = grouping;
								}
								last->name = plist->name;
								last->type = plist->type;
								last->next = NULL;
							}
						}
					}
					else
					{   /* The reducer exists in the YAML */

						/* We precalculated the grouping columns */
						grouping = obj->u.task.grouping;
					}

					/* Fill in the buffer */
					bufcat(&buffer, "SELECT ");
					for (plist = grouping; plist; plist = plist->next)
					{
						bufcat(&buffer, plist->name);
						bufcat(&buffer, ", ");
					}

					/* Call the aggregation function */
					if (reducer && !reducer->internal)
						bufcat(&buffer, doc->prefix);
					bufcat(&buffer, obj->u.task.reducer.name);
					bufcat(&buffer, "(");

					if (reducer)
					{
						plist = reducer->u.reducer.parameters;
						for (; plist; plist = plist->next)
						{
							/* Check if parameter is one of the input columns */
							for (scan = columns; scan; scan = scan->next)
								if (!strcasecmp(plist->name, scan->name))
								{
									bufcat(&buffer, plist->name);
									break;
								}

							/* Task inputs need to scan the grouping columns */
							if (!scan)
								for (scan = ingrouping; scan; scan = scan->next)
									if (!strcasecmp(plist->name, scan->name))
									{
										bufcat(&buffer, plist->name);
										break;
									}

							/* Check if this parameter is in global_plist */
							if (!scan)
							{
								for (scan = global_plist;
									 scan;
									 scan = scan->next)
								{
									if (!strcasecmp(plist->name, scan->name))
									{
										/*
										 * (HACK)
										 * Note that global_plist overloads the
										 * plist structure using the "type"
										 * field to store "value".
										 * (HACK)
										 */
										bufcat(&buffer, "'");
										bufcat(&buffer, scan->type);
										bufcat(&buffer, "'::");
										bufcat(&buffer, plist->type);
										break;
									}
								}
							}

							/*
							 * If we couldn't find it issue a warning
							 * and set to NULL
							 */
							if (!scan)
							{
								if (global_verbose_flag)
									fprintf(stderr, "       ");
								fprintf(stderr,
										"WARNING: unset parameter - "
										"%s(%s => NULL)\n",
										reducer->name, plist->name);
								bufcat(&buffer, "NULL::");
								bufcat(&buffer, plist->type);
							}
							if (plist->next)
								bufcat(&buffer, ", ");
						}

						/* Handle ORDERING, if specified */
						clist = reducer->u.reducer.ordering;
						if (clist)
							bufcat(&buffer, " ORDER BY ");
						for(; clist; clist = clist->next)
						{
							bufcat(&buffer, clist->value);
							if (clist->next)
								bufcat(&buffer, ", ");
						}

					}
					else
					{
						/*
						 * non-yaml reducer always takes "value" as the
						 * input column
						 */

						/* Check if "value" is one of the input columns */
						for (scan = columns; scan; scan = scan->next)
							if (!strcasecmp(scan->name, "value"))
							{
								bufcat(&buffer, "value");
								break;
							}

						/* Task inputs also need to scan the grouping columns */
						if (!scan)
							for (scan = ingrouping; scan; scan = scan->next)
								if (!strcasecmp(plist->name, scan->name))
								{
									bufcat(&buffer, "value");
									break;
								}

						/* Check if this parameter is in global_plist */
						if (!scan)
							for (scan = global_plist; scan; scan = scan->next)
								if (!strcasecmp(scan->name, "value"))
								{
									/*
									 * (HACK)
									 * Note that global_plist overloads the
									 * plist structure using the "type" field
									 * to store "value".
									 * (HACK)
									 */
									bufcat(&buffer, "'");
									bufcat(&buffer, scan->type);
									bufcat(&buffer, "'::");
									bufcat(&buffer, plist->type);
									break;
								}

						if (!scan)
						{
							if (global_verbose_flag)
								fprintf(stderr, "       ");
							fprintf(stderr,
									"WARNING: unset parameter - "
									"%s(value => NULL)\n",
									obj->u.task.reducer.name);
							bufcat(&buffer, "NULL");
						}
					}

					bufcat(&buffer, ") as ");

					if (reducer)
					{
						plist = reducer->u.reducer.returns;
						XASSERT(plist);  /* Need to have a return! */

						/*
						 * If the reducer has a finalizer we push it outside of
						 * the context of the UDA so that we can properly handle
						 * set returning/column returning functions.
						 */
						if (reducer->u.reducer.finalizer.name)
							bufcat(&buffer, "r");
						else
							bufcat(&buffer, plist->name);
					}
					else
					{
						/*
						 * non-yaml reducer always return a single column
						 * named "value"
						 */
						bufcat(&buffer, "value");
					}
					bufcat(&buffer, "\nFROM ");
					if (mapper)
					{
						bufcat(&buffer, "(");
						bufcat(&buffer, qbuffer->buffer);
						bufcat(&buffer, ") mapsubq");
					}
					else
					{
						bufcat(&buffer, qbuffer->buffer);
					}
					if (grouping)
					{
						bufcat(&buffer, "\nGROUP BY ");
						for (plist = grouping; plist; plist = plist->next)
						{
							bufcat(&buffer, plist->name);
							if (plist->next)
								bufcat(&buffer, ", ");
						}
					}

					/*
					 * Swap the buffer into the qbuffer for input as the next
					 * stage of the query pipeline.
					 */
					swap = qbuffer;
					qbuffer = buffer;
					buffer = swap;
					bufreset(buffer);

					/*
					 * Add the return columns to the grouping columns and set
					 * it to the current columns.
					 *
					 * Note that unlike the columns set by the mapper or the
					 * input this is a list that must be de-allocated.
					 */
					columns = last;

					/*
					 * If the reducer had a finalizer we push it into another
					 * nested subquery since user defined aggregates aren't
					 * allowed to return sets.
					 *
					 * NOTE: this code mostly duplicates the MAP code above
					 */
					if (reducer && reducer->u.reducer.finalizer.name)
					{
						mapred_object_t *finalizer;

						finalizer = reducer->u.reducer.finalizer.object;
						XASSERT(finalizer);  /* FIXME */
						XASSERT(finalizer->u.function.returns);

						/*
						 * If the finalizer returns multiple columns then we
						 * need an extra layer of wrapping to extract them.
						 */
						plist = finalizer->u.function.returns;
						if (plist->next)
						{
							bufcat(&buffer, "SELECT ");

							/* the grouping columns */
							for (plist = grouping;
								 plist;
								 plist = plist->next)
							{
								bufcat(&buffer, plist->name);
								bufcat(&buffer, ", ");
							}
							plist2 = finalizer->u.function.internal_returns;
							for (plist = finalizer->u.function.returns;
								 plist;
								 plist = plist->next)
							{
								if( finalizer->internal )
								{
									XASSERT( plist2 != NULL );
									bufcat(&buffer, plist2->name);
									plist2 = plist2->next;
								}
								else
								{
									bufcat(&buffer, plist->name);
								}

								bufcat(&buffer, "(r)");
								bufcat(&buffer, " as ");
								bufcat(&buffer, plist->name);
								if (plist->next)
									bufcat(&buffer, ", ");
							}
							bufcat(&buffer, "\nFROM (");
						}

						/*
						 * Call the function on the returned state from
						 * the reducer.
						 */
						bufcat(&buffer, "SELECT ");

						/* grouping columns */
						for (plist = grouping;
							 plist;
							 plist = plist->next)
						{
							bufcat(&buffer, plist->name);
							bufcat(&buffer, ", ");
						}

						if (!finalizer->internal)
							bufcat(&buffer, doc->prefix);
						bufcat(&buffer, finalizer->name);
						bufcat(&buffer, "(r) as ");

						/* break into cases again */
						plist = finalizer->u.function.returns;
						if (plist->next)
							bufcat(&buffer, "r");
						else
							bufcat(&buffer, plist->name);
						bufcat(&buffer, " FROM (");
						bufcat(&buffer, qbuffer->buffer);
						bufcat(&buffer, ") redxq\n");

						/*
						 * If we have that extra layer of wrapping
						 * then close it off
						 */
						if (finalizer->u.function.returns->next)
							bufcat(&buffer, ") redsubq\n");

						/*
						 * Swap the buffer into the qbuffer for input as the next
						 * stage of the query pipeline.
						 */
						swap = qbuffer;
						qbuffer = buffer;
						buffer = swap;
						bufreset(buffer);
					}
				}


				/*
				 * 4) Handle the final transform into the view definition:
				 *        "CREATE TEMPORARY VIEW . AS .;"
				 */
				bufcat(&buffer, "CREATE TEMPORARY VIEW ");
				bufcat(&buffer, obj->name);
				bufcat(&buffer, " AS\n");
				bufcat(&buffer, qbuffer->buffer);
				bufcat(&buffer, ";\n\n");

				/*
				 * If there was a reducer then we have to release the columns
				 * list, otherwise it is a pointer to an existing list and can
				 * be ignored.
				 */
				if (obj->u.task.reducer.name)
				{
					plist = columns;
					while (plist && plist != grouping)
					{
						newitem = plist;
						plist = plist->next;
						mapred_free(newitem);
					}
				}
				break;
			}

			case MAPRED_ADT:
				XASSERT(obj->name);
				mapred_setup_columns(conn, obj);

				/*
				 * ADT's have generated names that already include the
				 * document prefix
				 */
				bufcat(&buffer, "CREATE TYPE ");
				bufcat(&buffer, obj->name);
				bufcat(&buffer, " as (");
				for (plist = obj->u.adt.returns; plist; plist = plist->next)
				{
					bufcat(&buffer, plist->name);
					bufcat(&buffer, " ");
					bufcat(&buffer, plist->type);
					if (plist->next)
						bufcat(&buffer, ", ");
				}
				bufcat(&buffer, ");\n\n");
				break;

			default:
				XASSERT(false);
		}

		if (buffer->position > 0)
		{

			/*
			 * In print-only mode we do everything but run the queries
			 * ie, we still create and destroy objects.
			 */
			if (global_print_flag || global_debug_flag)
				printf("%s", buffer->buffer);

			/*
			 * Try to create the object, but failure should not terminate
			 * the transaction, so wrap it in a savepoint.
			 */
			PQexec(conn, "SAVEPOINT mapreduce_save");
			result = PQexec(conn, buffer->buffer);
			if (PQresultStatus(result) == PGRES_COMMAND_OK)
			{
				obj->created = true;
				PQexec(conn, "RELEASE SAVEPOINT mapreduce_save");
			}
			else
			{
				char *error = PQresultErrorField(result, PG_DIAG_SQLSTATE);

				/* rollback to savepoint */
				PQexec(conn, "ROLLBACK TO SAVEPOINT mapreduce_save");
				PQexec(conn, "RELEASE SAVEPOINT mapreduce_save");

				/*
				 * If we have an "object does not exist" error from a SQL input
				 * then it may just be a dependency issue, so we don't error
				 * right away.
				 */
				if (obj->kind != MAPRED_INPUT ||
					obj->u.input.type != MAPRED_INPUT_QUERY ||
					strcmp(error, OBJ_DOES_NOT_EXIST))
				{
					if (global_verbose_flag)
						fprintf(stderr, "     - ");
					fprintf(stderr, "%s", PQresultErrorMessage(result));
					XRAISE(MAPRED_SQL_ERROR, "Object creation Failure");
				}
				if (global_verbose_flag)
					fprintf(stderr, "       Error: %s\n",
							PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY));

				/*
				 * If it is an error that we think we can recover from then we don't
				 * log the error immediately, but write it to a buffer in the event
				 * that recovery wasn't successful.
				 */
				if (doc->errors)
				{
					if (global_verbose_flag)
						bufcat(&doc->errors, "  - ");
					bufcat(&doc->errors, "Error: ");
					bufcat(&doc->errors, (char*) mapred_kind_name[obj->kind]);
					if (obj->name)
					{
						bufcat(&doc->errors, " '");
						bufcat(&doc->errors, obj->name);
						bufcat(&doc->errors, "'");
					}
					bufcat(&doc->errors, ": ");
					bufcat(&doc->errors, PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY));
					if (obj->line > 0)
					{
						char numbuf[64];
						sprintf(numbuf, ", at line %d", obj->line);
						bufcat(&doc->errors, numbuf);
					}
					bufcat(&doc->errors, "\n");
				}
			}
		}

		/*
		 * INPUTS setup columns AFTER creation.
		 * All other objects handle it above prior to creation.
		 */
		if (obj->kind == MAPRED_INPUT && obj->created)
			mapred_setup_columns(conn, obj);
	}
	XFINALLY
	{
		if (buffer)
			mapred_free(buffer);
		if (qbuffer)
			mapred_free(qbuffer);
	}
	XTRY_END;

	return obj->created;
}


void mapred_remove_object(PGconn *conn, mapred_document_t *doc, mapred_object_t *obj)
{
	mapred_plist_t *plist    = NULL;
	const char     *ckind    = NULL;
	buffer_t       *buffer   = NULL;


	/* If the object wasn't created, then do nothing */
	if (!obj->created)
		return;

	if (obj->internal)
	{
		obj->created = false;
		return;
	}

	XTRY
	{
		buffer = makebuffer(100, 100);

		/* Otherwise attempt to create the object */
		switch (obj->kind)
		{
			case MAPRED_INPUT:
				XASSERT(obj->name);

				switch (obj->u.input.type)
				{
					case MAPRED_INPUT_TABLE:
						obj->created = false;
						break;  /* do nothing */

					case MAPRED_INPUT_FILE:
					case MAPRED_INPUT_GPFDIST:
					case MAPRED_INPUT_EXEC:
						bufcat(&buffer, "DROP EXTERNAL TABLE IF EXISTS ");
						bufcat(&buffer, doc->prefix);
						bufcat(&buffer, obj->name);
						bufcat(&buffer, " CASCADE;\n");
						break;

					case MAPRED_INPUT_QUERY:
						bufcat(&buffer, "DROP VIEW IF EXISTS ");
						bufcat(&buffer, obj->name);
						bufcat(&buffer, " CASCADE;\n");
						break;

					case MAPRED_INPUT_NONE:
					default:
						XASSERT(false);
				}
				break;

			case MAPRED_OUTPUT:
				/* nothing to do for outputs */
				obj->created = false;
				break;

				/*
				 * The function types have different defaults and generate
				 * slightly different error messages, but basically do the
				 * same thing.
				 */
			case MAPRED_MAPPER:
			case MAPRED_TRANSITION:
			case MAPRED_COMBINER:
			case MAPRED_FINALIZER:
				ckind = mapred_kind_name[obj->kind];

				bufcat(&buffer, "DROP FUNCTION IF EXISTS ");
				if (!obj->internal)
					bufcat(&buffer, doc->prefix);
				bufcat(&buffer, obj->name);
				bufcat(&buffer, "(");
				for (plist = obj->u.function.parameters;
					 plist;
					 plist = plist->next)
				{  /* Handle parameter list */
					bufcat(&buffer, plist->type);
					if (plist->next)
						bufcat(&buffer, ", ");
				}
				bufcat(&buffer, ") CASCADE;\n");
				break;


			case MAPRED_REDUCER:
			{
				mapred_object_t  *transition = obj->u.reducer.transition.object;

				XASSERT(obj->name);
				XASSERT(transition);   /* is this a good assumption? */

				if (!transition->u.function.parameters)
					break;  /* FIXME */
				XASSERT(transition->u.function.parameters);

				bufcat(&buffer, "DROP AGGREGATE IF EXISTS ");
				if (!obj->internal)
					bufcat(&buffer, doc->prefix);
				bufcat(&buffer, obj->name);
				bufcat(&buffer, "(");

				/*
				 * The first parameter of the transition function is the 'state'
				 * and is not listed as a parameter of the reducer, but all the
				 * rest of the parameters are
				 */
				plist = transition->u.function.parameters;
				for (plist = plist->next; plist; plist = plist->next)
				{
					bufcat(&buffer, plist->type);
					if (plist->next)
						bufcat(&buffer, ", ");
				}
				bufcat(&buffer, ") CASCADE;\n");
				break;
			}

			case MAPRED_TASK:
			case MAPRED_EXECUTION:
				XASSERT(obj->name);

				bufcat(&buffer, "DROP VIEW IF EXISTS ");
				bufcat(&buffer, obj->name);
				bufcat(&buffer, " CASCADE;\n");
				break;

			case MAPRED_ADT:
				bufcat(&buffer, "DROP TYPE IF EXISTS ");
				bufcat(&buffer, obj->name);
				bufcat(&buffer, " CASCADE;\n");
				break;

			default:
				XASSERT(false);
		}

		if (buffer->position > 0)
		{
			PGresult   *result;

			/*
			 * In print-only mode we do everything but run the queries
			 * ie, we still create and destroy objects.
			 */
			if (global_print_flag || global_debug_flag)
				printf("%s", buffer->buffer);

			/* Try to delete the object, but don't raise exception on error */
			result = PQexec(conn, buffer->buffer);
			if (PQresultStatus(result) == PGRES_COMMAND_OK)
				obj->created = false;
			else
			{
				char *error = PQresultErrorField(result, PG_DIAG_SQLSTATE);

				/*
				 * Errors that we can expect/ignore:
				 *
				 *     IN_FAILED_SQL_TRANSACTION -
				 *        another error has occured and the transaction was
				 *        aborted
				 *
				 */
				if (strcmp(error, IN_FAILED_SQL_TRANSACTION))
				{
					if (global_verbose_flag)
						fprintf(stderr, "    - ");
					if (obj->name)
						fprintf(stderr,
								"[WARNING] Error dropping '%s'\n", obj->name);
					else
						fprintf(stderr,
								"[WARNING] Error dropping unnamed object\n");
					if (global_verbose_flag)
						fprintf(stderr, "    - ");
					fprintf(stderr, "%s", PQerrorMessage(conn));
				}
			}
			PQclear(result);
		}
	}
	XFINALLY
	{
		if (buffer)
			mapred_free(buffer);
	}
	XTRY_END;
}
