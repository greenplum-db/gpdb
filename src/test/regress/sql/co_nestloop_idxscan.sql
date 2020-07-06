--
-- Nested loop join with index scan on CO table, test for MPP-17658
--
-- The test should also make sure the AO/AOCO table's bitmap state
-- get re-init in BitmapHeapScanState if the current scan on AO/AOCO table
-- not finished, and after ExecReScanBitmapHeapScan get called which will free
-- current bitmap state.
-- If the scan read all from AO/AOCO, the bitmap state in BitmapHeapScanState
-- always get re-init, so this case is not considered.
-- This is test through Nested Loop Semi Join, since it garentees that if
-- find a match, a new outer slot is request, which the inner plan may not
-- read all tuples. The inner plan of the Nested Loop Semi Join is Bitmap
-- Heap Scan. So for a new outer slot, the inner plan need to rescan from
-- the begining.
--

create schema co_nestloop_idxscan;
create table co_nestloop_idxscan.foo (id bigint, data text) with (appendonly=true, orientation=column)
distributed by (id);
create table co_nestloop_idxscan.bar (id bigint) distributed by (id);

-- Changing the text to be smaller doesn't repro the issue
insert into co_nestloop_idxscan.foo select i, repeat('xxxxxxxxxx', 100000) from generate_series(1,50) i;
insert into co_nestloop_idxscan.bar values (1);

create index foo_id_idx on co_nestloop_idxscan.foo(id);

-- test with hash join
explain select f.id from co_nestloop_idxscan.foo f, co_nestloop_idxscan.bar b where f.id = b.id;
select f.id from co_nestloop_idxscan.foo f, co_nestloop_idxscan.bar b where f.id = b.id;

-- test with nested loop join
set optimizer_enable_hashjoin = off;
set enable_hashjoin=off;
set enable_nestloop=on;
explain select f.id from co_nestloop_idxscan.foo f, co_nestloop_idxscan.bar b where f.id = b.id;
select f.id from co_nestloop_idxscan.foo f, co_nestloop_idxscan.bar b where f.id = b.id;

-- test with nested loop join and index scan
set enable_seqscan = off;
-- start_ignore
-- Known_opt_diff: OPT-929
-- end_ignore
explain select f.id from co_nestloop_idxscan.bar b, co_nestloop_idxscan.foo f where f.id = b.id;
select f.id from co_nestloop_idxscan.foo f, co_nestloop_idxscan.bar b where f.id = b.id;
set optimizer_enable_hashjoin = on;

-- test with Nested Loop Semi Join for AO/AOCS freed bitmap state get re-init.
-- Make sure each bitmap index scan contains more than 1 matched tuples,
-- to make sure rescan frees bitmap state in BitmapHeapScanState. Since if only
-- 1 tuple matched, the bitmap state in BitmapHeapScanState always get re-init
-- when read all matched tuples.
insert into co_nestloop_idxscan.foo select i%10, repeat('xxxxxxxxxx', 100000) from generate_series(1,20) i;

-- Fill enouth tuples on same segment for the outer relation in next loop join
-- to make sure rescan get called for inner plan.
insert into co_nestloop_idxscan.bar values (1);

-- turn off the optimizer since we can not make the orca generate the same plan with planner.
set optimizer = off;
-- The outher plan of the Nested Loop Semi Join should be Seq Scan on bar b.
-- The inner plain should be a Bitmap Heap Scan on foo f.
-- So the Bitmap Heap Scan will call ExecReScanBitmapHeapScan for new outer slot.
explain select b.id from co_nestloop_idxscan.bar b where b.id in (select f.id from co_nestloop_idxscan.foo f where f.id in (1, 2, 3, 4, 5, 6));
select b.id from co_nestloop_idxscan.bar b where b.id in (select f.id from co_nestloop_idxscan.foo f where f.id in (1, 2, 3, 4, 5, 6));

reset optimizer;
drop schema co_nestloop_idxscan cascade;
