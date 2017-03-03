-- @gucs gp_create_table_random_default_distribution=off
\d+ ao_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part01%' order by oid;
\d+ ao_part02
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part02%' order by oid;
\d+ ao_part03
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part03%' order by oid;
\d+ ao_part04
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part04%' order by oid;
\d+ ao_part05
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part05%' order by oid;
\d+ ao_part06
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part06%' order by oid;
\d+ ao_part07
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part07%' order by oid;
\d+ ao_part08
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part08%' order by oid;
\d+ ao_part09
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part09%' order by oid;
\d+ ao_part10
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part10%' order by oid;
\d+ ao_part11
select oid::regclass, relkind, relstorage from pg_class where relname like 'ao_part11%' order by oid;
