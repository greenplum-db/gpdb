-- TABLE value expression can only be used inside ETF call
-- Cannot be used in any other places.

    -- TVE after select
    SELECT TABLE(select a from t1);
    -- ERROR:  invalid use of TABLE value expression

    -- TVE after select
    SELECT TABLE(select a from t1) + 1;
    -- ERROR:  operator does not exist: anytable + integer

    -- TVE in FROM clause
    SELECT * FROM TABLE(select a from t1);
    -- ERROR:  syntax error at or near "TABLE"

    -- ETF in IN clause
    SELECT * FROM t1 WHERE a IN ( 
        transform( TABLE(select a,e from t1 where a < 5) )
    );
    -- ERROR:  operator does not exist: integer = outtable

    -- TVE in IN clause
    SELECT * FROM t1 WHERE a IN ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  operator does not exist: integer = anytable

    -- TVE in  NOT IN clause
    SELECT * FROM t1 WHERE a NOT IN ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  operator does not exist: integer <> anytable

    -- TVE in EXIST clause
    SELECT * FROM t1 WHERE EXISTS ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  syntax error at or near "TABLE"

    -- TVE in NOT EXIST clause
    SELECT * FROM t1 WHERE NOT EXISTS ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  syntax error at or near "TABLE"

    -- TVE in ANY/SOME clause
    SELECT * FROM t1 WHERE a < ANY ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  op ANY/ALL (array) requires array on right side

    -- TVE in ALL clause
    SELECT * FROM t1 WHERE a > ALL ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  op ANY/ALL (array) requires array on right side

    -- TVE in GROUP BY clause
    SELECT avg(a) FROM t1 GROUP BY TABLE(select c from t1);
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in HAVING clause
    SELECT avg(a) FROM t1 GROUP BY c HAVING TABLE(select 51) > 50;
    -- ERROR:  operator does not exist: anytable > integer

    -- TVE in window function
    select a, c, TABLE (SELECT a from t1) over(partition by c) from t1 where a < 10;
    -- ERROR:  syntax error at or near "over"

    -- TVE in window function
    select a, c, avg(a) over(partition by TABLE (SELECT a from t1)) from t1 where a < 10;
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in ORDER BY clause
    SELECT a FROM t1 ORDER BY TABLE(select a from t1);
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in LIMIT clause
    SELECT a FROM t1 LIMIT TABLE(select 4);
    -- ERROR:  argument of LIMIT must be type bigint, not type anytable

    -- nested TABLE() expression
    SELECT a FROM TABLE(TABLE(select 4,'haha'::text));
    -- ERROR:  syntax error at or near "TABLE"
