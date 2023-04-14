--
-- Tests on RLE compression and delta encoding.
-- 
-- start_matchignore
-- m/.*compression_ratio .*[02-9]+[.]?(\d+)?/
-- end_matchignore

set time zone PST8PDT;
set datestyle='ISO';
set intervalstyle='sql_standard';

set gp_default_storage_options='checksum=off';

\set HIDE_TABLEAM off

--
-- Table with rle_type different levels + delta columns
--
Create table delta_all(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_all'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_all

Insert into delta_all select i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_all select i/5, i/12, date '2012-02-02' + i/5,
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

--
-- Alter a delta column to a non-supported column
-- Alter one delta column to another delta column
--
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

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_alter'  and c.oid=e.attrelid  order by relname, attnum;

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

--
-- Table with rle+ delta - create table, create bitmap index + insert data, select with index scan
--
Create table delta_bitmap_ins(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

Create index dl_ix_bt on  delta_bitmap_ins using bitmap(a1);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_bitmap_ins'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_bitmap_ins

Insert into delta_bitmap_ins select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_bitmap_ins select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(100,130)i;


select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_bitmap_ins');

set enable_seqscan=off;
set enable_indexscan=on;

select a1,a2,a3,a4,a5,a6,a7 from delta_bitmap_ins where a1< 3 order by a1,a2,a3,a4,a5,a6,a7 limit 5;

reset enable_seqscan;
reset enable_indexscan;

--
-- Table with rle+ delta - create table, create btree index + insert data, select with index scan
--
Create table delta_btree_ins(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

Create index dl_ix_br on  delta_btree_ins(a1);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_btree_ins'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_btree_ins

Insert into delta_btree_ins select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_btree_ins select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(100,130)i;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_btree_ins');

set enable_seqscan=off;
set enable_indexscan=on;

select a1,a2,a3,a4,a5,a6,a7 from delta_btree_ins where a1< 3 order by a1,a2,a3,a4,a5,a6,a7 limit 5;

reset enable_seqscan;
reset enable_indexscan;

--
-- Table with rle+ delta - create table, insert data, create bitmap index, select with index scan
--
Create table delta_ins_bitmap(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_ins_bitmap'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_ins_bitmap

Insert into delta_ins_bitmap select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_ins_bitmap select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(100,130)i;

Create index dl_ins_bt_ix on  delta_ins_bitmap using bitmap(a3);

\d+ delta_ins_bitmap

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_ins_bitmap');

set enable_seqscan=off;
set enable_indexscan=on;

select * from delta_ins_bitmap where a3='2012-02-04' order by a1,a2,a3,a4,a5,a6,a7 limit 5;

reset enable_seqscan;
reset enable_indexscan;

--
-- Table with rle+ delta - create table, insert data, create btree index, select with index scan
--
Create table delta_ins_btree(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=rle_type,compresslevel=2),
    a3 date ENCODING (compresstype=rle_type,compresslevel=3),
    a4 time ENCODING (compresstype=rle_type,compresslevel=4),
    a5 timestamp ENCODING (compresstype=rle_type),
    a6 timestamp with time zone ENCODING (compresstype=rle_type, compresslevel=1),
    a7 text ENCODING (compresstype=rle_type,compresslevel=4)
    ) with(appendonly=true, orientation=column);


select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_ins_btree'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_ins_btree

Insert into delta_ins_btree select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(1,100)i;

Insert into delta_ins_btree select
    i/5, i/12, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/20=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:22:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end,
    case when i/10=0 then 'some value for text column' else 'lets try inserting a different value' end
    from generate_series(100,130)i;


Create index dl_br_ix on  delta_ins_btree(a3);

\d+ delta_ins_btree

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_ins_btree');

set enable_seqscan=off;
set enable_indexscan=on;

select * from delta_ins_btree where a3='2012-02-04'  order by a1,a2,a3,a4,a5,a6,a7 limit 5;

reset enable_seqscan;
reset enable_indexscan;

--
-- Many blocks with delta compression
--
Create table delta_blocks(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1,blocksize=8192);


Create index dl_bt_ix on delta_blocks using bitmap(a1);

Create index dl_ix on delta_blocks(a3);

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

\d+ delta_blocks


Select a1,a2,a6 from delta_blocks where a4 = '20:13:11.232421' order by a1 limit 10;


-- The dl_bt_ix index is quite large, over 600 MB. Let's drop the table, along
-- with the indexes, to keep the regression database size in check.
drop table delta_blocks;

--
-- Table with  delta + none compression on some columns
--
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

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_none'  and c.oid=e.attrelid  order by relname, attnum;

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
    
Select a2, a3, a5 from delta_none where a1 <2 order by a2, a3, a5 limit 10;


--
-- Out-of-order DELETE
-- taken from uao test_suite
--

-- This contrived choice of distribution key is to ensure that
-- subsequent DELETE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order delete.
CREATE TABLE delta_one (a INT, b INT, c CHAR(100)) WITH (appendonly=true, orientation=column, compresstype=rle_type)
DISTRIBUTED BY (b);

\d+ delta_one
INSERT INTO delta_one SELECT i as a, 0 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO delta_one SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE delta_two (a INT, b INT, c CHAR(100)) DISTRIBUTED BY (b);
-- Populate delta_two such that at least one segment contains multiple
-- occurences of the same value for a.
INSERT INTO delta_two SELECT i as a, 0 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 0 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE delta_one;
ANALYZE delta_two;


DELETE FROM delta_one USING delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 10 OR delta_two.a = 40000 OR delta_two.a = 20000);

-- Ensure that tuples to be deleted are from the same GPDB segment.
-- This query should return the same output irrespective of GPDB
-- configuration (1 segdb, 2 or more segdbs).

SELECT distinct(a) FROM delta_one
WHERE gp_segment_id = 1 AND delta_one.a IN (12, 80001, 1001)
ORDER BY a;

DELETE FROM delta_one USING delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 12 OR delta_two.a = 80001 OR delta_two.a = 1001);

--
-- Out-of-order UPDATE
--
-- taken from uao test_suite
--

DROP TABLE IF EXISTS delta_one;
DROP TABLE IF EXISTS delta_two;

-- This contrived choice of distribution key is to ensure that
-- subsequent UPDATE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order updates.
CREATE TABLE delta_one (a INT, b INT, c CHAR(100)) WITH (appendonly=true, orientation=column, compresstype=rle_type)
DISTRIBUTED BY(b);
INSERT INTO delta_one SELECT i as a, 0 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO delta_one SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE delta_two (a INT, b INT, c CHAR(100)) DISTRIBUTED BY(b);
-- Insert unique values for delta_two.a so that we don't get "multiple
-- updates to a row by the same query is not allowed" error later when
-- we join delta_one and delta_two on a in update statements.  This particular
-- error is covered by the test case "doubleupdate_command.sql".


INSERT INTO delta_two SELECT i as a, 0 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE delta_one;
ANALYZE delta_two;

set enable_nestloop=false;
UPDATE delta_one SET a = 0 FROM delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 10 OR delta_two.a = 40000 OR delta_two.a = 20000);

-- Ensure that tuples to be updated are all from the same GPDB segment.
SELECT distinct(a) FROM delta_one WHERE gp_segment_id = 1 AND
delta_one.a IN (12, 80001, 1001) ORDER BY a;

UPDATE delta_one SET a = 1 FROM delta_two WHERE
delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 12 OR delta_two.a = 80001 OR delta_two.a = 1001);


--
-- Out of order update, delete on delta compressed columns
--
Create table delta_update_t1(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1);

Insert into delta_update_t1 values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (2, 2147483630, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (2, 2147483650, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (12, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800012, 2145499999, '2024-07-31', '14:22:25.778899', '2014-07-30 10:26:31', '2014-07-30 14:26:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Create table delta_update_t2(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1);

Insert into delta_update_t2 values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-28', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776893-07'),
    (1, 2147483649, '2014-07-28', '14:22:23.776891', '2014-07-30 14:22:58.356228', '2014-07-30 14:22:23.776893-07'),
    (1, 2147483649, '2014-07-29', '14:22:23.776891', '2014-07-30 14:22:58.356228', '2014-07-30 14:22:23.776893-07'),
    (2, 2147483630, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (2, 2147483650, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (1, 2147483651, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777898-07'),
    (12, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366248', '2014-07-30 14:22:23.777898-07'),
    (12, 2147483661, '2014-07-31', '14:22:23.776891', '2014-07-30 14:22:58.366248', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483661, '2014-07-31', '14:22:23.776891', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07');


Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;

-- Temporarily disable ORCA. It throws an error for this:
--  ERROR:  multiple updates to a row by the same query is not allowed.
-- We're not trying to test the optimizer here, so that's OK.
set optimizer=off;
Update delta_update_t1 set a2 = delta_update_t2.a2 from delta_update_t2 where delta_update_t1.a1 = delta_update_t2.a1;
reset optimizer;
 
Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;

delete from delta_update_t1 using delta_update_t2 where delta_update_t1.a1 = delta_update_t2.a1;

Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;

--
-- Table with delta on one column zlib on other columns
--
Create table delta_zlib(
    a1 integer ENCODING (compresstype=rle_type,compresslevel=1),
    a2 bigint ENCODING (compresstype=zlib,compresslevel=1),
    a3 date ENCODING (compresstype=zlib,compresslevel=2),
    a4 time ENCODING (compresstype=zlib,compresslevel=3),
    a5 timestamp ENCODING (compresstype=zlib,compresslevel=4),
    a6 timestamp with time zone ENCODING (compresstype=zlib,compresslevel=5),
    a7 integer ENCODING (compresstype=zlib,compresslevel=6),
    a8 bigint ENCODING (compresstype=zlib,compresslevel=7),
    a9 text ENCODING (compresstype=rle_type,compresslevel=2)
    ) with(appendonly=true, orientation=column);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'delta_zlib'  and c.oid=e.attrelid  order by relname, attnum;

\d+ delta_zlib

Insert into delta_zlib select 
    i/20, 12/i, date '2012-02-02' + i/5,
    case when i/5=1 then time '20:13:14.343536' when i/5=2 then time '12:13:11.232421' when i/5=4 then time '10:12:13.241122' else '00:02:03' end,
    case when i/5=3 then timestamp '2014-07-30 14:22:58.356229' when i/5=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2015-02-05 12:00:33.778899' end,
    case when i/5=2 then timestamp '2014-07-30 14:22:23.776892-07' when i/5=3 then timestamp '2014-07-22 11:12:13.006892-07' else timestamp '2012-06-30 04:00:00.333892-07' end,
    i*2/15, 200/i, 
    case when i/20=0 then 'some value for text column' else 'lets try inserting a different value' end  
    from generate_series(1,100) i ;

select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('delta_zlib');

select * from delta_zlib order by a1,a2,a3,a4,a5,a6,a7,a8,a9 limit 5;

select * from delta_zlib order by a1 desc ,a2,a3,a4,a5,a6,a7,a8,a9 limit 5;

Select a2,a3 from delta_zlib where a1 <3  order by a2,a3 limit 10 ;

Select a1,a2 from delta_zlib where a7 <2 order by a1,a2 limit 5 ; 


--
-- Insert into columns with rle_type level 1 + delta + null
--
Create table rle_type_1_delta_null(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_1_delta_null'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_1_delta_null

Insert into rle_type_1_delta_null values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_1_delta_null');

Select * from rle_type_1_delta_null order by a1;

--
-- Insert into columns with rle_type compresslevel 2 + delta.
--
Create table rle_type_2_delta_chkt(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=2, checksum=true);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_2_delta_chkt'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_2_delta_chkt


Insert into rle_type_2_delta_chkt values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_2_delta_chkt');

Select * from rle_type_2_delta_chkt order by a1;

Create table rle_type_2_delta_chkf(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=2, checksum=false);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_2_delta_chkf'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_2_delta_chkf


Insert into rle_type_2_delta_chkf values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_2_delta_chkf');

Select * from rle_type_2_delta_chkf order by a1;

--
-- Insert into columns with rle_type level 1 + delta + null
--
Create table rle_type_2_delta_null(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=2);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_2_delta_null'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_2_delta_null

Insert into rle_type_2_delta_null values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_2_delta_null');

Select * from rle_type_2_delta_null order by a1;

Select * from rle_type_2_delta_null where a5 is not null order by a1;

--
-- Insert into columns with rle_type level 1 + delta + null
--
Create table rle_type_3_delta_null(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=3);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_3_delta_null'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_3_delta_null

Insert into rle_type_3_delta_null values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_3_delta_null');

Select * from rle_type_3_delta_null order by a1;

--
-- Insert into columns with rle_type level 1 + delta + null
--
Create table rle_type_4_delta_null(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=4);

select attrelid::regclass as relname, attnum, filenum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'rle_type_4_delta_null'  and c.oid=e.attrelid  order by relname, attnum;

\d+ rle_type_4_delta_null

Insert into rle_type_4_delta_null values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (null, null, null, null, null, null),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (1000, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07');

Select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('rle_type_4_delta_null');

Select * from rle_type_4_delta_null order by a1;

