DROP TABLE IF EXISTS filter_test;
CREATE TABLE filter_test (i int, j int) DISTRIBUTED BY (i);
INSERT INTO filter_test VALUES
  (1, 1),
  (2, 1),
  (3, 1),
  (4, 2),
  (NULL, 2),
  (6, 2),
  (7, 3),
  (8, NULL),
  (9, 3),
  (10, NULL);

SELECT i,j FROM filter_test order by i;

-- TEST COUNT(*)
SELECT count(*) FROM filter_test;
SELECT count(*) FILTER (WHERE TRUE) FROM filter_test;
SELECT count(*) FILTER (WHERE FALSE) FROM filter_test;
SELECT count(*) FILTER (WHERE i < 5) FROM filter_test;
SELECT count(*) FROM filter_test WHERE i < 5;
SELECT count(*) FILTER (WHERE j = 1) FROM filter_test;
SELECT count(*) FROM filter_test WHERE j = 1;

-- TEST COUNT(i)
SELECT count(i) FROM filter_test;
SELECT count(i) FILTER (WHERE TRUE) FROM filter_test;
SELECT count(i) FILTER (WHERE FALSE) FROM filter_test;
SELECT count(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT count(i) FROM filter_test WHERE i < 5;
SELECT count(i) FILTER (WHERE j = 1) FROM filter_test;
SELECT count(i) FROM filter_test WHERE j = 1;

-- TEST MAX(*) => should error
SELECT max(*) FROM filter_test;
SELECT max(*) FILTER (WHERE i < 5) FROM filter_test;


-- TEST MAX(i)
SELECT max(i) FROM filter_test;
SELECT max(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT max(i) FROM filter_test WHERE i < 5;

-- TEST MIN(i)
SELECT min(i) FROM filter_test;
SELECT min(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT min(i) FROM filter_test WHERE i < 5;

-- TEST MAX(i)
SELECT MAX(i) FROM filter_test;
SELECT MAX(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT MAX(i) FROM filter_test WHERE i < 5;

-- TEST AVG(i)
SELECT AVG(i) FROM filter_test;
SELECT AVG(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT AVG(i) FROM filter_test WHERE i < 5;

-- TEST SUM(i)
SELECT sum(i) FROM filter_test;
SELECT sum(i) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i) FROM filter_test WHERE i < 5;

-- SUM is special since it is non-strict to handle datatype upconversion.
-- Run tests to make sure it works against different datatypes
SELECT sum(i::int2) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i::int4) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i::int8) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i::float) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i::float8) FILTER (WHERE i < 5) FROM filter_test;
SELECT sum(i::numeric) FILTER (WHERE i < 5) FROM filter_test;

-- TEST with cumulative aggs
SELECT i, j, count(j) OVER (order by i) FROM filter_test ORDER BY i;
SELECT i, j, count(j) FILTER (WHERE i % 2 = 1) OVER (order by i) FROM filter_test ORDER BY i;
SELECT i, j, count(j) OVER (order by i) FROM filter_test WHERE i % 2 = 1 ORDER BY i;

-- TEST with partitioned aggs
select i, j, count(i) over (partition by j) from filter_test ORDER BY j, i;
select i, j, count(i) filter (WHERE i % 2 = 1) over (partition by j) from filter_test ORDER BY j, i;

-- TEST with rolling window aggs
select i, j, count(i) over(w) from filter_test 
window w as (order by i rows between 1 preceding and 1 following) ORDER BY i;
select i, j, count(i) filter (where j = 2) over(w) from filter_test 
window w as (order by i rows between 1 preceding and 1 following) ORDER BY i;

-- TEST with group by
select j, count(i) from filter_test group by j ORDER BY j;
select o.*, sum(count_num) over (order by j) as count_subtotal
from (select j, count(i) filter (WHERE i%2 = 0) as count_even,
                count(i) filter (WHERE i%2 = 1) as count_odd,
                count(i) as count_num
      from filter_test group by j) o
ORDER BY j;
select count(i) from filter_test;

-- TEST with multi-parameter aggs
select covar_pop(i,j) from filter_test;
select covar_pop(i,j) from filter_test where i < 5;
select covar_pop(i,j) filter (where i < 5) from filter_test;
select covar_pop(i,j) from filter_test where j in (1,2);
select covar_pop(i,j) filter (where j in (1,2)) from filter_test;

-- TEST with window functions => should error
select i, j, row_number() over (partition by j order by i) from filter_test ORDER BY j,i;
select i, j, row_number() filter (WHERE i % 2 = 1) over (partition by j order by i) 
FROM filter_test ORDER BY j, i;
select i, rank() over (order by i) from filter_test ORDER BY i;
select i, rank() filter (where true) over (order by i) from filter_test ORDER BY i;
select i, ntile(3) over (order by i) from filter_test ORDER BY i;
select i, ntile(3) filter (where true) over (order by i) from filter_test ORDER BY i;
select i, ntile(4-j) over (partition by j order by i) 
FROM filter_test where j < 4 ORDER BY j, i;
select i, ntile(4-j) filter (where true) over (partition by j order by i) 
from filter_test where j < 4 ORDER BY j, i;

-- TEST against non aggregate function => should error
select i, lower('Hello') from filter_test order by i;
select i, lower('Hello') filter (where i < 5) from filter_test order by i;

-- TEST against function like projection => should error
create or replace function maketuple() returns setof filter_test 
as 'select * from filter_test' language sql READS SQL DATA;

select i(t), j(t) from (select maketuple() as t) x order by i(t);
select i(t) filter (where true) from (select maketuple() as t) x order by i(t);
drop function maketuple();

-- TEST non-exsistant function => same error
select chocolate(i) from filter_test;
select chocolate(i) filter (where true) from filter_test;

-- TEST filter from outside scope => error
select * from (select i from filter_test where j = 1) x1, 
              (select sum(i) filter (where i < x1.i) from filter_test where j = 2) x2;

-- TEST against user defined aggregation function
create or replace function _maxodd(oldmax int, newval int) returns int as $$
  SELECT CASE WHEN $1 is NULL 
              THEN (CASE WHEN abs($2) % 2 = 1 THEN $2 
                         ELSE NULL END)
              ELSE (CASE WHEN abs($2) % 2 = 0 THEN $1
                         ELSE (CASE WHEN $2 > $1 
                                    THEN $2 ELSE $1 END) 
                         END)
              END;
$$ LANGUAGE sql STRICT CONTAINS SQL;

create aggregate maxodd(sfunc = _maxodd, basetype = int, stype = int, initcond = 0);

SELECT maxodd(i) from filter_test;
SELECT maxodd(i) FILTER (WHERE TRUE) from filter_test;
SELECT j, maxodd(i) from filter_test group by j order by j;
SELECT j, maxodd(i) FILTER (WHERE TRUE) from filter_test group by j order by j;

-- TEST NON-STRICT user defined aggregation function => should error
create or replace function _maxodd(oldmax int, newval int) returns int as $$
  SELECT CASE WHEN $1 is NULL 
              THEN (CASE WHEN abs($2) % 2 = 1 THEN $2 
                         ELSE NULL END)
              ELSE (CASE WHEN abs($2) % 2 = 0 THEN $1
                         ELSE (CASE WHEN $2 > $1 
                                    THEN $2 ELSE $1 END) 
                         END)
              END;
$$ LANGUAGE sql CONTAINS SQL;
SELECT maxodd(i) FROM filter_test;
SELECT maxodd(i) FILTER (WHERE TRUE) from filter_test;
SELECT j, maxodd(i) FROM filter_test group by j order by j;
SELECT j, maxodd(i) FILTER (WHERE TRUE) from filter_test group by j order by j;

-- TEST view deparsing of FILTER expressions.
CREATE VIEW filter_view AS SELECT count(*) FILTER (WHERE TRUE) FROM filter_test;
SELECT definition from pg_views WHERE viewname='filter_view';

drop aggregate maxodd(int);
drop function _maxodd(int,int);
