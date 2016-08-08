-- 
-- @description Guc setting at role level for compresstype


-- Guc value compresstype=zlib
Alter role dsp_role1 set gp_default_storage_options="appendonly=true, compresstype=zlib";

-- Set the role level guc to quicklz
Alter role dsp_role2 set gp_default_storage_options="appendonly=true,compresstype=quicklz";

-- Set the role level guc to quicklz with orientation=column
Alter role dsp_role3 set gp_default_storage_options="appendonly=true,orientation=column,compresstype=quicklz";

-- GUC with invalid value, invalid combinations
Alter role dsp_role4 set gp_default_storage_options="appendonly=true,compresstype=xxx";
Alter role dsp_role4 set gp_default_storage_options="appendonly=true, compresstype=rle_type";
Alter role dsp_role4 set gp_default_storage_options="appendonly=true, compresstype=quicklz,compresslevel=4";

-- Set the role level guc to rle_type
Alter role dsp_role4 set gp_default_storage_options="appendonly=true, compresstype=rle_type, orientation=column";

-- GUC with appendonly=true not specified
Alter role dsp_role5 set gp_default_storage_options="compresstype=zlib";


\c dsp_db1 dsp_role1

show gp_default_storage_options;

-- Create able with no options
Drop table if exists ao_rl_ct_t1;
Create table ao_rl_ct_t1 ( i int, j int);

Create index ap_t1_ix on ao_rl_ct_t1(i);

Insert into ao_rl_ct_t1 select i, i+1 from generate_series(1,10) i;

update ao_rl_ct_t1 set j=i where i>5;

Delete from ao_rl_ct_t1 where i<2;

Select count(*) from ao_rl_ct_t1;

\d+ ao_rl_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t1');

SET gp_default_storage_options='appendonly=true, compresstype=quicklz, blocksize=8192';
show gp_default_storage_options;

Alter table ao_rl_ct_t1 add column k int default 1;
\d+ ao_rl_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ct_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t1');

RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_rl_ct_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_rl_ct_t2;
Create table ao_rl_ct_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_rl_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t2;

\d+ ao_rl_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t2');

Drop table ao_rl_ct_t2;



-- Create table with compresstype=rle_type(not valid)

Drop table if exists ao_rl_ct_t3;
Create table ao_rl_ct_t3 ( i int, j int) with(compresstype=rle_type);


-- Create table with compresstype=none

Drop table if exists ao_rl_ct_t4;
Create table ao_rl_ct_t4 ( i int, j int) with(compresstype=none);

Insert into ao_rl_ct_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t4;

\d+ ao_rl_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t4';

Drop table ao_rl_ct_t4;

\c dsp_db2 dsp_role2

show gp_default_storage_options;

-- Alter the table and add a column with new compresstype
Drop table if exists ao_rl_ct_t4;
Create table ao_rl_ct_t4 ( i int, j int) with(orientation=column);

Insert into ao_rl_ct_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t4;

Alter table ao_rl_ct_t4 add column k int default 10 ENCODING (compresstype=quicklz);

\d+ ao_rl_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t4';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t4');

Drop table ao_rl_ct_t4;


-- Create table with compresstype=rle_type  and orientation=column

Drop table if exists ao_rl_ct_t3;
Create table ao_rl_ct_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);


Insert into ao_rl_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t3;

\d+ ao_rl_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t3');

Drop table ao_rl_ct_t3;

-- Create table with a new compresstype at column level for one column 

Drop table if exists ao_rl_ct_t5;
Create table ao_rl_ct_t5 ( i int, j int encoding(compresstype=quicklz)) with(orientation=column);

Insert into ao_rl_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t5;

\d+ ao_rl_ct_t5;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_ct_t5'::regclass;

Drop table ao_rl_ct_t5;

-- Create table with compresstype in default column encoding

Drop table if exists ao_rl_ct_t6;
Create table ao_rl_ct_t6 ( i int, j int, DEFAULT COLUMN ENCODING (compresstype=quicklz)) with(orientation=column);

Insert into ao_rl_ct_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t6;

\d+ ao_rl_ct_t6;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_ct_t6'::regclass;

Drop table ao_rl_ct_t6;
-- Create table with invalid value for compresstype

Drop table if exists ao_rl_ct_t4;
Create table ao_rl_ct_t4 ( i int, j int) with(compresstype=xxxx);



-- Create table with orientation,checksum,compresslevel

Drop table if exists ao_rl_ct_t5;
Create table ao_rl_ct_t5 ( i int, j int) with(orientation=column, compresslevel=1, checksum=true);

Insert into ao_rl_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t5;

\d+ ao_rl_ct_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t5');

Drop table ao_rl_ct_t5;

\c dsp_db3 dsp_role3

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_rl_ct_t1;
Create table ao_rl_ct_t1 ( i int, j int);

Insert into ao_rl_ct_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_rl_ct_t1;

\d+ ao_rl_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t1';

SET gp_default_storage_options='appendonly=true, compresstype=quicklz';
show gp_default_storage_options;

Alter table ao_rl_ct_t1 add column k int default 1;
Alter table ao_rl_ct_t1 add column l int default 1 encoding(blocksize=8192);;
\d+ ao_rl_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ct_t1';
select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_ct_t1'::regclass order by attnum;

RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_rl_ct_t1;

\c dsp_db4 dsp_role4

show gp_default_storage_options;

-- Create table with no options 

Drop table if exists ao_rl_ct_t2;
Create table ao_rl_ct_t2 ( i int, j int);

Insert into ao_rl_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t2;

\d+ ao_rl_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t2';

Drop table ao_rl_ct_t2;


-- Create table with appendonly=false

Drop table if exists ao_rl_ct_t3;
Create table ao_rl_ct_t3 ( i int, j int) with(appendonly=false);

Insert into ao_rl_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t3;

\d+ ao_rl_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ct_t3';

Drop table ao_rl_ct_t3;


-- Create table with appendonly=true

Drop table if exists ao_rl_ct_t4;
Create table ao_rl_ct_t4 ( i int, j int) with(appendonly=true);

-- Guc with no appendonly=true specified
\dsp_db4 dsp_role5
show gp_default_storage_options;


-- With no option it will a heap table
Drop table if exists ao_rl_ct_t1;
Create table ao_rl_ct_t1( i int, j int);

Insert into ao_rl_ct_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t1;

\d+ ao_rl_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ct_t1';

SET gp_default_storage_options='appendonly=true, compresstype=quicklz';
show gp_default_storage_options;

Alter table ao_rl_ct_t1 add column k int default 1;
\d+ ao_rl_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ct_t1';

RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_rl_ct_t1;
-- with appendonly=true specified the table will be created as AO table with compresstype

Drop table if exists ao_rl_ct_t2;
Create table ao_rl_ct_t2( i int, j int) with(appendonly=true);
Insert into ao_rl_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ct_t2;

\d+ ao_rl_ct_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ct_t2');

Drop table ao_rl_ct_t2;

