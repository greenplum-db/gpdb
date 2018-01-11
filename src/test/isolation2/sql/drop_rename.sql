-- Test if ALTER RENAME followed by ANALYZE executed concurrently with
-- DROP does not introduce inconsistency between master and segments.
-- DROP should be blocked because of ALTER-ANALYZE.  After being
-- unblocked, DROP should lookup the old name again and fail with
-- relation does not exist error.

1:drop table if exists t1;
1:drop table if exists newt1;
1:create table t1 (a int, b text) distributed by (a);
1:insert into t1 select i, 'abc '||i from generate_series(1,10)i;
1:begin;
1:alter table t1 rename to newt1;
1:analyze newt1;
-- this drop should block to acquire AccessExclusive lock on t1's OID.
2&:drop table t1;
1:commit;
2<:
2:select count(*) from newt1;

-- DROP is executed concurrently with ALTER RENAME but not ANALYZE.
1:drop table if exists t2;
1:drop table if exists newt2;
1:create table t2 (a int, b text) distributed by (a);
1:insert into t2 select i, 'pqr '||i from generate_series(1,10)i;
1:begin;
1:alter table t2 rename to newt2;
2&:drop table t2;
1:commit;
2<:
2:select count(*) from newt2;

-- The same, but with DROP IF EXISTS. (We used to have a bug, where the DROP
-- command found and drop the relation in the segments, but not in master.)
1:drop table if exists t3;
1:create table t3 (a int, b text) distributed by (a);
1:insert into t3 select i, '123 '||i from generate_series(1,10)i;
1:begin;
1:alter table t3 rename to t3_new;
2&:drop table if exists t3;
1:commit;
2<:
2:select count(*) from t3;
2:select relname from pg_class where relname like 't3%';
2:select relname from gp_dist_random('pg_class') where relname like 't3%';

1:drop table if exists t3;
1:create table t3 (a int, b text) distributed by (a);
1:insert into t3 select i, '123 '||i from generate_series(1,10)i;
1:begin;
1:drop table t3;
2&:drop table if exists t3;
3&:drop table t3;
1:commit;
3<:
2<:
2:select count(*) from t3;
