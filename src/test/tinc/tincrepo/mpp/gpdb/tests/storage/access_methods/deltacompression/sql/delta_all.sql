-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Table with rle_type different levels + delta columns

-- start_ignore
Drop table if exists delta_all;
-- end_ignore

Create table delta_all(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_all'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_all

Insert into delta_all select                                                                                                                                           
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,    
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,  
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07' 
    else timestamp '2014-07-22 14:00:00.333892-07' end,  
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end  
    from generate_series(1,100)i; 

Insert into delta_all select                                                                                                                                           
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,    
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07' 
    else timestamp '2014-07-22 14:00:00.333892-07' end,  
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end  
    from generate_series(100,130)i; 

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_all');

select * from delta_all order by a1,a2,a3,a4,a5,a6,a7 limit 5;
select * from delta_all order by a1 desc ,a2,a3,a4,a5,a6,a7 limit 5;
    
Select distinct a1, a7 from delta_all order by a1,a7 limit 5;

Select distinct a2, a5 from delta_all order by a2,a5 limit 5; 

