-- @Description UAOCS MVCC serial and 2 updates 
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentupdate', 't1_update_one_tuple');
begin;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 18001 ;
update sto_uaocs_emp_formvcc_serial set sal = 11111 where sal = 18001 ;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate' 
AND script='t1_update_one_tuple';
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 11111 ;
select pg_sleep(10);
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where sal = 11111 ;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate' 
AND script='t1_update_one_tuple';

