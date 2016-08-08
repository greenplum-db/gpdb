-- @description : Alter  UAO tables and execute DML statements on the tables
-- 
Drop table if exists sto_alt_uao2_oid cascade;
create table sto_alt_uao2_oid (i int, j char(10)) with (appendonly=true, orientation=column, oids=false) DISTRIBUTED RANDOMLY;
\d+ sto_alt_uao2_oid
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao2_oid');

insert into sto_alt_uao2_oid select i , 'aa' from generate_series(1,5) as i;
select * from sto_alt_uao2_oid order by 1;
select count(*) AS only_visi_tups from sto_alt_uao2_oid;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_oid;
set gp_select_invisible = false;

--Alter table SET without OIDs
Alter table sto_alt_uao2_oid SET without oids;

\d+ sto_alt_uao2_oid
select * from sto_alt_uao2_oid order by 1;
update sto_alt_uao2_oid set j= 'new val'  where i=3; 
select * from sto_alt_uao2_oid order by 1;
select count(*) AS only_visi_tups from sto_alt_uao2_oid;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_oid;
set gp_select_invisible = false;
