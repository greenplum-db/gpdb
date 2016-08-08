-- Without any input characteristic SCATTER clause
-- Note: should not have redistribute motion involved

    EXPLAIN SELECT * FROM transform( TABLE(select * from intable) );

    EXPLAIN SELECT * FROM transform( TABLE(select a,e from t1 where a < 10) );

    -- Using ORDER BY outside ETF
    EXPLAIN SELECT * FROM transform( TABLE(select * from intable) )
    ORDER BY b;

