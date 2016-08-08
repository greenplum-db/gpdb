-- @Description UAOCS MVCC readcommit and 2 updates on same row
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readcommit_concurrentupdate_delete', 't2_update_delete_one_tuple');
begin;
set transaction isolation level READ COMMITTED;
delete from  sto_uaocs_emp_formvcc  where sal = 18002.00 ;
select empno,ename from sto_uaocs_emp_formvcc  where sal = 18002.00 ;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readcommit_concurrentupdate_delete' 
AND script='t2_update_delete_one_tuple';
select empno,ename from sto_uaocs_emp_formvcc  where sal = 18002.00 ;


