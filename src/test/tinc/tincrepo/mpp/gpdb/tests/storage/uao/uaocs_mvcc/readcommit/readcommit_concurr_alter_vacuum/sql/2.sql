-- @Description UAOCS MVCC readcommit and  alter + vacuum
--  Transaction 2 of 2 (vacuum)
-- 

select pg_sleep(3);
\d+ sto_uaocs_mvcc_vacuum_alter;
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurr_alter_vacuum', 't2_vacuum_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter;
set gp_select_invisible = false;

set transaction isolation level READ COMMITTED;
vacuum full sto_uaocs_mvcc_vacuum_alter ;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurr_alter_vacuum' 
AND script='t2_vacuum_tuples';

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter;
set gp_select_invisible = false;
\d+ sto_uaocs_mvcc_vacuum_alter;
