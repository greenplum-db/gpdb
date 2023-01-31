-- start_ignore
CREATE SCHEMA qp_orca_fallback;
SET search_path to qp_orca_fallback;
-- end_ignore

-- Test the optimizer_enable_dml_constraints GUC, which forces GPORCA to fall back when there
-- are NULL or CHECK constraints on a table.

set optimizer_trace_fallback = on;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);

set optimizer_enable_dml_constraints = off;
explain insert into constr_tab values (1,2,3);

set optimizer_enable_dml_constraints=on;
explain insert into constr_tab values (1,2,3);

-- The remaining tests require a row in the table.
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_enable_dml_constraints=off;

explain update constr_tab set a = 10;
explain update constr_tab set b = 10;

set optimizer_enable_dml_constraints=on;
explain update constr_tab set b = 10;

-- Same, with NOT NULL constraint.
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_enable_dml_constraints=off;
explain update constr_tab set a = 10;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int NOT NULL, c int NOT NULL, d int NOT NULL) DISTRIBUTED BY (a,b);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_enable_dml_constraints=off;
explain update constr_tab set b = 10;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int, b int, c int, d int) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_enable_dml_constraints=off;
explain update constr_tab set a = 10;

-- Test ORCA fallback on "FROM ONLY"

CREATE TABLE homer (a int, b int, c int)
DISTRIBUTED BY (a)
PARTITION BY range(b)
    SUBPARTITION BY range(c)
        SUBPARTITION TEMPLATE (
            START(40) END(46) EVERY(3)
        )
(START(0) END(4) EVERY(2));

INSERT INTO homer VALUES (1,0,40),(2,1,43),(3,2,41),(4,3,44);

SELECT * FROM ONLY homer;

SELECT * FROM ONLY homer_1_prt_1;

UPDATE ONLY homer SET c = c + 1;
SELECT * FROM homer;

DELETE FROM ONLY homer WHERE a = 3;
SELECT * FROM homer;

-- ORCA should not fallback just because external tables are in FROM clause
-- start_ignore
CREATE TABLE heap_t1 (a int, b int) DISTRIBUTED BY (b);
CREATE EXTERNAL TABLE ext_table_no_fallback (a int, b int) LOCATION ('gpfdist://myhost:8080/test.csv') FORMAT 'CSV';
-- end_ignore
EXPLAIN SELECT * FROM ext_table_no_fallback;
EXPLAIN SELECT * FROM ONLY ext_table_no_fallback;
EXPLAIN INSERT INTO heap_t1 SELECT * FROM ONLY ext_table_no_fallback;

set optimizer_enable_dml=off;
EXPLAIN INSERT INTO homer VALUES (1,0,40),(2,1,43),(3,2,41),(4,3,44);
EXPLAIN UPDATE ONLY homer SET c = c + 1;
EXPLAIN DELETE FROM ONLY homer WHERE a = 3;
set optimizer_enable_dml=on;

create table foo(a int, b int);
insert into foo select i%100, i%100 from generate_series(1,10000)i;
analyze foo;
set optimizer_enable_hashagg = on;
set optimizer_enable_groupagg = on;
explain select count(*) from foo group by a;
set optimizer_enable_hashagg = off;
set optimizer_enable_groupagg = on;
explain select count(*) from foo group by a;
set optimizer_enable_hashagg = off;
set optimizer_enable_groupagg = off;
explain select count(*) from foo group by a;

-- Orca should fallback for RTE_TABLEFUNC RTE type
explain SELECT * FROM xmltable('/root' passing '' COLUMNS element text);

create table ext_part(a int) partition by list(a);
create table p1(a int);
create external web table p2_ext (like p1) EXECUTE 'cat something.txt' FORMAT 'TEXT';
alter table ext_part attach partition p1 for values in (1);
alter table ext_part attach partition p2_ext for values in (2);
explain insert into ext_part values (1);
explain delete from ext_part where a=1;
explain update ext_part set a=1;
-- start_ignore
-- FIXME: gpcheckcat fails due to mismatching distribution policy if this table isn't dropped
-- Keep this table around once this is fixed
drop table ext_part;
-- end_ignore
