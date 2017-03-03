-- @gucs gp_create_table_random_default_distribution=off
\d+ ao_compr01
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_compr02%' order by oid;
\d+ ao_compr02_1_prt_splita
\d+ ao_compr02_1_prt_splitb
\d+ ao_compr03_1_prt_2_2_prt_subothers
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_compr03%' order by oid;
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_compr04%' order by oid;
\d+ ao_compr04_1_prt_2_2_prt_a
\d+ ao_compr04_1_prt_2_2_prt_b
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_compr05%' order by oid;
\d+ co_compr01
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_compr02%' order by oid;
--\d+ co_compr02_1_prt_splita (MPP-17284)
--\d+ co_compr02_1_prt_splitb (MPP-17284)
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_compr03%' order by oid;
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_compr04%' order by oid;
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_compr05%' order by oid;
\d+ co_compr06

