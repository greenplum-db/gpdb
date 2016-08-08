-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multi_encoding  table : drop column with default value non NULL
DROP table if exists aoco_multi_encoding;
CREATE TABLE aoco_multi_encoding (
         col1 int ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         col2 char(5) ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         col3 text ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         col4 timestamp ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192),
         col5 date ENCODING (compresstype=ZLIB,compresslevel=8,blocksize=8192)
)with (appendonly=true,orientation=column);
alter table aoco_multi_encoding ADD COLUMN added_col6  bytea default ("decode"(repeat('1234567890',10000),'greenplum'));
select count(*) as added_col6 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='aoco_multi_encoding' and attname='added_col6';
alter table aoco_multi_encoding DROP COLUMN added_col6;
select count(*) as added_col6 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='aoco_multi_encoding' and attname='added_col6';
VACUUM aoco_multi_encoding;

