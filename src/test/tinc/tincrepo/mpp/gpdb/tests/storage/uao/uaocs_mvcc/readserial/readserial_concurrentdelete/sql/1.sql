-- @Description UAOCS MVCC serial and 2 deletes
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentdelete', 't1_update_one_tuple');
begin;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 18003;
delete from sto_uaocs_emp_formvcc_serial  where sal = 18003;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurrentdelete' 
AND script='t1_update_one_tuple';
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 33333;
select pg_sleep(10);
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 33333;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentdelete' 
AND script='t1_update_one_tuple';

