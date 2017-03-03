-- @gucs gp_create_table_random_default_distribution=off
\d+ mixed_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part01%' order by oid;
\d+ mixed_part02
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part02%' order by oid;
\d+ mixed_part03
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part03%' order by oid;
\d+ mixed_part04
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part04%' order by oid;
\d+ mixed_part05
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part05%' order by oid;
\d+ mixed_part06
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part06%' order by oid;
