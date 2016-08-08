-- 
-- @created 2014-06-06 12:00:00
-- @modified 2014-06-06 12:00:00
-- @tags storage
-- @description AOCO multiple insert/update to create multiple segfiles 

DROP TABLE IF EXISTS multi_segfile_tab;
CREATE TABLE multi_segfile_tab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_tab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_tab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
insert into multi_segfile_tab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 
insert into multi_segfile_tab (select i,'xx'||i, 'Can this be a long text please?',i+.5,'10'||i,100.23+i,cast(now() as date),cast(now() as timestamp) from generate_series(100,66900) i);

update multi_segfile_tab set c_name = 'bcx' where c_custkey % 5 = 0; 
vacuum multi_segfile_tab;
insert into multi_segfile_tab (select i,'yy'||i, 'Just another long string',i+.5,'10'||i,100.23+i,cast(now() as date),cast(now() as timestamp) from generate_series(100,6900) i);
