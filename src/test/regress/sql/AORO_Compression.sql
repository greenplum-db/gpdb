-- Test basic create table for AO/RO table succeeds for zlib compression
-- Given a row-oriented table with compresstype zlib
CREATE TABLE a_aoro_table_with_zlib_compression(col int) WITH (APPENDONLY=true, COMPRESSTYPE=zlib, COMPRESSLEVEL=1, ORIENTATION=row);
SELECT pg_size_pretty(pg_relation_size('a_aoro_table_with_zlib_compression')),
       get_ao_compression_ratio('a_aoro_table_with_zlib_compression');
-- When I insert data
INSERT INTO a_aoro_table_with_zlib_compression SELECT i from generate_series(1, 100)i;
-- Then the data will be compressed according to a consistent compression ratio
SELECT pg_size_pretty(pg_relation_size('a_aoro_table_with_zlib_compression')),
       get_ao_compression_ratio('a_aoro_table_with_zlib_compression');

-- Test basic create table for AO/RO table fails for rle compression. rle is only supported for columnar tables.
CREATE TABLE a_aoro_table_with_rle_type_compression(col int) WITH (APPENDONLY=true, COMPRESSTYPE=rle_type, COMPRESSLEVEL=1, ORIENTATION=row);

-- Test get_ao_compression_ratio
CREATE TABLE test_table (date date)
PARTITION BY RANGE(date)
(
PARTITION test_cmp_02 START ('2022-02-01'::date) END ('2022-03-01'::date) EVERY ('1 mon'::interval) WITH (tablename='test_table_1_prt_cmp_202202', appendonly='true', orientation='row', compresstype=zlib),
START ('2022-03-01'::date) END ('2022-04-01'::date) EVERY ('1 mon'::interval) WITH (tablename='test_table_1_prt_123', appendonly='false')
);

select get_ao_compression_ratio(t.relid) from (select relid from pg_partition_tree('test_table') where isleaf != false) as t;

drop table test_table;
