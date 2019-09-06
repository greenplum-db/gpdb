-- Given that we built with and have quicklz compression available
-- Test basic create table for AO/CO table succeeds for quicklz compression

-- Given a column-oriented table with compresstype quicklz
CREATE TABLE a_aoco_table_with_quicklz_compression(col text) WITH (APPENDONLY=true, COMPRESSTYPE=quicklz, compresslevel=1, ORIENTATION=column);
-- Before I insert data, the size is 0 and compression ratio is unavailable (-1)
SELECT pg_size_pretty(pg_relation_size('a_aoco_table_with_quicklz_compression')),
       get_ao_compression_ratio('a_aoco_table_with_quicklz_compression');
-- When I insert data
INSERT INTO a_aoco_table_with_quicklz_compression values('ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh');
-- Then the data will be compressed according to a consistent compression ratio
SELECT pg_size_pretty(pg_relation_size('a_aoco_table_with_quicklz_compression')),
       get_ao_compression_ratio('a_aoco_table_with_quicklz_compression');
