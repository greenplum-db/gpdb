-- https://gist.github.com/kainwen/df7c0d3149684e1256d31b5c39e02de7#file-gp7-sql
-- This test is included in https://github.com/greenplum-db/gpdb/issues/14982

-- The test creates an operator that performs absolute value comparisons of the input data.
-- The operator belongs to a customized btree opfamily 16433, which supports cross-type (20, 23) comparisons.

-- The test then creates two tables, t1 and t2.
-- Both are distributed by column a, and partitioned by column b.
-- t1.a is of type int, belonging to hash opfamily 1977, which supports cross-type (20, 21, 23) comparisons.
-- t2.a is of type int, belonging to hash opfamily 1977, which supports cross-type (20, 21, 23) comparisons.
-- t1.b is of type int4, belonging to btree opfamily 1976, which supports cross-type (20, 21, 23) comparisons.
-- t2.b is of type int8, belonging to btree opfamily 1976, which supports cross-type (20, 21, 23) comparisons.

-- As of now, ORCA doesn't consider partition key's opfamily when performing partition pruning
-- Internally, operator |=| of opfamily 16433 is treated as operator = of opfamily 1977 or 1976, and generated
-- incorrect hash join condition and unjustified partition pruning. This fix compares the operator to the
-- column's opfamily, thence deriving correct properties from the predicates.

-- greenplum
create schema partition_prune_opfamily;
set search_path=partition_prune_opfamily;
SET optimizer_trace_fallback=on;

-- start_ignore
create language plpython3u;
-- end_ignore

-- proc for int4
create function abseq4(a int4, b int4) returns bool as $$
return abs(a) == abs(b)
$$ language plpython3u strict immutable;

create function abslt4(a int4, b int4) returns bool as $$
return abs(a) < abs(b)
$$ language plpython3u strict immutable;

create function absle4(a int4, b int4) returns bool as $$
return abs(a) <= abs(b)
$$ language plpython3u strict immutable;

create function absgt4(a int4, b int4) returns bool as $$
return abs(a) > abs(b)
$$ language plpython3u strict immutable;

create function absge4(a int4, b int4) returns bool as $$
return abs(a) >= abs(b)
$$ language plpython3u strict immutable;

create function abscmp4(a int4, b int4) returns int as $$
if abs(a) < abs(b):
    return -1
if abs(a) > abs(b):
    return 1
return 0
$$ language plpython3u strict immutable;

-- procs for int8
create function abseq8(a int8, b int8) returns bool as $$
return abs(a) == abs(b)
$$ language plpython3u strict immutable;

create function abslt8(a int8, b int8) returns bool as $$
return abs(a) < abs(b)
$$ language plpython3u strict immutable;

create function absle8(a int8, b int8) returns bool as $$
return abs(a) <= abs(b)
$$ language plpython3u strict immutable;

create function absgt8(a int8, b int8) returns bool as $$
return abs(a) > abs(b)
$$ language plpython3u strict immutable;

create function absge8(a int8, b int8) returns bool as $$
return abs(a) >= abs(b)
$$ language plpython3u strict immutable;

create function abscmp8(a int8, b int8) returns int as $$
if abs(a) < abs(b):
    return -1
if abs(a) > abs(b):
    return 1
return 0
$$ language plpython3u strict immutable;

-- procs for cross type int4 int8
create function abseq48(a int4, b int8) returns bool as $$
return abs(a) == abs(b)
$$ language plpython3u strict immutable;

create function abslt48(a int4, b int8) returns bool as $$
return abs(a) < abs(b)
$$ language plpython3u strict immutable;

create function absle48(a int4, b int8) returns bool as $$
return abs(a) <= abs(b)
$$ language plpython3u strict immutable;

create function absgt48(a int4, b int8) returns bool as $$
return abs(a) > abs(b)
$$ language plpython3u strict immutable;

create function absge48(a int4, b int8) returns bool as $$
return abs(a) >= abs(b)
$$ language plpython3u strict immutable;

create function abscmp48(a int4, b int8) returns int as $$
if abs(a) < abs(b):
    return -1
if abs(a) > abs(b):
    return 1
return 0
$$ language plpython3u strict immutable;

--procs for cross type int8 int4
create function abseq84(a int8, b int4) returns bool as $$
return abs(a) == abs(b)
$$ language plpython3u strict immutable;

create function abslt84(a int8, b int4) returns bool as $$
return abs(a) < abs(b)
$$ language plpython3u strict immutable;

create function absle84(a int8, b int4) returns bool as $$
return abs(a) <= abs(b)
$$ language plpython3u strict immutable;

create function absgt84(a int8, b int4) returns bool as $$
return abs(a) > abs(b)
$$ language plpython3u strict immutable;

create function absge84(a int8, b int4) returns bool as $$
return abs(a) >= abs(b)
$$ language plpython3u strict immutable;

create function abscmp84(a int8, b int4) returns int as $$
if abs(a) < abs(b):
    return -1
if abs(a) > abs(b):
    return 1
return 0
$$ language plpython3u strict immutable;

-- |=|
create operator |=| (
  procedure = abseq4,
  leftarg = int4,
  rightarg = int4,
  commutator = |=|,
  merges);

create operator |=| (
  procedure = abseq8,
  leftarg = int8,
  rightarg = int8,
  commutator = |=|,
  merges);

create operator |=| (
  procedure = abseq48,
  leftarg = int4,
  rightarg = int8,
  commutator = |=|,
  merges);

create operator |=| (
  procedure = abseq84,
  leftarg = int8,
  rightarg = int4,
  commutator = |=|,
  merges);

-- |<|
create operator |<| (
  procedure = abslt4,
  leftarg = int4,
  rightarg = int4,
  commutator = |<|);

create operator |<| (
  procedure = abslt8,
  leftarg = int8,
  rightarg = int8,
  commutator = |<|);

create operator |<| (
  procedure = abslt48,
  leftarg = int4,
  rightarg = int8,
  commutator = |<|);

create operator |<| (
  procedure = abslt84,
  leftarg = int8,
  rightarg = int4,
  commutator = |<|);

-- |>|
create operator |>| (
  procedure = absgt4,
  leftarg = int4,
  rightarg = int4,
  commutator = |>|);

create operator |>| (
  procedure = absgt8,
  leftarg = int8,
  rightarg = int8,
  commutator = |>|);

create operator |>| (
  procedure = absgt48,
  leftarg = int4,
  rightarg = int8,
  commutator = |>|);

create operator |>| (
  procedure = absgt84,
  leftarg = int8,
  rightarg = int4,
  commutator = |>|);

-- |<=|
create operator |<=| (
  procedure = absle4,
  leftarg = int4,
  rightarg = int4,
  commutator = |<=|);

create operator |<=| (
  procedure = absle8,
  leftarg = int8,
  rightarg = int8,
  commutator = |<=|);

create operator |<=| (
  procedure = absle48,
  leftarg = int4,
  rightarg = int8,
  commutator = |<=|);
 
create operator |<=| (
  procedure = absle84,
  leftarg = int8,
  rightarg = int4,
  commutator = |<=|);

-- |>=|
create operator |>=| (
  procedure = absge4,
  leftarg = int4,
  rightarg = int4,
  commutator = |>=|);

create operator |>=| (
  procedure = absge8,
  leftarg = int8,
  rightarg = int8,
  commutator = |>=|);

create operator |>=| (
  procedure = absge48,
  leftarg = int4,
  rightarg = int8,
  commutator = |>=|);

create operator |>=| (
  procedure = absge84,
  leftarg = int8,
  rightarg = int4,
  commutator = |>=|);

--------

create operator family abs_int_ops using btree;

create operator class abs_int4_ops for type int4
  using btree FAMILY abs_int_ops  as
  operator 1 |<|,
  operator 3 |=|,
  operator 5 |>|,
  function 1 abscmp4(int4, int4);

create operator class abs_int8_ops for type int8
  using btree FAMILY abs_int_ops as
  operator 1 |<|,
  operator 3 |=|,
  operator 5 |>|,
  function 1 abscmp8(int8, int8);

alter operator family abs_int_ops using btree add
  -- cross-type comparisons int4 vs int8
  operator 1 |<| (int4, int8),
  operator 2 |<=| (int4, int8),
  operator 3 |=| (int4, int8),
  operator 4 |>=| (int4, int8),
  operator 5 |>| (int4, int8),
  function 1 abscmp48(int4, int8),
  
  -- cross-type comparisons int8 vs int4
  operator 1 |<| (int8, int4),
  operator 2 |<=| (int8, int4),
  operator 3 |=| (int8, int4),
  operator 4 |>=| (int8, int4),
  operator 5 |>| (int8, int4),
  function 1 abscmp84(int8, int4);

-- gpdb

-- start_ignore
DROP TABLE t1;
DROP TABLE t2;
-- end_ignore

create table t1 (a int, b int4)
partition by list (b)
(
  partition p1 values (1),
  partition p2 values (-1)
);

create table t2 (a int, b int8)
partition by list (b)
(
  partition p1 values (1),
  partition p2 values (-1)
);

insert into t1 values (1,1), (1,-1), (-1,1), (-1,-1);
insert into t2 values (1,1), (1,-1), (-1,1), (-1,-1);

explain select * from t1 where t1.b |=| 1;
select * from t1 where t1.b |=| 1;

explain select * from t1, t2 where t1.b = t2.b and t2.b |=| 1;
select * from t1, t2 where t1.b = t2.b and t2.b |=| 1;

explain select * from t1, t2 where t1.a = t2.a and t1.b |=| t2.b;
select * from t1, t2 where t1.a = t2.a and t1.b |=| t2.b;

explain select * from t1, t2 where t1.a = t2.a and t1.b |=| t2.b and t2.b = 1;
select * from t1, t2 where t1.a = t2.a and t1.b |=| t2.b and t2.b = 1;

-- CLEANUP

-- start_ignore
drop schema if exists partition_prune_opfamily cascade;
-- end_ignore
