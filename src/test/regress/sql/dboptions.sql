--
-- Test create/alter database options
--

-- Test CONNECTION LIMIT
create database limitdb connection limit 1;
select -1 as gp_segment_id, datconnlimit from pg_database where datname='limitdb'
union
select gp_segment_id, datconnlimit from gp_dist_random('pg_database') where datname='limitdb'
order by gp_segment_id;

alter database limitdb connection limit 2;
select -1 as gp_segment_id, datconnlimit from pg_database where datname='limitdb'
union
select gp_segment_id, datconnlimit from gp_dist_random('pg_database') where datname='limitdb'
order by gp_segment_id;

alter database limitdb with connection limit 0;

-- create a regular user as superusers are exempt from limits
create user connlimit_test_user;
-- add superuser to pg_hba.conf
\! echo "local    all         connlimit_test_user         trust" >> $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
select pg_reload_conf();

-- should fail, because the connection limit is 0
\! psql limitdb -c "select 'connected'" -U connlimit_test_user

-- Test ALLOW_CONNECTIONS
create database limitdb2 allow_connections = true;
select -1 as gp_segment_id, datconnlimit, datallowconn from pg_database where datname='limitdb2'
union
select gp_segment_id, datconnlimit, datallowconn from gp_dist_random('pg_database') where datname='limitdb2'
order by gp_segment_id;

alter database limitdb2 with allow_connections = false;
select -1 as gp_segment_id, datconnlimit, datallowconn from pg_database where datname='limitdb2'
union
select gp_segment_id, datconnlimit, datallowconn from gp_dist_random('pg_database') where datname='limitdb2'
order by gp_segment_id;

-- should fail, as we have disallowed connections
\! psql limitdb2 -c "select 'connected'" -U connlimit_test_user

-- Test IS_TEMPLATE
create database templatedb is_template=true;
select -1 as gp_segment_id, datistemplate from pg_database where datname = 'templatedb'
union
select gp_segment_id, datistemplate from gp_dist_random('pg_database') where datname = 'templatedb'
order by gp_segment_id;

\c templatedb
create table templatedb_table(i int);

\c regression
create database copieddb template templatedb;

\c copieddb
-- check that the table is carried over from the template
\d templatedb_table
\c regression

-- cannot drop a template database
drop database templatedb;

alter database templatedb with is_template=false;
select -1 as gp_segment_id, datistemplate from pg_database where datname = 'templatedb'
union
select gp_segment_id, datistemplate from gp_dist_random('pg_database') where datname = 'templatedb'
order by gp_segment_id;

-- Test ALTER DATABASE with funny characters. (There used to be a quoting
-- bug in dispatching ALTER DATABASE .. CONNECTION LIMIT.)
alter database limitdb rename to "limit_evil_'""_db";
alter database "limit_evil_'""_db" connection limit 3;
select -1 as gp_segment_id, datconnlimit from pg_database where datname like 'limit%db'
union
select gp_segment_id, datconnlimit from gp_dist_random('pg_database') where datname like 'limit%db'
order by gp_segment_id;

-- re-allow connections to avoid downstream pg_upgrade --check test error
alter database limitdb2 with allow_connections = true;
-- remove rule from pg_hba.conf for connlimit_test_user
\! sed -i '$ d' $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
select pg_reload_conf();
