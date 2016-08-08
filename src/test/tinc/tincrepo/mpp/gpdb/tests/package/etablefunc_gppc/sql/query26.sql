-- ETF sub-query contains WINDOW function
    select * from transform( TABLE(
        select avg(a) over(partition by c)::int,e from t1
        order by d
        scatter by d
    ) )
    order by a limit 5;

