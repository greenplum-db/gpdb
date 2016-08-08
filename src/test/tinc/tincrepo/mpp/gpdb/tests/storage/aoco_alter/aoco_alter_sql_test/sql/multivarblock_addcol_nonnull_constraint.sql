-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO multivarblock table : add column with constraint and default value non NULL
 -- Negative test
alter table multivarblock_tab add column added_col66 int CONSTRAINT multivarblock_tab_check1 CHECK (added_col66 < 10)  default 30;
select count(*) as added_col66 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col66';
 -- Positive test
alter table multivarblock_tab add column added_col66 int CONSTRAINT multivarblock_tab_check1 CHECK (added_col66 < 10)  default 5;
select count(*) as added_col66 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col66';
