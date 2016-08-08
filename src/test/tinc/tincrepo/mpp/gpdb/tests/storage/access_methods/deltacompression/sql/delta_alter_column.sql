-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Alter a delta column to a non-supported column
-- Alter one delta column to another delta column

-- start_ignore
Drop table if exists delta_alter;
-- end_ignore

Create table delta_alter(
    id serial,
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_alter'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_alter

Insert into delta_alter(a1,a2,a3,a4,a5,a6,a7)  select                                                                                                                                           
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,    
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,  
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07' 
    else timestamp '2014-07-22 14:00:00.333892-07' end,  
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end  
    from generate_series(1,100)i; 

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_alter');

select * from delta_alter order by id limit 10;

-- Alter from delta column to non-delta column
Alter table delta_alter Alter column a2 Type double precision;

Alter table delta_alter Alter column a4 Type interval;

\d+ delta_alter

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_alter');

select a1,a2,a3,a4,a5,a6,a7 from delta_alter order by id limit 10;

-- Alter column to other delta column

Alter table delta_alter Alter column a1 Type bigint;

Alter table delta_alter Alter column a6 Type timestamp;

Alter table delta_alter Alter column a5 Type timestamp with time zone;

\d+ delta_alter

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_alter');

select a1,a2,a3,a4,a5,a6,a7 from delta_alter order by id limit 10;

