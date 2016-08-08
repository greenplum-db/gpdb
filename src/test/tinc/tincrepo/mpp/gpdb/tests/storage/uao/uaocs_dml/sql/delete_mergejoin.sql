-- @description Tests AO delete with a merge join
-- 

set enable_hashjoin=off;
set enable_nestloop=off;
set enable_seqscan=off;
set enable_bitmapscan=off;
DELETE FROM foo using BAR WHERE foo.a = bar.a AND foo.b = bar.b AND foo.c = bar.c AND foo.d = bar.d;
