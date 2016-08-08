-- @Description UAOCS MVCC serial and 1 update and 1 insert
--  Transaction 1 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrent_upd_ins', 't1_update_one_tuple');
begin;
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18005;
update sto_uaocs_emp_formvcc_serial set sal = 55555.00 where sal = 18005;
update sto_uaocs_mvcc_status set updover  = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_upd_ins' 
AND script='t1_update_one_tuple';
select pg_sleep(20);
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 55555.00;
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 55555.00;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_upd_ins' 
AND script='t1_update_one_tuple';

