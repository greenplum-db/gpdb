CREATE FUNCTION gp_lz4_constructor(internal, internal, bool) RETURNS internal
LANGUAGE C VOLATILE AS '$libdir/gp_lz4_compression.so', 'lz4_constructor';
COMMENT ON FUNCTION gp_lz4_constructor(internal, internal, bool) IS 'lz4 compressor and decompressor constructor';

CREATE FUNCTION gp_lz4_destructor(internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_lz4_compression.so', 'lz4_destructor';
COMMENT ON FUNCTION gp_lz4_destructor(internal) IS 'lz4 compressor and decompressor destructor';

CREATE FUNCTION gp_lz4_compress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_lz4_compression.so', 'lz4_compress';
COMMENT ON FUNCTION gp_lz4_compress(internal, int4, internal, int4, internal, internal) IS 'lz4 compressor';

CREATE FUNCTION gp_lz4_decompress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_lz4_compression.so', 'lz4_decompress';
COMMENT ON FUNCTION gp_lz4_decompress(internal, int4, internal, int4, internal, internal) IS 'lz4 decompressor';

CREATE FUNCTION gp_lz4_validator(internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_lz4_compression.so', 'lz4_validator';
COMMENT ON FUNCTION gp_lz4_validator(internal) IS 'lz4 compression validator';

INSERT INTO pg_catalog.pg_compression (compname, compconstructor, compdestructor, compcompressor, compdecompressor, compvalidator, compowner)
VALUES ('lz4', 'gp_lz4_constructor', 'gp_lz4_destructor', 'gp_lz4_compress', 'gp_lz4_decompress', 'gp_lz4_validator', 10 /* BOOTSTRAP_SUPERUSERID */);
