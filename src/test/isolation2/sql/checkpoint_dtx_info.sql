-- Scenario to test, CHECKPOINT getting distributed transaction
-- information between COMMIT processing time window
-- `XLogInsert(RM_XACT_ID, XLOG_XACT_DISTRIBUTED_COMMIT)` and
-- insertedDistributedCommitted(). `delayChkpt` protects this
-- case. There used to bug in placement of getDtxCheckPointInfo() in
-- checkpoint code causing, transaction to be committed on coordinator
-- and aborted on segments. Test case is meant to validate
-- getDtxCheckPointInfo() gets called after
-- GetVirtualXIDsDelayingChkpt().
--
-- Test controls the progress of COMMIT executed in session 1 and of
-- CHECKPOINT executed in the checkpointer process, with high-level
-- flow:
--
-- 1. session 1: COMMIT is blocked at start_insertedDistributedCommitted
-- 2. checkpointer: Start a CHECKPOINT and wait to reach before_wait_VirtualXIDsDelayingChkpt
-- 3. session 1: COMMIT is resumed
-- 4. checkpointer: CHECKPOINT is resumed and executes to keep_log_seg to finally introduce panic and perform crash recovery
--
-- Bug existed when getDtxCheckPointInfo() was invoked before
-- GetVirtualXIDsDelayingChkpt(), getDtxCheckPointInfo() will not
-- contain the distributed transaction in session1 whose state is
-- DTX_STATE_INSERTED_COMMITTED.  Therefore, after crash recovery, the
-- 2PC transaction that has been committed on coordinator will be
-- considered as orphaned prepared transaction hence is aborted at
-- segments. As a result the SELECT executed by session3 used to fail
-- because the twopcbug table only existed on the coordinator.
--
1: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'suspend', 1);
1: begin;
1: create table twopcbug(i int, j int);
1&: commit;
-- wait to make sure the commit is taking place and blocked at start_insertedDistributedCommitted
2: select gp_wait_until_triggered_fault('start_insertedDistributedCommitted', 1, 1);
2: select gp_inject_fault_infinite('before_wait_VirtualXIDsDelayingChkpt', 'skip', 1);
33&: checkpoint;
2: select gp_inject_fault_infinite('keep_log_seg', 'panic', 1);
-- wait to make sure we don't resume commit processing before this
-- step in checkpoint
2: select gp_wait_until_triggered_fault('before_wait_VirtualXIDsDelayingChkpt', 1, 1);
-- reason for this inifinite wait is just to avoid test flake. Without
-- this joining step "1<" may see "COMMIT" sometimes or "server closed
-- the connection unexpectedly" otherwise. With this its always
-- "server closed the connection unexpectedly".
2: select gp_inject_fault_infinite('after_xlog_xact_distributed_commit', 'infinite_loop', 1);
2: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'resume', 1);
1<:
33<:
-- wait until coordinator is up for querying.
3: select 1;
3: select count(1) from twopcbug;

-- Validate CHECKPOINT XLOG record length, verifying issue
-- https://github.com/greenplum-db/gpdb/issues/12977.
-- The extended CHECKPOINT WAL record contains global transaction
-- information, it could exceed the previous expected length in
-- SizeOfXLogRecordDataHeaderShort, result in crash recovery
-- failure on coordinator. The solution is adding the expected length
-- in SizeOfXLogRecordDataHeaderLong also, to fixup the missing condition.
create table ckpt_xlog_len_tbl(a int, b int);

-- Need to start at least 18 concurrent sessions to create a long header
-- CHECKPOINT WAL record, which size is not less than 256.
2q:
33q:

10: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'suspend', 1);

10: begin;
10: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
10&: commit;

11: begin;
11: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
11&: commit;

12: begin;
12: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
12&: commit;

13: begin;
13: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
13&: commit;

14: begin;
14: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
14&: commit;

15: begin;
15: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
15&: commit;

16: begin;
16: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
16&: commit;

17: begin;
17: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
17&: commit;

18: begin;
18: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
18&: commit;

19: begin;
19: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
19&: commit;

20: begin;
20: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
20&: commit;

21: begin;
21: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
21&: commit;

22: begin;
22: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
22&: commit;

23: begin;
23: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
23&: commit;

24: begin;
24: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
24&: commit;

25: begin;
25: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
25&: commit;

26: begin;
26: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
26&: commit;

27: begin;
27: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
27&: commit;

28: begin;
28: insert into ckpt_xlog_len_tbl select i,i from generate_series(1,10)i;
28&: commit;

-- wait to make sure the commit is taking place and blocked at start_insertedDistributedCommitted
2: select gp_wait_until_triggered_fault('start_insertedDistributedCommitted', 1, 1);
2: select gp_inject_fault_infinite('before_wait_VirtualXIDsDelayingChkpt', 'skip', 1);
33&: checkpoint;
2: select gp_inject_fault_infinite('keep_log_seg', 'panic', 1);
-- wait to make sure we don't resume commit processing before this
-- step in checkpoint
2: select gp_wait_until_triggered_fault('before_wait_VirtualXIDsDelayingChkpt', 1, 1);
-- reason for this inifinite wait is just to avoid test flake. Without
-- this joining step "1<" may see "COMMIT" sometimes or "server closed
-- the connection unexpectedly" otherwise. With this its always
-- "server closed the connection unexpectedly".
2: select gp_inject_fault_infinite('after_xlog_xact_distributed_commit', 'infinite_loop', 1);
2: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'resume', 1);

10<:
33<:
11<:
12<:
13<:
14<:
15<:
16<:
17<:
18<:
19<:
20<:
21<:
22<:
23<:
24<:
25<:
26<:
27<:
28<:

3q:
3: select 1;
3: select count(*) from ckpt_xlog_len_tbl;

3: drop table ckpt_xlog_len_tbl;
