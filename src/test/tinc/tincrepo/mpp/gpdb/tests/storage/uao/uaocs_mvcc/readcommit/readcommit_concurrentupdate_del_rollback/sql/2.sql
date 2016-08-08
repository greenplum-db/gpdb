-- @Description UAOCS MVCC readcommit and 2 updates on same row
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_delete_rollback', 't2_update_delete_rollback_one_tuple');
begin;
set transaction isolation level READ COMMITTED;
update sto_uaocs_dept_formvcc set dname = dname || '_dept';
select * from sto_uaocs_dept_formvcc order by dname;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete_rollback' 
AND script='t2_update_delete_rollback_one_tuple';
select * from sto_uaocs_dept_formvcc order by dname;


