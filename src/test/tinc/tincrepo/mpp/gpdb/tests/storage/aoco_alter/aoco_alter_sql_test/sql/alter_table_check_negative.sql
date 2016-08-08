-- 
-- @created 2014-05-19 12:00:00
-- @modified 2014-05-19 12:00:00
-- @tags storage
-- @description AOCO table add column with conflicting default and check - Negative testcase

Drop table if exists aoco_tab_check;
create table aoco_tab_check (i int, j char(20)) with (appendonly=true, orientation=column);
insert into aoco_tab_check select i , 'abc'||i from generate_series(1,2) i;
insert into aoco_tab_check select i , 'xyz'||i from generate_series(101,102) i;
alter table aoco_tab_check ADD COLUMN added_col12 character varying(35) default 'abc'  CHECK (added_col12 is NULL);
