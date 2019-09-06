LOAD '@abs_builddir@/query_info_hook_test/query_info_hook_test@DLSUFFIX@';
SET client_min_messages='warning';
SET optimizer=off;

-- Test Normal case
SELECT * FROM generate_series(1, 3);

-- Test Error case: Error out in executor.
CREATE FUNCTION error_in_execution() RETURNS setof int VOLATILE LANGUAGE plpgsql AS $$
BEGIN
RAISE EXCEPTION 'error in function execution';
END;
$$;
SELECT * FROM error_in_execution();

-- Test Error case: Error out in planner.
SELECT * FROM generate_series(1, 3/0);

-- Test query abort
select pg_cancel_backend(pg_backend_pid());
-- Test alter table set distributed by
CREATE TABLE queryInfoHookTable1 (id int, name text) DISTRIBUTED BY(id);
ALTER TABLE queryInfoHookTable1 SET DISTRIBUTED BY (name);
-- Test Hash node
CREATE TABLE tb_a(a int);
SELECT a FROM tb_a WHERE a IN (SELECT max(a) FROM tb_a);
DROP TABLE tb_a;
-- Test SPI
-- start_ignore
CREATE TABLE tb_b(b int);
INSERT INTO tb_b SELECT generate_series(1, 10);
-- end_ignore
CREATE OR REPLACE FUNCTION spi_func() RETURNS setof record
LANGUAGE plpgsql AS $$
DECLARE
    obj1 record;
BEGIN
    FOR obj1 IN SELECT * FROM tb_b
    LOOP
        RETURN next obj1;
    END LOOP;
    RETURN;
END
$$;
SELECT * FROM spi_func() AS (a int) order by 1; 
-- Test function
SELECT CASE WHEN information_schema._pg_truetypmod(a.*, b.*) > 0 THEN 0 ELSE 0 END
FROM
  (SELECT * FROM pg_catalog.pg_attribute LIMIT 1) a,
  (SELECT * FROM pg_catalog.pg_type LIMIT 1) b
;
