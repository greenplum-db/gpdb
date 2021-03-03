/*-------------------------------------------------------------------------
 *
 * pg_compress.h
 * 
 * 		Representation of compression algorithms for user extensibility of
 * 		compression used by the storage layer.
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_compression.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_COMPRESSION
#define PG_COMPRESSION

#include "catalog/genbki.h"
#include "catalog/pg_compression_d.h"
#include "fmgr.h"
#include "utils/relcache.h"

/* ----------------
 *		pg_compression definition.  cpp turns this into
 *		typedef struct FormData_pg_compression
 * ----------------
 */

CATALOG(pg_compression,7056,CompressionRelationId)
{
	NameData	compname;
	regproc		compconstructor BKI_LOOKUP(pg_proc);
	regproc		compdestructor BKI_LOOKUP(pg_proc);
	regproc		compcompressor BKI_LOOKUP(pg_proc);
	regproc		compdecompressor BKI_LOOKUP(pg_proc);
	regproc		compvalidator BKI_LOOKUP(pg_proc);
	Oid			compowner BKI_DEFAULT(PGUID);
} FormData_pg_compression;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(compconstructor REFERENCES pg_proc(oid));
FOREIGN_KEY(compdestructor REFERENCES pg_proc(oid));
FOREIGN_KEY(compcompressor REFERENCES pg_proc(oid));
FOREIGN_KEY(compdecompressor REFERENCES pg_proc(oid));
FOREIGN_KEY(compvalidator REFERENCES pg_proc(oid));
FOREIGN_KEY(compowner REFERENCES pg_authid(oid));

/* ----------------
 *		Form_pg_compression corresponds to a pointer to a tuple with
 *		the format of pg_compression relation.
 * ----------------
 */
typedef FormData_pg_compression *Form_pg_compression;

#define NUM_COMPRESS_FUNCS 5

#define COMPRESSION_CONSTRUCTOR 0
#define COMPRESSION_DESTRUCTOR 1
#define COMPRESSION_COMPRESS 2
#define COMPRESSION_DECOMPRESS 3
#define COMPRESSION_VALIDATOR 4

typedef struct CompressionState
{
	/*
	 * Allows a constructor to tell the calling level the maximum storage
	 * required for input of the given size. Different algorithms need
	 * different maximum buffers.
	 */
	size_t (*desired_sz)(size_t input);

	void *opaque; /* algorithm specific stuff opaque to the caller */
} CompressionState;

typedef struct StorageAttributes
{
	char *comptype; /* compresstype field */
	int complevel; /* compresslevel field */
	size_t blocksize; /* blocksize field */
	Oid	typid; /* Oid of the type being compressed */
} StorageAttributes;

extern CompressionState *callCompressionConstructor(PGFunction constructor,
										TupleDesc tupledesc,
										StorageAttributes *sa,
										bool is_compress);

extern void callCompressionDestructor(PGFunction destructor, CompressionState *state);

extern void callCompressionActuator(PGFunction func, const void *src,
									int32 src_sz, char *dst, int32 dst_sz,
									int32 *dst_used, CompressionState *state);

extern void callCompressionValidator(PGFunction func, char *comptype,
									 int32 complevel, int32 blocksize,
									 Oid typid);

extern bool compresstype_is_valid(char *compresstype);
extern List *default_column_encoding_clause(Relation rel);
extern PGFunction *GetCompressionImplementation(char *comptype);
extern bool is_storage_encoding_directive(char *name);

#endif   /* PG_COMPRESSION */
