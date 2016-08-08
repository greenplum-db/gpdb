-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Table with delta on one column quicklz on other columns

-- start_ignore
Drop table if exists delta_quicklz;
-- end_ignore

Create table delta_quicklz(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=quicklz,compresslevel=1),
    a3 date ENCODING (compresstype=rle_type,compresslevel=2),
    a4 time ENCODING (compresstype=rle_type,compresslevel=3),
    a5 timestamp ENCODING (compresstype=quicklz,compresslevel=1),
    a6 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1),
    a7 integer ENCODING (compresstype=rle_type,compresslevel=4),
    a8 bigint ENCODING (compresstype=quicklz,compresslevel=1),
    a9 text ENCODING (compresstype=rle_type,compresslevel=2)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_quicklz'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_quicklz

Insert into delta_quicklz select 
    i/20, i/12, date '2012-02-02' + i/5,
    case when i/5=1 then time '20:13:14.343536' when i/5=2 then time '12:13:11.232421' when i/5=4 then time '10:12:13.241122' else '00:02:03' end,
    case when i/5=3 then timestamp '2014-07-30 14:22:58.356229' when i/5=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2015-02-05 12:00:33.778899' end,
    case when i/5=2 then timestamp '2014-07-30 14:22:23.776892-07' when i/5=3 then timestamp '2014-07-22 11:12:13.006892-07' else timestamp '2012-06-30 04:00:00.333892-07' end,
    i*2/15, 200/i, 
    case when i/20=0 then 'some value for text column' else 'lets try inserting a different value' end  
    from generate_series(1,100) i ;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_quicklz');

select a1,a2,a5,a7,a9 from delta_quicklz order by a1,a2,a5,a7,a9 limit 5;
select a1,a3,a4,a6,a8 from delta_quicklz order by a1 desc ,a3,a4,a6,a8 limit 5;
    
