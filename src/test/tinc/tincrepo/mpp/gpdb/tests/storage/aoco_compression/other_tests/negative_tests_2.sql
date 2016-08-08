-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--Create table with compresstype =none, compresslevel=0

Drop table if exists neg_tb1 ;
Create table neg_tb1 (a1 int ENCODING (compresstype=none,compresslevel=0,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);

\d+ neg_tb1

--Create table with mispelled compresstype

Drop table if exists neg_tb2 ;
Create table neg_tb2 (a1 int ENCODING (compresstype=abc,compresslevel=1,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);


--Create table with not allowed compresslevel
Drop table if exists neg_tb3 ;
Create table neg_tb3 (a1 int ENCODING (compresstype=quicklz,compresslevel=5,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);


Drop table if exists neg_tb4 ;
Create table neg_tb4 (a1 int ENCODING (compresstype=zlib,compresslevel=11,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);

--Create table with not allowed blocksize
Drop table if exists neg_tb5 ;
Create table neg_tb5 (a1 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);



--Create table with case insensitive compresstype

Drop table if exists neg_tb6 ;
Create table neg_tb6 (a1 int ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1);

\d+ neg_tb6


--Alter tables

--invalid compresstype

Alter table neg_tb6 add column a6 int ENCODING (compresstype=qwer,compresslevel=1,blocksize=32768);

--invalid compresslevel
Alter table neg_tb6 add column a6 int ENCODING (compresstype=zlib,compresslevel=12,blocksize=32768);
Alter table neg_tb6 add column a6 int ENCODING (compresstype=quicklz,compresslevel=2,blocksize=32768);

--invalid blocksize
Alter table neg_tb6 add column a6 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111);



--Alter type

Alter type int set default ENCODING (compresstype=qwer,compresslevel=1,blocksize=32768);

--invalid compresslevel
Alter type int set default  ENCODING (compresstype=zlib,compresslevel=12,blocksize=32768);
Alter type int set default  ENCODING (compresstype=quicklz,compresslevel=2,blocksize=32768);

--invalid blocksize
Alter type int set default  ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111);


--Create tables at column reference storage level

Drop table if exists neg_tb11 ;
Create table neg_tb11 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1 ENCODING (compresstype=none,compresslevel=0,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);

\d+ neg_tb11

--Create table with mispelled compresstype

Drop table if exists neg_tb12 ;
Create table neg_tb12 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1 ENCODING (compresstype=abc,compresslevel=1,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);


--Create table with not allowed compresslevel
Drop table if exists neg_tb13 ;
Create table neg_tb13 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1  ENCODING (compresstype=quicklz,compresslevel=5,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);


Drop table if exists neg_tb14 ;
Create table neg_tb14 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1 ENCODING (compresstype=zlib,compresslevel=11,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);


--Create table with not allowed blocksize
Drop table if exists neg_tb15 ;
Create table neg_tb15 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111))  with (appendonly=true,orientation=column) distributed by (a1);



--Create table with case insensitive compresstype

Drop table if exists neg_tb16 ;
Create table neg_tb16 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, Column a1 ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768))  with (appendonly=true,orientation=column) distributed by (a1);

\d+ neg_tb16


--Create tables with column reference at partition level

Drop table if exists neg_tb21 ;
Create table neg_tb21 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                      Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=none,compresslevel=0,blocksize=32768));

\d+ neg_tb21

--Create table with mispelled compresstype

Drop table if exists neg_tb22 ;
Create table neg_tb22 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                     Partition by range(a1) (start(1) end(1000) every(500), Column a1  ENCODING (compresstype=abc,compresslevel=1,blocksize=32768));


--Create table with not allowed compresslevel
Drop table if exists neg_tb23 ;
Create table neg_tb23 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date )  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=quicklz,compresslevel=5,blocksize=32768));


Drop table if exists neg_tb24 ;
Create table neg_tb24 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=zlib,compresslevel=11,blocksize=32768));


--Create table with not allowed blocksize
Drop table if exists neg_tb25 ;
Create table neg_tb25 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date )  with (appendonly=true,orientation=column) distributed by (a1)
                   Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111));



--Create table with case insensitive compresstype

Drop table if exists neg_tb26 ;
Create table neg_tb26 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                  Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768));

\d+ neg_tb26


--Create tables with column reference at subpartition level

Drop table if exists neg_tb31 ;
Create table neg_tb31 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                     Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=none,compresslevel=0,blocksize=32768)) 
                    (start(1) end(1000) every(500)); 

\d+ neg_tb31

--Create table with mispelled compresstype

Drop table if exists neg_tb32 ;
Create table neg_tb32 (a1 int ,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=abc,compresslevel=1,blocksize=32768)) 
                    (start(1) end(1000) every(500));


--Create table with not allowed compresslevel
Drop table if exists neg_tb33 ;
Create table neg_tb33 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date )  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=quicklz,compresslevel=5,blocksize=32768)) 
                    (start(1) end(1000) every(500)); 


Drop table if exists neg_tb34 ;
Create table neg_tb34 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=zlib,compresslevel=11,blocksize=32768)) 
                    (start(1) end(1000) every(500)); 


--Create table with not allowed blocksize
Drop table if exists neg_tb35 ;
Create table neg_tb35 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date )  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=11111)) 
                    (start(1) end(1000) every(500)); 



--Create table with case insensitive compresstype

Drop table if exists neg_tb36 ;
Create table neg_tb36 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  with (appendonly=true,orientation=column) distributed by (a1)
                    Partition by range(a1) Subpartition by list(a2) subpartition template 
                    ( subpartition part1 values('M') ,
                    subpartition part2 values('F') , Column a1 ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768)) 
                    (start(1) end(1000) every(500)); 

\d+ neg_tb36


--Encodng clause conflicts with WITH clause

Drop table if exists neg_tb40 ;
Create table neg_tb40 (a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),a2 char(5),a3 text,a4 timestamp ,a5 date)  
                     with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1,blocksize=32768) distributed by (a1);

Drop table if exists neg_tb41 ;
Create table neg_tb41 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, column a1  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768))  
                           with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1,blocksize=32768) distributed by (a1);

Drop table if exists neg_tb42 ;                           
Create table neg_tb42 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date, default column ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768))  
                           with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1,blocksize=32768) distributed by (a1);

Drop table if exists neg_tb43 ;
Create table neg_tb43 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  
                           with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1,blocksize=32768) distributed by (a1) 
                           Partition by range(a1) (start(1) end(1000) every(500), Column a1 ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768));

Drop table if exists neg_tb44 ;
Create table neg_tb44 (a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date)  
                           with (appendonly=true,orientation=column,compresstype=quicklz,compresslevel=1,blocksize=32768) distributed by (a1) 
                           Partition by range(a1) Subpartition by list(a2) subpartition template 
                           ( subpartition part1 values('M') ,
                           subpartition part2 values('F') , Column a1 ENCODING (compresstype=QuicKLz,compresslevel=1,blocksize=32768)) 
                          (start(1) end(1000) every(500)); 