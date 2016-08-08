-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_parttab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col11';
alter table multi_segfile_parttab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col11';
VACUUM multi_segfile_parttab;
