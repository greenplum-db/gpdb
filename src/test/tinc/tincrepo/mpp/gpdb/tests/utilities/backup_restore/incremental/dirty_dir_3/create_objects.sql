-- @gucs gp_create_table_random_default_distribution=off
-- Create heap table with constraints
Create  table ib_heap_01 ( a int unique, b int, c int, d text not null , constraint c_chk check(c <20)) distributed by(a) ;

-- Create heap table in a user created schema
Create schema ib_heap_schema;
Create  table ib_heap_schema.ib_heap_01 ( a int unique, b int, c int, d text not null , constraint c_chk check(c <20)) distributed by(a) ;

-- Create heap_part table
create table ib_heap_part01 (a int , b int, c int , d text) distributed by(a) 
partition by range(b) subpartition by list(c) subpartition template 
(subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6)) 
(start(1) end(4) every(2));

-- Create AO table with constraints
Create table ib_ao_01 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true) distributed by(a) ;

-- Create AO table with compression attributes
Create table ib_ao_02 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) 
with(appendonly=true, compresstype=zlib, compresslevel=5, blocksize=8192) distributed by(a) ;

-- Create AO part table with mixed partition attributes
Create table ib_ao_03 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true) 
distributed by(a) partition by range(b) subpartition by list(c ) subpartition template(
subpartition sub1 values(1,2) with(appendonly=false),
subpartition sub2 values(3,4) with(appendonly=true),
subpartition sub3 values(5,6) with(appendonly=true, orientation=column),
subpartition sub4 values(7) with(appendonly=true, compresstype=quicklz, compresslevel=1, blocksize=8192),
subpartition sub5 values(8)) (default partition others, partition p1 start(1) end(5));

-- Create AO table in a user created schema
Create schema ib_ao_schema;
Create table ib_ao_schema.ib_ao_01 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true) distributed by(a) ;

-- Create AO table in a user created tablespace


-- Create CO table with constraints
Create table ib_co_01 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true, orientation=column) distributed by(a) ;

-- Create CO table with compression attributes in WITH clause
Create table ib_co_02 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) 
with(appendonly=true, orientation=column, compresstype=zlib, compresslevel=5, blocksize=8192) distributed by(a) ;

-- Create CO partition table wtih mixed partition atributes

Create table ib_co_03 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true, orientation=column) distributed by(a) partition by range(b) subpartition by list(c ) subpartition template(
subpartition sub1 values(1,2) with(appendonly=false),
subpartition sub2 values(3,4) with(appendonly=true),
subpartition sub3 values(5,6) with(appendonly=true, orientation=column),
subpartition sub4 values(7) with(appendonly=true, compresstype=quicklz, compresslevel=1, blocksize=8192),
subpartition sub5 values(8)) (default partition others, partition p1 start(1) end(5));

-- Create CO table with compression attributes using encoding clause
Create table ib_co_04 (a int ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192) , b int, c int, d text,
COLUMN b ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
DEFAULT COLUMN ENCODING (compresstype=quicklz,blocksize=8192)) with(appendonly=true, orientation=column);

-- Create CO partition table with compression attributes
Create table ib_co_05 ( a int , b int, c int, d text) with(appendonly=true, orientation=column)
partition by range(b) subpartition by list(c ) subpartition template(
subpartition sub1 values(1,2),
subpartition sub2 values(3,4), 
COLUMN a  ENCODING (compresstype=zlib,compresslevel=3,blocksize=8192),
COLUMN b  ENCODING (compresstype=rle_type)) (default partition others, partition p1 start(1) end(5));

-- Create CO table in user created schema
Create schema ib_co_schema;
Create table ib_co_schema.ib_co_01 ( a int , b int, c int, d text not null , constraint c_chk check(c <20)) with(appendonly=true, orientation=column) distributed by(a) ;

-- Create CO table in user created tablespace


-- Create btree index on AO table
Create index ao_bitix on ao_table20(numeric_col);

-- Create btree index on CO table
Create index co_bitix on co_table20(numeric_col);

-- Create bitmap index on AO table
Create index ao_btrix on ao_table19 using bitmap(bigint_col);

-- Create bitmap index on AO table
Create index co_btrix on co_table19 using bitmap(bigint_col);

-- Recreating a dropped table with same name and configuration
CREATE TABLE aoschema1.ao_table22(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

Create index ao_table16_idx_new on ao_table16(bigint_col);
