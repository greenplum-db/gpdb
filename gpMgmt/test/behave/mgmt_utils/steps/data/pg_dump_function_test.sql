CREATE ORDERED AGGREGATE agg_array(anyelement) (
    SFUNC = array_append,
    STYPE = anyarray,
    INITCOND = '{}'
);
