-- @Description UAOCS MVCC serial and 2 updates 
--  Transaction 2 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentupdate', 't2_update_one_tuple');
select pg_sleep(4);
begin;
set transaction isolation level SERIALIZABLE;
update sto_uaocs_emp_formvcc_serial set sal = 22222 where sal = 18002 ;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 22222 ;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate' 
AND script='t2_update_one_tuple';
select empno, ename  from sto_uaocs_emp_formvcc_serial where sal = 22222; 


