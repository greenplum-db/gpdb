-- ETF with scatter, order and limit

    EXPLAIN SELECT * FROM transform( TABLE(
        select a, e from t1 
        order by a 
        limit 10
        scatter by a
    ) );

