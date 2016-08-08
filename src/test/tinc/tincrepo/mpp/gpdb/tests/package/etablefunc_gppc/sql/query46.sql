-- ETF distribution is the same as underlying source table
-- Should not have redistribution motion, except for SCATTER RANDOMLY

-- Table t1 is distributed by column a
-- Table t3 is distributed by columns a and e
-- Table t4 is distributed randomly
    DROP TABLE IF EXISTS t3;
    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    INSERT INTO t3 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    DROP TABLE IF EXISTS t4;
    CREATE TABLE t4 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    INSERT INTO t4 SELECT i, i/3, i%2, 100-i, 'text'||i 
    FROM generate_series(1,100) i;


    -- SCATTER BY the same single distribution key
    -- of the source table
    -- No redistribution should be involved
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 scatter by a
    ) );

    -- SCATTER BY the same composite distribution keys (a, e)
    -- of the source table
    -- No redistribution should be involved
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t3 scatter by a, e
    ) );

    -- Both source table and ETF sub query are SCATTER RANDOMLY
    -- Redistribution is needed
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t4 scatter randomly
    ) );

