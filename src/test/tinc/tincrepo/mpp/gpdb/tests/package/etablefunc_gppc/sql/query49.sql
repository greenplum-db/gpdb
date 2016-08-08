-- ETF sub-query using order by
-- Note: ORDER BY clause inside ETF sub query can only keep sorting order within each segment.

    -- ORDER BY inside ETF sub query
    -- Note: sort within each segment before calling ETF
    EXPLAIN SELECT * FROM transform( TABLE(
        select id, value from intable order by id
    ) );

    -- ORDER BY inside ETF sub query and 
    -- out layer
    EXPLAIN SELECT * FROM transform( TABLE(
        select id, value from intable order by id
    ) ) ORDER BY b desc;


