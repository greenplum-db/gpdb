-- @product_version gpdb: [4.3.99-]
DROP EXTERNAL TABLE IF EXISTS exttab_cte_1;
DROP EXTERNAL TABLE IF EXISTS exttab_cte_2;

DROP TABLE IF EXISTS exttab_cte_err;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_cte_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_cte_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_cte_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_cte_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_cte_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_cte_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: CTE with segment reject limit reached
with cte1 as 
(
SELECT e1.i, e2.j FROM exttab_cte_2 e1, exttab_cte_1 e2
WHERE e1.i = e2.i
)
SELECT * FROM cte1, exttab_cte_2 e3 where cte1.i = e3.i;

select relname, filename, linenum, bytenum, errmsg from gp_read_error_log('exttab_cte_2')
UNION
select relname, filename, linenum, bytenum, errmsg from gp_read_error_log('exttab_cte_1')
order by linenum;

-- Test: CTE without segment reject limit exceeded
select gp_truncate_error_log('exttab_cte_1');
select gp_truncate_error_log('exttab_cte_2');


with cte1 as
(
SELECT e1.i, e2.j FROM exttab_cte_1 e1, exttab_cte_1 e2 WHERE e1.i = e2.i AND e1.i > 5
UNION
SELECT e1.i, e2.j FROM exttab_cte_1 e1, exttab_cte_1 e2 WHERE e1.i = e2.i AND e1.i < 7
)
SELECT  cte1.i , cte1.j
FROM cte1, exttab_cte_1 e3 where cte1.i = e3.i and e3.i = ( select max(cte1.i) FROM cte1);

select relname, filename, linenum, bytenum, errmsg from gp_read_error_log('exttab_cte_1') order by linenum;

-- Test: CTE insert into with segment reject limit exceeded
DROP TABLE IF EXISTS exttab_cte_insert_1;

CREATE TABLE exttab_cte_insert_1 (LIKE exttab_cte_1);

SELECT gp_truncate_error_log('exttab_cte_1');

INSERT INTO exttab_cte_insert_1
with cte1 as 
(
SELECT e1.i as c1, e2.i as c2, e1.j as c3, e2.j as c4 FROM exttab_cte_1 e1, exttab_cte_1 e2, exttab_cte_2 e3 
WHERE e1.i = e2.i and e1.i = e3.i
)
SELECT (SELECT max(c1) FROM cte1) as max_c1,
(SELECT max(c2) FROM cte1) as max_c2;

SELECT * FROM exttab_cte_insert_1;

SELECT count(*) > 0 FROM gp_read_error_log('exttab_cte_1');
SELECT count(*) > 0 FROM gp_read_error_log('exttab_cte_2');


-- Test: CTE insert into without segment reject limit exceeded
SELECT gp_truncate_error_log('exttab_cte_1');
SELECT gp_truncate_error_log('exttab_cte_2');

INSERT INTO exttab_cte_insert_1
with cte1 as 
(
SELECT e1.i as c1, e2.i as c2, e1.j as c3, e2.j as c4 FROM exttab_cte_1 e1, exttab_cte_1 e2, exttab_cte_1 e3 
WHERE e1.i = e2.i and e1.i = e3.i
)
SELECT (SELECT max(c1) FROM cte1) as max_c1,
(SELECT max(c2) FROM cte1) as max_c2;

SELECT * FROM exttab_cte_insert_1;

SELECT count(*) > 0 FROM gp_read_error_log('exttab_cte_1');


-- Test: Error tables through shared scans
SET gp_cte_sharing = True;

-- Segment reject limit exceeded within shared scan
SELECT gp_truncate_error_log('exttab_cte_1');
SELECT gp_truncate_error_log('exttab_cte_2');


with cte1 as 
(
SELECT e1.i as c1, e2.i as c2, e1.j as c3, e2.j as c4 FROM exttab_cte_1 e1, exttab_cte_1 e2, exttab_cte_2 e3 
WHERE e1.i = e2.i and e1.i = e3.i
)
SELECT COUNT(*) from cte1 c1, cte1 c2;

SELECT count(*) FROM gp_read_error_log('exttab_cte_1');
SELECT count(*) FROM gp_read_error_log('exttab_cte_2');

-- Segment reject limit not exceeded within shared scan
SELECT gp_truncate_error_log('exttab_cte_1');
SELECT gp_truncate_error_log('exttab_cte_2');

with cte1 as 
(
SELECT e1.i as c1, e2.i as c2, e1.j as c3, e2.j as c4 FROM exttab_cte_1 e1, exttab_cte_1 e2, exttab_cte_1 e3 
WHERE e1.i = e2.i and e1.i = e3.i
)
SELECT COUNT(*) from cte1 c1, cte1 c2;

SELECT count(*) FROM gp_read_error_log('exttab_cte_1');
SELECT count(*) FROM gp_read_error_log('exttab_cte_2');

