-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Table with delta on one column zlib,quicklz,none  on other columns

-- start_ignore
Drop table if exists delta_mix;
-- end_ignore

Create table delta_mix(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=zlib,compresslevel=1),
    a3 date ENCODING (compresstype=rle_type,compresslevel=2),
    a4 time ENCODING (compresstype=quicklz,compresslevel=1),
    a5 timestamp ENCODING (compresstype=none),
    a6 timestamp with time zone ENCODING (compresstype=rle_type),
    a7 integer,
    a8 bigint ENCODING (compresstype=zlib,compresslevel=7),
    a9 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_mix'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_mix

Insert into delta_mix select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end, i*2/15, 200/i,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_mix select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end, i*2/15, 200/i,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(100,130)i;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_mix');

select a1,a2,a3,a7 from delta_mix order by a1,a2,a3,a7 limit 5;
select * from delta_mix order by a1 desc ,a2,a3,a4,a5,a6,a7,a8,a9 limit 5;
    
Select distinct a1, a9 from delta_mix order by a1, a9 limit 5;

Select distinct a2, a5 from delta_mix order by a2, a5 limit 3; 
