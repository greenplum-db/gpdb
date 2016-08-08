-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multiple insert to create multiple var-block

DROP TABLE IF EXISTS multivarblock_toast;
CREATE TABLE multivarblock_toast (
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
insert into multivarblock_toast values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multivarblock_toast values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
insert into multivarblock_toast values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 

