create table mpp3488 (a char(1), b timestamp, d char(3))
distributed by (a)
partition by range (b)
(
           partition aa start ('2008-01-01') end ('2009-01-01') every(interval '1 hour')
);
