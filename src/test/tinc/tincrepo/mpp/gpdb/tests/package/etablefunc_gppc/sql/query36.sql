-- ETF can be used in the subquery

    -- ETF in InitPlan
    select array (select a from transform 
    ( TABLE(select * from intable order by id scatter by value) ) order by a);
    -- This works correctly

    -- Use ETF as Qual
    SELECT * FROM t1 WHERE a < (
        SELECT max(b) FROM 
            transform( TABLE(select a,e from t1 where a <5) )
    ) ORDER BY a, b;

