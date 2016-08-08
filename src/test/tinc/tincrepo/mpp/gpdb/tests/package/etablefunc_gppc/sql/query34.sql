-- Negative: ETF must be called via FROM
-- The following queries should fail

-- ETF after SELECT
select transform( TABLE(select * from intable));

-- ETF in ORDER BY
select * from t1 order by transform( TABLE(select * from intable));

-- ETF in LIMIT
select * from t1 LIMIT transform (TABLE(select * from intable));

-- ETF in GROUP BY
select avg(a) from t1 group by 
                  (transform(TABLE(select * from t1 where a<10 )));

-- ETF in IN clause
select * from t1 where a in 
              (transform(TABLE(select a,e from t1 where a<10)));

-- Positive: ETF can be used for table JOIN operation
-- The followings should succeed

    -- join table t1 with ETF
    SELECT t1.* from t1, transform( 
        TABLE(select a,e from t1 where a <10 order by a scatter by a) ) t2
    WHERE t1.a = t2.b order by t1.a;

    -- join table t1 with ETF, a different format
    SELECT t1.* from t1
    JOIN transform( TABLE(select a,e from t1 
                          where a <10 order by a scatter by a) ) t2
    ON t1.a = t2.b 
    ORDER BY t1.a;

