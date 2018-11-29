CREATE FUNCTION gp_zstd_constructor(internal, internal, bool) RETURNS internal
LANGUAGE C VOLATILE AS '$libdir/gp_zstd_compression.so', 'zstd_constructor';
COMMENT ON FUNCTION gp_zstd_constructor(internal, internal, bool) IS 'zstd compressor and decompressor constructor';

CREATE FUNCTION gp_zstd_destructor(internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_zstd_compression.so', 'zstd_destructor';
COMMENT ON FUNCTION gp_zstd_destructor(internal) IS 'zstd compressor and decompressor destructor';

CREATE FUNCTION gp_zstd_compress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_zstd_compression.so', 'zstd_compress';
COMMENT ON FUNCTION gp_zstd_compress(internal, int4, internal, int4, internal, internal) IS 'zstd compressor';

CREATE FUNCTION gp_zstd_decompress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_zstd_compression.so', 'zstd_decompress';
COMMENT ON FUNCTION gp_zstd_decompress(internal, int4, internal, int4, internal, internal) IS 'zstd decompressor';

CREATE FUNCTION gp_zstd_validator(internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/gp_zstd_compression.so', 'zstd_validator';
COMMENT ON FUNCTION gp_zstd_validator(internal) IS 'zstdcompression validator';

INSERT INTO pg_catalog.pg_compression (compname, compconstructor, compdestructor, compcompressor, compdecompressor, compvalidator, compowner)
VALUES ('zstd', 'gp_zstd_constructor', 'gp_zstd_destructor', 'gp_zstd_compress', 'gp_zstd_decompress', 'gp_zstd_validator', 10 /* BOOTSTRAP_SUPERUSERID */);
