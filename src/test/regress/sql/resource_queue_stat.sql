set stats_queue_level=on;

-- start_ignore
drop role resqueuetest;
drop resource queue q;
-- end_ignore

create resource queue q with (active_statements = 10);
create user resqueuetest with resource queue q;

set role resqueuetest;

select 1;
select 1;
select 1;

select pg_sleep(0.1);
-- will display q.n_queries_exec=4, and after the query it becomes 5
-- The query should sleep more than PGSTAT_STAT_INTERVAL first to ensure
-- the fresh resource queue stats are synced to stat files. 
select pg_sleep(0.6), queuename, n_queries_exec from pg_stat_resqueues where queuename = 'q';

-- drop the queue
reset role;
drop role resqueuetest;
drop resource queue q;

set stats_queue_level=on;

-- create the queue and test
create resource queue q with (active_statements = 10);
create user resqueuetest with resource queue q;

set role resqueuetest;

select 1;
select 1;
select 1;

select pg_sleep(0.1);
-- will display q.n_queries_exec=4, and after the query it becomes 5
-- The query should sleep more than PGSTAT_STAT_INTERVAL first to ensure
-- the fresh resource queue stats are synced to stat files. 
select pg_sleep(0.6), queuename, n_queries_exec from pg_stat_resqueues where queuename = 'q';

-- create another queue, do switch test
reset role;
set stats_queue_level=on;

-- start_ignore
drop role resqueuetest1;
drop resource queue q1;
-- end_ignore

create resource queue q1 with (active_statements = 10);
create user resqueuetest1 with resource queue q1;

-- now change the role
set role resqueuetest1;
select 1;
select 1;
select 1;

select pg_sleep(0.1);
-- will display q.n_queries_exec=5, q1.n_queries_exec=4
-- The query should sleep more than PGSTAT_STAT_INTERVAL first to ensure
-- the fresh resource queue stats are synced to stat files. 
select pg_sleep(0.6), queuename, n_queries_exec from pg_stat_resqueues where queuename = 'q1';
select pg_sleep(0.6), queuename, n_queries_exec from pg_stat_resqueues where queuename = 'q';

-- clean
reset role;
drop role resqueuetest;
drop resource queue q;
drop role resqueuetest1;
drop resource queue q1;
