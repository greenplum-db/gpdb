/*-------------------------------------------------------------------------
 *
 * reloptions_gp.c
 *	  GPDB-specific relation options.
 *
 * These are in a separate file from reloptions.c, in order to reduce
 * conflicts when merging with upstream code.
 *
 *
 * Portions Copyright (c) 2017-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/reloptions_gp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/bitmap.h"
#include "access/reloptions.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "miscadmin.h"

/*
 * Confusingly, RELOPT_KIND_HEAP is also used for AO/CO tables. To reduce
 * the confusion in this file, use this macro to check for "heap or AO/CO
 * table".
 */
#define KIND_IS_RELATION(kind) ((kind) == RELOPT_KIND_HEAP)

/*
 * GPDB reloptions specification.
 */

static relopt_bool boolRelOpts_gp[] =
{
	{
		{
			SOPT_APPENDONLY,
			"Append only storage parameter",
			RELOPT_KIND_HEAP
		},
		AO_DEFAULT_APPENDONLY,
	},
	{
		{
			SOPT_CHECKSUM,
			"Append table checksum",
			RELOPT_KIND_HEAP
		},
		AO_DEFAULT_CHECKSUM
	},
	/* list terminator */
	{{NULL}}
};

static relopt_int intRelOpts_gp[] =
{
	{
		{
			SOPT_FILLFACTOR,
			"Packs bitmap index pages only to this percentage",
			RELOPT_KIND_BITMAP
		},
		BITMAP_DEFAULT_FILLFACTOR, BITMAP_MIN_FILLFACTOR, 100
	},
	{
		{
			SOPT_BLOCKSIZE,
			"AO tables block size in bytes",
			RELOPT_KIND_HEAP
		},
		AO_DEFAULT_BLOCKSIZE, MIN_APPENDONLY_BLOCK_SIZE, MAX_APPENDONLY_BLOCK_SIZE
	},
	{
		{
			SOPT_COMPLEVEL,
			"AO table compression level",
			RELOPT_KIND_HEAP
		},
		AO_DEFAULT_COMPRESSLEVEL, AO_MIN_COMPRESSLEVEL, AO_MAX_COMPRESSLEVEL
	},
	{
		{
			SOPT_FILLFACTOR,
			"Packs bitmap index pages only to this percentage",
			RELOPT_KIND_INTERNAL
		},
		HEAP_DEFAULT_FILLFACTOR, HEAP_MIN_FILLFACTOR, 100
	},
	/* list terminator */
	{{NULL}}
};

static relopt_real realRelOpts_gp[] =
{
	/* list terminator */
	{{NULL}}
};

static relopt_string stringRelOpts_gp[] =
{
	{
		{
			SOPT_COMPTYPE,
			"AO tables compression type",
			RELOPT_KIND_HEAP
		},
		0, true, ""
	},
	{
		{
			SOPT_ORIENTATION,
				"AO tables orientation",
				RELOPT_KIND_HEAP
		},
			0, false, ""
	},
	/* list terminator */
	{{NULL}}
};

static void free_options_deep(relopt_value *options, int num_options);
static relopt_value *get_option_set(relopt_value *options, int num_options, const char *opt_name);

/*
 * initialize_reloptions_gp
 * 		initialization routine for GPDB reloptions
 *
 * We use the add_*_option interface in reloptions.h to add GPDB-specific options.
 */
void
initialize_reloptions_gp(void)
{
	int			i;
	static bool	initialized = false;

	/* only add these on first call. */
	if (initialized)
		return;
	initialized = true;

	/* Set GPDB specific options */
	for (i = 0; boolRelOpts_gp[i].gen.name; i++)
	{
		add_bool_reloption(boolRelOpts_gp[i].gen.kind,
						   (char *) boolRelOpts_gp[i].gen.name,
						   (char *) boolRelOpts_gp[i].gen.desc,
						   boolRelOpts_gp[i].default_val);
	}

	for (i = 0; intRelOpts_gp[i].gen.name; i++)
	{
		add_int_reloption(intRelOpts_gp[i].gen.kind,
						  (char *) intRelOpts_gp[i].gen.name,
						  (char *) intRelOpts_gp[i].gen.desc,
						  intRelOpts_gp[i].default_val,
						  intRelOpts_gp[i].min,
						  intRelOpts_gp[i].max);
	}

	for (i = 0; realRelOpts_gp[i].gen.name; i++)
	{
		add_real_reloption(realRelOpts_gp[i].gen.kind,
						   (char *) realRelOpts_gp[i].gen.name,
						   (char *) realRelOpts_gp[i].gen.desc,
						   realRelOpts_gp[i].default_val,
						   realRelOpts_gp[i].min, realRelOpts_gp[i].max);
	}

	for (i = 0; stringRelOpts_gp[i].gen.name; i++)
	{
		add_string_reloption(stringRelOpts_gp[i].gen.kind,
							 (char *) stringRelOpts_gp[i].gen.name,
							 (char *) stringRelOpts_gp[i].gen.desc,
							 NULL);
	}
}

/*
 * This is set whenever the GUC gp_default_storage_options is set.
 */
static StdRdOptions ao_storage_opts;
static bool ao_storage_opts_changed = false;

bool
isDefaultAOCS(void)
{
	return ao_storage_opts.columnstore;
}

bool
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
		 * Record value of "appendonly" option as true always.  Return the
		 * value specified by user in aovalue.  Setting appendonly=true always
		 * in the array of datums enables us to reuse default_reloptions() and
		 * validateAppendOnlyRelOptions().  If validations are successful, we
		 * keep the user specified value for appendonly.
		 */
		appendStringInfo(&buf, "%s=%s", SOPT_APPENDONLY, "true");
	}
	else if (pg_strcasecmp(SOPT_BLOCKSIZE, name) == 0)
	{
		if (!parse_int(value, &intval, 0 /* unit flags */ ,
					   NULL /* hint message */ ))
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
		if (!parse_int(value, &intval, 0 /* unit flags */ ,
					   NULL /* hint message */ ))
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
	ao_opts->orientation = NULL;
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

	if (ao_storage_opts.orientation)
		free(ao_storage_opts.orientation);

	resetAOStorageOpts(&ao_storage_opts);
	ao_storage_opts_changed = false;
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

	if (ao_storage_opts.orientation)
	{
		free(ao_storage_opts.orientation);
		ao_storage_opts.orientation = NULL;
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

	if (copy->orientation != NULL)
	{
		ao_storage_opts.orientation = strdup(copy->orientation);
		if (ao_storage_opts.orientation == NULL)
			elog(ERROR, "out of memory");
	}

	ao_storage_opts_changed = true;
}

static int	setDefaultCompressionLevel(char *compresstype);

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
	int			dims[1];
	int			lbs[1];
	Datum		result;
	ArrayBuildState *astate;

	bool		foundAO = false;
	const char *cp;
	const char *name_st = NULL;
	const char *value_st = NULL;
	char	   *name = NULL,
			   *value = NULL;
	enum state
	{
		/*
		 * Consume whitespace at the beginning of a name token.
		 */
		LEADING_NAME,

		/*
		 * Name token is being scanned.  Allowed characters are alphabets,
		 * whitespace and '='.
		 */
		NAME_TOKEN,

		/*
		 * Name token was terminated by whitespace.  This state scans the
		 * trailing whitespace after name token.
		 */
		TRAILING_NAME,

		/*
		 * Whitespace after '=' and before value token.
		 */
		LEADING_VALUE,

		/*
		 * Value token is being scanned.  Allowed characters are alphabets,
		 * digits, '_'.  Value should be delimited by a ',', whitespace or end
		 * of string '\0'.
		 */
		VALUE_TOKEN,

		/*
		 * Whitespace after value token.
		 */
		TRAILING_VALUE,

		/*
		 * End of string.  This state can only be entered from VALUE_TOKEN or
		 * TRAILING_VALUE.
		 */
		EOS
	};
	enum state	st = LEADING_NAME;

	/*
	 * Initialize ArrayBuildState ourselves rather than leaving it to
	 * accumArrayResult().  This aviods the catalog lookup (pg_type) performed
	 * by accumArrayResult().
	 */
	astate = (ArrayBuildState *) palloc(sizeof(ArrayBuildState));
	astate->mcontext = CurrentMemoryContext;
	astate->alen = 10;			/* Initial number of name=value pairs. */
	astate->dvalues = (Datum *) palloc(astate->alen * sizeof(Datum));
	astate->dnulls = (bool *) palloc(astate->alen * sizeof(bool));
	astate->nelems = 0;
	astate->element_type = TEXTOID;
	astate->typlen = -1;
	astate->typbyval = false;
	astate->typalign = 'i';

	cp = opts_str - 1;
	do
	{
		++cp;
		switch (st)
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
					strncpy(name, name_st, cp - name_st);
					name[cp - name_st] = '\0';
					for (name_st = name; *name_st != '\0'; ++name_st)
						*(char *) name_st = pg_tolower(*name_st);
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
					strncpy(value, value_st, cp - value_st);
					value[cp - value_st] = '\0';
					for (value_st = value; *value_st != '\0'; ++value_st)
						*(char *) value_st = pg_tolower(*value_st);
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
				 * We better get out of the loop right after entering this
				 * state.  Therefore, we should never get here.
				 */
				elog(ERROR, "invalid value \"%s\" for GUC", opts_str);
				break;
		};
	} while (*cp != '\0');
	if (st != EOS)
		elog(ERROR, "invalid value \"%s\" for GUC", opts_str);
	if (!foundAO)
	{
		/*
		 * Add "appendonly=true" datum if it was not explicitly specified by
		 * user.  This is needed to validate the array of datums constructed
		 * from user specified options.
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
	char	   *strval;
	char		intval[MAX_SOPT_VALUE_LEN];
	Datum	   *withDatums = NULL;
	text	   *t;
	int			len,
				i,
				withLen,
				soptLen,
				nWithOpts = 0;
	ArrayType  *withArr;
	ArrayBuildState *astate = NULL;
	bool		foundAO = false,
				foundBlksz = false,
				foundComptype = false,
				foundComplevel = false,
				foundChecksum = false,
				foundOrientation = false;

	/*
	 * withOpts must be parsed to see if an option was spcified in WITH()
	 * clause.
	 */
	if (DatumGetPointer(withOpts) != NULL)
	{
		withArr = DatumGetArrayTypeP(withOpts);
		Assert(ARR_ELEMTYPE(withArr) == TEXTOID);
		deconstruct_array(withArr, TEXTOID, -1, false, 'i', &withDatums,
						  NULL, &nWithOpts);

		/*
		 * Include options specified in WITH() clause in the same order as
		 * they are specified.  Otherwise we will end up with regression
		 * failures due to diff with respect to answer file.
		 */
		for (i = 0; i < nWithOpts; ++i)
		{
			t = DatumGetTextP(withDatums[i]);
			strval = VARDATA(t);

			/*
			 * Text datums are usually not null terminated.  We must never
			 * access beyond their length.
			 */
			withLen = VARSIZE(t) - VARHDRSZ;

			/*
			 * withDatums[i] may not be used directly.  It may be e.g.
			 * "aPPendOnly=tRue".  Therefore we don't set it as reloptions as
			 * is.
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
				 * Record "none" as compresstype in reloptions if it was
				 * explicitly specified in WITH clause.
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
			 * Record fillfactor only if it's specified in WITH clause.
			 * Default fillfactor is assumed otherwise.
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
	 * Record compression options only if compression is enabled.  No need to
	 * check compresstype here as by the time we get here, "opts" should have
	 * been set by default_reloptions() correctly.
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

void
validate_and_adjust_options(StdRdOptions *result,
							relopt_value *options,
							int num_options, relopt_kind kind, bool validate)
{
	int			i;
	relopt_value *fillfactor_opt;
	relopt_value *appendonly_opt;
	relopt_value *blocksize_opt;
	relopt_value *comptype_opt;
	relopt_value *complevel_opt;
	relopt_value *checksum_opt;
	relopt_value *orientation_opt;

	/* fillfactor */
	fillfactor_opt = get_option_set(options, num_options, SOPT_FILLFACTOR);
	if (fillfactor_opt != NULL)
	{
		result->fillfactor = fillfactor_opt->values.int_val;
	}
	/* appendonly */
	appendonly_opt = get_option_set(options, num_options, SOPT_APPENDONLY);
	if (appendonly_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"appendonly\" in a non relation object is not supported")));
		result->appendonly = appendonly_opt->values.bool_val;
	}

	/* blocksize */
	blocksize_opt = get_option_set(options, num_options, SOPT_BLOCKSIZE);
	if (blocksize_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"blocksize\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option 'blocksize' for base relation. "
							"Only valid for Append Only relations")));

		result->blocksize = blocksize_opt->values.int_val;

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
	comptype_opt = get_option_set(options, num_options, SOPT_COMPTYPE);
	if (comptype_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresstype\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"compresstype\" for base relation."
							" Only valid for Append Only relations")));

		result->compresstype = pstrdup(comptype_opt->values.string_val);
		if (!compresstype_is_valid(result->compresstype))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("unknown compresstype \"%s\"",
							result->compresstype)));
		for (i = 0; i < strlen(result->compresstype); i++)
			result->compresstype[i] = pg_tolower(result->compresstype[i]);
	}

	/* compression level */
	complevel_opt = get_option_set(options, num_options, SOPT_COMPLEVEL);
	if (complevel_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresslevel\" in a non relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option 'compresslevel' for base "
							"relation. Only valid for Append Only relations")));

		result->compresslevel = complevel_opt->values.int_val;

		if (result->compresstype &&
			pg_strcasecmp(result->compresstype, "none") != 0 &&
			result->compresslevel == 0 && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresstype can\'t be used with compresslevel 0")));
		if (result->compresslevel < 0)
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range (should be positive)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}

		/*
		 * use the default compressor if compresslevel was indicated but not
		 * compresstype. must make a copy otherwise str_tolower below will
		 * crash.
		 */
		if (result->compresslevel > 0 && !result->compresstype)
			result->compresstype = pstrdup(AO_DEFAULT_COMPRESSTYPE);

		/* Check upper bound of compresslevel for each compression type */

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "zlib") == 0) &&
			(result->compresslevel > 9))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for zlib "
								"(should be in the range 1 to 9)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "zstd") == 0) &&
			(result->compresslevel > 19))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for zstd "
								"(should be in the range 1 to 19)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "quicklz") == 0) &&
			(result->compresslevel != 1))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for quicklz "
								"(should be 1)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "rle_type") == 0) &&
			(result->compresslevel > 4))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for rle_type "
								"(should be in the range 1 to 4)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}
	}

	/* checksum */
	checksum_opt = get_option_set(options, num_options, SOPT_CHECKSUM);
	if (checksum_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"checksum\" in a non relation "
							"object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"checksum\" for base relation. "
							"Only valid for Append Only relations")));
		result->checksum = checksum_opt->values.bool_val;
	}
	/* Disable checksum for heap relations. */
	else if (result->appendonly == false)
		result->checksum = false;

	/* columnstore */
	orientation_opt = get_option_set(options, num_options, SOPT_ORIENTATION);
	if (orientation_opt != NULL)
	{
		if (!KIND_IS_RELATION(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"orientation\" in a non "
							"relation object is not supported")));

		if (!result->appendonly && validate)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("invalid option \"orientation\" for base relation. "
							"Only valid for Append Only relations")));

		if (!(pg_strcasecmp(orientation_opt->values.string_val, "column") == 0 ||
			  pg_strcasecmp(orientation_opt->values.string_val, "row") == 0) &&
			validate)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid parameter value for \"orientation\": "
							"\"%s\"", orientation_opt->values.string_val)));
		}

		result->columnstore = (pg_strcasecmp(orientation_opt->values.string_val, "column") == 0 ?
							   true : false);

		if (result->compresstype &&
			(pg_strcasecmp(result->compresstype, "rle_type") == 0) &&
			!result->columnstore)
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
			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
}

void
validate_and_refill_options(StdRdOptions *result, relopt_value *options,
							int numrelopts, relopt_kind kind, bool validate)
{
	if (validate &&
		ao_storage_opts_changed &&
		KIND_IS_RELATION(kind))
	{
		if (!(get_option_set(options, numrelopts, SOPT_APPENDONLY)))
			result->appendonly = ao_storage_opts.appendonly;

		if (!(get_option_set(options, numrelopts, SOPT_FILLFACTOR)))
			result->fillfactor = ao_storage_opts.fillfactor;

		if (!(get_option_set(options, numrelopts, SOPT_BLOCKSIZE)))
			result->blocksize = ao_storage_opts.blocksize;

		if (!(get_option_set(options, numrelopts, SOPT_COMPLEVEL)))
			result->compresslevel = ao_storage_opts.compresslevel;

		if (!(get_option_set(options, numrelopts, SOPT_COMPTYPE)))
			result->compresstype = ao_storage_opts.compresstype;

		if (!(get_option_set(options, numrelopts, SOPT_CHECKSUM)))
			result->checksum = ao_storage_opts.checksum;

		if (!(get_option_set(options, numrelopts, SOPT_ORIENTATION)))
			result->columnstore = ao_storage_opts.columnstore;
	}

	validate_and_adjust_options(result, options, numrelopts, kind, validate);
}

void
parse_validate_reloptions(StdRdOptions *result, Datum reloptions,
						  bool validate, relopt_kind kind)
{
	relopt_value *options;
	int			num_options;

	options = parseRelOptions(reloptions, validate, RELOPT_KIND_HEAP,
							  &num_options);

	validate_and_adjust_options(result, options, num_options, kind, validate);
	free_options_deep(options, num_options);
}

/*
 * validateAppendOnlyRelOptions
 *
 *		Various checks for validity of appendonly relation rules.
 */
void
validateAppendOnlyRelOptions(bool ao,
							 int blocksize,
							 int safewrite,
							 int complevel,
							 char *comptype,
							 bool checksum,
							 char relkind,
							 bool co)
{
	if (relkind != RELKIND_RELATION)
	{
		if (ao)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("appendonly may only be specified for base relations")));

		if (checksum)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("checksum may only be specified for base relations")));

		if (comptype)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("compresstype may only be specified for base relations")));
	}

	/*
	 * If this is not an appendonly relation, no point in going further.
	 */
	if (!ao)
		return;

	if (comptype &&
		(pg_strcasecmp(comptype, "quicklz") == 0 ||
		 pg_strcasecmp(comptype, "zlib") == 0 ||
		 pg_strcasecmp(comptype, "rle_type") == 0 ||
		 pg_strcasecmp(comptype, "zstd") == 0))
	{
		if (!co &&
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

		if (comptype && (pg_strcasecmp(comptype, "zlib") == 0) &&
			(complevel < 0 || complevel > 9))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range (should be between 0 and 9)",
							complevel)));
		}

		if (comptype && (pg_strcasecmp(comptype, "zstd") == 0) &&
			(complevel < 0 || complevel > 19))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range for zstd "
							"(should be in the range 1 to 19)", complevel)));
		}

		if (comptype && (pg_strcasecmp(comptype, "quicklz") == 0) &&
			(complevel != 1))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range for quicklz "
							"(should be 1)", complevel)));
		}
		if (comptype && (pg_strcasecmp(comptype, "rle_type") == 0) &&
			(complevel < 0 || complevel > 4))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range for rle_type "
							"(should be in the range 1 to 4)", complevel)));
		}
	}

	if (blocksize < MIN_APPENDONLY_BLOCK_SIZE ||
		blocksize > MAX_APPENDONLY_BLOCK_SIZE ||
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
 * otherwise default for both zlib, quicklz, zstd and RLE to level 1.
 */
static int
setDefaultCompressionLevel(char *compresstype)
{
	if (!compresstype || pg_strcasecmp(compresstype, "none") == 0)
		return 0;
	else
		return 1;
}

void
free_options_deep(relopt_value *options, int num_options)
{
	int			i;

	for (i = 0; i < num_options; ++i)
	{
		if (options[i].isset &&
			options[i].gen->type == RELOPT_TYPE_STRING &&
			options[i].values.string_val != NULL)
		{
			pfree(options[i].values.string_val);
		}
	}
	pfree(options);
}

relopt_value *
get_option_set(relopt_value *options, int num_options, const char *opt_name)
{
	int			i;
	int			opt_name_len;
	int			cmp_len;

	opt_name_len = strlen(opt_name);
	for (i = 0; i < num_options; ++i)
	{
		cmp_len = options[i].gen->namelen > opt_name_len ? opt_name_len : options[i].gen->namelen;
		if (options[i].isset && pg_strncasecmp(options[i].gen->name, opt_name, cmp_len) == 0)
			return &options[i];
	}
	return NULL;
}
