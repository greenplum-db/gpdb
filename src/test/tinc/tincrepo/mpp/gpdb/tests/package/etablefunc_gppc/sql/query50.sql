-- ETF sub-query contains LIMIT
    -- Using LIMIT without any filter or sorting
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 limit 10
    ) );

    -- Using LIMIT with filter 
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 where a > 10 limit 10
    ) );

    -- Using LIMIT with order and filter
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 where a < 10 
        order by a limit 10
    ) );

    -- Using LIMIT and SCATTER BY inside ETF sub query
    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 
        limit 10
        scatter by a
    ) );

