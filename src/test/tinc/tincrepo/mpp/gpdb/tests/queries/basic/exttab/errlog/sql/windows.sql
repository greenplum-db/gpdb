-- Test 1: scans in window queries with and without seg reject limit reached
DROP EXTERNAL TABLE IF EXISTS exttab_windows_1;
DROP EXTERNAL TABLE IF EXISTS exttab_windows_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_windows_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_windows_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_windows_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_windows_2.tbl

-- reaches reject limit
CREATE EXTERNAL TABLE exttab_windows_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_windows_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- without reaching segment reject limit
with cte1 as(
 select t1.i as i,
        sum(t2.i) sum_i,
        avg(sum(t2.i)) over
          (partition by t2.j)
          avg_j,
        rank() over
          (partition by t2.j order by t1.j) rnk_j
 from exttab_windows_1 t1, exttab_windows_1 t2
 where t1.i = t2.i
 group by t1.i,t2.j,t1.j
),
cte2 as
(
 select t1.i as i,
        sum(t2.i) sum_i,
        avg(sum(t2.i)) over
          (partition by t2.j)
          avg_j,
        rank() over
          (partition by t2.j order by t1.j) rnk_j
 from exttab_windows_1 t1, exttab_windows_1 t2
 where t1.i = t2.i
 group by t1.i, t2.j, t1.j
)
SELECT * FROM cte1 c1, cte2 c2
WHERE c1.i = c2.i
ORDER BY c1.i
limit 5;

SELECT relname,linenum,errmsg,rawdata FROM gp_read_error_log('exttab_windows_1') ORDER BY linenum LIMIT 2;

-- with reaching segment reject limit
SELECT gp_truncate_error_log('exttab_windows_1');
SELECT gp_truncate_error_log('exttab_windows_2');

with cte1 as(
 select t1.i as i,
        sum(t2.i) sum_i,
        avg(sum(t2.i)) over
          (partition by t2.j)
          avg_j,
        rank() over
          (partition by t2.j order by t1.j) rnk_j
 from exttab_windows_1 t1, exttab_windows_2 t2
 where t1.i = t2.i
 group by t1.i,t2.j,t1.j
),
cte2 as
(
 select t1.i as i,
        sum(t2.i) sum_i,
        avg(sum(t2.i)) over
          (partition by t2.j)
          avg_j,
        rank() over
          (partition by t2.j order by t1.j) rnk_j
 from exttab_windows_1 t1, exttab_windows_2 t2
 where t1.i = t2.i
 group by t1.i, t2.j, t1.j
)
SELECT * FROM cte1 c1, cte2 c2
WHERE c1.i = c2.i
ORDER BY c1.i
limit 5;

SELECT COUNT(*) > 0 
FROM
(
SELECT * FROM gp_read_error_log('exttab_windows_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_windows_2')
) FOO;
