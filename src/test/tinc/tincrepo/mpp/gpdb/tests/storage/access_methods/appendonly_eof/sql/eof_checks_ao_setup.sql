-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS test_ao_mpp19912;
CREATE TABLE test_ao_mpp19912 ( i int, j int) WITH (appendonly=true);
INSERT INTO test_ao_mpp19912 SELECT i, i FROM generate_series(1, 1000) i;


DROP TABLE IF EXISTS test_ao_mpp19912_supp1;
DROP TABLE IF EXISTS test_ao_mpp19912_supp2;
DROP TABLE IF EXISTS test_ao_mpp19912_supp3;
DROP TABLE IF EXISTS test_ao_mpp19912_supp4;

CREATE TABLE test_ao_mpp19912_supp1 ( i int, j int) WITH (appendonly=true);
CREATE TABLE test_ao_mpp19912_supp2 ( i int, j int) WITH (appendonly=true);
CREATE TABLE test_ao_mpp19912_supp3 ( i int, j int) WITH (appendonly=true);
CREATE TABLE test_ao_mpp19912_supp4 ( i int, j int) WITH (appendonly=true);

