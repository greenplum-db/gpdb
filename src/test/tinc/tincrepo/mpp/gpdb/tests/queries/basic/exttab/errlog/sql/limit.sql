DROP EXTERNAL TABLE IF EXISTS exttab_limit_1 cascade;
DROP EXTERNAL TABLE IF EXISTS exttab_limit_2 cascade;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_limit_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_limit_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_limit_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_limit_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_limit_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_limit_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: LIMIT queries without segment reject limit reached
-- Note that even though we use exttab_limit_2 here , the LIMIT 3 will not throw a segment reject limit error
-- order 0
with cte1 as 
(
SELECT e1.i, e2.j FROM exttab_limit_1 e1, exttab_limit_1 e2
WHERE e1.i = e2.i LIMIT 5
)
SELECT * FROM cte1, exttab_limit_2 e3 where cte1.i = e3.i LIMIT 3;

SELECT count(*) FROM gp_read_error_log('exttab_limit_1');


SELECT gp_truncate_error_log('exttab_limit_1');
SELECT gp_truncate_error_log('exttab_limit_2');


-- Note that even though we use exttab_limit_2 here , the LIMIT 3 will not throw a segment reject limit error
SELECT * FROM
(
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 5)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 3)
) FOO
LIMIT 5;

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_limit_1');
SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_limit_2');


-- Test: LIMIT queries with segment reject limit reached
SELECT gp_truncate_error_log('exttab_limit_1');
SELECT gp_truncate_error_log('exttab_limit_2');


-- order 0
with cte1 as 
(
SELECT e1.i, e2.j FROM exttab_limit_1 e1, exttab_limit_1 e2
WHERE e1.i = e2.i LIMIT 3
)
SELECT * FROM cte1, exttab_limit_2 e3 where cte1.i = e3.i LIMIT 5;

SELECT count(*) > 0 FROM gp_read_error_log('exttab_limit_2');


SELECT gp_truncate_error_log('exttab_limit_1');
SELECT gp_truncate_error_log('exttab_limit_2');

SELECT * FROM
(
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_1 LIMIT 3)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 5)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 5)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 5)
UNION
(SELECT * FROM exttab_limit_2 LIMIT 5)
) FOO
LIMIT 5;

SELECT count(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_limit_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_limit_2')
) FOO;


-- This query will materialize exttab_limit_2 completely even if LIMIT is just 3 and hence will throw segment reject limit reached
SELECT gp_truncate_error_log('exttab_limit_1');
SELECT gp_truncate_error_log('exttab_limit_2');

select * from exttab_limit_1 e1, exttab_limit_2 e2 where e1.i = e2.i LIMIT 3;


SELECT count(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_limit_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_limit_2')
) FOO;
