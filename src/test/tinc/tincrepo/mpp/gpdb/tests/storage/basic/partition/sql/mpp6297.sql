CREATE TABLE mpp6297 (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') );

ALTER TABLE mpp6297 SPLIT PARTITION FOR ('2008-01-01')
AT ('2008-01-16')
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE mpp6297 ADD DEFAULT PARTITION other;

ALTER TABLE mpp6297 SPLIT DEFAULT PARTITION
START ('2009-01-01') INCLUSIVE
END ('2009-02-01') EXCLUSIVE
INTO (PARTITION jan09, PARTITION other);

-- Uses pg_get_partition_def for pg_dump and gp_dump
select pg_get_partition_def('mpp6297'::regclass, true, true);

\! pg_dump -t mpp6297 -s -O @DBNAME@ > @out_dir@/mpp6297-pg_dump.out

drop table mpp6297;

