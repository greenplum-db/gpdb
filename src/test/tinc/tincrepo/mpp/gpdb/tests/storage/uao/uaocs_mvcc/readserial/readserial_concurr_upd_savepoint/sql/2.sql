-- @Description UAOCS MVCC readserial and 2 updates on same row then rollback
--  Transaction 2 of 2
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrentupdate_savepoint', 't2_update_one_tuple_rollback');
begin;
set transaction isolation level SERIALIZABLE;
insert into sto_uaocs_emp_formvcc_serial values
(300,'JANA','ADMIN',6,'12-17-1990',19001,10,4);
select empno,ename from sto_uaocs_emp_formvcc_serial where sal=19001;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrentupdate_savepoint' 
AND script='t2_update_one_tuple_rollback';
select empno,ename from sto_uaocs_emp_formvcc_serial where sal=19001;


