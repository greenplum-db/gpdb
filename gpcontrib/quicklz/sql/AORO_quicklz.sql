-- Given that we built with and have quicklz compression available
-- Test basic create table for AO/RO table succeeds for quicklz compression

-- Given a row-oriented table with compresstype quicklz
create table a_aoro_table_with_quicklz_compression(col text) WITH (APPENDONLY=true, COMPRESSTYPE=quicklz, compresslevel=1);
-- Before inserting data, the size is 0 and ratio is 1 (for a row-oriented table, ends up being 1)
select pg_size_pretty(pg_relation_size('a_aoro_table_with_quicklz_compression')),
       get_ao_compression_ratio('a_aoro_table_with_quicklz_compression');
-- When I insert data
insert into a_aoro_table_with_quicklz_compression values('ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh');
-- Then the data will be compressed according to a consistent compression ratio
select pg_size_pretty(pg_relation_size('a_aoro_table_with_quicklz_compression')),
       get_ao_compression_ratio('a_aoro_table_with_quicklz_compression');

