-- @gucs gp_create_table_random_default_distribution=off
\d+ co_part01
select oid::regclass, relkind, relstorage from pg_class where relname like 'co_part01%' order by oid;
