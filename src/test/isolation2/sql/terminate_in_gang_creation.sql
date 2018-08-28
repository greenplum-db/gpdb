-- SIGSEGV issue when freeing gangs
--
-- When SIGTERM is handled during gang creation we used to trigger
-- a wild pointer access like below backtrace:
--
-- #0  raise
-- #1  StandardHandlerForSigillSigsegvSigbus_OnMainThread
-- #2  <signal handler called>
-- #3  MemoryContextFreeImpl
-- #4  cdbconn_termSegmentDescriptor
-- #5  DisconnectAndDestroyGang
-- #6  freeGangsForPortal
-- #7  AbortTransaction
-- ...
-- #14 ProcessInterrupts
-- #15 createGang_async
-- #16 createGang
-- #17 AllocateWriterGang

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (c1 int, c2 int) DISTRIBUTED BY (c1);

10: BEGIN;

SELECT gp_inject_fault('create_gang_in_progress', 'reset', 1);
SELECT gp_inject_fault('create_gang_in_progress', 'suspend', 1);

10&: SELECT * FROM foo a JOIN foo b USING (c2);

SELECT gp_wait_until_triggered_fault('create_gang_in_progress', 1, 1);

SELECT pg_terminate_backend(pid) FROM pg_stat_activity
 WHERE query = 'SELECT * FROM foo a JOIN foo b USING (c2);';

SELECT gp_inject_fault('create_gang_in_progress', 'resume', 1);

10<:
10q:

DROP TABLE foo;
