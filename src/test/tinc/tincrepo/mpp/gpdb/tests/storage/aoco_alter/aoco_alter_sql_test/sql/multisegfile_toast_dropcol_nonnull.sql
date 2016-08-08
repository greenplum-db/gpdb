-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_toast ADD COLUMN added_col44  bytea default ("decode"(repeat('1234567890',10000),'escape'));
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col44';
alter table multi_segfile_toast DROP COLUMN added_col44;
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col44';
VACUUM multi_segfile_toast;
