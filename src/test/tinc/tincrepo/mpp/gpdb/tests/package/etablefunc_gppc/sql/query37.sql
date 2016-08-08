-- ETF can be called within following sub query expression: 
-- IN/NOT IN, EXISTS / NOT EXISTS, ANY/SOME, ALL

    -- ETF called in IN
    SELECT * FROM t1 WHERE a IN (
    SELECT b FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ORDER BY a, b;

    -- ETF called in IN
    SELECT * FROM t1 WHERE a NOT IN ( -- using NOT IN here
    SELECT b FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ;

    -- For EXISTS and NOT EXISTS
    SELECT * FROM t1 WHERE EXISTS (
    SELECT 1 FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ORDER BY a, b;

    SELECT * FROM t1 WHERE NOT EXISTS (
        SELECT 1 FROM transform(
            TABLE(select a,e from t1
                  order by a
                  scatter randomly))
    ) AND a < 10 ;

    -- For ANY/SOME
    SELECT * FROM t1 WHERE a> ANY (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a>98
                  order by a
                  scatter randomly))
    );

    SELECT * FROM t1 WHERE a < SOME (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a<3
                  order by a
                  scatter randomly))
    );

    -- For ALL
    SELECT * FROM t1 WHERE a > ALL (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a<98
                  order by a
                  scatter randomly))
    ) ORDER BY a;

