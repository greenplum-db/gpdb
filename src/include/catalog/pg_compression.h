/*-------------------------------------------------------------------------
 *
 * pg_compress.h
 * 
 * 		Representation of compression algorithms for user extensibility of
 * 		compression used by the storage layer.
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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

#include "utils/relcache.h"

/* ----------------
 *		pg_compression definition.  cpp turns this into
 *		typedef struct FormData_pg_compression
 * ----------------
 */
#define CompressionRelationId	3056

CATALOG(pg_compression,3056)
{
	NameData	compname;			
	regproc		compconstructor;	
	regproc		compdestructor;		
	regproc		compcompressor;		
	regproc		compdecompressor;	
	regproc		compvalidator;		
	Oid			compowner;			
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


/* ----------------
 *		compiler constants for pg_compression
 * ----------------
 */
#define Natts_pg_compression					7
#define Anum_pg_compression_compname			1
#define Anum_pg_compression_compconstructor		2
#define Anum_pg_compression_compdestructor		3
#define Anum_pg_compression_compcompressor		4
#define Anum_pg_compression_compdecompressor	5
#define Anum_pg_compression_compvalidator		6
#define Anum_pg_compression_compowner			7

/* Initial contents */
DATA(insert OID = 3060 ( zlib gp_zlib_constructor gp_zlib_destructor gp_zlib_compress gp_zlib_decompress gp_zlib_validator PGUID ));

DATA(insert OID = 3062 ( rle_type gp_rle_type_constructor gp_rle_type_destructor gp_rle_type_compress gp_rle_type_decompress gp_rle_type_validator PGUID ));

DATA(insert OID = 3063 ( none gp_dummy_compression_constructor gp_dummy_compression_destructor gp_dummy_compression_compress gp_dummy_compression_decompress gp_dummy_compression_validator PGUID ));

DATA(insert OID = 3070 ( zstd gp_zstd_constructor gp_zstd_destructor gp_zstd_compress gp_zstd_decompress gp_zstd_validator PGUID ));

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
extern List *default_column_encoding_clause(void);
extern PGFunction *GetCompressionImplementation(char *comptype);
extern bool is_storage_encoding_directive(char *name);

#endif   /* PG_COMPRESSION */
