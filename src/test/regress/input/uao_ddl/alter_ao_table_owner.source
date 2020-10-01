create schema alter_ao_table_owner_@amname@;
set search_path="$user",alter_ao_table_owner_@amname@,public;
SET default_table_access_method=@amname@;
--- start_ignore
drop owned by uao_user2 cascade;
Drop role if exists uao_user2 ;
--- stop_ignore
--Alter table to a new owner
Create role uao_user2;
create table sto_alt_uao4 (i int, j char(10)) DISTRIBUTED BY (i);
insert into sto_alt_uao4 select i , 'cc'||i from generate_series(1,5) as i;
Alter Table sto_alt_uao4  Owner to uao_user2;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND
 visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao4');
select 1 as owner_changed from pg_class where relname='sto_alt_uao4' and
 relowner = (select oid from pg_roles where rolname = 'uao_user2');
select * from sto_alt_uao4 order by 1;
update sto_alt_uao4 set j = 'azzza3' where i = 3;
select * from sto_alt_uao4 order by 1;
delete from sto_alt_uao4;
select * from sto_alt_uao4;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao4;
set gp_select_invisible = false;
