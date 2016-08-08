-- @Description UAOCS MVCC readcommit and 2 updates on same row then rollback
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_rollback', 't2_update_one_tuple_rollback');
begin;
set transaction isolation level READ COMMITTED;
update sto_uaocs_emp_formvcc set sal = 11111.00 where sal = 18003.00 ;
select empno,ename from sto_uaocs_emp_formvcc where sal = 11111.00;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_rollback' 
AND script='t2_update_one_tuple_rollback';
select empno,ename from sto_uaocs_emp_formvcc where sal = 11111.00;


