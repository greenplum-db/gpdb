DROP EXTERNAL TABLE IF EXISTS exttab_subq_1;
DROP EXTERNAL TABLE IF EXISTS exttab_subq_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_subq_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_subq_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_subq_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_subq_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_subq_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_subq_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: subqueries reaching segment reject limit
TRUNCATE TABLE exttab_subq_err;

SELECT sum(distinct e1.i), sum(distinct e2.i), e1.j FROM

(SELECT i, j FROM exttab_subq_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subq_2 WHERE i < 10) e2

group by e1.j;

SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_subq_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subq_2')
) FOO;



SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');


SELECT sum(distinct e1.i), sum(distinct e2.i), e1.j FROM

(SELECT i, j FROM exttab_subq_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subq_1 WHERE i < 10) e2

group by e1.j
HAVING sum(distinct e1.i) > (SELECT max(i) FROM exttab_subq_2);


SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_subq_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subq_2')
) FOO;


-- CSQ
SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT e1.i , e1.j FROM
exttab_subq_1 e1, exttab_subq_1 e2
WHERE e1.j = e2.j and 
e1.i + 1 IN ( SELECT i from exttab_subq_2 WHERE i <= e1.i);

SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_subq_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subq_2')
) FOO;



SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT ( SELECT i FROM exttab_subq_2 WHERE i <= e1.i) as i, e1.j
FROM exttab_subq_2 e1, exttab_subq_1 e2
WHERE e1.i = e2.i;

SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_subq_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subq_2')
) FOO;

-- Test: subqueries without reaching segment reject limit
SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT sum(distinct e1.i), sum(distinct e2.i), e1.j FROM

(SELECT i, j FROM exttab_subq_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subq_1 WHERE i < 10) e2

group by e1.j order by 3,2,1;

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_subq_1');


SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT sum(distinct e1.i), sum(distinct e2.i), e1.j FROM

(SELECT i, j FROM exttab_subq_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subq_1 WHERE i < 10) e2

group by e1.j
HAVING sum(distinct e1.i) > (SELECT max(i) FROM exttab_subq_1);

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_subq_1');


SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT e1.i , e1.j FROM
exttab_subq_1 e1, exttab_subq_1 e2
WHERE e1.j = e2.j and 
e1.i + 1 IN ( SELECT i from exttab_subq_1 WHERE i <= e1.i);

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_subq_1');

SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

SELECT ( SELECT i FROM exttab_subq_1 WHERE i <= e1.i) as i, e1.j
FROM exttab_subq_1 e1, exttab_subq_1 e2
WHERE e1.i = e2.i;

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_subq_1');


-- Test: CTAS with subqueries and segment reject limit reached
DROP TABLE IF EXISTS exttab_subq_ctas_1;

SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

CREATE TABLE exttab_subq_ctas_1 AS
SELECT e1.i , e1.j FROM
exttab_subq_1 e1, exttab_subq_1 e2
WHERE e1.j = e2.j and 
e1.i + 1 IN ( SELECT i from exttab_subq_2 WHERE i <= e1.i);

-- should error out
SELECT * FROM exttab_subq_ctas_1;

SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_subq_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subq_2')
) FOO;

-- Test: CTAS with subqueries and segment reject within limits
DROP TABLE IF EXISTS exttab_subq_ctas_2;

SELECT gp_truncate_error_log('exttab_subq_1');
SELECT gp_truncate_error_log('exttab_subq_2');

CREATE TABLE exttab_subq_ctas_2 AS
SELECT e1.i , e1.j FROM
exttab_subq_1 e1, exttab_subq_1 e2
WHERE e1.j = e2.j and 
e1.i + 1 IN ( SELECT i from exttab_subq_1 WHERE i <= e1.i);

-- should not error out
SELECT * FROM exttab_subq_ctas_2;

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_subq_1');





 




