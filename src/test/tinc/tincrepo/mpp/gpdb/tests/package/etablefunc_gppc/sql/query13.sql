-- Negative: using anytable as general data type should fail
-- NOTICE:  Table doesn't have 'distributed by' clause, 
--          and no column type is suitable for a distribution key. 
--          Creating a NULL policy entry.
    CREATE TABLE tmpTable1 (a anytable);

    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id) ) 
    order by b limit 2;

    -- Verify TVE inside ETF can be casted
    -- to anytable type and query still works fine
    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anytable) 
    order by b limit 2;

    -- Verify TVE inside ETF cannot be casted
    -- to any type (such as anyelement) other than anytable
    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anyelement) 
    order by b limit 2;

    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anyarray) 
    order by b limit 2;

    -- Verify array_append(anyarray, anyelement)
    select array_append(array['value1','value2']::anyarray, 'value3'::anyelement);

    -- Verify anyarray cannot be casted to anytable
    select array_append(array['value1','value2']::anytable, 'value3');

    -- Verify anyelement cannot be casted to anytable
    select array_append(array['value1','value2'], 'value3'::anytable);

    -- Verify pseudo types anytable and anyelement cannot be used for PREPARE
    PREPARE p1(anyelement) AS SELECT $1;
    -- ERROR:  type "anyelement" is not a valid parameter for PREPARE

    PREPARE p2(anytable) AS SELECT $1;
    -- ERROR:  type "anytable" is not a valid parameter for PREPARE
