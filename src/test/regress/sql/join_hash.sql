--
-- exercises for the hash join code
--

begin;

-- If virtualbuckets is much larger than innerndistinct, and
-- outerndistinct is much larger than innerndistinct. Then most
-- tuples of the outer table will match the empty bucket. So when
-- we calculate the cost of traversing the bucket, we need to ignore
-- the tuple matching empty bucket.
savepoint settings;
set max_parallel_workers_per_gather = 0;
create table join_hash_t_small(a int);
create table join_hash_t_big(b int);

insert into join_hash_t_small select i%100 from generate_series(0, 3000)i;
insert into join_hash_t_big select i%100000 from generate_series(1, 100000)i ;

analyze join_hash_t_small;
analyze join_hash_t_big;

explain (costs off) select * from join_hash_t_small, join_hash_t_big where a = b;
rollback to settings;

ROLLBACK;
