-- @description : Alter  UAO tables and execute DML statements on the tables
-- 
Drop table if exists sto_alt_uao3_idx cascade;
CREATE TABLE sto_alt_uao3_idx(
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly=true) DISTRIBUTED RANDOMLY;
insert into sto_alt_uao3_idx values ('1_zero', 1, '1_zero', 1);
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao3_idx');

select * from sto_alt_uao3_idx order by bigint_col;
update sto_alt_uao3_idx set numeric_col = 100 where text_col = '1_zero';
select count(*) AS only_visi_tups from sto_alt_uao3_idx;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao3_idx;
set gp_select_invisible = false;
-- Alter table cluster on indexname
Create index sto_alt_uao3_idx_idx on sto_alt_uao3_idx(bigint_col);
Alter table sto_alt_uao3_idx cluster on sto_alt_uao3_idx_idx;
select * from sto_alt_uao3_idx order by bigint_col;

update sto_alt_uao3_idx set numeric_col = 111 where text_col = '1_zero';
select * from sto_alt_uao3_idx order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao3_idx;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao3_idx;
set gp_select_invisible = false;

