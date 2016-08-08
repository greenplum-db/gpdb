create table mpp7232a (a int, b int) 
partition by range (b) (start (1) end (10) every (1));
select pg_get_partition_def('mpp7232a'::regclass, true);
alter table mpp7232a rename partition for (rank(1)) to alpha;
alter table mpp7232a rename partition for (rank(2)) to bravo;
alter table mpp7232a rename partition for (rank(3)) to charlie;
alter table mpp7232a rename partition for (rank(4)) to delta;
alter table mpp7232a rename partition for (rank(5)) to echo;
select partitionname, partitionrank from pg_partitions where tablename like 'mpp7232a' order by 2;
select pg_get_partition_def('mpp7232a'::regclass, true);

create table mpp7232b  (a int, b int) partition by range (b) 
(PARTITION alpha START (1) END (10) EVERY (1));
select partitionname, partitionrank from pg_partitions where tablename like 'mpp7232b' order by 2; 
alter table mpp7232b rename partition for (rank(4)) to foo;
select pg_get_partition_def('mpp7232b'::regclass, true);

\! pg_dump -t mpp7232a -s -O @DBNAME@ > @out_dir@/mpp7232-pg_dump.out
\! pg_dump -t mpp7232b -s -O @DBNAME@ >> @out_dir@/mpp7232-pg_dump.out

drop table mpp7232a;
drop table mpp7232b;
