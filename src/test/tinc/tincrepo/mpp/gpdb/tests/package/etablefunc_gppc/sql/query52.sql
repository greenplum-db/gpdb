-- ETF init plan

    EXPLAIN SELECT ARRAY(
        SELECT a FROM transform( TABLE(
            select a, e from t1
        ) )
    );

    EXPLAIN SELECT ARRAY(
        SELECT a FROM transform( TABLE(
            select a, e from t1
            order by a
            scatter by a
        ) )
    );

    EXPLAIN SELECT ARRAY(
        SELECT a FROM transform( TABLE(
            select a, e from t1
            order by a
            scatter randomly
        ) )
    );

