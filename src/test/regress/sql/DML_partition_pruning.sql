-- Test memory consumption for DML partition pruning for Postgres optimzier
-- Before this patch inheritance_planner build plan for each child relation
-- in spite of provided predicates and as a result, easily overcame protected
-- memory limit

CREATE TABLE t_part1 (
    key1 int, update_me int, dummy1 int, dummy2 int, dummy3 int, dummy4 int, dummy5 int, dummy6 int, dummy7 int,
    dummy8 int, dummy9 int, dummy10 int, dummy11 int, dummy12 int, dummy13 int, dummy14 int, dummy15 int,
    dummy16 int, dummy17 int, dummy18 int, dummy19 int, dummy20 int, dummy21 int, dummy22 int, dummy23 int,
    dummy24 int, dummy25 int, dummy26 int, dummy27 int, dummy28 int, dummy29 int, dummy30 int, dummy31 int,
    dummy32 int, dummy33 int, dummy34 int, dummy35 int, dummy36 int, dummy37 int, dummy38 int, dummy39 int,
    dummy40 int, dummy41 int, dummy42 int, dummy43 int, dummy44 int, dummy45 int, dummy46 int, dummy47 int,
    dummy48 int, dummy49 int, dummy50 int, dummy51 int, dummy52 int, dummy53 int, dummy54 int, dummy55 int,
    dummy56 int, dummy57 int, dummy58 int, dummy59 int, dummy60 int, dummy61 int, dummy62 int, dummy63 int,
    dummy64 int, dummy65 int, dummy67 int, dummy68 int, dummy69 int, dummy70 int, dummy71 int, dummy72 int,
    dummy73 int, dummy74 int, dummy75 int, dummy76 int, dummy77 int, dummy78 int, dummy79 int, dummy80 int,
    dummy81 int, dummy82 int, dummy83 int, dummy84 int
)
DISTRIBUTED BY (key1)
PARTITION BY RANGE (key1) (start (1) end (400) every (1));

set optimizer=off;

explain (costs off)
update t_part1 trg
set update_me = src.update_me
from (
      select
      r.key1,
      r.update_me as update_me,
      row_number() over() rn
      from t_part1 r
      where r.key1 = 2
) src
where  trg.key1 = src.key1
        and trg.key1 = 2;

DROP TABLE t_part1;
