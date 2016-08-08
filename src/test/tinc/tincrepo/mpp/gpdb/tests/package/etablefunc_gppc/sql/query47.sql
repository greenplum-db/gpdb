-- ETF sub-query distribution is different than source table

-- Create t1 distributed by a
-- Create t3 distributed by a, e
    DROP TABLE IF EXISTS t3;
    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    -- SCATTER BY the single column (b)
    -- Source table is distributed by a different column (a)
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 scatter by b
    ) );

    -- SCATTER BY two columns (a, e)
    -- Source table is distributed by single column (a)
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 scatter by a, e
    ) );

    -- SCATTER BY by single column (a)
    -- Source table is distributed by two columns (a, e)
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t3 scatter by a
    ) );
  
    -- SCATTER BY by two columns (e, a)
    -- Source table is distributed by two columns (a, e)
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t3 scatter by e, a
    ) );
  
    -- SCATTER Randomly
    -- Source table is distributed by two columns (a)
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 scatter randomly
    ) );

