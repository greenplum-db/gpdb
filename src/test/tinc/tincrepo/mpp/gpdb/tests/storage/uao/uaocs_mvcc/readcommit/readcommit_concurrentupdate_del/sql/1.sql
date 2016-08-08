-- @Description UAOCS MVCC readcommit and 1 updates + 1 delete on same row
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_delete', 't1_update_delete_one_tuple');
begin;
update sto_uaocs_emp_formvcc set ename = 'New Name'  where sal = 18002.00 ;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete' 
AND script='t1_update_delete_one_tuple';
select pg_sleep(5);
select empno,ename from sto_uaocs_emp_formvcc where sal = 18002;
commit;
select empno,ename from sto_uaocs_emp_formvcc where sal = 18002;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete' 
AND script='t1_update_delete_one_tuple';

