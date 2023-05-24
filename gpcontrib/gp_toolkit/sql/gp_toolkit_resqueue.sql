-- Tests for the functions and views in gp_toolkit that use resource queue.
-- gp_resource_manager=queue need to be set for this test to run as expected.

-- Create an empty database to test in, because some of the gp_toolkit views
-- are really slow, when there are a lot of objects in the database.
create database toolkit_testdb;
\c toolkit_testdb

create role toolkit_admin superuser createdb;
create role toolkit_user1 login;


-- Test Resource Queue views


-- GP Resource Queue Activity
select * from gp_toolkit.gp_resq_activity;

-- GP Resource Queue Activity by Queue
-- There is no resource queue, so should be empty
select * from gp_toolkit.gp_resq_activity_by_queue;

-- gp_resq_role
select * from gp_toolkit.gp_resq_role where rrrolname like 'toolkit%';

-- gp_locks_on_resqueue
-- Should be empty because there is no one in the queue
select * from gp_toolkit.gp_locks_on_resqueue;

-- GP Resource Queue Activity for User
set session authorization toolkit_user1;

select resqname, resqstatus from gp_toolkit.gp_resq_activity where resqname='pg_default';
reset session authorization;
-- should be empty because the sql is completed
select * from gp_toolkit.gp_resq_activity where resqrole = 'toolkit_user1';

-- gp_pgdatabase_invalid
-- Should be empty unless there is failure in the segment, it's a view from gp_pgdatabase
select * from gp_toolkit.gp_pgdatabase_invalid;

-- Test that the statistics on resource queue usage are properly updated and
-- reflected in the pg_stat_resqueues view
set stats_queue_level=on;
create resource queue q with (active_statements = 10);
create user resqueuetest with resource queue q;
set role resqueuetest;
select 1;
select n_queries_exec from pg_stat_resqueues where queuename = 'q';
reset role;
drop role resqueuetest;
drop resource queue q;

\c contrib_regression
drop database toolkit_testdb;
drop role toolkit_user1;
drop role toolkit_admin;
