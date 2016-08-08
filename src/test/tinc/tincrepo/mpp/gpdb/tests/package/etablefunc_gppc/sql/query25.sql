-- ETF sub-query contains aggregation
    SELECT * FROM transform( TABLE(select count(*)::int,'haha'::text from t1) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51 
                               group by c 
                               order by avg(a)) ) ORDER BY b;

    -- This query should fail since "scatter by a"
    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51 
                               group by c 
                               order by avg(a)
                               scatter by a) ) ORDER BY b;

    -- This query should succeed since "scatter by c"
    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51
                               group by c
                               order by avg(a)
                               scatter by c) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by c) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by avg(a)::int) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by 'haha'::text) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 
                               where a < 51 group by c order by avg(a) scatter by c) ) ORDER BY b;

SELECT sum(b) FROM transform( 
    TABLE(select (rank() over (order by a))::int,'haha'::text 
          from t1 scatter by (rank() over (order by a))::int) );

