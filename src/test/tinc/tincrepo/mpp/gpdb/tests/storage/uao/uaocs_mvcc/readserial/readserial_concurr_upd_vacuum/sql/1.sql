-- @Description UAOCS MVCC readserial and  update + vacuum
--  Transaction 1 of 2 (update)
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurr_upd_vacuum', 't1_update_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = false;
begin;
update sto_uaocs_mvcc_vacuum_upd_serial set population = population + 1;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurr_upd_vacuum' 
AND script='t1_update_tuples';
select pg_sleep(5);
select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = false;
commit;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurr_upd_vacuum' 
AND script='t1_update_tuples';

