create table mpp8031 (oid integer,
odate timestamp without time zone,
cid integer)
PARTITION BY RANGE(odate)
(
PARTITION foo START ('2005-05-01 00:00:00'::timestamp
without time zone) END ('2005-07-01 00:00:00'::timestamp
without time zone) EVERY ('2 mons'::interval),

START ('2005-07-01 00:00:00'::timestamp without time zone)
END ('2006-01-01 00:00:00'::timestamp without time zone)
EVERY ('2 mons'::interval)
);
explain analyze select a.* from mpp8031 a, mpp8031 b where a.oid = b.oid;
drop table mpp8031;
