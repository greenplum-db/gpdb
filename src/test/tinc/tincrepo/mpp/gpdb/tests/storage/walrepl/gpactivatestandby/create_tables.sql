-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
Drop table if exists gpact_t1;
Create table gpact_t1( i int , t text) with (appendonly=true, orientation=column, compresstype=quicklz) partition by range(i) (start(1) end(20) every(2));
Insert into gpact_t1 values(generate_series(1,19), 'a table before gpactivatestandby');
