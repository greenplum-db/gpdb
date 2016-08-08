-- @Description UAOCS MVCC readcommit_uaocs and 2 updates on same row
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate', 't2_update_one_tuple');
begin;
set transaction isolation level READ COMMITTED;
update sto_uaocs_emp_formvcc set sal = 18005.00 where sal = 18001.00 ;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate' 
AND script='t2_update_one_tuple';
select empno,ename  from sto_uaocs_emp_formvcc where sal = 18005.00;

