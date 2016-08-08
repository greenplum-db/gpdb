-- ETF sub-query contains join table / view

    -- Create another table t2 so to test join tables t1 and t2.
    DROP TABLE IF EXISTS t2;
    CREATE TABLE t2 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a);

    INSERT INTO t2 SELECT i, i/3, i%2, 100-i, 'text'||i 
    FROM generate_series(1,100) i;

-- ETF sub-query: join table works correctly when using format:
-- SELECT FROM t1 JOIN t2 ON t1.a=t2.a
select * from transform(
    TABLE(select t1.a,t2.e from t1
          join t2 on t1.a=t2.a
          where t1.a <10 order by t1.a scatter by t2.c) ) 
order by b;

-- ETF sub-query joins table and putting join condition in where cause
select * from transform(
    TABLE(select t1.a,t2.e from t1,t2 where t1.a=t2.a
          and t1.a < 10 order by t1.a scatter by t2.c) )
order by b;

drop table t2;
