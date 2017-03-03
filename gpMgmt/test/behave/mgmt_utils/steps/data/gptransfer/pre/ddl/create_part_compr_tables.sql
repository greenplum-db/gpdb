\c gptest;

-- @description AO and CO partiton compression tables
-- @author Divya Sivanandan

-- AO part_compr tables
--start_ignore
Drop table if exists sto_aocomp_p1 ;
--end_ignore
create table sto_aocomp_p1( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (
partition p1 start(1) end(5) with (appendonly = true),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, compresstype=quicklz));

insert into sto_aocomp_p1 values(1, generate_series(1,14), 4, 5); 

-- CO part_compr tables
--start_ignore
Drop table if exists sto_cocomp_p1;
--end_ignore
create table sto_cocomp_p1
(
distcol int, ptcol int, subptcol int ) 
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) 
subpartition template (
default subpartition subothers, 
subpartition sub1 values(1,2,3) with(appendonly=true, orientation = column , compresstype=zlib), 
subpartition sub2 values(4,5,6) with (appendonly=true, orientation = column, compresstype=quicklz), 
subpartition sub3 values(7,8,9) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into sto_cocomp_p1 values(generate_series(1,5), generate_series(1,15), generate_series(1,10));

--start_ignore
Drop table if exists sto_cocomp_p2;
--end_ignore
Create table sto_cocomp_p2 (col_int1 int, col_int2 int, col_text text) 
             WITH (appendonly=true, orientation=column) distributed randomly
             Partition by range(col_int1) (start(1)  end(8) every(2) ,
             COLUMN  col_int1 ENCODING (compresstype=rle_type),
             COLUMN col_int2 ENCODING (compresstype=quicklz),
             DEFAULT COLUMN ENCODING (compresstype=zlib));

Insert into sto_cocomp_p2 values(generate_series(1,7), 100, 'value to co comp part table');
