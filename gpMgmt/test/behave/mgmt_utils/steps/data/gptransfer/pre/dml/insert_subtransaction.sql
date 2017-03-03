\c gptest;

-- start_ignore
drop table if exists subtx2_commit_t1;
-- end_ignore
create table subtx2_commit_t1(a1 int,a2 text) distributed randomly;
