-- The transaction that drop a table will
-- hold Access Exclusive Lock on that table,
-- then other transactions do select|update|delete
-- on that table will wait on that lock.

-- Greenplum has a upgrade-lock logic to avoid global
-- deadlock, this logic is still in the code for ao-table
-- and select-for-update cases even we have GDD now.
-- The lock-upgrade logic is implemented by `CdbTryOpenRelation`,
-- this function might return a NULL pointer if the relation is
-- dropped (or some other reasons).

-- Previous code of parserOpenTable invokes CdbTryOpenRelation,
-- and use the result without checking if the value is a NULL
-- pointer so that leads to SIGSEGV.

-- This bug has been fixed by adding a check in parserOpenTable.
-- This test is used to confirm the fix.

1: create table tab_select_dropped_table (c int);
1: begin;
1: drop table tab_select_dropped_table;

2&: select * from tab_select_dropped_table;

1: end;
2<:
