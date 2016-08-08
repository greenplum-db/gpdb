-- @Description UAOCS MVCC readserial and  alter + vacuum
--  Transaction 2 of 2 (vacuum)
-- 

select pg_sleep(3);
\d+ sto_uaocs_mvcc_vacuum_alter_serial;
insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurr_alter_vacuum', 't2_vacuum_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = false;

set transaction isolation level SERIALIZABLE;
vacuum full sto_uaocs_mvcc_vacuum_alter_serial ;
update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurr_alter_vacuum' 
AND script='t2_vacuum_tuples';

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_alter_serial;
set gp_select_invisible = false;
\d+ sto_uaocs_mvcc_vacuum_alter_serial;
