-- @Description UAOCS MVCC readcommit and 2 updates on same row and savepoint
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_savepoint', 't1_update_one_tuple_rollback');
begin;
update sto_uaocs_emp_formvcc set sal = 22222.00 where sal = 30000;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_savepoint' 
AND script='t1_update_one_tuple_rollback';
savepoint abc;
select empno,ename from sto_uaocs_emp_formvcc where sal = 22222;
update sto_uaocs_emp_formvcc set sal = 12121.00 where sal = 22222;
select pg_sleep(5);
select empno,ename from sto_uaocs_emp_formvcc where sal = 12121;
rollback to savepoint abc;
commit;
select empno,ename from sto_uaocs_emp_formvcc where sal = 12121;
select empno,ename from sto_uaocs_emp_formvcc where sal = 22222;


update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_savepoint' 
AND script='t1_update_one_tuple_rollback';

