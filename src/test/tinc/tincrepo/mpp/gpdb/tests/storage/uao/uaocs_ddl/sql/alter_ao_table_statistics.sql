-- @description : Alter  UAO tables and execute DML statements on the tables
-- 
Drop table if exists sto_alt_uao2_stats cascade;
CREATE TABLE sto_alt_uao2_stats(
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly=true, orientation=column) DISTRIBUTED RANDOMLY;
insert into sto_alt_uao2_stats values ('1_zero', 1, '1_zero', 1);
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao2_stats');
select * from sto_alt_uao2_stats order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_stats;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_stats;
set gp_select_invisible = false;
-- Alter column  set statistics
Alter table sto_alt_uao2_stats  alter column bigint_col set statistics 3;
select * from sto_alt_uao2_stats order by bigint_col;

update sto_alt_uao2_stats set numeric_col = 1 where text_col = '1_zero';
select * from sto_alt_uao2_stats order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_stats;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_stats;
set gp_select_invisible = false;
