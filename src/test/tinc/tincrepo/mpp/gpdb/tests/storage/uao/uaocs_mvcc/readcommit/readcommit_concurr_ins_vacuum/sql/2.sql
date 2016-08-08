-- @Description UAOCS MVCC readcommit and  insert + vacuum
--  Transaction 2 of 2 (vacuum)
-- 

select pg_sleep(3);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurr_ins_vacuum', 't2_insert_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = false;

update sto_uaocs_mvcc_status set updover = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_ins_vacuum' 
AND script='t2_insert_tuples';
set transaction isolation level READ COMMITTED;

vacuum full sto_uaocs_mvcc_vacuum ;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_ins_vacuum' 
AND script='t2_insert_tuples';

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = false;

