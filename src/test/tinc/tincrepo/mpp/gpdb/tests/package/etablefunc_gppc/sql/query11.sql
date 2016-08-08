-- TABLE() value expressions can never be involved in an expression. 
    SELECT TABLE(select a from t1) + 1;
    -- ERROR:  operator does not exist: anytable + integer

    SELECT * FROM t1 WHERE a IN ( 
        TABLE(select a from t1 where a < 5) * 2
    );
    -- ERROR:  operator does not exist: anytable * integer
