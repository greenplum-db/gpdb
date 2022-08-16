CREATE SCHEMA partition_subquery;
SET SEARCH_PATH=partition_subquery;

-- Given a partition table
CREATE TABLE pt1(id int) DISTRIBUTED BY (id) PARTITION BY RANGE (id) (DEFAULT PARTITION p1);

-- When I run a query, outermost query, and it is selecting FROM a subquery
-- And that subquery, subquery 1, contains another subquery, subquery 2
-- And the outermost query aggregates over a column from an inherited table
-- And the subquery 1 is prevented from being pulled up into a join
SELECT id FROM (
	SELECT id, sum(id) OVER() as sum_id FROM (
		SELECT id FROM pt1
	) as sq1
) as sq2 GROUP BY id;
-- Then, the query executes successfully

--start_ignore
DROP TABLE IF EXISTS pt1;
--end_ignore


-- When a query had a partitioned table and a correlated subquery, it 
-- failed with Query Optimizer. There was implemented a fix which
-- fixes this problem.
DROP TABLE IF EXISTS t1, t2;

CREATE TABLE t1 (a INT) PARTITION BY RANGE (a) (START (1) END (3) EVERY (1));
CREATE TABLE t2 (b INT8) DISTRIBUTED BY (b);

INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (2), (3);

EXPLAIN SELECT * FROM t1 WHERE a <= (
    SELECT * FROM t2
    WHERE t2.b <= (SELECT * FROM t2 AS t3 WHERE t3.b = t2.b)
    AND t1.a = t2.b);

SELECT * FROM t1 WHERE a <= (
    SELECT * FROM t2
    WHERE t2.b <= (SELECT * FROM t2 AS t3 WHERE t3.b = t2.b)
    AND t1.a = t2.b);
