-- @gucs gp_create_table_random_default_distribution=off
create schema schema_ao;

create table schema_ao.ao_table1(a int) with (appendonly=true);

insert into schema_ao.ao_table1 values (0);
insert into schema_ao.ao_table1 values (1);
insert into schema_ao.ao_table1 values (2);

CREATE TABLE ao_compr01(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, compresstype='zlib', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_text);

insert into ao_compr01 values ('0_zero',0);
insert into ao_compr01 values ('1_one',1);
insert into ao_compr01 values ('2_two',2);

create table ao_compr02( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, compresstype=quicklz));

insert into ao_compr02 values(1, generate_series(1,14), 4, 5); 

create table ao_compr03
(
distcol int, ptcol int, subptcol int ) 
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) 
subpartition template (
default subpartition subothers, 
subpartition sub1 values(1,2,3) with(appendonly=true), 
subpartition sub2 values(4,5,6) with (appendonly=true, orientation = column), 
subpartition sub3 values(7,8,9) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into ao_compr03 values(generate_series(1,5), generate_series(1,15), generate_series(1,10));

