-- 
-- @created 2014-06-06 12:00:00
-- @modified 2014-06-06 12:00:00
-- @tags storage
-- @description AOCO Create table for Alter table add column in utility mode - Negative testcase

DROP TABLE IF EXISTS alter_aoco_tab_utilitymode;
CREATE TABLE alter_aoco_tab_utilitymode (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey);
insert into alter_aoco_tab_utilitymode (select i,'xx'||i, 'This is my long text ',i+.5,'10'||i,100.23+i,cast(now() as date),cast(now() as timestamp) from generate_series(1,1000) i);
 \d alter_aoco_tab_utilitymode;
