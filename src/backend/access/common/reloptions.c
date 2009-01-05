/*-------------------------------------------------------------------------
 *
 * reloptions.c
 *	  Core support for relation options (pg_class.reloptions)
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/access/common/reloptions.c,v 1.13 2009/01/05 17:14:28 alvherre Exp $
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
 * (i) decide on a class (integer, real, bool, string), name, default value,
 * upper and lower bounds (if applicable).
 * (ii) add a record below.
 * (iii) add it to StdRdOptions if appropriate
 * (iv) add a block to the appropriate handling routine (probably
 * default_reloptions)
 * (v) don't forget to document the option
 *
 * Note that we don't handle "oids" in relOpts because it is handled by
 * interpretOidsOption().
 */

static relopt_bool boolRelOpts[] =
{
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
	/* list terminator */
	{ { NULL } }
};

static relopt_real realRelOpts[] =
{
	/* list terminator */
	{ { NULL } }
};

static relopt_string stringRelOpts[] = 
{
	/* list terminator */
	{ { NULL } }
};

static relopt_gen **relOpts = NULL;
static int last_assigned_kind = RELOPT_KIND_LAST_DEFAULT + 1;

static int		num_custom_options = 0;
static relopt_gen **custom_options = NULL;
static bool		need_initialization = true;

static void initialize_reloptions(void);
static void parse_one_reloption(relopt_value *option, char *text_str,
					int text_len, bool validate);

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
int
add_reloption_kind(void)
{
	if (last_assigned_kind >= RELOPT_KIND_MAX)
		ereport(ERROR,
				(errmsg("user-defined relation parameter types limit exceeded")));

	return last_assigned_kind++;
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
allocate_reloption(int kind, int type, char *name, char *desc)
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
	newoption->kind = kind;
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
add_bool_reloption(int kind, char *name, char *desc, bool default_val)
{
	relopt_bool	   *newoption;

	newoption = (relopt_bool *) allocate_reloption(kind, RELOPT_TYPE_BOOL,
												   name, desc);
	newoption->default_val = default_val;

	add_reloption((relopt_gen *) newoption);
}

/*
 * add_int_reloption
 * 		Add a new integer reloption
 */
void
add_int_reloption(int kind, char *name, char *desc, int default_val,
				  int min_val, int max_val)
{
	relopt_int	   *newoption;

	newoption = (relopt_int *) allocate_reloption(kind, RELOPT_TYPE_INT,
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
add_real_reloption(int kind, char *name, char *desc, double default_val,
				  double min_val, double max_val)
{
	relopt_real	   *newoption;

	newoption = (relopt_real *) allocate_reloption(kind, RELOPT_TYPE_REAL,
												   name, desc);
	newoption->default_val = default_val;
	newoption->min = min_val;
	newoption->max = max_val;

	add_reloption((relopt_gen *) newoption);
}

/*
 * add_string_reloption
 *		Add a new string reloption
 */
void
add_string_reloption(int kind, char *name, char *desc, char *default_val)
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
	newoption->gen.kind = kind;
	newoption->gen.namelen = strlen(name);
	newoption->gen.type = RELOPT_TYPE_STRING;
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

	MemoryContextSwitchTo(oldcxt);

	add_reloption((relopt_gen *) newoption);
}

/*
 * This is set whenever the GUC gp_default_storage_options is set.
 */
static StdRdOptions ao_storage_opts;

inline bool
isDefaultAOCS(void)
{
	return ao_storage_opts.columnstore;
}

inline bool
isDefaultAO(void)
{
	return ao_storage_opts.appendonly;
}

/*
 * Accumulate a new datum for one AO storage option.
 */
static void
accumAOStorageOpt(char *name, char *value,
				  ArrayBuildState *astate, bool *foundAO, bool *aovalue)
{
	text	   *t;
	bool		boolval;
	int			intval;
	StringInfoData buf;

	Assert(astate);

	initStringInfo(&buf);

	if (pg_strcasecmp(SOPT_APPENDONLY, name) == 0)
	{
		if (!parse_bool(value, &boolval))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid bool value \"%s\" for storage option \"%s\"",
							value, name)));
		/* "appendonly" option is explicitly specified. */
		if (foundAO != NULL)
			*foundAO = true;
		if (aovalue != NULL)
			*aovalue = boolval;

		/*
		 * Record value of "appendonly" option as true always.  Return
		 * the value specified by user in aovalue.  Setting
		 * appendonly=true always in the array of datums enables us to
		 * reuse default_reloptions() and
		 * validateAppendOnlyRelOptions().  If validations are
		 * successful, we keep the user specified value for
		 * appendonly.
		 */
		appendStringInfo(&buf, "%s=%s", SOPT_APPENDONLY, "true");
	}
	else if (pg_strcasecmp(SOPT_BLOCKSIZE, name) == 0)
	{
		if (!parse_int(value, &intval, 0 /* unit flags */,
					   NULL /* hint message */))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid integer value \"%s\" for storage option \"%s\"",
							value, name)));
		appendStringInfo(&buf, "%s=%d", SOPT_BLOCKSIZE, intval);
	}
	else if (pg_strcasecmp(SOPT_COMPTYPE, name) == 0)
	{
		appendStringInfo(&buf, "%s=%s", SOPT_COMPTYPE, value);
	}
	else if (pg_strcasecmp(SOPT_COMPLEVEL, name) == 0)
	{
		if (!parse_int(value, &intval, 0 /* unit flags */,
					   NULL /* hint message */))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid integer value \"%s\" for storage option \"%s\"",
							value, name)));
		appendStringInfo(&buf, "%s=%d", SOPT_COMPLEVEL, intval);
	}
	else if (pg_strcasecmp(SOPT_CHECKSUM, name) == 0)
	{
		if (!parse_bool(value, &boolval))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid bool value \"%s\" for storage option \"%s\"",
							value, name)));
		appendStringInfo(&buf, "%s=%s", SOPT_CHECKSUM, boolval ? "true" : "false");
	}
	else if (pg_strcasecmp(SOPT_ORIENTATION, name) == 0)
	{
		if ((pg_strcasecmp(value, "row") != 0) &&
			(pg_strcasecmp(value, "column") != 0))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid value \"%s\" for storage option \"%s\"",
							value, name)));
		}
		appendStringInfo(&buf, "%s=%s", SOPT_ORIENTATION, value);
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid storage option \"%s\"", name)));
	}

	t = cstring_to_text(buf.data);

	accumArrayResult(astate, PointerGetDatum(t), /* disnull */ false,
					 TEXTOID, CurrentMemoryContext);
	pfree(t);
	pfree(buf.data);
}

/*
 * Reset appendonly storage options to factory defaults.  Callers must
 * free ao_opts->compresstype before calling this method.
 */
inline void
resetAOStorageOpts(StdRdOptions *ao_opts)
{
	ao_opts->appendonly = AO_DEFAULT_APPENDONLY;
	ao_opts->blocksize = AO_DEFAULT_BLOCKSIZE;
	ao_opts->checksum = AO_DEFAULT_CHECKSUM;
	ao_opts->columnstore = AO_DEFAULT_COLUMNSTORE;
	ao_opts->compresslevel = AO_DEFAULT_COMPRESSLEVEL;
	ao_opts->compresstype = NULL;
}

/*
 * This needs to happen whenever gp_default_storage_options GUC is
 * reset.
 */
void
resetDefaultAOStorageOpts(void)
{
	if (ao_storage_opts.compresstype)
		free(ao_storage_opts.compresstype);
	resetAOStorageOpts(&ao_storage_opts);
}

const StdRdOptions *
currentAOStorageOptions(void)
{
	return (const StdRdOptions *) &ao_storage_opts;
}

/*
 * Set global appendonly storage options.
 */
void
setDefaultAOStorageOpts(StdRdOptions *copy)
{
	if (ao_storage_opts.compresstype)
	{
		free(ao_storage_opts.compresstype);
		ao_storage_opts.compresstype = NULL;
	}
	memcpy(&ao_storage_opts, copy, sizeof(ao_storage_opts));
	if (copy->compresstype != NULL)
	{
		if (pg_strcasecmp(copy->compresstype, "none") == 0)
		{
			/* Represent compresstype=none as NULL (MPP-25073). */
			ao_storage_opts.compresstype = NULL;
		}
		else
		{
			ao_storage_opts.compresstype = strdup(copy->compresstype);
			if (ao_storage_opts.compresstype == NULL)
				elog(ERROR, "out of memory");
		}
	}
}

static int setDefaultCompressionLevel(char* compresstype);

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
 * Accept a string of the form "name=value,name=value,...".  Space
 * around ',' and '=' is allowed.  Parsed values are stored in
 * corresponding fields of StdRdOptions object.  The parser is a
 * finite state machine that changes states for each input character
 * scanned.
 */
Datum
parseAOStorageOpts(const char *opts_str, bool *aovalue)
{
	int dims[1];
	int lbs[1];
	Datum result;
	ArrayBuildState *astate;

	bool foundAO = false;
	const char *cp;
	const char *name_st=NULL;
	const char *value_st=NULL;
	char *name=NULL, *value=NULL;
	enum state {
		/*
		 * Consume whitespace at the beginning of a name token.
		 */
		LEADING_NAME,

		/*
		 * Name token is being scanned.  Allowed characters are
		 * alphabets, whitespace and '='.
		 */
		NAME_TOKEN,

		/*
		 * Name token was terminated by whitespace.  This state scans
		 * the trailing whitespace after name token.
		 */
		TRAILING_NAME,

		/*
		 * Whitespace after '=' and before value token.
		 */
		LEADING_VALUE,

		/*
		 * Value token is being scanned.  Allowed characters are
		 * alphabets, digits, '_'.  Value should be delimited by a
		 * ',', whitespace or end of string '\0'.
		 */
		VALUE_TOKEN,

		/*
		 * Whitespace after value token.
		 */
		TRAILING_VALUE,

		/*
		 * End of string.  This state can only be entered from
		 * VALUE_TOKEN or TRAILING_VALUE.
		 */
		EOS
	};
	enum state st = LEADING_NAME;

	/*
	 * Initialize ArrayBuildState ourselves rather than leaving it to
	 * accumArrayResult().  This aviods the catalog lookup (pg_type)
	 * performed by accumArrayResult().
	 */
	astate = (ArrayBuildState *) palloc(sizeof(ArrayBuildState));
	astate->mcontext = CurrentMemoryContext;
	astate->alen = 10; /* Initial number of name=value pairs. */
	astate->dvalues = (Datum *) palloc(astate->alen * sizeof(Datum));
	astate->dnulls = (bool *) palloc(astate->alen * sizeof(bool));
	astate->nelems = 0;
	astate->element_type = TEXTOID;
	astate->typlen = -1;
	astate->typbyval = false;
	astate->typalign = 'i';

	cp = opts_str-1;
	do
	{
		++cp;
		switch(st)
		{
			case LEADING_NAME:
				if (isalpha(*cp))
				{
					st = NAME_TOKEN;
					name_st = cp;
				}
				else if (!isspace(*cp))
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid storage option name in \"%s\"",
									opts_str)));
				}
				break;
			case NAME_TOKEN:
				if (isspace(*cp))
					st = TRAILING_NAME;
				else if (*cp == '=')
					st = LEADING_VALUE;
				else if (!isalpha(*cp))
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid storage option name in \"%s\"",
									opts_str)));
				if (st != NAME_TOKEN)
				{
					name = palloc(cp - name_st + 1);
					strncpy(name, name_st, cp-name_st);
					name[cp-name_st] = '\0';
					for (name_st = name; *name_st != '\0'; ++name_st)
						*(char *)name_st = pg_tolower(*name_st);
				}
				break;
			case TRAILING_NAME:
				if (*cp == '=')
					st = LEADING_VALUE;
				else if (!isspace(*cp))
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid value for option \"%s\", expected \"=\"", name)));
				break;
			case LEADING_VALUE:
				if (isalnum(*cp))
				{
					st = VALUE_TOKEN;
					value_st = cp;
				}
				else if (!isspace(*cp))
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid value for option \"%s\"", name)));
				break;
			case VALUE_TOKEN:
				if (isspace(*cp))
					st = TRAILING_VALUE;
				else if (*cp == '\0')
					st = EOS;
				else if (*cp == ',')
					st = LEADING_NAME;
				/* Need to check '_' for rle_type */
				else if (!(isalnum(*cp) || *cp == '_'))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("invalid value for option \"%s\"", name)));
				if (st != VALUE_TOKEN)
				{
					value = palloc(cp - value_st + 1);
					strncpy(value, value_st, cp-value_st);
					value[cp-value_st] = '\0';
					for (value_st = value; *value_st != '\0'; ++value_st)
						*(char *)value_st = pg_tolower(*value_st);
					Assert(name);
					accumAOStorageOpt(name, value, astate,
									  &foundAO, aovalue);
					pfree(name);
					name = NULL;
					pfree(value);
					value = NULL;
				}
				break;
			case TRAILING_VALUE:
				if (*cp == ',')
					st = LEADING_NAME;
				else if (*cp == '\0')
					st = EOS;
				else if (!isspace(*cp))
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("syntax error after \"%s\"", value)));
				break;
			case EOS:
				/*
				 * We better get out of the loop right after entering
				 * this state.  Therefore, we should never get here.
				 */
				elog(ERROR, "invalid value \"%s\" for GUC", opts_str);
				break;
		};
	} while(*cp != '\0');
	if (st != EOS)
		elog(ERROR, "invalid value \"%s\" for GUC", opts_str);
	if (!foundAO)
	{
		/*
		 * Add "appendonly=true" datum if it was not explicitly
		 * specified by user.  This is needed to validate the array of
		 * datums constructed from user specified options.
		 */
		accumAOStorageOpt(SOPT_APPENDONLY, "true", astate, NULL, NULL);
	}

	lbs[0] = 1;
	dims[0] = astate->nelems;
	result = makeMdArrayResult(astate, 1, dims, lbs, CurrentMemoryContext, false);
	pfree(astate->dvalues);
	pfree(astate->dnulls);
	pfree(astate);
	return result;
}

/*
 * Return a datum that is array of "name=value" strings for each
 * appendonly storage option in opts.  This datum is used to populate
 * pg_class.reloptions during relation creation.
 *
 * To avoid catalog bloat, we only create "name=value" item for those
 * values in opts that are not specified in WITH clause and are
 * different from their initial defaults.
 */
Datum
transformAOStdRdOptions(StdRdOptions *opts, Datum withOpts)
{
	char *strval;
	char intval[MAX_SOPT_VALUE_LEN];
	Datum *withDatums = NULL;
	text *t;
	int len, i, withLen, soptLen, nWithOpts = 0;
	ArrayType *withArr;
	ArrayBuildState *astate = NULL;
	bool foundAO = false, foundBlksz = false, foundComptype = false,
			foundComplevel = false, foundChecksum = false,
			foundOrientation = false;
	/*
	 * withOpts must be parsed to see if an option was spcified in
	 * WITH() clause.
	 */
	if (DatumGetPointer(withOpts) != NULL)
	{
		withArr = DatumGetArrayTypeP(withOpts);
		Assert(ARR_ELEMTYPE(withArr) == TEXTOID);
		deconstruct_array(withArr, TEXTOID, -1, false, 'i', &withDatums,
						  NULL, &nWithOpts);
		/*
		 * Include options specified in WITH() clause in the same
		 * order as they are specified.  Otherwise we will end up with
		 * regression failures due to diff with respect to answer
		 * file.
		 */
		for (i = 0; i < nWithOpts; ++i)
		{
			t = DatumGetTextP(withDatums[i]);
			strval = VARDATA(t);
			/*
			 * Text datums are usually not null terminated.  We must
			 * never access beyond their length.
			 */
			withLen = VARSIZE(t) - VARHDRSZ;

			/*
			 * withDatums[i] may not be used directly.  It may be
			 * e.g. "aPPendOnly=tRue".  Therefore we don't set it as
			 * reloptions as is.
			 */
			soptLen = strlen(SOPT_APPENDONLY);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_APPENDONLY, soptLen) == 0)
			{
				foundAO = true;
				strval = opts->appendonly ? "true" : "false";
				len = VARHDRSZ + strlen(SOPT_APPENDONLY) + 1 + strlen(strval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_APPENDONLY, strval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_BLOCKSIZE);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_BLOCKSIZE, soptLen) == 0)
			{
				foundBlksz = true;
				snprintf(intval, MAX_SOPT_VALUE_LEN, "%d", opts->blocksize);
				len = VARHDRSZ + strlen(SOPT_BLOCKSIZE) + 1 + strlen(intval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_BLOCKSIZE, intval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_COMPTYPE);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_COMPTYPE, soptLen) == 0)
			{
				foundComptype = true;
				/*
				 * Record "none" as compresstype in reloptions if it
				 * was explicitly specified in WITH clause.
				 */
				strval = (opts->compresstype != NULL) ?
						opts->compresstype : "none";
				len = VARHDRSZ + strlen(SOPT_COMPTYPE) + 1 + strlen(strval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_COMPTYPE, strval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_COMPLEVEL);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_COMPLEVEL, soptLen) == 0)
			{
				foundComplevel = true;
				snprintf(intval, MAX_SOPT_VALUE_LEN, "%d", opts->compresslevel);
				len = VARHDRSZ + strlen(SOPT_COMPLEVEL) + 1 + strlen(intval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_COMPLEVEL, intval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_CHECKSUM);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_CHECKSUM, soptLen) == 0)
			{
				foundChecksum = true;
				strval = opts->checksum ? "true" : "false";
				len = VARHDRSZ + strlen(SOPT_CHECKSUM) + 1 + strlen(strval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_CHECKSUM, strval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_ORIENTATION);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_ORIENTATION, soptLen) == 0)
			{
				foundOrientation = true;
				strval = opts->columnstore ? "column" : "row";
				len = VARHDRSZ + strlen(SOPT_ORIENTATION) + 1 + strlen(strval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_ORIENTATION, strval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
			/*
			 * Record fillfactor only if it's specified in WITH
			 * clause.  Default fillfactor is assumed otherwise.
			 */
			soptLen = strlen(SOPT_FILLFACTOR);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_FILLFACTOR, soptLen) == 0)
			{
				snprintf(intval, MAX_SOPT_VALUE_LEN, "%d", opts->fillfactor);
				len = VARHDRSZ + strlen(SOPT_FILLFACTOR) + 1 + strlen(intval);
				/* +1 leaves room for sprintf's trailing null */
				t = (text *) palloc(len + 1);
				SET_VARSIZE(t, len);
				sprintf(VARDATA(t), "%s=%s", SOPT_FILLFACTOR, intval);
				astate = accumArrayResult(astate, PointerGetDatum(t), false,
										  TEXTOID, CurrentMemoryContext);
			}
		}
	}
	/* Include options that are not defaults and not already included. */
	if ((opts->appendonly != AO_DEFAULT_APPENDONLY) && !foundAO)
	{
		/* appendonly */
		strval = opts->appendonly ? "true" : "false";
		len = VARHDRSZ + strlen(SOPT_APPENDONLY) + 1 + strlen(strval);
		/* +1 leaves room for sprintf's trailing null */
		t = (text *) palloc(len + 1);
		SET_VARSIZE(t, len);
		sprintf(VARDATA(t), "%s=%s", SOPT_APPENDONLY, strval);
		astate = accumArrayResult(astate, PointerGetDatum(t),
								  false, TEXTOID,
								  CurrentMemoryContext);
	}
	if ((opts->blocksize != AO_DEFAULT_BLOCKSIZE) && !foundBlksz)
	{
		/* blocksize */
		snprintf(intval, MAX_SOPT_VALUE_LEN, "%d", opts->blocksize);
		len = VARHDRSZ + strlen(SOPT_BLOCKSIZE) + 1 + strlen(intval);
		/* +1 leaves room for sprintf's trailing null */
		t = (text *) palloc(len + 1);
		SET_VARSIZE(t, len);
		sprintf(VARDATA(t), "%s=%s", SOPT_BLOCKSIZE, intval);
		astate = accumArrayResult(astate, PointerGetDatum(t),
								  false, TEXTOID,
								  CurrentMemoryContext);
	}
	/*
	 * Record compression options only if compression is enabled.  No
	 * need to check compresstype here as by the time we get here,
	 * "opts" should have been set by default_reloptions() correctly.
	 */
	if (opts->compresslevel > AO_DEFAULT_COMPRESSLEVEL &&
		opts->compresstype != NULL)
	{
		if (!foundComptype && (
				(pg_strcasecmp(opts->compresstype, AO_DEFAULT_COMPRESSTYPE) == 0
				 && opts->compresslevel == 1 && !foundComplevel) ||
				pg_strcasecmp(opts->compresstype,
							  AO_DEFAULT_COMPRESSTYPE) != 0))
		{
			/* compress type */
			strval = opts->compresstype;
			len = VARHDRSZ + strlen(SOPT_COMPTYPE) + 1 + strlen(strval);
			/* +1 leaves room for sprintf's trailing null */
			t = (text *) palloc(len + 1);
			SET_VARSIZE(t, len);
			sprintf(VARDATA(t), "%s=%s", SOPT_COMPTYPE, strval);
			astate = accumArrayResult(astate, PointerGetDatum(t),
									  false, TEXTOID,
									  CurrentMemoryContext);
		}
		/* When compression is enabled, default compresslevel is 1. */
		if ((opts->compresslevel != 1) &&
			!foundComplevel)
		{
			/* compress level */
			snprintf(intval, MAX_SOPT_VALUE_LEN, "%d", opts->compresslevel);
			len = VARHDRSZ + strlen(SOPT_COMPLEVEL) + 1 + strlen(intval);
			/* +1 leaves room for sprintf's trailing null */
			t = (text *) palloc(len + 1);
			SET_VARSIZE(t, len);
			sprintf(VARDATA(t), "%s=%s", SOPT_COMPLEVEL, intval);
			astate = accumArrayResult(astate, PointerGetDatum(t),
									  false, TEXTOID,
									  CurrentMemoryContext);
		}
	}
	if ((opts->checksum != AO_DEFAULT_CHECKSUM) && !foundChecksum)
	{
		/* checksum */
		strval = opts->checksum ? "true" : "false";
		len = VARHDRSZ + strlen(SOPT_CHECKSUM) + 1 + strlen(strval);
		/* +1 leaves room for sprintf's trailing null */
		t = (text *) palloc(len + 1);
		SET_VARSIZE(t, len);
		sprintf(VARDATA(t), "%s=%s", SOPT_CHECKSUM, strval);
		astate = accumArrayResult(astate, PointerGetDatum(t),
								  false, TEXTOID,
								  CurrentMemoryContext);
	}
	if ((opts->columnstore != AO_DEFAULT_COLUMNSTORE) && !foundOrientation)
	{
		/* orientation */
		strval = opts->columnstore ? "column" : "row";
		len = VARHDRSZ + strlen(SOPT_ORIENTATION) + 1 + strlen(strval);
		/* +1 leaves room for sprintf's trailing null */
		t = (text *) palloc(len + 1);
		SET_VARSIZE(t, len);
		sprintf(VARDATA(t), "%s=%s", SOPT_ORIENTATION, strval);
		astate = accumArrayResult(astate, PointerGetDatum(t),
								  false, TEXTOID,
								  CurrentMemoryContext);
	}
	return astate ?
			makeArrayResult(astate, CurrentMemoryContext) :
			PointerGetDatum(NULL);
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
		if (relOpts[i]->kind == kind)
			numoptions++;

	if (numoptions == 0)
	{
		*numrelopts = 0;
		return NULL;
	}

	reloptions = palloc(numoptions * sizeof(relopt_value));

	for (i = 0, j = 0; relOpts[i]; i++)
	{
		if (relOpts[i]->kind == kind)
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
	}

	pfree(optiondatums);

	*numrelopts = numoptions;
	return reloptions;
}

void
parse_validate_reloptions(StdRdOptions *result, Datum reloptions,
						  bool validate, char relkind)
{
	static const char *const default_keywords[] = {
			SOPT_FILLFACTOR,
			SOPT_APPENDONLY,
			SOPT_BLOCKSIZE,
			SOPT_COMPTYPE,
			SOPT_COMPLEVEL,
			SOPT_CHECKSUM,
			SOPT_ORIENTATION
	};
	char	   *values[ARRAY_SIZE(default_keywords)];
	int			j = 0;

	parseRelOptions(reloptions, ARRAY_SIZE(default_keywords),
					default_keywords, values, validate);

	/* fillfactor */
	if (values[0] != NULL)
	{
		if (!parse_int(values[0], &result->fillfactor, 0, NULL))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("fillfactor must be an integer: \"%s\"",
								values[0])));
		}
	}

	/* appendonly */
	if (values[1] != NULL)
	{
		if (relkind != RELKIND_RELATION)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"appendonly\" in a non relation object is not supported")));

		if (!parse_bool(values[1], &result->appendonly))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid parameter value for \"appendonly\": "
							"\"%s\"", values[1])));
		}
	}

	/* blocksize */
	if (values[2] != NULL)
	{
		if (relkind != RELKIND_RELATION && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"blocksize\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option 'blocksize' for base relation. "
							"Only valid for Append Only relations")));

		result->blocksize = pg_atoi(values[2], sizeof(int32), 0);

		if (result->blocksize < MIN_APPENDONLY_BLOCK_SIZE ||
			result->blocksize > MAX_APPENDONLY_BLOCK_SIZE ||
			result->blocksize % MIN_APPENDONLY_BLOCK_SIZE != 0)
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("block size must be between 8KB and 2MB and be"
								" an 8KB multiple. Got %d", result->blocksize)));

			result->blocksize = DEFAULT_APPENDONLY_BLOCK_SIZE;
		}

	}

	/* compression type */
	if (values[3] != NULL)
	{
		if (relkind != RELKIND_RELATION && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresstype\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"compresstype\" for base relation."
							" Only valid for Append Only relations")));

		result->compresstype = pstrdup(values[3]);
		if (!compresstype_is_valid(result->compresstype))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("unknown compresstype \"%s\"",
							result->compresstype)));
		for (j = 0; j < strlen(result->compresstype); j++)
			result->compresstype[j] = pg_tolower(result->compresstype[j]);
	}

	/* compression level */
	if (values[4] != NULL)
	{
		if (relkind != RELKIND_RELATION && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresslevel\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option 'compresslevel' for base "
							"relation. Only valid for Append Only relations")));

		result->compresslevel = pg_atoi(values[4], sizeof(int32), 0);

		if (result->compresstype &&
			pg_strcasecmp(result->compresstype, "none") != 0 &&
			result->compresslevel == 0 && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresstype can\'t be used with compresslevel 0")));
		if (result->compresslevel < 0 || result->compresslevel > 9)
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range (should be "
								"between 0 and 9)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(
					result->compresstype);
		}

		/*
		 * use the default compressor if compresslevel was indicated but not
		 * compresstype. must make a copy otherwise str_tolower below will
		 * crash.
		 */
		if (result->compresslevel > 0 && !result->compresstype)
			result->compresstype = pstrdup(AO_DEFAULT_COMPRESSTYPE);

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "quicklz") == 0) &&
			(result->compresslevel != 1))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for "
								"quicklz (should be 1)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(
					result->compresstype);
		}

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "rle_type") == 0) &&
			(result->compresslevel > 4))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for rle_type"
								" (should be in the range 1 to 4)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(
					result->compresstype);
		}
	}

	/* checksum */
	if (values[5] != NULL)
	{
		if (relkind != RELKIND_RELATION && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"checksum\" in a non relation "
							"object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"checksum\" for base relation. "
							"Only valid for Append Only relations")));

		if (!parse_bool(values[5], &result->checksum) && validate)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid parameter value for \"checksum\": \"%s\"",
							values[5])));
		}
	}
	/* Disable checksum for heap relations. */
	else if (result->appendonly == false)
		result->checksum = false;

	/* columnstore */
	if (values[6] != NULL)
	{
		if (relkind != RELKIND_RELATION && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"orientation\" in a non "
							"relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"orientation\" for base relation. "
							"Only valid for Append Only relations")));

		if (!(pg_strcasecmp(values[6], "column") == 0 ||
			  pg_strcasecmp(values[6], "row") == 0) &&
			validate)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid parameter value for \"orientation\": "
							"\"%s\"", values[6])));
		}

		result->columnstore = (pg_strcasecmp(values[6], "column") == 0 ?
							   true : false);

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "rle_type") == 0) &&
			! result->columnstore)
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("%s cannot be used with Append Only relations "
								"row orientation", result->compresstype)));
		}
	}

	if (result->appendonly && result->compresstype != NULL)
		if (result->compresslevel == AO_DEFAULT_COMPRESSLEVEL)
			result->compresslevel = setDefaultCompressionLevel(
					result->compresstype);
	for (int i = 0; i < ARRAY_SIZE(default_keywords); i++)
	{
		if (values[i])
		{
			pfree(values[i]);
		}
	}
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
			option->values.string_val = value;
			nofree = true;
			parsed = true;
			/* no validation possible */
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
 * Option parser for anything that uses StdRdOptions (i.e. fillfactor only)
 */
bytea *
default_reloptions(Datum reloptions, bool validate, relopt_kind kind)
{
	relopt_value   *options;
	StdRdOptions   *rdopts;
	StdRdOptions	lopts;
	int				numoptions;
	int				len;
	int				i;

	options = parseRelOptions(reloptions, validate, kind, &numoptions);

	/* if none set, we're done */
	if (numoptions == 0)
		return NULL;

	MemSet(&lopts, 0, sizeof(StdRdOptions));

	for (i = 0; i < numoptions; i++)
	{
		HANDLE_INT_RELOPTION("fillfactor", lopts.fillfactor, options[i]);
	}

	pfree(options);

	len = sizeof(StdRdOptions);
	rdopts = palloc(len);
	memcpy(rdopts, &lopts, len);
	SET_VARSIZE(rdopts, len);

	return (bytea *) rdopts;
}

/*
 * Parse options for heaps (and perhaps someday toast tables).
 */
bytea *
heap_reloptions(char relkind, Datum reloptions, bool validate)
{
	return default_reloptions(reloptions, validate, RELOPT_KIND_HEAP);
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

/*
 * validateAppendOnlyRelOptions
 *
 *		Various checks for validity of appendonly relation rules.
 */
void validateAppendOnlyRelOptions(bool ao,
								  int blocksize,
								  int safewrite,
								  int complevel,
								  char* comptype,
								  bool checksum,
								  char relkind,
								  bool co)
{
	if (relkind != RELKIND_RELATION)
	{
		if(ao)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("appendonly may only be specified for base relations")));

		if(checksum)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("checksum may only be specified for base relations")));

		if(comptype)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("compresstype may only be specified for base relations")));
	}

	/*
	 * If this is not an appendonly relation, no point in going
	 * further.
	 */
	if (!ao)
		return;

	if (comptype &&
		(pg_strcasecmp(comptype, "quicklz") == 0 ||
		 pg_strcasecmp(comptype, "zlib") == 0 ||
		 pg_strcasecmp(comptype, "rle_type") == 0))
	{

		if (! co &&
			pg_strcasecmp(comptype, "rle_type") == 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("%s cannot be used with Append Only relations row orientation",
							comptype)));
		}

		if (comptype && complevel == 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresstype cannot be used with compresslevel 0")));

		if (complevel < 0 || complevel > 9)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range (should be between 0 and 9)",
							complevel)));

		if (comptype && (pg_strcasecmp(comptype, "quicklz") == 0) &&
			(complevel != 1))
		{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for quicklz "
								 "(should be 1)", complevel)));
		}
		if (comptype && (pg_strcasecmp(comptype, "rle_type") == 0) &&
			(complevel > 4))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range for rle_type "
							"(should be in the range 1 to 4)", complevel)));
		}
	}

	if (blocksize < MIN_APPENDONLY_BLOCK_SIZE || blocksize > MAX_APPENDONLY_BLOCK_SIZE ||
	    blocksize % MIN_APPENDONLY_BLOCK_SIZE != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size must be between 8KB and 2MB and "
						"be an 8KB multiple, Got %d", blocksize)));

	if (safewrite > MAX_APPENDONLY_BLOCK_SIZE || safewrite % 8 != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("safefswrite size must be less than 8MB and "
						"be a multiple of 8")));

	if (gp_safefswritesize > blocksize)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size (%d) is smaller gp_safefswritesize (%d). "
						"increase blocksize or decrease gp_safefswritesize if it "
						"is safe to do so on this file system",
						blocksize, gp_safefswritesize)));
}

/*
 * if no compressor type was specified, we set to no compression (level 0)
 * otherwise default for both zlib, quicklz and RLE to level 1.
 */
static int setDefaultCompressionLevel(char* compresstype)
{
	if(!compresstype || pg_strcasecmp(compresstype, "none") == 0)
		return 0;
	else
		return 1;
}
