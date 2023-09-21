-- case 1: test gp_detect_data_correctness
create table data_correctness_detect(a int, b int);
create table data_correctness_detect_randomly(a int, b int) distributed randomly;
create table data_correctness_detect_replicated(a int, b int) distributed replicated;

set gp_detect_data_correctness = on;
-- should no data insert
insert into data_correctness_detect select i, i from generate_series(1, 100) i;
select count(*) from data_correctness_detect;
insert into data_correctness_detect_randomly select i, i from generate_series(1, 100) i;
select count(*) from data_correctness_detect_randomly;
insert into data_correctness_detect_replicated select i, i from generate_series(1, 100) i;
select count(*) from data_correctness_detect_replicated;
set gp_detect_data_correctness = off;

-- insert some data that not belongs to it
1U: insert into data_correctness_detect select i, i from generate_series(1, 100) i;
1U: insert into data_correctness_detect_randomly select i, i from generate_series(1, 100) i;
1U: insert into data_correctness_detect_replicated select i, i from generate_series(1, 100) i;
set gp_detect_data_correctness = on;
insert into data_correctness_detect select * from data_correctness_detect;
insert into data_correctness_detect select * from data_correctness_detect_randomly;
insert into data_correctness_detect select * from data_correctness_detect_replicated;

-- clean up
set gp_detect_data_correctness = off;
drop table data_correctness_detect;
drop table data_correctness_detect_randomly;
drop table data_correctness_detect_replicated;

