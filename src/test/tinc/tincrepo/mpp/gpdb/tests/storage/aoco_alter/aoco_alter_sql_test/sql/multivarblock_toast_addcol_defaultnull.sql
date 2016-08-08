-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multivarblock table : add column toast with default value NULL

alter table multivarblock_toast ADD COLUMN added_col3 bytea  default NULL;
select count(*) as added_col3 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col3';
