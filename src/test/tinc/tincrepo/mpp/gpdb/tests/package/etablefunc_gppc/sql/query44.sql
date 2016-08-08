-- ETF query plan: singleton, with scatter

    -- This query should fail
    EXPLAIN SELECT * FROM transform( TABLE(
        select count(a)::int,'haha'::text from t1
        scatter by a
    ) );
    -- ERROR:  column "t1.a" must appear in the GROUP BY clause or be used in an aggregate function

    EXPLAIN SELECT * FROM transform( TABLE(
        select count(*)::int,'haha'::text from t1
        scatter randomly
    ) );

    EXPLAIN SELECT * FROM transform( TABLE(
        select a,e from t1 where a=60
        scatter by d
    ) );

