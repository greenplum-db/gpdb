-- MPP-3625
-- Johnny Soedomo
-- These are negative test. Expected to fail
\echo '-- Negative Test, Alter expected to fail'
create table mpp3625 (i int, a date) partition by list(i)
subpartition by range(a)
subpartition template (
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
)
(partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8));
alter table mpp3625 split partition for(1) at (1,2) into (partition f1a, partition f1b); -- Expected to fail

drop table mpp3625;

create table mpp3625 (i int, a date) partition by range(a)
subpartition by list(i) subpartition template
(subpartition a values(1, 2, 3, 4),
subpartition b values(5, 6, 7, 8), default subpartition default_part)
(
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
);
insert into mpp3625 values (1,'2001-01-01');
insert into mpp3625 values (2,'2001-05-01');
insert into mpp3625 values (3,'2001-01-01');
insert into mpp3625 values (4,'2002-01-01');
insert into mpp3625 values (5,'2001-08-01');
insert into mpp3625 values (6,'2004-09-01');
insert into mpp3625 values (7,'2001-11-01');
insert into mpp3625 values (8,'2005-01-01');
insert into mpp3625 values (9,'2001-12-01');
alter table mpp3625 split partition for(1) at (1,2) into (partition f1a, partition f1b); -- Expected Error
alter table mpp3625 split default partition at (9); -- Error
alter table mpp3625 split partition for('2001-01-01') at ('2001-06-01'); -- Error
alter table mpp3625 split default partition start ('2001-01-01') end ('2001-06-01') into (partition aa, partition nodate); -- Error

drop table mpp3625;
