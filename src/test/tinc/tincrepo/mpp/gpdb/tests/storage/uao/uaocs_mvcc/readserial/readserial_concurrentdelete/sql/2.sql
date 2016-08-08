-- @Description UAOCS MVCC serial and 2 deletes  
--  Transaction 2 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentdelete', 't2_update_one_tuple');
select pg_sleep(4);
begin;
set transaction isolation level SERIALIZABLE;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 18002;
delete from sto_uaocs_emp_formvcc_serial  where sal = 18002;
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 18002;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentdelete' 
AND script='t2_update_one_tuple';


