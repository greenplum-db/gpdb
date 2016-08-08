-- @Description UAOCS MVCC readcommit and  vacuum + vacuum
--  Transaction 1 of 2 (insert)
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurr_vacuum_vacuum', 't1_insert_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum2;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum2;
set gp_select_invisible = false;
set transaction isolation level read committed;

vacuum full sto_uaocs_mvcc_vacuum2;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_vacuum_vacuum' 
AND script='t1_insert_tuples';
select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum2;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum2;
set gp_select_invisible = false;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_vacuum_vacuum' 
AND script='t1_insert_tuples';

