\c gptest;

set enable_seqscan=off;
set enable_mergejoin=on;
set enable_nestloop=off;
set enable_hashjoin=off;
set enable_bitmapscan=off;
set enable_indexscan=on;

--allow catalog modification
set allow_system_table_mods=dml;
update pg_class set reltuples=1 where relname='tt2';

--explain select tt1.x from tt2 full outer join tt1 on (tt1.x=tt2.z) where tt2.z=2;

select tt1.x from tt2 full outer join tt1 on (tt1.x=tt2.z) where tt2.z=2;
