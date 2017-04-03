-- 
-- @description Add/split/exchange partition to a partition table after the gp_default_operations changed with GUC value

-- Parent table with (appendonly=true, orientation=row), set guc to orientation column, Add a partition
\c dsp_db1

show gp_default_storage_options;

Drop table if exists ap_pr_t01;
Create table ap_pr_t01( i int, j int, k int) with(appendonly=true) partition by range(i) (start(1) end(10) every(5));

Insert into ap_pr_t01 select i, i+1, i+2 from generate_series(1,9) i;
Select count(*) from ap_pr_t01;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t01%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t01%') order by columnstore;

SET gp_default_storage_options="appendonly=true, orientation=column";

show gp_default_storage_options;

Alter table ap_pr_t01 add partition p3 start(11) end(15);

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t01%' order by relname;

RESET gp_default_storage_options;



-- Parent table with appendonly=false, guc with compresstype quicklz, Add an appendonly partition

Drop table if exists ap_pr_t02;
Create table ap_pr_t02( i int, j int, k int) with(appendonly=false) partition by range(i) (start(1) end(10) every(5));

Insert into ap_pr_t02 select i, i+1, i+2 from generate_series(1,9) i;
Select count(*) from ap_pr_t02;

SET gp_default_storage_options="appendonly=true, compresstype=quicklz";

show gp_default_storage_options;

Alter table ap_pr_t02 add partition p3 start(11) end(15);

select relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t02%';

RESET gp_default_storage_options;



-- Parent table with orientation=column, guc with orientaion=row, add partiiton

Drop table if exists ap_pr_t03;
Create table ap_pr_t03( i int, j int, k int) with(appendonly=true, orientation=column) partition by range(i) (start(1) end(10) every(5));

Insert into ap_pr_t03 select i, i+1, i+2 from generate_series(1,9) i;
Select count(*) from ap_pr_t03;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t03%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t03%') order by columnstore;

SET gp_default_storage_options="appendonly=true";

show gp_default_storage_options;

Alter table ap_pr_t03 add partition p3 start(11) end(15);

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t03%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t03%') order by columnstore;

RESET gp_default_storage_options;



-- Parent with appendonly=true, guc set to default, add partition with no option, add partiton with orientation=column

Drop table if exists ap_pr_t04;
Create table ap_pr_t04( i int, j int, k int) with(appendonly=true) partition by range(i) (start(1) end(10) every(5));

Insert into ap_pr_t04 select i, i+1, i+2 from generate_series(1,9) i;
Select count(*) from ap_pr_t04;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t04%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t04%') order by columnstore;

SET gp_default_storage_options to default;

show gp_default_storage_options;

Alter table ap_pr_t04 add partition p3 start(11) end(15);
Alter table ap_pr_t04 add partition p4 start(16) end(18) with(appendonly=true,orientation=column);

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t04%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t04%') order by columnstore;

RESET gp_default_storage_options;



-- Parent with appendonly=false, guc to appendonly=true, split partition , check the status of new parts

show gp_default_storage_options; 

Drop table if exists ap_pr_t05;
Create table ap_pr_t05( i int, j int, k int) partition by range(i) (start(1) end(10) every(5));

Insert into ap_pr_t05 select i, i+1, i+2 from generate_series(1,9) i;
Select count(*) from ap_pr_t05;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t05%' order by relname;

SET gp_default_storage_options="appendonly=true";

Alter table ap_pr_t05 split partition for (rank(2)) at (7) into (partition split_p1, partition split_p2);

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t05%' order by relname;

RESET gp_default_storage_options;



-- Parent with appendonly=true, Partition p1 with orientation column, GUC set to orientation column, Exchange with a AO table, heap table

show gp_default_storage_options; 
Drop table if exists ap_pr_t06;
Create table ap_pr_t06( i int, j int, k int) with(appendonly=true) partition by range(i) (partition p1 start(1) end(5) with(appendonly=true,orientation=column), partition p2 start(5) end(10), partition p3 start(10) end(15));

Insert into ap_pr_t06 select i, i+1, i+2 from generate_series(1,13) i;
Select count(*) from ap_pr_t06;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t06%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t06%') order by columnstore;

Drop table if exists exch_t06_1;
Create table exch_t06_1 ( i int, j int, k int) with(appendonly=true);
Insert into exch_t06_1 select i, i+1, i+2 from generate_series(1,4) i;

Drop table if exists exch_t06_2;
Create table exch_t06_2 ( i int, j int, k int) with(appendonly=false);
Insert into exch_t06_2 select i, i+1, i+2 from generate_series(5,9) i;

SET gp_default_storage_options="appendonly=true, orientation=column";

show gp_default_storage_options;

Alter table ap_pr_t06 exchange partition p1 with table  exch_t06_1;
Alter table ap_pr_t06 exchange partition p2 with table  exch_t06_2;


select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t06%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t06%') order by columnstore;


-- Partition with sub-partitions and encoding clause at sub-part level
SET gp_default_storage_options='compresstype=rle_type, orientation=column';

show gp_default_storage_options;

create table ap_pr_t07 (i int, j int)
with (appendonly = true, orientation=column)
partition by range(j)
    subpartition by list (i)
    subpartition template(
        subpartition sp1 values(1, 2, 3, 4, 5),
        column i encoding(compresstype=zlib),
        column j encoding(compresstype=quicklz)
    )
(partition p1 start(1) end(10)
);
Insert into ap_pr_t07 values (1,1),(2,2),(3,3),(4,4),(5,5),(1,5),(2,3),(3,4);
Select count(*) from ap_pr_t07;

select relname, relkind, relstorage, reloptions from pg_class where relname like 'ap_pr_t07%' order by relname;
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid in  (select oid from pg_class where relname  like 'ap_pr_t07%') order by columnstore;

select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c
where c.relname = 'ap_pr_t07' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum;

SET gp_default_storage_options='appendonly=false';
show gp_default_storage_options;

