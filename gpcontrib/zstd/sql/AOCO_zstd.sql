-- Given that we built with and have zstd compression available
-- Test basic create table for AO/CO table succeeds for zstd compression

-- Given a column-oriented table with compresstype zstd
DROP TABLE IF EXISTS a_aoco_table_with_zstd_compression;
CREATE TABLE a_aoco_table_with_zstd_compression(col text) WITH (APPENDONLY=true, COMPRESSTYPE=zstd, compresslevel=1, ORIENTATION=column);
-- Before I insert data, the size is 0 and compression ratio is unavailable (-1)
SELECT pg_size_pretty(pg_relation_size('a_aoco_table_with_zstd_compression')),
       get_ao_compression_ratio('a_aoco_table_with_zstd_compression');
-- After I insert data
INSERT INTO a_aoco_table_with_zstd_compression values('ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh');
-- Then the data will be compressed according to a consistent compression ratio
select pg_size_pretty(pg_relation_size('a_aoco_table_with_zstd_compression')),
       get_ao_compression_ratio('a_aoco_table_with_zstd_compression');