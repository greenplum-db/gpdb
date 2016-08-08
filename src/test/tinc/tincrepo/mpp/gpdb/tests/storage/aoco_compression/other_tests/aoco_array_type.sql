-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- AO table
-- start_ignore
Drop table if exists ao_array;
-- end_ignore

Create table ao_array( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true);

Insert into ao_array select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from ao_array where a1<10  order by a2,a4 limit 5;

-- AO with compression

-- start_ignore
Drop table if exists ao_array_compr;
-- end_ignore

Create table ao_array_compr( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true, compresstype=zlib);

Insert into ao_array_compr select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from ao_array_compr where a1<10  order by a2,a4 limit 5;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('ao_array_compr');


-- CO tables
-- start_ignore
Drop table if exists co_array;
-- end_ignore

Create table co_array( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true, orientation=column);

Insert into co_array select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from co_array where a1<10  order by a2,a4 limit 5;

-- CO tables with zlib

-- start_ignore
Drop table if exists co_array_zlib;
-- end_ignore

Create table co_array_zlib ( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true, orientation=column, compresstype=zlib);

Insert into co_array_zlib select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from co_array_zlib where a1<10  order by a2,a4 limit 5;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_array_zlib');


-- CO tables with quicklz

-- start_ignore
Drop table if exists co_array_quicklz;
-- end_ignore

Create table co_array_quicklz ( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true, orientation=column, compresstype=quicklz);

Insert into co_array_quicklz select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from co_array_quicklz where a1<10  order by a2,a4 limit 5;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_array_quicklz');
-- CO tables with RLE

-- start_ignore
Drop table if exists co_array_rle;
-- end_ignore

Create table co_array_rle ( a1 int, a2 integer[], a3 text, a4 integer[][]) with(appendonly=true, orientation=column, compresstype=rle_type);

Insert into co_array_rle select i, ARRAY[i, i+5, i*5], 'array_type', ARRAY[[i/5,i/10],[i/10,i/5]] from generate_series(1,100) i;

Select a2,a4 from co_array_rle where a1<10  order by a2,a4 limit 5;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_array_rle');
