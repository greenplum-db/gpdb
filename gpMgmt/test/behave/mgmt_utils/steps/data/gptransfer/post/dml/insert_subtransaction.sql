\c gptest;

begin;
savepoint sp1; 
insert into subtx2_commit_t1 values(generate_series(1,10), 'sub-transactions with sub-commits and rollbacks');
 release sp1; 
savepoint sp2;
insert into subtx2_commit_t1 values(generate_series(1,10), 'sub-transactions with sub-commits and rollbacks');
rollback to sp2;
commit;

select count(*) from subtx2_commit_t1;
