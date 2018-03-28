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
