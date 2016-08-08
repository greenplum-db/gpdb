-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
insert into r1 values (generate_series(1,1600));
insert into r1 values (generate_series(1,1600));
insert into r1 values (generate_series(1,1600));
insert into r1 values (generate_series(1,1600));
insert into r1 values (generate_series(1,1600));