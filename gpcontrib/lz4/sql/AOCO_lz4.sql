-- Given that we built with and have lz4 compression available
-- Test basic create table for AO/CO table succeeds for lz4 compression

-- Given a column-oriented table with compresstype lz4
DROP TABLE IF EXISTS a_aoco_table_with_lz4_compression;
CREATE TABLE a_aoco_table_with_lz4_compression(col text) WITH (APPENDONLY=true, COMPRESSTYPE=lz4, compresslevel=1, ORIENTATION=column);
-- Before I insert data, the size is 0 and compression ratio is unavailable (-1)
SELECT pg_size_pretty(pg_relation_size('a_aoco_table_with_lz4_compression')),
       get_ao_compression_ratio('a_aoco_table_with_lz4_compression');
-- After I insert data
INSERT INTO a_aoco_table_with_lz4_compression values('ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh');
-- Then the data will be compressed according to a consistent compression ratio
select pg_size_pretty(pg_relation_size('a_aoco_table_with_lz4_compression')),
       get_ao_compression_ratio('a_aoco_table_with_lz4_compression');