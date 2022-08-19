-- Given that we built with and have lz4 compression available
-- Test basic create table for AO/RO table succeeds for lz4 compression

-- Given a row-oriented table with compresstype lz4
create table a_aoro_table_with_lz4_compression(col text) WITH (APPENDONLY=true, ORIENTATION=row, COMPRESSTYPE=lz4, compresslevel=1);
-- Before inserting data, the size is 0 and ratio is 1 (for a row-oriented table, ends up being 1)
select pg_size_pretty(pg_relation_size('a_aoro_table_with_lz4_compression')),
       get_ao_compression_ratio('a_aoro_table_with_lz4_compression');
-- After I insert data
insert into a_aoro_table_with_lz4_compression values('ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh');
-- Then the data will be compressed according to a consistent compression ratio
select pg_size_pretty(pg_relation_size('a_aoro_table_with_lz4_compression')),
       get_ao_compression_ratio('a_aoro_table_with_lz4_compression');
