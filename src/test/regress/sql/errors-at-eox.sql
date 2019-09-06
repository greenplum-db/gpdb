--
-- Test what happens, when an ERROR occurs at various stages of COMMIT
-- or ABORT processing. This test uses the fault injection mechanism
-- built in to the server, to induce an ERROR at strategic places.
--
-- Create a plain table that we can insert to, to verify after the
-- transaction whether the transaction's effects are visible.
CREATE TABLE dtm_testtab(id int4);

-- throw an ERROR, just after we have broadcast the PREPARE of the
-- transaction to segments.
select gp_inject_fault('dtm_broadcast_prepare', 'error', 1::smallint);

insert into dtm_testtab values (1), (2);

select * from dtm_testtab;

-- throw an ERROR, in master, just before we have broadcast the COMMIT
-- PREPARED to segments.
select gp_inject_fault('dtm_broadcast_commit_prepared', 'error', 1::smallint);

insert into dtm_testtab values (3), (4);

select * from dtm_testtab;

-- throw an ERROR, after we have flushed the (two-phase) commit record
-- to disk in master.
select gp_inject_fault('dtm_xlog_distributed_commit', 'error', 1::smallint);

insert into dtm_testtab values (3), (4);

select * from dtm_testtab;

