-- @Description UAOCS MVCC serial and 1 delete and another insert 
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrent_del_ins', 't1_update_one_tuple');
begin;
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18004;
delete from  sto_uaocs_emp_formvcc_serial where sal = 18004;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_del_ins' 
AND script='t1_update_one_tuple';
select pg_sleep(8);
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18004;
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18004;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_del_ins' 
AND script='t1_update_one_tuple';

