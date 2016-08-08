-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multiple insert to create multiple var-block for table with partitions

DROP TABLE IF EXISTS multivarblock_parttab;
CREATE TABLE multivarblock_parttab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
insert into multivarblock_parttab values( 1, 'aa','this is a looong text' , 4.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multivarblock_parttab values( 2, 'ab','this is also a looong text' , 7.5, '3456789',3000.45,'2014/08/10',now()); 
insert into multivarblock_parttab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',4000.25,'2014/08/10',now());

