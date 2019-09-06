DROP DATABASE IF EXISTS zstd_column_compression;
CREATE DATABASE zstd_column_compression;
\c zstd_column_compression

PREPARE attribute_encoding_check AS
SELECT attrelid::regclass AS relname,
attnum, attoptions FROM pg_class c, pg_attribute_encoding e
WHERE c.relname = $1 AND c.oid=e.attrelid
ORDER BY relname, attnum;

drop type if exists int42 cascade;
create type int42;
CREATE FUNCTION int42_in(cstring)
RETURNS int42
AS 'int4in'
LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION int42_out(int42)
RETURNS cstring
AS 'int4out'
LANGUAGE internal IMMUTABLE STRICT;

CREATE TYPE int42 (
internallength = 4,
input = int42_in,
output = int42_out,
alignment = int4,
default = 42,
passedbyvalue,
compresstype="zlib",
blocksize=65536,
compresslevel=1
);

-- Ensure type has been created with compresstype zlib
select typoptions from pg_type_encoding where typid='public.int42'::regtype;

alter type int42 set default encoding (compresstype=zstd);
-- Ensure compresstype for type has been modified to be zstd
select typoptions from pg_type_encoding where typid='public.int42'::regtype;

-- Given an AO/CO table using the int42 type with zstd compresstype
CREATE TABLE IF NOT EXISTS aoco_table_compressed_type (i int42) with(appendonly = true, orientation=column);
-- Attribute should show as compressed
EXECUTE attribute_encoding_check ('aoco_table_compressed_type');
-- When I insert data
insert into aoco_table_compressed_type select '123456'::int42 from generate_series(1, 1000)i;
-- Then the data should be compressed and return a consistent compression ratio
-- Because the ratio varies depending on the version, use > here instead of checking the exact ratio.
select get_ao_compression_ratio('aoco_table_compressed_type') > 21 as zstd_compress_ratio;

-- Given an AO/RO table using the int42 type with zstd compresstype
CREATE TABLE IF NOT EXISTS aoro_table_compressed_type (i int42) WITH (appendonly=true, orientation=row);
-- No results are returned from the attribute encoding check, as compression with zstd is not supported for row-oriented tables
EXECUTE attribute_encoding_check ('aoro_table_compressed_type');

-- Given a heap table using the int42 type with zstd compresstype
CREATE TABLE IF NOT EXISTS heap_table_compressed_type (i int42);
-- No results are returned from the attribute encoding check, as compression with zstd is not supported for heap tables
EXECUTE attribute_encoding_check ('heap_table_compressed_type');

-- Given an AO/CO table with a regular int column and a default column encoding of compresstype zstd
CREATE TABLE IF NOT EXISTS aoco_table_default_encoding (i int, default column encoding (compresstype=zstd, compresslevel=1)) with(appendonly = true, orientation=column);
-- Attribute should show as compressed
EXECUTE attribute_encoding_check ('aoco_table_default_encoding');
-- When I insert data
INSERT into aoco_table_default_encoding select 1 from generate_series(1, 100);
-- Then the data should be compressed and return a consistent compression ratio
-- Because the ratio varies depending on the version, use > here instead of checking the exact ratio.
SELECT get_ao_compression_ratio('aoco_table_default_encoding') > 7 as zstd_compress_ratio;
