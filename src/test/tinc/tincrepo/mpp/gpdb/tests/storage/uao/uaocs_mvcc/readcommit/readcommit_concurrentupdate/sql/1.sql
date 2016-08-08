-- @Description UAOCS MVCC readcommit_uaocs and 2 updates on same row
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate', 't1_update_one_tuple');
begin;
update sto_uaocs_emp_formvcc set sal = 18009.00 where sal = 18001.00 ;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate' 
AND script='t1_update_one_tuple';
select pg_sleep(5);
select empno,ename  from sto_uaocs_emp_formvcc where sal = 18009.00;
commit;
select empno,ename  from sto_uaocs_emp_formvcc where sal = 18009.00;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate' 
AND script='t1_update_one_tuple';

