-- @Description UAOCS MVCC readserial and 1 updates and savepoint and 1 insert
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentupdate_savepoint', 't1_update_one_tuple_rollback');
begin;
update sto_uaocs_emp_formvcc_serial set sal = 77777.00 where sal = 18007;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate_savepoint' 
AND script='t1_update_one_tuple_rollback';
savepoint abc;
select empno,ename from sto_uaocs_emp_formvcc_serial where sal = 77777;
update sto_uaocs_emp_formvcc_serial set sal = 72121.00 where sal = 77777;
select pg_sleep(5);
select empno,ename from sto_uaocs_emp_formvcc_serial where sal = 72121;
rollback to savepoint abc;
commit;
select empno,ename from sto_uaocs_emp_formvcc_serial where sal = 72121;
select empno,ename from sto_uaocs_emp_formvcc_serial where sal = 77777;


update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate_savepoint' 
AND script='t1_update_one_tuple_rollback';

