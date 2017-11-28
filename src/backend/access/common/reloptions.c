/*-------------------------------------------------------------------------
 *
 * reloptions.c
 *	  Core support for relation options (pg_class.reloptions)
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/access/common/reloptions.c,v 1.27 2009/05/24 22:22:44 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gist_private.h"
#include "access/hash.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "catalog/pg_type.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/guc_tables.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "miscadmin.h"

/*
 * Contents of pg_class.reloptions
 *
 * To add an option:
 *
 * (i) decide on a type (integer, real, bool, string), name, default value,
 * upper and lower bounds (if applicable); for strings, consider a validation
 * routine.
 * (ii) add a record below (or use add_<type>_reloption).
 * (iii) add it to the appropriate options struct (perhaps StdRdOptions)
 * (iv) add it to the appropriate handling routine (perhaps
 * default_reloptions)
 * (v) don't forget to document the option
 *
 * Note that we don't handle "oids" in relOpts because it is handled by
 * interpretOidsOption().
 *
 * To add an GPDB option, please touch reloptions_gp.c rather than this file.
 */

static relopt_bool boolRelOpts[] =
{
#if 0
	{
		{
			"autovacuum_enabled",
			"Enables autovacuum in this relation",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		true
	},
	{
		{
			"fastupdate",
			"Enables \"fast update\" feature for this GIN index",
			RELOPT_KIND_GIN
		},
		true
	},
#endif
	/* list terminator */
	{ { NULL } }
};

static relopt_int intRelOpts[] =
{
	{
		{
			"fillfactor",
			"Packs table pages only to this percentage",
			RELOPT_KIND_HEAP
		},
		HEAP_DEFAULT_FILLFACTOR, HEAP_MIN_FILLFACTOR, 100
	},
	{
		{
			"fillfactor",
			"Packs btree index pages only to this percentage",
			RELOPT_KIND_BTREE
		},
		BTREE_DEFAULT_FILLFACTOR, BTREE_MIN_FILLFACTOR, 100
	},
	{
		{
			"fillfactor",
			"Packs hash index pages only to this percentage",
			RELOPT_KIND_HASH
		},
		HASH_DEFAULT_FILLFACTOR, HASH_MIN_FILLFACTOR, 100
	},
	{
		{
			"fillfactor",
			"Packs gist index pages only to this percentage",
			RELOPT_KIND_GIST
		},
		GIST_DEFAULT_FILLFACTOR, GIST_MIN_FILLFACTOR, 100
	},
#if 0
	{
		{
			"autovacuum_vacuum_threshold",
			"Minimum number of tuple updates or deletes prior to vacuum",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		50, 0, INT_MAX
	},
	{
		{
			"autovacuum_analyze_threshold",
			"Minimum number of tuple inserts, updates or deletes prior to analyze",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		50, 0, INT_MAX
	},
	{
		{
			"autovacuum_vacuum_cost_delay",
			"Vacuum cost delay in milliseconds, for autovacuum",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		20, 0, 100
	},
	{
		{
			"autovacuum_vacuum_cost_limit",
			"Vacuum cost amount available before napping, for autovacuum",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		200, 1, 10000
	},
	{
		{
			"autovacuum_freeze_min_age",
			"Minimum age at which VACUUM should freeze a table row, for autovacuum",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		100000000, 0, 1000000000
	},
	{
		{
			"autovacuum_freeze_max_age",
			"Age at which to autovacuum a table to prevent transaction ID wraparound",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		200000000, 100000000, 2000000000
	},
	{
		{
			"autovacuum_freeze_table_age",
			"Age at which VACUUM should perform a full table sweep to replace old Xid values with FrozenXID",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		}, 150000000, 0, 2000000000
	},
#endif
	/* list terminator */
	{ { NULL } }
};

static relopt_real realRelOpts[] =
{
#if 0
	{
		{
			"autovacuum_vacuum_scale_factor",
			"Number of tuple updates or deletes prior to vacuum as a fraction of reltuples",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		0.2, 0.0, 100.0
	},
	{
		{
			"autovacuum_analyze_scale_factor",
			"Number of tuple inserts, updates or deletes prior to analyze as a fraction of reltuples",
			RELOPT_KIND_HEAP | RELOPT_KIND_TOAST
		},
		0.1, 0.0, 100.0
	},
#endif
	/* list terminator */
	{ { NULL } }
};

static relopt_string stringRelOpts[] =
{
	/* list terminator */
	{ { NULL } }
};

static relopt_gen **relOpts = NULL;
static bits32 last_assigned_kind = RELOPT_KIND_LAST_DEFAULT;

static int		num_custom_options = 0;
static relopt_gen **custom_options = NULL;
static bool		need_initialization = true;

static void initialize_reloptions(void);
static void parse_one_reloption(relopt_value *option, char *text_str,
					int text_len, bool validate);

extern int num_reloptions_gp(void);
extern void initialize_reloptions_gp(void);
extern void validate_and_refill_options(StdRdOptions *result, relopt_value *options,
									int numoptions, relopt_kind kind, bool validate);

/*
 * initialize_reloptions
 * 		initialization routine, must be called before parsing
 *
 * Initialize the relOpts array and fill each variable's type and name length.
 */
static void
initialize_reloptions(void)
{
	int		i;
	int		j = 0;

	for (i = 0; boolRelOpts[i].gen.name; i++)
		j++;
	for (i = 0; intRelOpts[i].gen.name; i++)
		j++;
	for (i = 0; realRelOpts[i].gen.name; i++)
		j++;
	for (i = 0; stringRelOpts[i].gen.name; i++)
		j++;

	initialize_reloptions_gp();

	j += num_custom_options;

	if (relOpts)
		pfree(relOpts);
	relOpts = MemoryContextAlloc(TopMemoryContext,
								 (j + 1) * sizeof(relopt_gen *));

	j = 0;
	for (i = 0; boolRelOpts[i].gen.name; i++)
	{
		relOpts[j] = &boolRelOpts[i].gen;
		relOpts[j]->type = RELOPT_TYPE_BOOL;
		relOpts[j]->namelen = strlen(relOpts[j]->name);
		j++;
	}

	for (i = 0; intRelOpts[i].gen.name; i++)
	{
		relOpts[j] = &intRelOpts[i].gen;
		relOpts[j]->type = RELOPT_TYPE_INT;
		relOpts[j]->namelen = strlen(relOpts[j]->name);
		j++;
	}

	for (i = 0; realRelOpts[i].gen.name; i++)
	{
		relOpts[j] = &realRelOpts[i].gen;
		relOpts[j]->type = RELOPT_TYPE_REAL;
		relOpts[j]->namelen = strlen(relOpts[j]->name);
		j++;
	}

	for (i = 0; stringRelOpts[i].gen.name; i++)
	{
		relOpts[j] = &stringRelOpts[i].gen;
		relOpts[j]->type = RELOPT_TYPE_STRING;
		relOpts[j]->namelen = strlen(relOpts[j]->name);
		j++;
	}

	for (i = 0; i < num_custom_options; i++)
	{
		relOpts[j] = custom_options[i];
		j++;
	}

	/* add a list terminator */
	relOpts[j] = NULL;
}

/*
 * add_reloption_kind
 * 		Create a new relopt_kind value, to be used in custom reloptions by
 * 		user-defined AMs.
 */
relopt_kind
add_reloption_kind(void)
{
	/* don't hand out the last bit so that the enum's behavior is portable */
	if (last_assigned_kind >= RELOPT_KIND_MAX)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("user-defined relation parameter types limit exceeded")));
	last_assigned_kind <<= 1;
	return (relopt_kind) last_assigned_kind;
}

/*
 * add_reloption
 * 		Add an already-created custom reloption to the list, and recompute the
 * 		main parser table.
 */
static void
add_reloption(relopt_gen *newoption)
{
	static int		max_custom_options = 0;

	if (num_custom_options >= max_custom_options)
	{
		MemoryContext	oldcxt;

		oldcxt = MemoryContextSwitchTo(TopMemoryContext);

		if (max_custom_options == 0)
		{
			max_custom_options = 8;
			custom_options = palloc(max_custom_options * sizeof(relopt_gen *));
		}
		else
		{
			max_custom_options *= 2;
			custom_options = repalloc(custom_options,
									  max_custom_options * sizeof(relopt_gen *));
		}
		MemoryContextSwitchTo(oldcxt);
	}
	custom_options[num_custom_options++] = newoption;

	need_initialization = true;
}

/*
 * allocate_reloption
 * 		Allocate a new reloption and initialize the type-agnostic fields
 * 		(for types other than string)
 */
static relopt_gen *
allocate_reloption(bits32 kinds, int type, char *name, char *desc)
{
	MemoryContext	oldcxt;
	size_t			size;
	relopt_gen	   *newoption;

	Assert(type != RELOPT_TYPE_STRING);

	oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	switch (type)
	{
		case RELOPT_TYPE_BOOL:
			size = sizeof(relopt_bool);
			break;
		case RELOPT_TYPE_INT:
			size = sizeof(relopt_int);
			break;
		case RELOPT_TYPE_REAL:
			size = sizeof(relopt_real);
			break;
		default:
			elog(ERROR, "unsupported option type");
			return NULL;	/* keep compiler quiet */
	}

	newoption = palloc(size);

	newoption->name = pstrdup(name);
	if (desc)
		newoption->desc = pstrdup(desc);
	else
		newoption->desc = NULL;
	newoption->kinds = kinds;
	newoption->namelen = strlen(name);
	newoption->type = type;

	MemoryContextSwitchTo(oldcxt);

	return newoption;
}

/*
 * add_bool_reloption
 * 		Add a new boolean reloption
 */
void
add_bool_reloption(bits32 kinds, char *name, char *desc, bool default_val)
{
	relopt_bool	   *newoption;

	newoption = (relopt_bool *) allocate_reloption(kinds, RELOPT_TYPE_BOOL,
												   name, desc);
	newoption->default_val = default_val;

	add_reloption((relopt_gen *) newoption);
}

/*
 * add_int_reloption
 * 		Add a new integer reloption
 */
void
add_int_reloption(bits32 kinds, char *name, char *desc, int default_val,
				  int min_val, int max_val)
{
	relopt_int	   *newoption;

	newoption = (relopt_int *) allocate_reloption(kinds, RELOPT_TYPE_INT,
												  name, desc);
	newoption->default_val = default_val;
	newoption->min = min_val;
	newoption->max = max_val;

	add_reloption((relopt_gen *) newoption);
}

/*
 * add_real_reloption
 * 		Add a new float reloption
 */
void
add_real_reloption(bits32 kinds, char *name, char *desc, double default_val,
				  double min_val, double max_val)
{
	relopt_real	   *newoption;

	newoption = (relopt_real *) allocate_reloption(kinds, RELOPT_TYPE_REAL,
												   name, desc);
	newoption->default_val = default_val;
	newoption->min = min_val;
	newoption->max = max_val;

	add_reloption((relopt_gen *) newoption);
}

/*
 * add_string_reloption
 *		Add a new string reloption
 *
 * "validator" is an optional function pointer that can be used to test the
 * validity of the values.  It must elog(ERROR) when the argument string is
 * not acceptable for the variable.  Note that the default value must pass
 * the validation.
 */
void
add_string_reloption(bits32 kinds, char *name, char *desc, char *default_val,
					 validate_string_relopt validator)
{
	MemoryContext	oldcxt;
	relopt_string  *newoption;
	int				default_len = 0;

	oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	if (default_val)
		default_len = strlen(default_val);

	newoption = palloc0(sizeof(relopt_string) + default_len);

	newoption->gen.name = pstrdup(name);
	if (desc)
		newoption->gen.desc = pstrdup(desc);
	else
		newoption->gen.desc = NULL;
	newoption->gen.kinds = kinds;
	newoption->gen.namelen = strlen(name);
	newoption->gen.type = RELOPT_TYPE_STRING;
	newoption->validate_cb = validator;
	if (default_val)
	{
		strcpy(newoption->default_val, default_val);
		newoption->default_len = default_len;
		newoption->default_isnull = false;
	}
	else
	{
		newoption->default_val[0] = '\0';
		newoption->default_len = 0;
		newoption->default_isnull = true;
	}

	/* make sure the validator/default combination is sane */
	if (newoption->validate_cb)
		(newoption->validate_cb) (newoption->default_val);

	MemoryContextSwitchTo(oldcxt);

	add_reloption((relopt_gen *) newoption);
}

/*
 * Transform a relation options list (list of DefElem) into the text array
 * format that is kept in pg_class.reloptions.
 *
 * This is used for three cases: CREATE TABLE/INDEX, ALTER TABLE SET, and
 * ALTER TABLE RESET.  In the ALTER cases, oldOptions is the existing
 * reloptions value (possibly NULL), and we replace or remove entries
 * as needed.
 *
 * If ignoreOids is true, then we should ignore any occurrence of "oids"
 * in the list (it will be or has been handled by interpretOidsOption()).
 *
 * Note that this is not responsible for determining whether the options
 * are valid.
 *
 * Both oldOptions and the result are text arrays (or NULL for "default"),
 * but we declare them as Datums to avoid including array.h in reloptions.h.
 */
Datum
transformRelOptions(Datum oldOptions, List *defList,
					bool ignoreOids, bool isReset)
{
	Datum		result;
	ArrayBuildState *astate;
	ListCell   *cell;

	/* no change if empty list */
	if (defList == NIL)
		return oldOptions;

	/* We build new array using accumArrayResult */
	astate = NULL;

	/* Copy any oldOptions that aren't to be replaced */
	if (PointerIsValid(DatumGetPointer(oldOptions)))
	{
		ArrayType  *array = DatumGetArrayTypeP(oldOptions);
		Datum	   *oldoptions;
		int			noldoptions;
		int			i;

		Assert(ARR_ELEMTYPE(array) == TEXTOID);

		deconstruct_array(array, TEXTOID, -1, false, 'i',
						  &oldoptions, NULL, &noldoptions);

		for (i = 0; i < noldoptions; i++)
		{
			text	   *oldoption = DatumGetTextP(oldoptions[i]);
			char	   *text_str = VARDATA(oldoption);
			int			text_len = VARSIZE(oldoption) - VARHDRSZ;

			/* Search for a match in defList */
			foreach(cell, defList)
			{
				DefElem    *def = lfirst(cell);
				int			kw_len = strlen(def->defname);

				if (text_len > kw_len && text_str[kw_len] == '=' &&
					pg_strncasecmp(text_str, def->defname, kw_len) == 0)
					break;
			}
			if (!cell)
			{
				/* No match, so keep old option */
				astate = accumArrayResult(astate, oldoptions[i],
										  false, TEXTOID,
										  CurrentMemoryContext);
			}
		}
	}

	/*
	 * If CREATE/SET, add new options to array; if RESET, just check that the
	 * user didn't say RESET (option=val).  (Must do this because the grammar
	 * doesn't enforce it.)
	 */
	foreach(cell, defList)
	{
		DefElem    *def = lfirst(cell);

		if (isReset)
		{
			if (def->arg != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("RESET must not include values for parameters")));
		}
		else
		{
			text	   *t;
			const char *value;
			Size		len;

			if (ignoreOids && pg_strcasecmp(def->defname, "oids") == 0)
				continue;

			/*
			 * Flatten the DefElem into a text string like "name=arg". If we
			 * have just "name", assume "name=true" is meant.
			 */
			if (def->arg != NULL)
				value = defGetString(def);
			else
				value = "true";
			len = VARHDRSZ + strlen(def->defname) + 1 + strlen(value);
			/* +1 leaves room for sprintf's trailing null */
			t = (text *) palloc(len + 1);
			SET_VARSIZE(t, len);
			sprintf(VARDATA(t), "%s=%s", def->defname, value);

			astate = accumArrayResult(astate, PointerGetDatum(t),
									  false, TEXTOID,
									  CurrentMemoryContext);
		}
	}

	if (astate)
		result = makeArrayResult(astate, CurrentMemoryContext);
	else
		result = (Datum) 0;

	return result;
}

/*
 * Convert the text-array format of reloptions into a List of DefElem.
 * This is the inverse of transformRelOptions().
 */
List *
untransformRelOptions(Datum options)
{
	List	   *result = NIL;
	ArrayType  *array;
	Datum	   *optiondatums;
	int			noptions;
	int			i;

	/* Nothing to do if no options */
	if (!PointerIsValid(DatumGetPointer(options)))
		return result;

	array = DatumGetArrayTypeP(options);

	Assert(ARR_ELEMTYPE(array) == TEXTOID);

	deconstruct_array(array, TEXTOID, -1, false, 'i',
					  &optiondatums, NULL, &noptions);

	for (i = 0; i < noptions; i++)
	{
		char	   *s;
		char	   *p;
		Node	   *val = NULL;

		s = TextDatumGetCString(optiondatums[i]);
		p = strchr(s, '=');
		if (p)
		{
			*p++ = '\0';
			val = (Node *) makeString(pstrdup(p));
		}
		result = lappend(result, makeDefElem(pstrdup(s), val));
	}

	return result;
}


/*
 * Interpret reloptions that are given in text-array format.
 *
 * options is a reloption text array as constructed by transformRelOptions.
 * kind specifies the family of options to be processed.
 *
 * The return value is a relopt_value * array on which the options actually
 * set in the options array are marked with isset=true.  The length of this
 * array is returned in *numrelopts.  Options not set are also present in the
 * array; this is so that the caller can easily locate the default values.
 *
 * If there are no options of the given kind, numrelopts is set to 0 and NULL
 * is returned.
 *
 * Note: values of type int, bool and real are allocated as part of the
 * returned array.  Values of type string are allocated separately and must
 * be freed by the caller.
 */
relopt_value *
parseRelOptions(Datum options, bool validate, relopt_kind kind,
				int *numrelopts)
{
	relopt_value *reloptions;
	int			numoptions = 0;
	int			i;
	int			j;

	if (need_initialization)
		initialize_reloptions();

	/* Build a list of expected options, based on kind */

	for (i = 0; relOpts[i]; i++)
		if (relOpts[i]->kinds & kind)
			numoptions++;

	if (numoptions == 0)
	{
		*numrelopts = 0;
		return NULL;
	}

	reloptions = palloc(numoptions * sizeof(relopt_value));

	for (i = 0, j = 0; relOpts[i]; i++)
	{
		if (relOpts[i]->kinds & kind)
		{
			reloptions[j].gen = relOpts[i];
			reloptions[j].isset = false;
			j++;
		}
	}

	/* Done if no options */
	if (PointerIsValid(DatumGetPointer(options)))
	{
		ArrayType  *array;
		Datum	   *optiondatums;
		int			noptions;
		bool		isArrayToBeFreed = false;

		array = DatumGetArrayTypeP(options);

		Assert(ARR_ELEMTYPE(array) == TEXTOID);

		isArrayToBeFreed = (array != (ArrayType *)DatumGetPointer(options));

		deconstruct_array(array, TEXTOID, -1, false, 'i',
						  &optiondatums, NULL, &noptions);

		for (i = 0; i < noptions; i++)
		{
			text	   *optiontext = DatumGetTextP(optiondatums[i]);
			char	   *text_str = VARDATA(optiontext);
			int			text_len = VARSIZE(optiontext) - VARHDRSZ;
			int			j;

			/* Search for a match in reloptions */
			for (j = 0; j < numoptions; j++)
			{
				int			kw_len = reloptions[j].gen->namelen;

				if (text_len > kw_len && text_str[kw_len] == '=' &&
					pg_strncasecmp(text_str, reloptions[j].gen->name,
								   kw_len) == 0)
				{
					parse_one_reloption(&reloptions[j], text_str, text_len,
										validate);
					break;
				}
			}

			if (j >= numoptions && validate)
			{
				char	   *s;
				char	   *p;

				s = TextDatumGetCString(optiondatums[i]);
				p = strchr(s, '=');
				if (p)
					*p = '\0';
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("unrecognized parameter \"%s\"", s)));
			}
		}

		if (isArrayToBeFreed)
		{
			pfree(array);
		}

		pfree(optiondatums);
	}

	*numrelopts = numoptions;
	return reloptions;
}

/*
 * Subroutine for parseRelOptions, to parse and validate a single option's
 * value
 */
static void
parse_one_reloption(relopt_value *option, char *text_str, int text_len,
					bool validate)
{
	char	   *value;
	int			value_len;
	bool		parsed;
	bool		nofree = false;

	if (option->isset && validate)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("parameter \"%s\" specified more than once",
						option->gen->name)));

	value_len = text_len - option->gen->namelen - 1;
	value = (char *) palloc(value_len + 1);
	memcpy(value, text_str + option->gen->namelen + 1, value_len);
	value[value_len] = '\0';

	switch (option->gen->type)
	{
		case RELOPT_TYPE_BOOL:
			{
				parsed = parse_bool(value, &option->values.bool_val);
				if (validate && !parsed)
					ereport(ERROR,
							(errmsg("invalid value for boolean option \"%s\": %s",
									option->gen->name, value)));
			}
			break;
		case RELOPT_TYPE_INT:
			{
				relopt_int	*optint = (relopt_int *) option->gen;

				parsed = parse_int(value, &option->values.int_val, 0, NULL);
				if (validate && !parsed)
					ereport(ERROR,
							(errmsg("invalid value for integer option \"%s\": %s",
									option->gen->name, value)));
				if (validate && (option->values.int_val < optint->min ||
								 option->values.int_val > optint->max))
					ereport(ERROR,
							(errmsg("value %s out of bounds for option \"%s\"",
									value, option->gen->name),
							 errdetail("Valid values are between \"%d\" and \"%d\".",
									   optint->min, optint->max)));
			}
			break;
		case RELOPT_TYPE_REAL:
			{
				relopt_real	*optreal = (relopt_real *) option->gen;

				parsed = parse_real(value, &option->values.real_val);
				if (validate && !parsed)
					ereport(ERROR,
							(errmsg("invalid value for floating point option \"%s\": %s",
									option->gen->name, value)));
				if (validate && (option->values.real_val < optreal->min ||
								 option->values.real_val > optreal->max))
					ereport(ERROR,
							(errmsg("value %s out of bounds for option \"%s\"",
									value, option->gen->name),
							 errdetail("Valid values are between \"%f\" and \"%f\".",
									   optreal->min, optreal->max)));
			}
			break;
		case RELOPT_TYPE_STRING:
			{
				relopt_string   *optstring = (relopt_string *) option->gen;

				option->values.string_val = value;
				nofree = true;
				if (validate && optstring->validate_cb)
					(optstring->validate_cb) (value);
				parsed = true;
			}
			break;
		default:
			elog(ERROR, "unsupported reloption type %d", option->gen->type);
			break;
	}

	if (parsed)
		option->isset = true;
	if (!nofree)
		pfree(value);
}

/*
 * Given the result from parseRelOptions, allocate a struct that's of the
 * specified base size plus any extra space that's needed for string variables.
 *
 * "base" should be sizeof(struct) of the reloptions struct (StdRdOptions or
 * equivalent).
 */
void *
allocateReloptStruct(Size base, relopt_value *options, int numoptions)
{
	Size	size = base;
	int		i;

	for (i = 0; i < numoptions; i++)
		if (options[i].gen->type == RELOPT_TYPE_STRING)
			size += GET_STRING_RELOPTION_LEN(options[i]) + 1;

	return palloc0(size);
}

/*
 * Given the result of parseRelOptions and a parsing table, fill in the
 * struct (previously allocated with allocateReloptStruct) with the parsed
 * values.
 *
 * rdopts is the pointer to the allocated struct to be filled; basesize is
 * the sizeof(struct) that was passed to allocateReloptStruct.  options and
 * numoptions are parseRelOptions' output.  elems and numelems is the array
 * of elements to be parsed.  Note that when validate is true, it is expected
 * that all options are also in elems.
 */
void
fillRelOptions(void *rdopts, Size basesize, relopt_value *options,
			   int numoptions, bool validate, relopt_parse_elt *elems,
			   int numelems)
{
	int		i;
	int		offset = basesize;

	for (i = 0; i < numoptions; i++)
	{
		int		j;
		bool	found = false;

		for (j = 0; j < numelems; j++)
		{
			if (pg_strcasecmp(options[i].gen->name, elems[j].optname) == 0)
			{
				relopt_string *optstring;
				char   *itempos = ((char *) rdopts) + elems[j].offset;
				char   *string_val;

				switch (options[i].gen->type)
				{
					case RELOPT_TYPE_BOOL:
						*(bool *) itempos = options[i].isset ?
							options[i].values.bool_val :
							((relopt_bool *) options[i].gen)->default_val;
						break;
					case RELOPT_TYPE_INT:
						*(int *) itempos = options[i].isset ?
							options[i].values.int_val :
							((relopt_int *) options[i].gen)->default_val;
						break;
					case RELOPT_TYPE_REAL:
						*(double *) itempos = options[i].isset ?
							options[i].values.real_val :
							((relopt_real *) options[i].gen)->default_val;
						break;
					case RELOPT_TYPE_STRING:
						optstring = (relopt_string *) options[i].gen;
						if (options[i].isset)
							string_val = options[i].values.string_val;
						else if (!optstring->default_isnull)
							string_val = optstring->default_val;
						else
							string_val = NULL;

						if (string_val == NULL)
							*(int *) itempos = 0;
						else
						{
							strcpy((char *) rdopts + offset, string_val);
							*(int *) itempos = offset;
							offset += strlen(string_val) + 1;
						}
						break;
					default:
						elog(ERROR, "unrecognized reloption type %c",
							 options[i].gen->type);
						break;
				}
				found = true;
				break;
			}
		}
		if (validate && !found)
			elog(ERROR, "storate parameter \"%s\" not found in parse table",
				 options[i].gen->name);
	}
	SET_VARSIZE(rdopts, offset);
}


/*
 * Option parser for anything that uses StdRdOptions (i.e. fillfactor only)
 */
bytea *
default_reloptions(Datum reloptions, bool validate, relopt_kind kind)
{
	relopt_value   *options;
	StdRdOptions   *rdopts;
	int				numoptions;

	/* The type of columnstores are different in StdRdOptions and options */
	relopt_parse_elt tab[] = {
		{"fillfactor", RELOPT_TYPE_INT, offsetof(StdRdOptions, fillfactor)},
		{SOPT_APPENDONLY, RELOPT_TYPE_BOOL, offsetof(StdRdOptions, appendonly)},
		{SOPT_BLOCKSIZE, RELOPT_TYPE_INT, offsetof(StdRdOptions, blocksize)},
		{SOPT_COMPLEVEL, RELOPT_TYPE_INT, offsetof(StdRdOptions, compresslevel)},
		{SOPT_COMPTYPE, RELOPT_TYPE_STRING, offsetof(StdRdOptions, compresstype)},
		{SOPT_CHECKSUM, RELOPT_TYPE_BOOL, offsetof(StdRdOptions, checksum)},
		{SOPT_ORIENTATION, RELOPT_TYPE_STRING, offsetof(StdRdOptions, orientation)}
	};

	options = parseRelOptions(reloptions, validate, kind, &numoptions);

	/* if none set, we're done */
	if (numoptions == 0)
		return NULL;

	rdopts = allocateReloptStruct(sizeof(StdRdOptions), options, numoptions);

	fillRelOptions((void *) rdopts, sizeof(StdRdOptions), options, numoptions,
				   validate, tab, lengthof(tab));

	validate_and_refill_options(rdopts, options, numoptions, kind, validate);

	pfree(options);

	return (bytea *) rdopts;
}

/*
 * Parse options for heaps (and perhaps someday toast tables).
 */
bytea *
heap_reloptions(char relkind, Datum reloptions, bool validate)
{
	switch (relkind)
	{
#if 0
		case RELKIND_TOASTVALUE:
			return default_reloptions(reloptions, validate, RELOPT_KIND_TOAST);
#endif
		case RELKIND_RELATION:
			return default_reloptions(reloptions, validate, RELOPT_KIND_HEAP | RELOPT_KIND_AO);
		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
		case RELKIND_VIEW:
		case RELKIND_COMPOSITE_TYPE:
		case RELKIND_SEQUENCE:
			return default_reloptions(reloptions, validate, RELOPT_KIND_AOAUX);
		default:
			/* sequences, composite types and views are not supported */
			/*
			 * GPDB_84_MERGE_FIXME: Make sure GPDB now support sequences,
			 * composite and views already.
			 */
			return NULL;
	}
}


/*
 * Parse options for indexes.
 *
 *	amoptions	Oid of option parser
 *	reloptions	options as text[] datum
 *	validate	error flag
 */
bytea *
index_reloptions(RegProcedure amoptions, Datum reloptions, bool validate)
{
	FmgrInfo	flinfo;
	FunctionCallInfoData fcinfo;
	Datum		result;

	Assert(RegProcedureIsValid(amoptions));

	/* Assume function is strict */
	if (!PointerIsValid(DatumGetPointer(reloptions)))
		return NULL;

	/* Can't use OidFunctionCallN because we might get a NULL result */
	fmgr_info(amoptions, &flinfo);

	InitFunctionCallInfoData(fcinfo, &flinfo, 2, NULL, NULL);

	fcinfo.arg[0] = reloptions;
	fcinfo.arg[1] = BoolGetDatum(validate);
	fcinfo.argnull[0] = false;
	fcinfo.argnull[1] = false;

	result = FunctionCallInvoke(&fcinfo);

	if (fcinfo.isnull || DatumGetPointer(result) == NULL)
		return NULL;

	return DatumGetByteaP(result);
}
