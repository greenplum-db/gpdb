/*-------------------------------------------------------------------------
 *
 * reloptions_gp.c
 *	  GPDB-specific relation options.
 *
 * These are in a separate file from reloptions.c, in order to reduce
 * conflicts when merging with upstream code.
 *
 *
 * Portions Copyright (c) 2017-Present VMware, Inc. or its affiliates.
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
#include "commands/defrem.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/analyze.h"

/*
 * Helper macro used for validation
 */
#define KIND_IS_APPENDOPTIMIZED(kind) (((kind) & RELOPT_KIND_APPENDOPTIMIZED) != 0)

/*
 * GPDB reloptions specification.
 */

static relopt_bool boolRelOpts_gp[] =
{
	{
		{
			SOPT_CHECKSUM,
			"Append table checksum",
			RELOPT_KIND_APPENDOPTIMIZED,
			AccessExclusiveLock
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
			RELOPT_KIND_BITMAP,
			ShareUpdateExclusiveLock	/* since it applies only to later
										 * inserts */
		},
		BITMAP_DEFAULT_FILLFACTOR, BITMAP_MIN_FILLFACTOR, 100
	},
	{
		{
			SOPT_BLOCKSIZE,
			"AO tables block size in bytes",
			RELOPT_KIND_APPENDOPTIMIZED,
			AccessExclusiveLock
		},
		AO_DEFAULT_BLOCKSIZE, MIN_APPENDONLY_BLOCK_SIZE, MAX_APPENDONLY_BLOCK_SIZE
	},
	{
		{
			SOPT_COMPLEVEL,
			"AO table compression level",
			RELOPT_KIND_APPENDOPTIMIZED,
			ShareUpdateExclusiveLock	/* since it applies only to later
										 * inserts */
		},
		AO_DEFAULT_COMPRESSLEVEL, AO_MIN_COMPRESSLEVEL, AO_MAX_COMPRESSLEVEL
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
			RELOPT_KIND_APPENDOPTIMIZED,
			AccessExclusiveLock
		},
		0, true, NULL, ""
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
		add_bool_reloption(boolRelOpts_gp[i].gen.kinds,
						   (char *) boolRelOpts_gp[i].gen.name,
						   (char *) boolRelOpts_gp[i].gen.desc,
						   boolRelOpts_gp[i].default_val);
		set_reloption_lockmode(boolRelOpts_gp[i].gen.name, boolRelOpts_gp[i].gen.lockmode);
	}

	for (i = 0; intRelOpts_gp[i].gen.name; i++)
	{
		add_int_reloption(intRelOpts_gp[i].gen.kinds,
						  (char *) intRelOpts_gp[i].gen.name,
						  (char *) intRelOpts_gp[i].gen.desc,
						  intRelOpts_gp[i].default_val,
						  intRelOpts_gp[i].min,
						  intRelOpts_gp[i].max);
		set_reloption_lockmode(intRelOpts_gp[i].gen.name, intRelOpts_gp[i].gen.lockmode);
	}

	for (i = 0; realRelOpts_gp[i].gen.name; i++)
	{
		add_real_reloption(realRelOpts_gp[i].gen.kinds,
						   (char *) realRelOpts_gp[i].gen.name,
						   (char *) realRelOpts_gp[i].gen.desc,
						   realRelOpts_gp[i].default_val,
						   realRelOpts_gp[i].min, realRelOpts_gp[i].max);
		set_reloption_lockmode(realRelOpts_gp[i].gen.name, realRelOpts_gp[i].gen.lockmode);
	}

	for (i = 0; stringRelOpts_gp[i].gen.name; i++)
	{
		add_string_reloption(stringRelOpts_gp[i].gen.kinds,
							 (char *) stringRelOpts_gp[i].gen.name,
							 (char *) stringRelOpts_gp[i].gen.desc,
							 NULL,
							 stringRelOpts_gp[i].validate_cb);
		set_reloption_lockmode(stringRelOpts_gp[i].gen.name, stringRelOpts_gp[i].gen.lockmode);
	}
}

/*
 * This is set whenever the GUC gp_default_storage_options is set.
 */
static StdRdOptions ao_storage_opts;
static bool ao_storage_opts_changed = false;

/*
 * Accumulate a new datum for one AO storage option.
 */
static void
accumAOStorageOpt(char *name, char *value,
				  ArrayBuildState *astate)
{
	text	   *t;
	bool		boolval;
	int			intval;
	StringInfoData buf;

	Assert(astate);

	initStringInfo(&buf);

	if (pg_strcasecmp(SOPT_BLOCKSIZE, name) == 0)
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
	else
	{
		/*
		 * Provide a user friendly message in case that the options are
		 * appendonly and its variants
		 */
		if (!pg_strcasecmp(name, "appendonly") ||
			!pg_strcasecmp(name, "appendoptimized") ||
			!pg_strcasecmp(name, "orientation"))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid storage option \"%s\"", name),
					 errhint("For table access methods use \"default_table_access_method\" instead.")));
		}
		else
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
	ao_opts->blocksize = AO_DEFAULT_BLOCKSIZE;
	ao_opts->checksum = AO_DEFAULT_CHECKSUM;
	ao_opts->compresslevel = AO_DEFAULT_COMPRESSLEVEL;
	ao_opts->compresstype[0] = '\0';
}

/*
 * This needs to happen whenever gp_default_storage_options GUC is
 * reset.
 */
void
resetDefaultAOStorageOpts(void)
{
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
	Assert(copy);

	memcpy(&ao_storage_opts, copy, sizeof(ao_storage_opts));
	if (pg_strcasecmp(copy->compresstype, "none") == 0)
	{
		/* Represent compresstype=none as an empty string (MPP-25073). */
		ao_storage_opts.compresstype[0] = '\0';
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
parseAOStorageOpts(const char *opts_str)
{
	int			dims[1];
	int			lbs[1];
	Datum		result;
	ArrayBuildState *astate;

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
					accumAOStorageOpt(name, value, astate);
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
	Datum	   *withDatums = NULL;
	Datum		d;
	text	   *t;
	int			i,
				withLen,
				soptLen,
				nWithOpts = 0;
	ArrayType  *withArr;
	ArrayBuildState *astate = NULL;
	bool		foundBlksz = false,
				foundComptype = false,
				foundComplevel = false,
				foundChecksum = false;

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
			 * "bLoCksiZe=3213".  Therefore we don't set it as reloptions as
			 * is.
			 */
			soptLen = strlen(SOPT_BLOCKSIZE);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_BLOCKSIZE, soptLen) == 0)
			{
				foundBlksz = true;
				d = CStringGetTextDatum(psprintf("%s=%d",
												 SOPT_BLOCKSIZE,
												 opts->blocksize));
				astate = accumArrayResult(astate, d, false, TEXTOID,
										  CurrentMemoryContext);
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
				d = CStringGetTextDatum(psprintf("%s=%s",
												 SOPT_COMPTYPE,
												 (opts->compresstype[0] ? opts->compresstype : "none")));
				astate = accumArrayResult(astate, d, false, TEXTOID,
										  CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_COMPLEVEL);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_COMPLEVEL, soptLen) == 0)
			{
				foundComplevel = true;
				d = CStringGetTextDatum(psprintf("%s=%d",
												 SOPT_COMPLEVEL,
												 opts->compresslevel));
				astate = accumArrayResult(astate, d, false, TEXTOID,
										  CurrentMemoryContext);
			}
			soptLen = strlen(SOPT_CHECKSUM);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_CHECKSUM, soptLen) == 0)
			{
				foundChecksum = true;
				d = CStringGetTextDatum(psprintf("%s=%s",
												 SOPT_CHECKSUM,
												 (opts->checksum ? "true" : "false")));
				astate = accumArrayResult(astate, d, false, TEXTOID,
										  CurrentMemoryContext);
			}

			/*
			 * Record fillfactor only if it's specified in WITH clause.
			 * Default fillfactor is assumed otherwise.
			 */
			soptLen = strlen(SOPT_FILLFACTOR);
			if (withLen > soptLen &&
				pg_strncasecmp(strval, SOPT_FILLFACTOR, soptLen) == 0)
			{
				d = CStringGetTextDatum(psprintf("%s=%d",
												 SOPT_FILLFACTOR,
												 opts->fillfactor));
				astate = accumArrayResult(astate, d, false, TEXTOID,
										  CurrentMemoryContext);
			}
		}
	}

	if ((opts->blocksize != AO_DEFAULT_BLOCKSIZE) && !foundBlksz)
	{
		d = CStringGetTextDatum(psprintf("%s=%d",
										 SOPT_BLOCKSIZE,
										 opts->blocksize));
		astate = accumArrayResult(astate, d, false, TEXTOID,
								  CurrentMemoryContext);
	}

	/*
	 * Record compression options only if compression is enabled.  No need to
	 * check compresstype here as by the time we get here, "opts" should have
	 * been set by default_reloptions() correctly.
	 */
	if (opts->compresslevel > AO_DEFAULT_COMPRESSLEVEL &&
		opts->compresstype[0])
	{
		if (!foundComptype && (
							   (pg_strcasecmp(opts->compresstype, AO_DEFAULT_COMPRESSTYPE) == 0
								&& opts->compresslevel == 1 && !foundComplevel) ||
							   pg_strcasecmp(opts->compresstype,
											 AO_DEFAULT_COMPRESSTYPE) != 0))
		{
			d = CStringGetTextDatum(psprintf("%s=%s",
											 SOPT_COMPTYPE,
											 opts->compresstype));
			astate = accumArrayResult(astate, d, false, TEXTOID,
									  CurrentMemoryContext);
		}
		/* When compression is enabled, default compresslevel is 1. */
		if ((opts->compresslevel != 1) &&
			!foundComplevel)
		{
			d = CStringGetTextDatum(psprintf("%s=%d",
											 SOPT_COMPLEVEL,
											 opts->compresslevel));
			astate = accumArrayResult(astate, d, false, TEXTOID,
									  CurrentMemoryContext);
		}
	}

	if ((opts->checksum != AO_DEFAULT_CHECKSUM) && !foundChecksum)
	{
		d = CStringGetTextDatum(psprintf("%s=%s",
										 SOPT_CHECKSUM,
										 (opts->checksum ? "true" : "false")));
		astate = accumArrayResult(astate, d, false, TEXTOID,
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
	relopt_value *blocksize_opt;
	relopt_value *comptype_opt;
	relopt_value *complevel_opt;
	relopt_value *checksum_opt;

	/* fillfactor */
	fillfactor_opt = get_option_set(options, num_options, SOPT_FILLFACTOR);
	if (fillfactor_opt != NULL)
	{
		result->fillfactor = fillfactor_opt->values.int_val;
	}

	/* blocksize */
	blocksize_opt = get_option_set(options, num_options, SOPT_BLOCKSIZE);
	if (blocksize_opt != NULL)
	{
		if (!KIND_IS_APPENDOPTIMIZED(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"blocksize\" in a non relation object is not supported")));

		result->blocksize = blocksize_opt->values.int_val;

		if (result->blocksize < MIN_APPENDONLY_BLOCK_SIZE ||
			result->blocksize > MAX_APPENDONLY_BLOCK_SIZE ||
			result->blocksize % MIN_APPENDONLY_BLOCK_SIZE != 0)
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("block size must be between 8KB and 2MB and be a multiple of 8KB"),
						 errdetail("Got block size %d.", result->blocksize)));

			result->blocksize = DEFAULT_APPENDONLY_BLOCK_SIZE;
		}

	}

	/* compression type */
	comptype_opt = get_option_set(options, num_options, SOPT_COMPTYPE);
	if (comptype_opt != NULL)
	{
		if (!KIND_IS_APPENDOPTIMIZED(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresstype\" in a non relation object is not supported")));

		if (!compresstype_is_valid(comptype_opt->values.string_val))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("unknown compresstype \"%s\"",
							comptype_opt->values.string_val)));
		for (i = 0; i < strlen(comptype_opt->values.string_val); i++)
			result->compresstype[i] = pg_tolower(comptype_opt->values.string_val[i]);
		result->compresstype[i] = '\0';
	}

	/* compression level */
	complevel_opt = get_option_set(options, num_options, SOPT_COMPLEVEL);
	if (complevel_opt != NULL)
	{
		if (!KIND_IS_APPENDOPTIMIZED(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"compresslevel\" in a non relation object is not supported")));

		result->compresslevel = complevel_opt->values.int_val;

		if (result->compresstype[0] &&
			pg_strcasecmp(result->compresstype, "none") != 0 &&
			result->compresslevel == 0 && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresstype \"%s\" can\'t be used with compresslevel 0",
							result->compresstype)));
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
		if (result->compresslevel > 0 && !result->compresstype[0])
			strlcpy(result->compresstype, AO_DEFAULT_COMPRESSTYPE, sizeof(result->compresstype));

		/* Check upper bound of compresslevel for each compression type */

		if (result->compresstype[0] &&
			(pg_strcasecmp(result->compresstype, "zlib") == 0))
		{
#ifndef HAVE_LIBZ
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("zlib compression is not supported by this build"),
					 errhint("Compile without --without-zlib to use zlib compression.")));
#endif
			if (result->compresslevel > 9)
			{
				if (validate)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("compresslevel=%d is out of range for zlib (should be in the range 1 to 9)",
									result->compresslevel)));

				result->compresslevel = setDefaultCompressionLevel(result->compresstype);
			}
		}

		if (result->compresstype[0] &&
			(pg_strcasecmp(result->compresstype, "zstd") == 0))
		{
#ifndef HAVE_LIBZSTD
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Zstandard library is not supported by this build"),
					 errhint("Compile with --with-zstd to use Zstandard compression.")));
#endif
			if (result->compresslevel > 19)
			{
				if (validate)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("compresslevel=%d is out of range for zstd (should be in the range 1 to 19)",
									result->compresslevel)));

				result->compresslevel = setDefaultCompressionLevel(result->compresstype);
			}
		}

		if (result->compresstype[0] &&
			(pg_strcasecmp(result->compresstype, "quicklz") == 0))
		{
#ifndef HAVE_LIBQUICKLZ
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("QuickLZ library is not supported by this build"),
					 errhint("Compile with --with-quicklz to use QuickLZ compression.")));
#endif
			if (result->compresslevel != 1)
			{
				if (validate)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("compresslevel=%d is out of range for quicklz (should be 1)",
									result->compresslevel)));

				result->compresslevel = setDefaultCompressionLevel(result->compresstype);
			}
		}

		if (result->compresstype[0] &&
			(pg_strcasecmp(result->compresstype, "rle_type") == 0) &&
			(result->compresslevel > 4))
		{
			if (validate)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for rle_type (should be in the range 1 to 4)",
								result->compresslevel)));

			result->compresslevel = setDefaultCompressionLevel(result->compresstype);
		}
	}

	/* checksum */
	checksum_opt = get_option_set(options, num_options, SOPT_CHECKSUM);
	if (checksum_opt != NULL)
	{
		if (!KIND_IS_APPENDOPTIMIZED(kind) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("usage of parameter \"checksum\" in a non relation object is not supported")));

		result->checksum = checksum_opt->values.bool_val;
	}

	if (result->compresstype[0] &&
		result->compresslevel == AO_DEFAULT_COMPRESSLEVEL)
	{
		result->compresslevel = setDefaultCompressionLevel(result->compresstype);
	}
}

void
validate_and_refill_options(StdRdOptions *result, relopt_value *options,
							int numrelopts, relopt_kind kind, bool validate)
{
	if (validate &&
		ao_storage_opts_changed &&
		KIND_IS_APPENDOPTIMIZED(kind))
	{
		if (!(get_option_set(options, numrelopts, SOPT_FILLFACTOR)))
			result->fillfactor = ao_storage_opts.fillfactor;

		if (!(get_option_set(options, numrelopts, SOPT_BLOCKSIZE)))
			result->blocksize = ao_storage_opts.blocksize;

		if (!(get_option_set(options, numrelopts, SOPT_COMPLEVEL)))
			result->compresslevel = ao_storage_opts.compresslevel;

		if (!(get_option_set(options, numrelopts, SOPT_COMPTYPE)))
			strlcpy(result->compresstype, ao_storage_opts.compresstype, sizeof(result->compresstype));

		if (!(get_option_set(options, numrelopts, SOPT_CHECKSUM)))
			result->checksum = ao_storage_opts.checksum;
	}

	validate_and_adjust_options(result, options, numrelopts, kind, validate);
}

void
parse_validate_reloptions(StdRdOptions *result, Datum reloptions,
						  bool validate, relopt_kind kind)
{
	relopt_value *options;
	int			num_options;

	options = parseRelOptions(reloptions, validate, kind, &num_options);

	validate_and_adjust_options(result, options, num_options, kind, validate);
	free_options_deep(options, num_options);
}

/*
 * validateAppendOnlyRelOptions
 *
 *		Various checks for validity of appendonly relation rules.
 */
void
validateAppendOnlyRelOptions(int blocksize,
							 int safewrite,
							 int complevel,
							 char *comptype,
							 bool checksum,
							 bool co)
{
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

		if (comptype && (pg_strcasecmp(comptype, "zlib") == 0))
		{
#ifndef HAVE_LIBZ
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("zlib compression is not supported by this build"),
					 errhint("Compile without --without-zlib to use zlib compression.")));
#endif
			if (complevel < 0 || complevel > 9)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range (should be between 0 and 9)",
								complevel)));
			}
		}

		if (comptype && (pg_strcasecmp(comptype, "zstd") == 0))
		{
#ifndef HAVE_LIBZSTD
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Zstandard library is not supported by this build"),
					 errhint("Compile without --without-zstd to use Zstandard compression.")));
#endif
			if (complevel < 0 || complevel > 19)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for zstd (should be in the range 1 to 19)",
								complevel)));
		}

		if (comptype && (pg_strcasecmp(comptype, "quicklz") == 0))
		{
#ifndef HAVE_LIBQUICKLZ
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("QuickLZ library is not supported by this build"),
					 errhint("Compile with --with-quicklz to use QuickLZ compression.")));
#endif
			if (complevel != 1)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("compresslevel=%d is out of range for quicklz (should be 1)",
								complevel)));
		}
		if (comptype && (pg_strcasecmp(comptype, "rle_type") == 0) &&
			(complevel < 0 || complevel > 4))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("compresslevel=%d is out of range for rle_type (should be in the range 1 to 4)",
							complevel)));
		}
	}

	if (blocksize < MIN_APPENDONLY_BLOCK_SIZE ||
		blocksize > MAX_APPENDONLY_BLOCK_SIZE ||
		blocksize % MIN_APPENDONLY_BLOCK_SIZE != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size must be between 8KB and 2MB and be an 8KB multiple, got %d", blocksize)));

	if (safewrite > MAX_APPENDONLY_BLOCK_SIZE || safewrite % 8 != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("safefswrite size must be less than 8MB and be a multiple of 8")));

	if (gp_safefswritesize > blocksize)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size (%d) is smaller gp_safefswritesize (%d)",
						blocksize, gp_safefswritesize),
				 errhint("Increase blocksize or decrease gp_safefswritesize if it is safe to do so on this file system.")));
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

/* ------------------------------------------------------------------------
 * Attribute Encoding specific functions
 * ------------------------------------------------------------------------
 */

/*
 * Add any missing encoding attributes (compresstype = none,
 * blocksize=...).  The column specific encoding attributes supported
 * today are compresstype, compresslevel and blocksize.  Refer to
 * pg_compression.c for more info.
 *
 */
static List *
fillin_encoding(List *aocoColumnEncoding)
{
	bool foundCompressType = false;
	bool foundCompressTypeNone = false;
	char *cmplevel = NULL;
	bool foundBlockSize = false;
	char *arg;
	List *retList = list_copy(aocoColumnEncoding);
	ListCell *lc;
	DefElem *el;
	const StdRdOptions *ao_opts = currentAOStorageOptions();

	foreach(lc, aocoColumnEncoding)
	{
		el = lfirst(lc);

		if (pg_strcasecmp("compresstype", el->defname) == 0)
		{
			foundCompressType = true;
			arg = defGetString(el);
			if (pg_strcasecmp("none", arg) == 0)
				foundCompressTypeNone = true;
		}
		else if (pg_strcasecmp("compresslevel", el->defname) == 0)
		{
			cmplevel = defGetString(el);
		}
		else if (pg_strcasecmp("blocksize", el->defname) == 0)
			foundBlockSize = true;
	}

	if (foundCompressType == false && cmplevel == NULL)
	{
		/* No compression option specified, use current defaults. */
		arg = ao_opts->compresstype[0] ?
				pstrdup(ao_opts->compresstype) : "none";
		el = makeDefElem("compresstype", (Node *) makeString(arg), -1);
		retList = lappend(retList, el);
		el = makeDefElem("compresslevel",
						 (Node *) makeInteger(ao_opts->compresslevel),
						 -1);
		retList = lappend(retList, el);
	}
	else if (foundCompressType == false && cmplevel)
	{
		if (strcmp(cmplevel, "0") == 0)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresslevel=0.
			 */
			el = makeDefElem("compresstype", (Node *) makeString("none"), -1);
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * User wants to enable compression by specifying non-zero
			 * compresslevel.  Therefore, choose default compresstype
			 * if configured, otherwise use zlib.
			 */
			if (ao_opts->compresstype[0] &&
				strcmp(ao_opts->compresstype, "none") != 0)
			{
				arg = pstrdup(ao_opts->compresstype);
			}
			else
			{
				arg = AO_DEFAULT_COMPRESSTYPE;
			}
			el = makeDefElem("compresstype", (Node *) makeString(arg), -1);
			retList = lappend(retList, el);
		}
	}
	else if (foundCompressType && cmplevel == NULL)
	{
		if (foundCompressTypeNone)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresstype=none.
			 */
			el = makeDefElem("compresslevel", (Node *) makeInteger(0), -1);
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * Valid compresstype specified.  Use default
			 * compresslevel if it's non-zero, otherwise use 1.
			 */
			el = makeDefElem("compresslevel",
							 (Node *) makeInteger(ao_opts->compresslevel > 0 ?
												  ao_opts->compresslevel : 1),
							 -1);
			retList = lappend(retList, el);
		}
	}
	if (foundBlockSize == false)
	{
		el = makeDefElem("blocksize", (Node *) makeInteger(ao_opts->blocksize), -1);
		retList = lappend(retList, el);
	}
	return retList;
}

/*
 * See if two encodings attempt to set the same parameters.
 */
static bool
encodings_overlap(List *a, List *b)
{
	ListCell *lca;
	foreach(lca, a)
	{
		ListCell *lcb;
		DefElem *ela = lfirst(lca);

		foreach(lcb, b)
		{
			DefElem *elb = lfirst(lcb);
			if (pg_strcasecmp(ela->defname, elb->defname) == 0)
				return true;
		}
	}
	return false;
}

/*
 * Validate the sanity of column reference storage clauses.
 *
 * 1. Ensure that we only refer to columns that exist.
 * 2. Ensure that each column is referenced either zero times or once.
 * 3. Ensure that the column reference storage clauses do not clash with the
 * 	  gp_default_storage_options
 */
static void
validateColumnStorageEncodingClauses(List *aocoColumnEncoding,
									 List *tableElts)
{
	ListCell *lc;
	struct HTAB *ht = NULL;
	struct colent {
		char colname[NAMEDATALEN];
		int count;
	} *ce = NULL;

	/* Generate a hash table for all the columns */
	foreach(lc, tableElts)
	{
		Node *n = lfirst(lc);

		if (IsA(n, ColumnDef))
		{
			ColumnDef *c = (ColumnDef *)n;
			char *colname;
			bool found = false;
			size_t n = NAMEDATALEN - 1 < strlen(c->colname) ?
							NAMEDATALEN - 1 : strlen(c->colname);

			colname = palloc0(NAMEDATALEN);
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->colname, n);
			colname[n] = '\0';

			if (!ht)
			{
				HASHCTL  cacheInfo;
				int      cacheFlags;

				memset(&cacheInfo, 0, sizeof(cacheInfo));
				cacheInfo.keysize = NAMEDATALEN;
				cacheInfo.entrysize = sizeof(*ce);
				cacheFlags = HASH_ELEM;

				ht = hash_create("column info cache",
								 list_length(tableElts),
								 &cacheInfo, cacheFlags);
			}

			ce = hash_search(ht, colname, HASH_ENTER, &found);

			/*
			 * The user specified a duplicate column name. We check duplicate
			 * column names VERY late (under MergeAttributes(), which is called
			 * by DefineRelation(). For the specific case here, it is safe to
			 * call out that this is a duplicate. We don't need to delay until
			 * we look at inheritance.
			 */
			if (found)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" duplicated",
								colname)));
			}
			ce->count = 0;
		}
	}

	/*
	 * If the table has no columns -- usually in the partitioning case -- then
	 * we can short circuit.
	 */
	if (!ht)
		return;

	/*
	 * All column reference storage directives without the DEFAULT
	 * clause should refer to real columns.
	 */
	foreach(lc, aocoColumnEncoding)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Assert(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
			continue;
		else
		{
			bool found = false;
			char colname[NAMEDATALEN];
			size_t collen = strlen(c->column);
			size_t n = NAMEDATALEN - 1 < collen ? NAMEDATALEN - 1 : collen;
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->column, n);
			colname[n] = '\0';

			ce = hash_search(ht, colname, HASH_FIND, &found);

			if (!found)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("column \"%s\" does not exist", colname)));

			ce->count++;

			if (ce->count > 1)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("column \"%s\" referenced in more than one COLUMN ENCODING clause",
								colname)));
		}
	}

	hash_destroy(ht);

	foreach(lc, aocoColumnEncoding)
	{
		ColumnReferenceStorageDirective *crsd = lfirst(lc);

		Datum d = transformRelOptions(PointerGetDatum(NULL),
									  crsd->encoding,
									  NULL, NULL,
									  true, false);
		StdRdOptions *stdRdOptions = (StdRdOptions *)default_reloptions(d,
																	true,
																	RELOPT_KIND_APPENDOPTIMIZED);

		validateAppendOnlyRelOptions(stdRdOptions->blocksize,
									 gp_safefswritesize,
									 stdRdOptions->compresslevel,
									 stdRdOptions->compresstype,
									 stdRdOptions->checksum,
									 true);
	}
}

/*
 * Make a default column storage directive from a WITH clause
 * Ignore options in the WITH clause that don't appear in
 * storage_directives for column-level compression.
 */
List *
form_default_storage_directive(List *enc)
{
	List *out = NIL;
	ListCell *lc;

	foreach(lc, enc)
	{
		DefElem *el = lfirst(lc);

		if (!el->defname)
			out = lappend(out, copyObject(el));

		if (pg_strcasecmp("oids", el->defname) == 0)
			continue;
		if (pg_strcasecmp("fillfactor", el->defname) == 0)
			continue;
		if (pg_strcasecmp("tablename", el->defname) == 0)
			continue;
		/* checksum is not a column specific attribute. */
		if (pg_strcasecmp("checksum", el->defname) == 0)
			continue;
		out = lappend(out, copyObject(el));
	}
	return out;
}

/*
 * Transform and validate the actual encoding clauses.
 *
 * We need tell the underlying system that these are AO/CO tables too,
 * hence the concatenation of the extra elements.
 *
 * If 'validate' is true, we validate that the optionsa are valid WITH options
 * for an AO table. Otherwise, any unrecognized options are passed through as
 * is.
 */
List *
transformStorageEncodingClause(List *aocoColumnEncoding, bool validate)
{
	ListCell   *lc;
	DefElem	   *dl;

	foreach(lc, aocoColumnEncoding)
	{
		dl = (DefElem *) lfirst(lc);
		if (pg_strncasecmp(dl->defname, SOPT_CHECKSUM, strlen(SOPT_CHECKSUM)) == 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("\"%s\" is not a column specific option",
							SOPT_CHECKSUM)));
		}
	}

	/* add defaults for missing values */
	aocoColumnEncoding = fillin_encoding(aocoColumnEncoding);

	/*
	 * The following two statements validate that the encoding clause is well
	 * formed.
	 */
	if (validate)
	{
		Datum		d;

		d = transformRelOptions(PointerGetDatum(NULL),
								aocoColumnEncoding,
								NULL, NULL,
								true, false);
		(void) default_reloptions(d, true, RELOPT_KIND_APPENDOPTIMIZED);
	}

	return aocoColumnEncoding;
}

/*
 * Find the column reference storage encoding clause for `column'.
 *
 * This is called by transformAttributeEncoding() in a loop but stenc should be
 * quite small in practice.
 */
static ColumnReferenceStorageDirective *
find_crsd(const char *column, List *stenc)
{
	ListCell *lc;

	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		if (c->deflt == false && strcmp(column, c->column) == 0)
			return c;
	}
	return NULL;
}

/*
 * Parse and validate COLUMN <col> ENCODING ... directives.
 *
 * The 'columns', 'stenc' and 'taboptions' arguments are parts of the
 * CREATE TABLE command:
 *
 * 'columns' - list of ColumnDefs
 * 'stenc' - list of ColumnReferenceStorageDirectives
 * 'withOptions' - list of WITH options
 *
 * ENCODING options can be attached to column definitions, like
 * "mycolumn integer ENCODING ..."; these go into ColumnDefs. They
 * can also be specified with the "COLUMN mycolumn ENCODING ..." syntax;
 * they go into the ColumnReferenceStorageDirectives. And table-wide
 * defaults can be given in the WITH clause.
 *
 * If any ENCODING clauses were given, *found_enc is set to true.
 * That's a separate output argument, because the returned list will
 * include defaults from the GUCs etc. even if no ENCODING clause was
 * given in this CREATE TABLE command.
 *
 * This function is called for RELKIND_PARTITIONED_TABLE as well even if we
 * don't store entries in pg_attribute_encoding for rootpartition. The reason
 * is to transformAttributeEncoding for parent as need to use them later while
 * creating partitions in GPDB legacy partitioning syntax. Hence, if
 * rootpartition add to the list, only encoding elements specified in command,
 * defaults based on GUCs and such are skipped. Each child partition would
 * independently later run through this logic and that time add those GUC
 * specific defaults if required. Reason to avoid adding defaults for
 * rootpartition is need to first merge partition level user specified options
 * and then need to add defaults only for remaining columns.
 *
 * NOTE: This is *not* performed during the parse analysis phase, like
 * most transformation, but only later in DefineRelation(). This needs
 * access to possible inherited columns, so it can only be done after
 * expanding them.
 *
 * GPDB_12_AFTER_MERGE_FIXME:
 *		Attempt to code, precedence, merging and access to possibly existing
 *		encodings in this function, in a manner similar to
 *		validateColumnStorageEncodingClauses(). Currently the whole transform,
 *		merge, validate dance is happening both inside and outside this function
 *		but not consistently in all cases. Look at CREATE and ALTER table for
 *		specifics.
 */
List *
transformAttributeEncoding(List *columns,
						   List *stenc,
						   List *withOptions,
						   bool rootpartition,
						   bool *found_enc)
{
	ListCell   *lc;
	ColumnReferenceStorageDirective *deflt = NULL;
	List	   *newenc = NIL;
	List	   *tmpenc;

	*found_enc = false;

	validateColumnStorageEncodingClauses(stenc, columns);

	/* get the default clause, if there is one. */
	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);
		Assert(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
		{
			/*
			 * Some quick validation: there should only be one default
			 * clause
			 */
			if (deflt)
				elog(ERROR, "only one default column encoding may be specified");

			deflt = copyObject(c);
			deflt->encoding = transformStorageEncodingClause(deflt->encoding, true);

			/*
			 * The default encoding and the with clause better not
			 * try and set the same options!
			 */
			if (encodings_overlap(withOptions, deflt->encoding))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("DEFAULT COLUMN ENCODING clause cannot override values set in WITH clause")));
		}

		*found_enc = true;
	}

	/*
	 * If no default has been specified, we might create one out of the
	 * WITH clause.
	 */
	tmpenc = form_default_storage_directive(withOptions);
	if (tmpenc)
	{
		deflt = makeNode(ColumnReferenceStorageDirective);
		deflt->deflt = true;
		deflt->encoding = transformStorageEncodingClause(tmpenc, false);
	}

	/*
	 * Loop over all columns. If a column has a column reference storage clause
	 * -- i.e., COLUMN name ENCODING () -- apply that. Otherwise, apply the
	 * default.
	 */
	foreach(lc, columns)
	{
		Node	   *elem = (Node *) lfirst(lc);
		ColumnDef *d;
		ColumnReferenceStorageDirective *c;
		List *encoding = NIL;

		/* GPDB_12_AFTER_MERGE_FIXME: can we have anything else here? This used to be an Insist */
		if (!IsA(elem, ColumnDef))
			continue;

		d = (ColumnDef *) elem;

		/*
		 * Find a storage encoding for this column, in this order:
		 *
		 * 1. An explicit encoding clause in the ColumnDef
		 * 2. A column reference storage directive for this column
		 * 3. A default column encoding in the statement
		 * 4. A default for the type.
		 */
		if (d->encoding)
		{
			encoding = transformStorageEncodingClause(d->encoding, true);
			*found_enc = true;
		}
		else
		{
			ColumnReferenceStorageDirective *s = find_crsd(d->colname, stenc);

			if (s)
				encoding = transformStorageEncodingClause(s->encoding, true);
			else
			{
				if (deflt)
					encoding = copyObject(deflt->encoding);
				else if (!rootpartition)
				{
					List	   *te;

					if (d->typeName)
						te = TypeNameGetStorageDirective(d->typeName);
					else
						te = NIL;

					if (te)
						encoding = copyObject(te);
					else
						encoding = default_column_encoding_clause(NULL);
				}
			}
		}

		if (encoding)
		{
			c = makeNode(ColumnReferenceStorageDirective);
			c->column = pstrdup(d->colname);
			c->encoding = encoding;

			newenc = lappend(newenc, c);
		}
	}

	Assert(rootpartition?true:list_length(newenc) == list_length(columns));
	return newenc;
}
