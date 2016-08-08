-- @Description UAOCS MVCC readcommit and 2 updates on same row then rollback
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_savepoint', 't2_update_one_tuple_rollback');
begin;
set transaction isolation level READ COMMITTED;
update sto_uaocs_emp_formvcc set sal = 22223 where sal = 30000;
update sto_uaocs_emp_formvcc set sal = 22223 where sal = 27000;
select empno,ename from sto_uaocs_emp_formvcc where sal=22223;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_savepoint' 
AND script='t2_update_one_tuple_rollback';
select empno,ename from sto_uaocs_emp_formvcc where sal=22223;


