-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Table with  delta + none compression on some columns

-- start_ignore
Drop table if exists delta_none;
-- end_ignore

Create table delta_none(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=none),
    a3 date ENCODING (compresstype=rle_type,compresslevel=2),
    a4 time ENCODING (compresstype=rle_type,compresslevel=3),
    a5 timestamp ,
    a6 timestamp with time zone ENCODING (compresstype=none),
    a7 integer ENCODING (compresstype=rle_type,compresslevel=4),
    a8 bigint,
    a9 text ENCODING (compresstype=rle_type,compresslevel=2)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_none'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_none

Insert into delta_none select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end, i*2/15, 200/i,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_none');

select a1,a2,a4,a9 from delta_none order by a1,a2,a4,a9 limit 5;
select * from delta_none order by a1 desc ,a2,a3,a4,a5,a6,a7,a8,a9 limit 5;
    
Select a2, a3, a5 from delta_none where a1 <2 order by a2 limit 5;

