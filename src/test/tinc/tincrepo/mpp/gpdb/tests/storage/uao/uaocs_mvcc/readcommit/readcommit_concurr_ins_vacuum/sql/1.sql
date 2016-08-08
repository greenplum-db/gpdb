-- @Description UAOCS MVCC readcommit and  insert + vacuum
--  Transaction 1 of 2 (insert)
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurr_ins_vacuum', 't1_insert_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = false;
begin;
insert into sto_uaocs_mvcc_vacuum select i, 'abc'||i, (random() * 500)::integer from generate_series(21,1000000) as i;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_ins_vacuum' 
AND script='t1_insert_tuples';
select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum;
set gp_select_invisible = false;
commit;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_ins_vacuum' 
AND script='t1_insert_tuples';

