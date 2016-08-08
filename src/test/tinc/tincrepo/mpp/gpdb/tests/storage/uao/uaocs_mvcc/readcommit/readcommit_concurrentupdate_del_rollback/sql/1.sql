-- @Description UAOCS MVCC readcommit and 1 updates + 1 delete on same row
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_delete_rollback', 't1_update_delete_rollback_one_tuple');
begin;
delete from sto_uaocs_dept_formvcc where deptno = 2;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete_rollback' 
AND script='t1_update_delete_rollback_one_tuple';
select pg_sleep(5);
select * from sto_uaocs_dept_formvcc order by dname;
rollback;
select * from sto_uaocs_dept_formvcc order by dname;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete_rollback' 
AND script='t1_update_delete_rollback_one_tuple';

