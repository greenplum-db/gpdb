-- @Description UAOCS MVCC readserial and  update + vacuum
--  Transaction 2 of 2 (vacuum)
-- 

select pg_sleep(2);
insert into sto_uaocs_mvcc_status (workload, script) values('readserial_concurr_upd_vacuum', 't2_vacuum_tuples');

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = false;

set transaction isolation level SERIALIZABLE;
vacuum full sto_uaocs_mvcc_vacuum_upd_serial ;

update sto_uaocs_mvcc_status set endtime = CURRENT_TIMESTAMP 
where workload='readserial_concurr_upd_vacuum' 
AND script='t2_vacuum_tuples';

select count(*) as only_visi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = true;
select count(*) as visi_and_invisi_rows from sto_uaocs_mvcc_vacuum_upd_serial;
set gp_select_invisible = false;

