-- ETF query plan for correlated sub-query

-- start_ignore
    explain select a,e from t1 where a > 
        (select avg(t1.a) from t1);

    EXPLAIN SELECT * FROM transform( TABLE(
        select a,e from t1 where a > 
            (select avg(a) from t1)
    ) );
-- end_ignore
    SELECT * FROM transform( TABLE(
        select a,e from t1 where a > 
            (select avg(a) from t1)
    ) ) ORDER BY b;
