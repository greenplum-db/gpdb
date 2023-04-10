-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- end_ignore

-- Test: free remapper's memory correctly in FATAL
-- Here is the background:
-- When the query with "record type cache" raises a FATAL, it runs into the following routine:
-- FATAL->AbortTransaction()
--     AtAbort_Portals()
--         MemoryContextDeleteChildren()     // delete ExecutorState memory context
--     ResourceOwnerRelease()
--         cleanup_interconnect_handle()
--             TeardownUDPIFCInterconnect()
--                 DestroyTupleRemapper()          // pfree remapper->typmodmap
-- It may cause SIGSEGV: remapper->typmodmap is alloced in MotionLayerMemCtxt,
-- but the memory context has been deleted in AtAbort_Portals() (it's the child of ExecutorState).
-- PR-15340 fixes it.

-- create udt type
1:create table tbl_pr15340 as select i as id from generate_series(1,8) i;
1:drop type if exists myudt;
1:create type myudt as (i int, c char);

-- should encounter the inject fatal rather than SIGSEGV
1:select gp_inject_fault_infinite('handled_typmodmap', 'fatal', 1);
1:select row((1,'a')::myudt), row((2,'b')::myudt, 2) from tbl_pr15340;
1<:
-- cleanup
2:select gp_inject_fault_infinite('handled_typmodmap', 'reset', 1);
2:drop table tbl_pr15340;
2:drop type if exists myudt;
2<: