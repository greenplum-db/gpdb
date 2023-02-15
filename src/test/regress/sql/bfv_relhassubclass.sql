CREATE TYPE test_type_14644 AS (a int, b text);
CREATE TABLE test_tb_14644 OF test_type_14644;
CREATE TABLE test_tb_14644_subclass () INHERITS (test_tb_14644);
DROP TABLE test_tb_14644_subclass;
select relhassubclass from pg_class where relname = 'test_tb_14644';
select relhassubclass from gp_dist_random('pg_class') where relname = 'test_tb_14644';
ANALYZE;
select relhassubclass from pg_class where relname = 'test_tb_14644';
select relhassubclass from gp_dist_random('pg_class') where relname = 'test_tb_14644';