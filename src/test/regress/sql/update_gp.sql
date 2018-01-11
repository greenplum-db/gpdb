-- Test DELETE and UPDATE on an inherited table.
-- The special aspect of this table is that the inherited table has
-- a different distribution key. 'p' table's distribution key matches
-- that of 'r', but 'p2's doesn't. Test that the planner adds a Motion
-- node correctly for p2.
create table todelete (a int) distributed by (a);
create table parent (a int, b int, c int) distributed by (a);
create table child (a int, b int, c int) inherits (parent) distributed by (b);

insert into parent select g, g, g from generate_series(1,5) g;
insert into child select g, g, g from generate_series(6,10) g;

insert into todelete select generate_series(3,4);

delete from parent using todelete where parent.a = todelete.a;

insert into todelete select generate_series(5,7);

update parent set c=c+100 from todelete where parent.a = todelete.a;

select * from parent;

drop table todelete;
drop table child;
drop table parent;

-- This is similar to the above, but with a partitioned table (which is
-- implemented by inheritance) rather than an explicitly inherited table.
-- The scans on some of the partitions degenerate into Result nodes with
-- False one-time filter, which don't need a Motion node.
create table todelete (a int, b int) distributed by (a);
create table target (a int, b int, c int)
        distributed by (a)
        partition by range (c) (start(1) end(5) every(1), default partition extra);

insert into todelete select g, g % 4 from generate_series(1, 10) g;
insert into target select g, 0, 3 from generate_series(1, 5) g;
insert into target select g, 0, 1 from generate_series(1, 5) g;

delete from target where c = 3 and a in (select b from todelete);

insert into todelete values (1, 5);

update target set b=target.b+100 where c = 3 and a in (select b from todelete);

select * from target;

-- Also test an update with a qual that doesn't match any partition. The
-- Append degenerates into a dummy Result with false One-Time Filter.
alter table target drop default partition;
update target set b = 10 where c = 10;

drop table todelete;
drop table target;
--
-- Explicit Distribution motion must be added if any of the child nodes
-- contains any motion excluding the motions in initplans.
-- These test cases and expectation are applicable for GPDB planner not for ORCA.
--
SET gp_autostats_mode = NONE;
CREATE TABLE keo1 ( user_vie_project_code_pk character varying(24), user_vie_fiscal_year_period_sk character varying(24), user_vie_act_cntr_marg_cum character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo1 VALUES ('1', '1', '1');

CREATE TABLE keo2 ( projects_pk character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo2 VALUES ('1');

CREATE TABLE keo3 ( sky_per character varying(24), bky_per character varying(24)) DISTRIBUTED BY (sky_per);
INSERT INTO keo3 VALUES ('1', '1');

CREATE TABLE keo4 ( keo_para_required_period character varying(6), keo_para_budget_date character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo4 VALUES ('1', '1');
-- Explicit Redistribution motion should be added in case of GPDB Planner (test case not applicable for ORCA)
EXPLAIN UPDATE keo1 SET user_vie_act_cntr_marg_cum = 234.682 FROM
    ( SELECT a.user_vie_project_code_pk FROM keo1 a INNER JOIN keo2 b 
        ON b.projects_pk=a.user_vie_project_code_pk
        WHERE a.user_vie_fiscal_year_period_sk =
          (SELECT MAX (sky_per) FROM keo3 WHERE bky_per =
             (SELECT keo4.keo_para_required_period FROM keo4 WHERE keo_para_budget_date =
                (SELECT min (keo4.keo_para_budget_date) FROM keo4)))
    ) t1
WHERE t1.user_vie_project_code_pk = keo1.user_vie_project_code_pk;
UPDATE keo1 SET user_vie_act_cntr_marg_cum = 234.682 FROM
    ( SELECT a.user_vie_project_code_pk FROM keo1 a INNER JOIN keo2 b 
        ON b.projects_pk=a.user_vie_project_code_pk
        WHERE a.user_vie_fiscal_year_period_sk =
          (SELECT MAX (sky_per) FROM keo3 WHERE bky_per =
             (SELECT keo4.keo_para_required_period FROM keo4 WHERE keo_para_budget_date =
                (SELECT min (keo4.keo_para_budget_date) FROM keo4)))
    ) t1
WHERE t1.user_vie_project_code_pk = keo1.user_vie_project_code_pk;
SELECT user_vie_act_cntr_marg_cum FROM keo1;

-- Explicit Redistribution motion should not be added in case of GPDB Planner (test case not applicable to ORCA)
CREATE TABLE keo5 (x int, y int) DISTRIBUTED BY (x);
INSERT INTO keo5 VALUES (1,1);
EXPLAIN DELETE FROM keo5 WHERE x IN (SELECT x FROM keo5 WHERE EXISTS (SELECT x FROM keo5 WHERE x < 2));
DELETE FROM keo5 WHERE x IN (SELECT x FROM keo5 WHERE EXISTS (SELECT x FROM keo5 WHERE x < 2));
SELECT x FROM keo5;

RESET gp_autostats_mode;
DROP TABLE keo1;
DROP TABLE keo2;
DROP TABLE keo3;
DROP TABLE keo4;
DROP TABLE keo5;
