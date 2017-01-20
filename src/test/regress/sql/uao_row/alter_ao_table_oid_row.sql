create schema alter_ao_table_oid_row;
set search_path="$user",alter_ao_table_oid_row,public;
SET gp_default_storage_options='orientation=row';
-- Alter UAO tables with OIDs and execute DML statements on the tables
create table sto_alt_uao2_oid (i int, j char(10)) with
(appendonly=true, oids=true) DISTRIBUTED RANDOMLY;
\d+ sto_alt_uao2_oid
insert into sto_alt_uao2_oid select i , 'aa' from generate_series(1,5) as i;
select count(oid) = 5 as passed from sto_alt_uao2_oid;
select * from sto_alt_uao2_oid;
delete from sto_alt_uao2_oid where i = 3;
insert into sto_alt_uao2_oid values (6, 'new tuple');
select * from sto_alt_uao2_oid;
set gp_select_invisible=true;
select count(*) from sto_alt_uao2_oid;
set gp_select_invisible=false;

--Alter table SET without OIDs
Alter table sto_alt_uao2_oid SET without oids;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimaprelid is
 not NULL AND visimapidxid is not NULL AND relid='sto_alt_uao2_oid'::regclass;

\d+ sto_alt_uao2_oid
select * from sto_alt_uao2_oid order by 1;
update sto_alt_uao2_oid set j= 'new val' where i=4;
select * from sto_alt_uao2_oid order by 1;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_oid;
set gp_select_invisible = false;
