PREPARE attribute_encoding_check AS
SELECT attrelid::regclass AS relname,
attnum, attoptions FROM pg_class c, pg_attribute_encoding e
WHERE c.relname = $1 AND c.oid=e.attrelid
ORDER BY relname, attnum;

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

alter type int42 set default encoding (compresstype=quicklz);
-- Ensure compresstype for type has been modified to be quicklz
select typoptions from pg_type_encoding where typid='public.int42'::regtype;

-- Given an AO/CO table using the int42 type with quicklz compresstype
create table aoco_table_compressed_type (i int42) with(appendonly = true, orientation=column);
-- Attribute should show as compressed
execute attribute_encoding_check('aoco_table_compressed_type');
-- When I insert data
insert into aoco_table_compressed_type select '123456'::int42 from generate_series(1, 1000)i;
-- Then the data should be compressed and return a consistent compression ratio
select get_ao_compression_ratio('aoco_table_compressed_type') as quicklz_compress_ratio;

-- Given an AO/RO table using the int42 type with quicklz compresstype
CREATE TABLE aoro_table_compressed_type (i int42) with(appendonly = true, orientation=row);
-- No results are returned from the attribute encoding check, as compression with quicklz is not supported for row-oriented tables
execute attribute_encoding_check('aoro_table_compressed_type');

-- Given a heap table using the int42 type with quicklz compresstype
CREATE TABLE heap_table_compressed_type (i int42);
-- No results are returned from the attribute encoding check, as compression with quicklz is not supported for heap tables
EXECUTE attribute_encoding_check('heap_table_compressed_type');

-- Given an AO/CO table with a regular int column and a default column encoding of compresstype quicklz
CREATE TABLE aoco_table_default_encoding (i int, default column encoding (compresstype=quicklz, compresslevel=1)) with(appendonly = true, orientation=column);
-- Attribute should show as compressed
execute attribute_encoding_check('aoco_table_default_encoding');
-- When I insert data
INSERT into aoco_table_default_encoding select 1 from generate_series(1, 100);
-- Then the data should be compressed and return a consistent compression ratio
SELECT get_ao_compression_ratio('aoco_table_default_encoding') as quicklz_compress_ratio;
