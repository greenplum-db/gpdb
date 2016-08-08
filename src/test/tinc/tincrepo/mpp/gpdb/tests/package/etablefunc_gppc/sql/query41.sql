-- ETF cardinality: currently is number of input rows

    EXPLAIN SELECT * FROM transform( TABLE(select a,e from t1) );

    EXPLAIN ANALYZE SELECT * FROM transform( TABLE(select a,e from t1) );

