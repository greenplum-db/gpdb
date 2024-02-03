-- Regression tests for prepareable statements
-- Force a generic plan to specifically test parameters
set plan_cache_mode=force_generic_plan;
SET optimizer_trace_fallback to on;

create schema qp_query_params;
set search_path=qp_query_params;

create table t1 (a int, b int);
insert into t1 select i, i from generate_series(1,4)i;
analyze t1;

CREATE TABLE part (
    a int ,
    b int,
    c text,
    d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
    start(1) end(6) every(2),
    default partition def);
insert into part select i,i,'abc',i*1.01 from generate_series(1,10)i;
analyze part;

-- Should simplify to false, Orca does not does not
PREPARE q1 as SELECT * from t1 where a=$1 and a!=$1;
explain (costs off) execute q1(3);
execute q1(3);
deallocate q1;

-- Should simplify to false, currently does not
PREPARE q1 as SELECT * from t1 where $1!=$1;
explain (costs off) execute q1(4);
execute q1(4);
deallocate q1;

-- Should perform static partition elimination, Orca does not
PREPARE q1 as SELECT * from part where a=$1;
explain (costs off) EXECUTE q1(2);
execute q1(2);

-- Ensure default partition is scanned
explain (costs off) EXECUTE q1(9);
execute q1(9);
deallocate q1;

-- Test multiple query params
PREPARE q1 as SELECT * from t1 where a=$1 and b=$2;
explain (costs off) execute q1(4,4);
execute q1(4,4);
deallocate q1;

-- Test param op param
PREPARE q1 as SELECT * from t1 where $1=$2;
explain (costs off) execute q1(4,5);
execute q1(4,5);
deallocate q1;

-- Should NOT produce a direct dispatch plan!
PREPARE q1 as SELECT * from t1 where a=$1;
explain (costs off) execute q1(5);
execute q1(5);
deallocate q1;

-- Test direct dispatch with delete. Explain doesn't show this, so we must verify results
PREPARE q1 as DELETE from t1 where a=$1;
explain (costs off) execute q1(1);
execute q1(1);
deallocate q1;
select count(*) from t1;

-- Test direct dispatch with insert. Explain doesn't show this, so we must verify results
PREPARE q1 as INSERT into t1 values ($1, $2);
explain (costs off) execute q1(1,3);
execute q1(1,3);
deallocate q1;
select count(*) from t1;

-- Test index with parameter
CREATE INDEX idx on t1(b);
PREPARE q1 as select * from t1 where b=$1;
explain (costs off) execute q1(3);
execute q1(3);
deallocate q1;
drop index idx;

-- Test limit with parameter
PREPARE q1 as select * from t1 order by a limit $1;
explain (costs off) execute q1(4);
execute q1(4);
deallocate q1;
