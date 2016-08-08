-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Test to check the new syntax for compression encoding wont work on heap tables.


-- Storage_directive
Drop table if exists CO_01_heap_table_storage_directive_ZLIB_8192_8;
CREATE TABLE CO_01_heap_table_storage_directive_ZLIB_8192_8 (
         a1 int ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a2 char(5) ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a3 text ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a4 timestamp ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a5 date ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192)
);


-- Column_referenc column    
Drop table if exists CO_02_heap_table_col_ref_QUICKLZ_32768_1;        
CREATE TABLE CO_02_heap_table_col_ref_QUICKLZ_32768_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a1 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
);         


-- Column_reference default    
Drop table if exists CO_03_heap_table_col_ref_QUICKLZ_8192_1;        
CREATE TABLE CO_03_heap_table_col_ref_QUICKLZ_8192_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, DEFAULT COLUMN ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=8192)
); 


-- Storage_directive and Column_reference
Drop table if exists CO_04_heap_table_col_ref_ZLIB_8192_1;
CREATE TABLE CO_04_heap_table_col_ref_ZLIB_8192_1 
        (a1 int  ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=8192),a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a3 ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768)
);   

-- Column_reference at partition level
Drop table if exists CO_05_heap_table_col_ref_ZLIB_8192_1;
CREATE TABLE CO_05_heap_table_col_ref_ZLIB_8192_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date) Partition by range(a1) (start(1) end(1000) every(500),  COLUMN a1 ENCODING (compresstype=zlib,compresslevel=4,blocksize=32768));

-- Column reference at sub partition level
Drop table if exists CO_06_heap_table_col_ref_zlib_8192_1;
CREATE TABLE CO_06_heap_table_col_ref_zlib_8192_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  
       Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F') , COLUMN a2 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) 
        (start(1) end(1000) every(500));


-- Alter a heap table to add a column with encoding
Drop table if exists heap_table_alter_col;        
CREATE TABLE heap_table_alter_col 
        (a1 int,a2 char(5) ,a3 date );
Alter table heap_table_alter_col add column a4 text default 'new' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768);

--Alter type to have encoding, Add a column with this type to an ao table

Alter type text set default encoding (compresstype=quicklz,compresslevel=1,blocksize=32768);
Alter table heap_table_alter_col add column a5 text default 'new' ;

\d+ heap_table_alter_col

--Alter type back to normal
Alter type text set default encoding (compresstype=none,compresslevel=0,blocksize=32768);

-- Test to check the new syntax for compression encoding wont work on AO tables.


-- Storage_directive
Drop table if exists CO_01_ao_table_storage_directive_ZLIB_8192_8;
CREATE TABLE CO_01_ao_table_storage_directive_ZLIB_8192_8 (
         a1 int ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a2 char(5) ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a3 text ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a4 timestamp ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a5 date ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192)
);


-- Column_reference column    
Drop table if exists CO_02_ao_table_col_ref_QUICKLZ_32768_1;         
CREATE TABLE CO_02_ao_table_col_ref_QUICKLZ_32768_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a1 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
) with (appendonly=true);         


-- Column_reference default    
Drop table if exists CO_03_ao_table_col_ref_QUICKLZ_32768_1;        
CREATE TABLE CO_03_ao_table_col_ref_QUICKLZ_32768_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, DEFAULT COLUMN ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=32768)
)  with (appendonly=true); 


-- Storage_directive and Column_reference
Drop table if exists CO_04_ao_table_str_dir_col_ref_ZLIB_8192_1;
CREATE TABLE CO_04_ao_table_str_dir_col_ref_ZLIB_8192_1 
        (a1 int  ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=8192),a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a3 ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768)
)  with (appendonly=true);   

-- Column_reference at partition level
Drop table if exists CO_05_ao_table_col_ref_ZLIB_8192_1;
CREATE TABLE CO_05_ao_table_col_ref_ZLIB_8192_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true) Partition by range(a1) (start(1) end(1000) every(500),  COLUMN a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768));

-- Column reference at sub partition level
Drop table if exists CO_06_ao_table_col_ref_zlib_8192_1;
CREATE TABLE CO_06_ao_table_col_ref_zlib_8192_1 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date) with (appendonly=true) 
       Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F') , COLUMN a2 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) 
        (start(1) end(1000) every(500));


-- Alter an ao table to add a column with encoding
Drop table if exists aop_table_alter_col;        
CREATE TABLE heap_table_alter_col 
        (a1 int,a2 char(5),a3 text ) with (appendonly=true) ;
Alter table ao_table_alter_col add column a6 int default 7 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768);


--Alter type to have encoding, Add a column with this type to an ao table

Alter type text set default encoding (compresstype=quicklz,compresslevel=1,blocksize=32768);
Alter table ao_table_alter_col add column a7 text default 'new' ;

\d+ ao_table_alter_col

--Alter type back to normal
Alter type text set default encoding (compresstype=none,compresslevel=0,blocksize=32768);


-- Inheritance: The INHERITS clause is not allowed in a table that contains a storage_directive or a column_reference_storage_directive

--Encoding at storage_directive level

Drop table if exists child_tb1;
Drop table if exists parent_tb1 cascade;

Create table parent_tb1 (a1 int ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_tb1 (c1 int, c2 char encoding(compresstype=quicklz)) inherits(parent_tb1) with (appendonly = true, orientation = column);


--Encoding at column_reference level

Drop table if exists child_tb2;
Drop table if exists parent_tb2 cascade;

Create table parent_tb2 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date, column a1 ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_tb2 (c1 int, c2 char, column c2 encoding(compresstype=quicklz)) inherits(parent_tb2) with (appendonly = true, orientation = column);


--parent table with no compression attributes

Drop table if exists child_tb3;
Drop table if exists parent_tb3 cascade;

Create table parent_tb3 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_tb3 (c1 int, c2 char, column c2 encoding(compresstype=quicklz)) inherits(parent_tb2) with (appendonly = true, orientation = column);

--Child table with no compression attributes

Drop table if exists child_tb4;
Drop table if exists parent_tb4 cascade;

Create table parent_tb4 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date, column a1 ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_tb4 (c1 int, c2 char) inherits(parent_tb2) with (appendonly = true, orientation = column);

\d+ child_tb4

--Like clause :The storage_direcitve and column_reference_storage_directive will be ignored by a table created using the LIKE clause


--Storage_directive

Drop table if exists child_like1;
Drop table if exists parent_like1 cascade;

Create table parent_like1 (a1 int ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768),a2 char(5),a3  date)  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_like1 (like parent_like1) with (appendonly = true, orientation = column);

\d+ parent_like1

\d+ child_like1


--column_reference

Drop table if exists child_like2;
Drop table if exists parent_like2 cascade;

Create table parent_like2 (a1 int ,a2 char(5),a3  date, column a1 ENCODING (compresstype=zlib,compresslevel=9,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);

Create table child_like2  (like parent_like2) with (appendonly = true, orientation = column);

\d+ parent_like2

\d+ child_like2


--column reference at partition level

Drop table if exists child_like3;
Drop table if exists parent_like3 cascade;

CREATE TABLE parent_like3 
        (a1 int,a2 char(5),a3 date)  with (appendonly=true,orientation=column) Partition by range(a1) (start(1) end(1000) every(500),  COLUMN a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768));

Create table child_like3  (like parent_like3) with (appendonly = true, orientation = column) Partition by range(a1) (start(1) end(1000) every(500));

\d+ parent_like3_1_prt_1

\d+ child_like3_1_prt_1

--column reference at subpartition level

Drop table if exists child_like4;
Drop table if exists parent_like4 cascade;

CREATE TABLE parent_like4 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date) with (appendonly=true,orientation=column) 
       Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F') , COLUMN a2 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) 
        (start(1) end(1000) every(500));

Create table child_like4  (like parent_like4) with (appendonly = true, orientation = column)
        Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F')) 
        (start(1) end(1000) every(500));

\d+ parent_like4_1_prt_1_2_prt_part1

\d+ child_like4_1_prt_1_2_prt_part1


--With caluse - should inherit  (this is not a negative -added for completion)

Drop table if exists child_like5;
Drop table if exists parent_like5 cascade;

Create table parent_like5 (a1 int ,a2 char(5),a3  date)  with (appendonly=true,orientation=column,compresstype=zlib,compresslevel=9,blocksize=32768) distributed by (a1);

Insert into parent_like5 values(generate_series(1,100),'asd','2011-02-11');
Create table child_like5  (like parent_like5) with (appendonly = true, orientation = column,compresstype=quicklz,compresslevel=1);

\d+ parent_like5

\d+ child_like5

-- CTAS table with compression
Drop table if exists ctas_neg;
create table ctas_neg with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1) as select * from parent_like5;

\d+ ctas_neg
