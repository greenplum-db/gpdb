-- ETF call returns rows contain null values in column e
-- Check scatter by null value column
-- Check order by null value column

-- Have source table t1 contain some null values

    INSERT INTO t1 SELECT i/200*200, i/200*200, i/200*200, i/200*200, null 
    FROM generate_series(200,210) i;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by a scatter by a ) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  scatter by e) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e scatter by a ) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e scatter by e ) ) order by b,a;

    DELETE FROM t1 WHERE e is null;
