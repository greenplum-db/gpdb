-- Test query using ETF with filters
-- MPP-14250
    select a,b from transform( TABLE(
        select id,value from intable 
            where id<8 )) 
    where b <3 order by b;

