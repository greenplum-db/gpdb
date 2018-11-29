drop table if exists t20;
drop table if exists t30;

create table t20 (c1 int, c2 int) distributed by (c1);
create table t30 (c1 int, c2 int) distributed by (c1);

-- c10, c11 simulate gpexpand's catalog lock protection
-- they will acquire the catalog lock in exclusive mode

-- c20, c30 simulate client sessions
-- they will acquire the catalog lock in shared mode

--
-- the catalog lock can not be acquired concurrently in exclusive mode
--

10: begin;
11: begin;

-- c10 acquired the catalog lock in exclusive mode
10: select gp_expand_lock_catalog();

-- c11 has to wait for c10
11&: select gp_expand_lock_catalog();

10: end;

-- c10 released the lock, c11 acquired it now
11<:
11: end;

--
-- client sessions do not block each other on catalog changes
--

20: begin;
30: begin;

-- c20 and c30 both acquired the catalog lock in shared mode
20: create table t21 (c1 int, c2 int) distributed by (c1);
30: create table t31 (c1 int, c2 int) distributed by (c1);

20: insert into t21 values (1,1);
30: insert into t31 values (1,1);

20: rollback;
30: rollback;

--
-- gpexpand must wait for in progress catalog changes to commit/rollback
--

10: begin;
20: begin;
30: begin;

-- c20 and c30 both acquired the catalog lock in shared mode
20: create table t21 (c1 int, c2 int) distributed by (c1);
30: create table t31 (c1 int, c2 int) distributed by (c1);

-- c10 can not acquire the lock in exclusive mode ...
10&: select gp_expand_lock_catalog();

20: insert into t21 values (1,1);
30: insert into t31 values (1,1);

20: rollback;
30: rollback;

-- ... until both c20 and c30 released it
10<:
10: end;

--
-- the catalog lock can be acquired in order
--

10: begin;
20: begin;
30: begin;

-- c20 acquired the catalog lock in shared mode
20: create table t21 (c1 int, c2 int) distributed by (c1);

-- c10 has to wait for c20
10&: select gp_expand_lock_catalog();

-- c30 can not acquire it already, even in shared mode
30: create table t31 (c1 int, c2 int) distributed by (c1);
30: rollback;

-- c20 can still make catalog changes
20: drop table t21;

20: rollback;

-- c20 released the lock, c10 acquired it now
10<:
10: end;

--
-- gpexpand does not block DMLs or readonly queries to catalogs
--

10: begin;
20: begin;
30: begin;

-- c10 acquired the catalog lock in exclusive mode
10: select gp_expand_lock_catalog();

-- c20 and c30 can still run DMLs
20: insert into t20 values (1,1);
20: select * from t20;
20: update t20 set c2=c1+1;
20: delete from t20;

30: insert into t30 values (1,1);
30: select * from t30;
30: update t30 set c2=c1+1;
30: delete from t30;

-- c20 and c30 can also run query catalogs
20: select relname from pg_class where oid='t20'::regclass;
30: select relname from pg_class where oid='t30'::regclass;

20: rollback;
30: rollback;

10: end;

--
-- catalog changes are disallowed when gpexpand is in progress
--

10: begin;

-- c20 has an old transaction
20: begin;

-- c10 acquired the catalog lock in exclusive mode
10: select gp_expand_lock_catalog();

-- c30 has a new transaction
30: begin;

-- neither c20 nor c30 are allowed to make catalog changes
20: create table t21 (c1 int, c2 int) distributed by (c1);
30: create table t31 (c1 int, c2 int) distributed by (c1);

20: rollback;
30: rollback;

10: end;
