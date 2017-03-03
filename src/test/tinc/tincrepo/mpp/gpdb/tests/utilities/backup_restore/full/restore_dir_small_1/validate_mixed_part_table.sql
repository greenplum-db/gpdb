-- @gucs gp_create_table_random_default_distribution=off
\d+ mixed_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part01%' order by oid;
\d+ mixed_part04
select oid::regclass, relkind, relstorage from pg_class where relname like 'mixed_part04%' order by oid;
