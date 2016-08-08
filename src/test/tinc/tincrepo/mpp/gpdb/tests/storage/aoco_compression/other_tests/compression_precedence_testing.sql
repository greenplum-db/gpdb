-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Compression precedence testing
--Drop the tables

DROP TABLE if exists CO_01_compr_precedence;
DROP TABLE if exists CO_02_compr_precedence;
DROP TABLE if exists CO_03_compr_precedence;
DROP TABLE if exists CO_04_compr_precedence;
DROP TABLE if exists CO_05_compr_precedence;
DROP TABLE if exists CO_06_compr_precedence;
DROP TABLE if exists CO_062_compr_precedence;
DROP TABLE if exists CO_063_compr_precedence;
DROP TABLE if exists CO_07_compr_precedence;
DROP TABLE if exists CO_072_compr_precedence;
DROP TABLE if exists CO_08_compr_precedence;
DROP TABLE if exists CO_082_compr_precedence;
DROP TABLE if exists CO_09_compr_precedence;
DROP TABLE if exists CO_10_compr_precedence;
DROP TABLE if exists CO_11_compr_precedence;


 
-- storage_directive =quicklz, column_reference=zlib => expected = quicklz

CREATE TABLE CO_01_compr_precedence (
         a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=8192),
         a2 char(5) ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a3 text ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a4 timestamp ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         a5 date ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192), COLUMN a1 ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=32768) ) WITH (appendonly=true, orientation=column);


-- check the compress_type of column a1
\d+ CO_01_compr_precedence



-- storage_directive=not defined on any column, column_reference=quicklz on a1 => expected = quicklz for a1, none for all others

CREATE TABLE CO_02_compr_precedence 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a1 ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=32768) ) WITH (appendonly=true, orientation=column);

-- check the compress_type of column a1 
\d+ CO_02_compr_precedence



-- storage_directive=compresstype zlib for a5; default column_referece=quicklz => expected zlib for a5

CREATE TABLE CO_03_compr_precedence (
         a1 int,
         a2 char(5),a3 text ,a4 timestamp ,
         a5 date ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192), 
         DEFAULT COLUMN ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768) ) WITH (appendonly=true, orientation=column);


-- check the compress_type of column a5 
\d+ CO_03_compr_precedence



-- storage_directive=none; WITH clause=quicklz => expected=quicklz

CREATE TABLE CO_04_compr_precedence 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date ) WITH (appendonly=true, orientation=column,compresstype=QUICKLZ,compresslevel=1,blocksize=32768);

-- check the compress_type of column a1
\d+ CO_04_compr_precedence




-- storage_directive = Not defined,Column a1 uses altered type int with compresstype=zlib => expectred=zlib

ALTER TYPE int SET DEFAULT ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=1048576);

CREATE TABLE CO_05_compr_precedence 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date ) WITH (appendonly=true, orientation=column);
        
-- check the compress_type of column a1
\d+ CO_05_compr_precedence        



-- storage_directive = defined for a1 as quicklz,Column a1 uses altered type int with compresstype=zlib => expected=quicklz

ALTER TYPE int SET DEFAULT ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=1048576);

CREATE TABLE CO_06_compr_precedence 
        (a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=1048576),a2 char(5),a3 text,a4 timestamp ,a5 date ) WITH (appendonly=true, orientation=column);
        
-- check the compress_type of column a1
\d+ CO_06_compr_precedence      


-- column reference = defined for a1 as zlib,Column a1 uses altered type int with compresstype=quicklz => expected=zlib

ALTER TYPE int SET DEFAULT ENCODING (compresstype=quicklz,compresslevel=1,blocksize=1048576);

CREATE TABLE CO_062_compr_precedence 
        (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a1 ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=1048576) ) WITH (appendonly=true, orientation=column);
        
-- check the compress_type of column a1
\d+ CO_062_compr_precedence   


-- with clause = defined for a1 as zlib,Column a1 uses altered type int with compresstype=quicklz => expected=zlib

ALTER TYPE int SET DEFAULT ENCODING (compresstype=quicklz,compresslevel=1,blocksize=1048576);

CREATE TABLE CO_063_compr_precedence 
        (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date ) WITH (appendonly=true, orientation=column,compresstype=ZLIB,compresslevel=1,blocksize=1048576);
        
-- check the compress_type of column a1
\d+ CO_063_compr_precedence   


--Alter the type back to none to not affect any other tests

ALTER TYPE int SET DEFAULT ENCODING (compresstype=none,compresslevel=0,blocksize=32768);


-- storage_directive=rle; partition level column reference=quicklz => expected =quicklz
CREATE TABLE CO_07_compr_precedence 
        (a1 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192),a2 char(5),a3 text,a4 timestamp ,a5 date )  WITH (appendonly=true, orientation=column)
        Partition by range(a1) (start(1) end(2000) every(500) , COLUMN  a1 ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=1048576));
        
-- check the compress_type of column a1 at table level
\d+ CO_07_compr_precedence 

--check the compress_type of column a1 at partition level
\d+ co_07_compr_precedence_1_prt_1


-- column reference =zlib; partition level column reference=quicklz => expected =quicklz
CREATE TABLE CO_072_compr_precedence 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, COLUMN a1  ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192) )  WITH (appendonly=true, orientation=column)
        Partition by range(a1) (start(1) end(2000) every(500) , COLUMN  a1 ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=1048576));
        
-- check the compress_type of column a1
\d+ CO_072_compr_precedence 

--check the compress_type of column a1 at partition level
\d+ co_072_compr_precedence_1_prt_1

-- storage_directive=quicklz;sub partition level column reference=zlib => expected =zlib for the columns at subpartition level

CREATE TABLE CO_08_compr_precedence 
        (a1 int ,a2 char(5),a3 text ENCODING (compresstype=QUICKLZ,compresslevel=1,blocksize=8192),a4 timestamp ,a5 date )  
        WITH (appendonly=true, orientation=column) 
        Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F'),
          COLUMN a2  ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
          COLUMN a3  ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)  )      
        (start(1) end(2000) every(500));
        
-- check the compress_type of column a2
\d+ CO_08_compr_precedence 

\d+ co_08_compr_precedence_1_prt_1_2_prt_part2

-- column_reference=quicklz; sub partition level column reference=zlib => expected =quicklz at subpartition level

CREATE TABLE CO_082_compr_precedence 
        (a1 int ,a2 char(5),a3 text ,a4 timestamp ,a5 date, COLUMN a2 ENCODING (compresstype=ZLIB,compresslevel=1,blocksize=32768) )  
        WITH (appendonly=true, orientation=column) 
        Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F'),
          COLUMN a2  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=1048576),
          COLUMN a3  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)  )      
        (start(1) end(2000) every(500));
        
-- check the compress_type of column a2
\d+ CO_082_compr_precedence 

\d+ co_082_compr_precedence_1_prt_1_2_prt_part2

-- table level with =zlib; partition level with =quicklz => expected =quicklz
CREATE TABLE CO_09_compr_precedence 
        (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date )  WITH (appendonly=true, orientation=column,compresstype=zlib) distributed randomly 
        Partition by range(a1) (start(1) end(2000) every(500)   WITH (appendonly=true, orientation=column,compresstype=QUICKLZ,compresslevel=1,blocksize=1048576));
        
-- check the compress_type of column a1
\d+ CO_09_compr_precedence 

\d+ co_09_compr_precedence_1_prt_1


-- table level with =zlib; subpartition level with =quicklz => expected =quicklz

CREATE TABLE CO_10_compr_precedence 
        (a1 int ,a2 char(5),a3 text ,a4 timestamp ,a5 date)  
        WITH (appendonly=true, orientation=column,compresstype=zlib) distributed randomly
        Partition by range(a1) Subpartition by list(a2) subpartition template 
        ( subpartition part1 values('M') ,
          subpartition part2 values('F') 
          WITH (appendonly=true, orientation=column,compresstype=QUICKLZ,compresslevel=1,blocksize=32768))      
        (start(1) end(2000) every(500));

        
\d+ CO_10_compr_precedence 

\d+ co_10_compr_precedence_1_prt_1_2_prt_part2


