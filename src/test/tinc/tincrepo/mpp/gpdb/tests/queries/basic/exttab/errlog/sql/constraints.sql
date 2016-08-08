-- Test 1: Insert into another table with unique constraints
DROP EXTERNAL TABLE IF EXISTS exttab_constraints_1 cascade;

-- Generate the file
\! python @script@ 10 1 > @data_dir@/exttab_constraints_1.tbl
-- Generate duplicates
\! python @script@ 10 1 >> @data_dir@/exttab_constraints_1.tbl

CREATE EXTERNAL TABLE exttab_constraints_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_constraints_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Empty error log
SELECT * FROM gp_read_error_log('exttab_constraints_1');

-- Should not error out
SELECT COUNT(*) FROM exttab_constraints_1;

-- Error log should have a couple of rows
SELECT COUNT(*) from gp_read_error_log('exttab_constraints_1');

DROP TABLE IF EXISTS exttab_constraints_insert_1;

CREATE TABLE exttab_constraints_insert_1 (LIKE exttab_constraints_1) distributed by (i);

ALTER TABLE exttab_constraints_insert_1 ADD CONSTRAINT exttab_uniq_constraint_1 UNIQUE (j);

-- This should fail
select gp_truncate_error_log('exttab_constraints_1');

INSERT INTO exttab_constraints_insert_1 SELECT * FROM exttab_constraints_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_constraints_1');


-- Test: Unique index constraint
DROP TABLE IF EXISTS exttab_constraints_insert_2;

CREATE TABLE exttab_constraints_insert_2 (LIKE exttab_constraints_1) distributed by (i);

CREATE UNIQUE INDEX exttab_uniq_idx ON exttab_constraints_insert_2 (j);

-- This should fail
select gp_truncate_error_log('exttab_constraints_1');

INSERT INTO exttab_constraints_insert_2 SELECT * FROM exttab_constraints_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_constraints_1');
