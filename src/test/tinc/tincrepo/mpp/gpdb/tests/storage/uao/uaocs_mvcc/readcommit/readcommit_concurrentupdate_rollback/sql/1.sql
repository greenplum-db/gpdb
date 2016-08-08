-- @Description UAOCS MVCC readcommit and 2 updates on same row and Rollback
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_rollback', 't1_update_one_tuple_rollback');
begin;
update sto_uaocs_emp_formvcc set sal = 99999.00 where sal = 18003.00 ;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_rollback' 
AND script='t1_update_one_tuple_rollback';
select pg_sleep(5);
select empno,ename  from sto_uaocs_emp_formvcc where sal = 99999;
rollback;
select empno,ename  from sto_uaocs_emp_formvcc where sal = 99999;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_rollback' 
AND script='t1_update_one_tuple_rollback';

