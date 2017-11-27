/*-------------------------------------------------------------------------
 *
 * reloptions.h
 *	  Core support for relation options (pg_class.reloptions)
 *
 * Note: the functions dealing with text-array reloptions values declare
 * them as Datum, not ArrayType *, to avoid needing to include array.h
 * into a lot of low-level code.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/access/reloptions.h,v 1.6 2009/01/01 17:23:56 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef RELOPTIONS_H
#define RELOPTIONS_H

#include "access/htup.h"
#include "nodes/pg_list.h"
#include "utils/rel.h"

#define AO_DEFAULT_APPENDONLY     false
#define AO_DEFAULT_BLOCKSIZE      DEFAULT_APPENDONLY_BLOCK_SIZE
/* Compression is turned off by default. */
#define AO_DEFAULT_COMPRESSLEVEL  0
/*
 * If compression is turned on without specifying compresstype, this
 * is the default.
 */
#define AO_DEFAULT_COMPRESSTYPE   "zlib"
#define AO_DEFAULT_CHECKSUM       true
#define AO_DEFAULT_COLUMNSTORE    false

extern Datum transformRelOptions(Datum oldOptions, List *defList,
					bool ignoreOids, bool isReset);

extern List *untransformRelOptions(Datum options);

extern void parseRelOptions(Datum options, int numkeywords,
				const char *const * keywords,
				char **values, bool validate);

extern bytea *default_reloptions(Datum reloptions, bool validate, char relkind,
				   int minFillfactor, int defaultFillfactor);

extern bytea *heap_reloptions(char relkind, Datum reloptions, bool validate);

extern bytea *index_reloptions(RegProcedure amoptions, Datum reloptions,
				 bool validate);

extern void validateAppendOnlyRelOptions(bool ao, int blocksize, int writesize,
										 int complevel, char* comptype, 
										 bool checksum, char relkind, bool co);

extern Datum transformAOStdRdOptions(StdRdOptions *opts, Datum withOpts);

extern void resetDefaultAOStorageOpts(void);
extern void resetAOStorageOpts(StdRdOptions *ao_opts);
extern bool isDefaultAOCS(void);
extern bool isDefaultAO(void);
extern void setDefaultAOStorageOpts(StdRdOptions *copy);
extern const StdRdOptions *currentAOStorageOptions(void);
extern Datum parseAOStorageOpts(const char *opts_str, bool *aovalue);
extern void parse_validate_reloptions(StdRdOptions *result, Datum reloptions,
									  bool validate, char relkind);

#endif   /* RELOPTIONS_H */
