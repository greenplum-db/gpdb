-- ETF call returns duplicate rows

    -- Have source table t1 contain some duplicated rows.

    INSERT INTO t1 SELECT i/100*100, i/100*100, i/100*100, i/100*100, 'text' 
    FROM generate_series(101,110) i;

    SELECT * FROM transform( TABLE (select a,e from t1 where a = 100 order by b scatter by a) ) order by b,a;

    DELETE FROM t1 where e='text';
