-- @Description UAOCS MVCC serial and 1 update and 1 insert
--  Transaction 2 of 2
-- 

insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurrent_upd_ins', 't2_update_one_tuple');
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in (100, 101, 102)order by 1; 
select pg_sleep(6);
begin;
set transaction isolation level SERIALIZABLE;
insert into sto_uaocs_emp_formvcc_serial values
(100,'Jeffery','ADMIN',6,'02-17-1996',1211,00,4),
(101,'Jacob','ENGINEER',6,'03-12-1996',1311,00,4),
(102,'Jenny','SALESMAN',6,'08-11-1996',1411,00,4);
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in (100, 101, 102) order by 1;
commit;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurrent_upd_ins' 
AND script='t2_update_one_tuple';
select empno, ename from sto_uaocs_emp_formvcc_serial where empno in (100, 101, 102) order by 1;


