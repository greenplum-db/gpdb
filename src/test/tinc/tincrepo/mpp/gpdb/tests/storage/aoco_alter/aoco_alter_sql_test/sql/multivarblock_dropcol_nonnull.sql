-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO multivarblock table : drop column with default value non NULL
alter table multivarblock_tab ADD COLUMN added_col55 TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW() ;
select count(*) as added_col55 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col55';
alter table multivarblock_tab DROP COLUMN added_col55;
select count(*) as added_col55 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col55';
VACUUM multivarblock_tab;
