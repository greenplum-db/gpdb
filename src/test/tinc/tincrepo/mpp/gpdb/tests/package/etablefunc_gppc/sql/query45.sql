-- ETF queries table master only table (typically these are catalog tables)

    -- Without SCATTER
    -- Note: there is no Redistribution node and Gather Motion node in plan
    EXPLAIN SELECT * FROM transform( TABLE(
        select dbid::int, hostname from gp_segment_configuration
    ) );

    -- With SCATTER BY
    EXPLAIN SELECT * FROM transform( TABLE(
        select dbid::int, hostname from gp_segment_configuration
        scatter by hostname
    ) );

    -- With SCATTER - randomly
    EXPLAIN SELECT * FROM transform( TABLE(
        select dbid::int, hostname from gp_segment_configuration
        scatter randomly
    ) );

