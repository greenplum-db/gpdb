-- @gucs gp_create_table_random_default_distribution=off
\echo -- start_ignore
-- ignore this output
\c dumpglobaltest
\echo -- end_ignore

-- Check schema and role are correctly restored by GPDBRESTORE
\du globaltest_role

\dn globaltest_schema

-- drop parttest_dump database
\echo -- start_ignore
-- ignore this output
\c template1
drop schema if exists globaltest_schema;
drop database if exists dumpglobaltest;
drop role if exists globaltest_role;
\echo -- end_ignore

