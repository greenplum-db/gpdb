-- Test the optimizer_dml_constraints GUC, which forces GPORCA to fall back when there
-- are NULL or CHECK constraints on a table.

set optimizer_trace_fallback = on;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);

set optimizer_dml_constraints = off;
explain insert into constr_tab values (1,2,3);

set optimizer_dml_constraints=on;
explain insert into constr_tab values (1,2,3);

-- The remaining tests require a row in the table.
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_dml_constraints=off;

explain update constr_tab set a = 10;
explain update constr_tab set b = 10;

set optimizer_dml_constraints=on;
explain update constr_tab set b = 10;

-- Same, with NOT NULL constraint.
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_dml_constraints=off;
explain update constr_tab set a = 10;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int NOT NULL, c int NOT NULL, d int NOT NULL) DISTRIBUTED BY (a,b);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_dml_constraints=off;
explain update constr_tab set b = 10;

DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int, b int, c int, d int) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);

set optimizer_dml_constraints=off;
explain update constr_tab set a = 10;
