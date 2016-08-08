-- ETF call returns rows contain null values in all columns

-- Have source table t1 contain some rows with all null values
INSERT INTO t1 SELECT null, null, null, null, null 
FROM generate_series(200,210) i;

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  order by a scatter by a) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  scatter by e) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  order by e) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null  
                                  order by e scatter by a) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null  
                                  order by e scatter by e) );

    DELETE FROM t1 where a is null;
