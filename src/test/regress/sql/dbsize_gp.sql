-- test gp_relation_filepath
SELECT regexp_replace(filepath, '[0-9]+', 'XXX', 'g') AS filepath, count(*) > 0 AS ok
    FROM gp_relation_filepath('pg_class') GROUP BY 1;
SELECT filepath, count(*) > 0 AS ok
    FROM gp_relation_filepath(0) GROUP BY 1;
