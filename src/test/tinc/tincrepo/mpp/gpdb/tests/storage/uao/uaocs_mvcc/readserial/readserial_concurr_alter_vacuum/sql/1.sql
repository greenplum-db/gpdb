-- @Description UAOCS MVCC readserial and  alter + vacuum
--  Transaction 1 of 2 (alter)
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurr_alter_vacuum', 't1_alter_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = false;
\d+ sto_uaocs_mvcc_vacuum_alter_serial;
begin;
alter table  sto_uaocs_mvcc_vacuum_alter_serial rename column name to new_name ;
alter table  sto_uaocs_mvcc_vacuum_alter_serial rename column population to new_population ;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurr_alter_vacuum' 
AND script='t1_alter_tuples';
select pg_sleep(9);
select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = false;
commit;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurr_alter_vacuum' 
AND script='t1_alter_tuples';

\d+ sto_uaocs_mvcc_vacuum_alter_serial;
