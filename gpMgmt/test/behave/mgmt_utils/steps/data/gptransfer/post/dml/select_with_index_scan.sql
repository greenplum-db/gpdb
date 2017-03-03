\c gptest;

set enable_seqscan=off;
set enable_bitmapscan=off;
set enable_indexscan=on;

-- start_ignore
-- explain analyze select a, b from test where b=10 order by a desc;
-- end_ignore

select a, b from test where b=10 order by a desc;
