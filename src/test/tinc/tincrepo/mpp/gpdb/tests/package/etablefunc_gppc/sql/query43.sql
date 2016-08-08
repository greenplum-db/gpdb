-- ETF query plan: singleton, no scatter

    EXPLAIN SELECT * FROM transform( TABLE(
        select count(*)::int,'haha'::text from t1
    ) );

    EXPLAIN SELECT * FROM transform( TABLE(
        select max(d)::int,'haha'::text from t1
    ) );

    EXPLAIN SELECT * FROM transform( TABLE(
        select avg(a)::int,'haha'::text from t1
    ) );

    EXPLAIN SELECT * FROM transform( TABLE(
        select a,e from t1 where a=60
    ) );

