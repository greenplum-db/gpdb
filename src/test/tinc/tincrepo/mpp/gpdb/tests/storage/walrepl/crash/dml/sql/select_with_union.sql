-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
    -- Additional queries to check data type conversion for UNION from Brian's comment
    select null as x union select 1::int as x order by x;

    select null as x union select 1::text as x order by x;

    select distinct null::int as x union select 1::int as x order by x;
