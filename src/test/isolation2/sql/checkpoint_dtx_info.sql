-- Currently `MyTmGxact->includeInCkpt = true` and `XLogInsert(RM_XACT_ID, XLOG_XACT_DISTRIBUTED_COMMIT)`
-- is already protected by delayChkpt, so these are an atomic operation from the outside perspective.
-- But getDtxCheckPointInfo() may see the middle state because getDtxCheckPointInfo() is called
-- before GetVirtualXIDsDelayingChkpt(). We have fixed it and add this testcase.
-- This testcase will fail if getDtxCheckPointInfo() is called before GetVirtualXIDsDelayingChkpt().

-- We accurately control the progress of COMMIT executed in session 1 and of CHECKPOINT executed
-- in the checkpointer process, so that events occur in the following specific order:
--
-- 1. session 1: COMMIT is blocked at start_insertedDistributedCommitted.
-- 2. checkpointer: Start a new CHECKPOINT and the CHECKPOINT is blocked at `checkpoint`.
-- 3. checkpointer: CHECKPOINT is resumed and executes to before_wait_VirtualXIDsDelayingChkpt.
-- 4. session 1: COMMIT is resumed and executes to after_xlog_xact_distributed_commit
-- 5. checkpointer: CHECKPOINT is resumed and executes to keep_log_seg and cause a panic.
--
-- if getDtxCheckPointInfo() is invoked before GetVirtualXIDsDelayingChkpt(), getDtxCheckPointInfo()
-- will not contain the distributed transaction in session1 whose state is DTX_STATE_INSERTED_COMMITTED.
-- Therefore, after crash recovery, the 2PC transaction that has been committed in master will be
-- considered by the master as an orphaned prepared transaction and will be rollbacked at segments.
-- So the SELECT executed by session3 will fail because the twopcbug table only exists on the master.
1: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'suspend', 1);
1: begin;
1: create table twopcbug(i int, j int);
1&: commit;
2: select gp_inject_fault_infinite('checkpoint', 'suspend', 1);
33&: checkpoint;
2: select gp_wait_until_triggered_fault('checkpoint', 1, 1);
2: select gp_inject_fault_infinite('keep_log_seg', 'panic', 1);
2: select gp_inject_fault_infinite('before_wait_VirtualXIDsDelayingChkpt', 'skip', 1);
2: select gp_inject_fault_infinite('checkpoint', 'resume', 1);
2: select gp_wait_until_triggered_fault('before_wait_VirtualXIDsDelayingChkpt', 1, 1);
2: select gp_inject_fault_infinite('after_xlog_xact_distributed_commit', 'infinite_loop', 1);
2: select gp_inject_fault_infinite('start_insertedDistributedCommitted', 'resume', 1);
1<:
33<:
-- wait until master is up for querying.
3: select 1;
3: select count(1) from twopcbug;

