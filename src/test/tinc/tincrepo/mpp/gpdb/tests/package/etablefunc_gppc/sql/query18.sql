-- ETF call returns empty result set

    -- ETF sub-query on an empty table: intable2
    create table intable2 (id int, value text) distributed by (id);
    SELECT * from transform( TABLE(select * from intable2) );

    -- ETF sub-query input returns 0 row via filter
    SELECT * from transform( TABLE(select a, e from t1 where a > 1000) );

    SELECT * from transform( TABLE(select a, e from t1 where a is null) );

    SELECT * from transform( TABLE(select a, e from t1 where a = e) );

    -- Also checking outer layer 
    SELECT * from transform( TABLE(select a, e from t1 where a > 10) )
    where a < 10;

    drop table intable2;
