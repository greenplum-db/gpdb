-- Test 1: scans in union queries with seg reject limit reached
DROP EXTERNAL TABLE IF EXISTS exttab_union_1;
DROP EXTERNAL TABLE IF EXISTS exttab_union_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_union_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_union_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_union_2.tbl

-- reaches reject limit
CREATE EXTERNAL TABLE exttab_union_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: Should error out as exttab_union_2 would reach it's reject limit
SELECT * FROM
(
SELECT * FROM exttab_union_1
UNION
SELECT * FROM exttab_union_2
UNION
SELECT * FROM exttab_union_1
UNION
SELECT * FROM exttab_union_2
UNION
SELECT * FROM exttab_union_1
UNION
SELECT * FROM exttab_union_2
UNION
SELECT * FROM exttab_union_1
UNION
SELECT * FROM exttab_union_2
) FOO
order by FOO.i;

-- Error table count
select count(*) > 0 from
(
SELECT * FROM gp_read_error_log('exttab_union_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_union_2')
) FOO;


-- Test: Insert into another table, with and without segment reject limits being reached
DROP TABLE IF EXISTS exttab_union_insert_1;

CREATE TABLE exttab_union_insert_1 (LIKE exttab_union_1);

SELECT gp_truncate_error_log('exttab_union_1');
SELECT gp_truncate_error_log('exttab_union_2');

insert into exttab_union_insert_1
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i;

-- should return 0 rows
SELECT * from exttab_union_insert_1;

-- Error table count, should have more than 0 rows, the total number is non-deterministic
select count(*) > 0 from 
(
SELECT * FROM gp_read_error_log('exttab_union_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_union_2')
) FOO;


SELECT gp_truncate_error_log('exttab_union_1');
SELECT gp_truncate_error_log('exttab_union_2');

-- should not error out as exttab_union_1 will not reach segment reject limit
insert into exttab_union_insert_1
SELECT e1.i, e2.j from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i
UNION
SELECT e1.i, e2.j from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i
UNION
SELECT e1.i, e2.j from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i;

-- should return the right result
SELECT * from exttab_union_insert_1 order by i;

-- Error table count
select count(*) > 0 FROM gp_read_error_log('exttab_union_1');

-- Test: CTAS, should error out with segment limit reached
DROP TABLE IF EXISTS exttab_union_ctas_1;
DROP TABLE IF EXISTS exttab_union_ctas_2;

SELECT gp_truncate_error_log('exttab_union_1');
SELECT gp_truncate_error_log('exttab_union_2');


CREATE TABLE exttab_union_ctas_1 as
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_union_2 e1 INNER JOIN exttab_union_2 e2 ON e1.i = e2.i;

-- should error out , table should not exist
SELECT * from exttab_union_ctas_1;

-- Error table count, should have more than 0 rows, the total number is non-deterministic
select count(*) > 0 from gp_read_error_log('exttab_union_2');

-- should not error out as exttab_union_1 will not reach segment reject limit
SELECT gp_truncate_error_log('exttab_union_1');
SELECT gp_truncate_error_log('exttab_union_2');

create table exttab_union_ctas_2 as
SELECT e1.i, count(e2.j) from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i group by e1.i
UNION
SELECT e1.i, count(e2.j) from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i group by e1.i
UNION
SELECT e1.i, count(e2.j) from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i group by e1.i
UNION
SELECT e1.i, count(e2.j) from exttab_union_1 e1 INNER JOIN exttab_union_1 e2 ON e1.i = e2.i group by e1.i;

-- Should contain results
SELECT * FROM exttab_union_ctas_2 order by i;

-- There should be error rows
SELECT count(*) > 0 FROM gp_read_error_log('exttab_union_1');
