-- ETF sub-query using view
    DROP VIEW IF EXISTS t1_view;
    CREATE VIEW t1_view as (
        SELECT a, b, c, d ,e from t1 
        WHERE a <10 ORDER BY d);

    SELECT * FROM transform( TABLE(select a, e from t1_view) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view 
                           order by b scatter by a) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view 
                           order by a) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view
                           where a < 6) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view) )
    WHERE a < 6 ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view
                           where a > 10) ) ORDER BY b;
    -- empty result set

    SELECT * FROM transform( TABLE(select a, e from t1_view) )
    WHERE b > 10 ORDER BY b;
    -- empty result set
