-- Query plan when ETF as a qual

    EXPLAIN SELECT * FROM t1 WHERE a IN (
        SELECT b FROM transform(
            TABLE(select a, e from t1_view)
    ) );

    explain analyze SELECT * FROM t1 WHERE a IN (
        SELECT b FROM transform(
            TABLE(select a, e from t1_view)
    ) );

    EXPLAIN SELECT * FROM t1 WHERE a NOT IN (
        SELECT b FROM transform(
            TABLE(select a, e from t1_view)
    ) );

    EXPLAIN ANALYZE SELECT * FROM t1 WHERE a NOT IN (
        SELECT b FROM transform(
            TABLE(select a, e from t1_view)
    ) );

