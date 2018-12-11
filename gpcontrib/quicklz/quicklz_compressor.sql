CREATE FUNCTION gp_quicklz_constructor(internal, internal, bool) RETURNS internal
LANGUAGE C VOLATILE AS '$libdir/quicklz_compressor.so', 'quicklz_constructor';
COMMENT ON FUNCTION gp_quicklz_constructor(internal, internal, bool) is 'quicklz constructor';

CREATE FUNCTION gp_quicklz_destructor(internal) RETURNS void
LANGUAGE C VOLATILE AS '$libdir/quicklz_compressor.so', 'quicklz_destructor';
COMMENT ON FUNCTION gp_quicklz_destructor(internal) is 'quicklz destructor';

CREATE FUNCTION gp_quicklz_compress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C IMMUTABLE AS '$libdir/quicklz_compressor.so', 'quicklz_compress';
COMMENT ON FUNCTION gp_quicklz_compress(internal, int4, internal, int4, internal, internal) is 'quicklz compressor';

CREATE FUNCTION gp_quicklz_decompress(internal, int4, internal, int4, internal, internal) RETURNS void
LANGUAGE C IMMUTABLE AS '$libdir/quicklz_compressor.so', 'quicklz_decompress';
COMMENT ON FUNCTION gp_quicklz_decompress(internal, int4, internal, int4, internal, internal) is 'quicklz compressor';

CREATE FUNCTION gp_quicklz_validator(internal) RETURNS void
LANGUAGE C IMMUTABLE AS '$libdir/quicklz_compressor.so', 'quicklz_validator';
COMMENT ON FUNCTION gp_quicklz_validator(internal) is 'quicklz compression validator';

-- There is no CREATE COMPRESSION command, so we have to insert the entry manually.
INSERT INTO pg_catalog.pg_compression (compname, compconstructor, compdestructor, compcompressor, compdecompressor, compvalidator, compowner)
VALUES ('quicklz', 'gp_quicklz_constructor', 'gp_quicklz_destructor', 'gp_quicklz_compress', 'gp_quicklz_decompress', 'gp_quicklz_validator', 10 /* BOOSTRAP_SUPERUSERID */);
