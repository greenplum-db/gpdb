-- ETF query plan of projection
-- Note: Projection does not show up in plan
-- So needs to run queries

    EXPLAIN SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter by a
    ) );

    SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter by a
    ) ) order by b limit 5;

    EXPLAIN SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter randomly
    ) );

    SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter randomly
    ) ) ORDER BY b desc limit 5;
