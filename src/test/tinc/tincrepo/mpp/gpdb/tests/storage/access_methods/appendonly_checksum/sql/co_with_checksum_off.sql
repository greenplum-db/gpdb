-- 
-- @created 2014-10-9 12:00:00
-- @modified 2014-10-9 12:00:00
-- @tags storage
-- @description AOCO multiple insert to create multiple var-block for table with btree index


DROP TABLE IF EXISTS co_checksum_on;
CREATE TABLE co_checksum_on (
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
CREATE INDEX co_checksum_on_btree_idx ON co_checksum_on USING btree (c_custkey);
insert into co_checksum_on values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into co_checksum_on values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
insert into co_checksum_on values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 
select count(*) from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'co_checksum_on');

