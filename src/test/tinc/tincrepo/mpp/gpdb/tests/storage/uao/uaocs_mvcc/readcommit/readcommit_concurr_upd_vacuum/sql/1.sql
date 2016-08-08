-- @Description UAOCS MVCC readcommit and  update + vacuum
--  Transaction 1 of 2 (update)
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurr_upd_vacuum', 't1_update_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd;
set gp_select_invisible = false;
begin;
select pg_sleep(1);
update sto_uaocs_mvcc_vacuum_upd set population = population + 1;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_upd_vacuum' 
AND script='t1_update_tuples';
select pg_sleep(5);
select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd;
set gp_select_invisible = false;
commit;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_upd_vacuum' 
AND script='t1_update_tuples';

