-- The input of ETF can only be TABLE value expression,
-- which only takes a subquery as an input
    SELECT * FROM transform( TABLE(select * from intable) );

    SELECT * FROM transform( TABLE(select distinct b,c from t1) );

    -- CONSTANT input
    SELECT * FROM transform( TABLE( SELECT 2 as id,'haha'::text as value FROM intable) );

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a, d) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a+d) ) order by b;
    -- a: will be 1, 2, 3 ... 100
    -- d: will be 100, 99, 98 ... 1

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 scatter randomly) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a desc, b) ) order by a,b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a + d) ) order by b;
    -- a: will be 1, 2, 3 ... 100
    -- d: will be 100, 99, 98 ... 1

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a || b ) ) order by b desc;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a asc, b desc) )
    order by a desc, b asc;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a scatter by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a scatter by e) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 
                           order by a,e scatter by a,e) ) order by b;

    SELECT * FROM transform( TABLE(select b, e from t1  where a<=10 
                           order by a scatter by a) ) order by b,a;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10
                           order by a scatter by b,c,d,e) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10
                           order by b,c,d,e scatter by b,c,d,e) ) order by b;
