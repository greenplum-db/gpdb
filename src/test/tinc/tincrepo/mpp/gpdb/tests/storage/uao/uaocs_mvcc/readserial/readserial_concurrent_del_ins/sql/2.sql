-- @Description UAOCS MVCC serial and 1 delete and another insert 
--  Transaction 2 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrent_del_ins', 't2_update_one_tuple');
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18004;
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in ( 200, 201, 202);
select pg_sleep(4);
begin;
set transaction isolation level SERIALIZABLE;
insert into sto_uaocs_emp_formvcc_serial values
(200,'Demi1','ADMIN',6,'12-17-1990',42100,00,4),
(201,'Demi2','ADMIN',6,'12-17-1990',43100,00,4),
(202,'Demi3','ADMIN',6,'12-17-1990',44100,00,4);
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in ( 200, 201, 202);
commit;
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in ( 200, 201, 202);
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_del_ins' 
AND script='t2_update_one_tuple';
select empno, ename from sto_uaocs_emp_formvcc_serial where  sal = 18004;


