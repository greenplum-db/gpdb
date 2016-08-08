-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_zlibtab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col22';
alter table multivarblock_zlibtab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col22';
VACUUM multivarblock_zlibtab;
