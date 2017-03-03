-- @gucs gp_create_table_random_default_distribution=off
\d+ co_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part01%' order by oid;
\d+ co_part02
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part02%' order by oid;
\d+ co_part03
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part03%' order by oid;
\d+ co_part04
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part04%' order by oid;
\d+ co_part05
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part05%' order by oid;
\d+ co_part06
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part06%' order by oid;
\d+ co_part07
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part07%' order by oid;
\d+ co_part08
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part08%' order by oid;
\d+ co_part09
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part09%' order by oid;
\d+ co_part10
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part10%' order by oid;
\d+ co_part11
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part11%' order by oid;
