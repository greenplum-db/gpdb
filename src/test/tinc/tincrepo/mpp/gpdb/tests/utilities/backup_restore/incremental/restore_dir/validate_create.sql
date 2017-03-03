-- @gucs gp_create_table_random_default_distribution=off
\d+ ib_heap_01
\d+ ib_heap_schema.ib_heap_01
\d+ ib_heap_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'ib_heap_part01%' order by oid;
\d+ ib_ao_01
\d+ ib_ao_02
\d+ ib_ao_03
select oid::regclass, relkind, relstorage from pg_class where relname like 'ib_ao_03%' order by oid;
\d+ ib_ao_03_1_prt_others_2_prt_sub4
\d+ ib_ao_schema.ib_ao_01
\d+ ib_co_01
\d+ ib_co_02
\d+ ib_co_04
\d+ ib_co_05
select oid::regclass, relkind, relstorage from pg_class where relname like 'ib_co_05%' order by oid;
\d+ ib_co_05_1_prt_p1_2_prt_sub2
\d+ ib_co_03
select oid::regclass, relkind, relstorage from pg_class where relname like 'ib_co_03%' order by oid;
\d+ ib_co_03_1_prt_p1_2_prt_sub4
\d+ ib_co_schema.ib_co_01
\d+ co_table19
\d+ co_table20
\d+ ao_table19
\d+ ao_table20
\d+ aoschema1.ao_table22
\d+ ao_table16
