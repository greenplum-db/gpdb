-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO multi_segfile table : add column with default value non NULL

alter table multi_segfile_tab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col1';

