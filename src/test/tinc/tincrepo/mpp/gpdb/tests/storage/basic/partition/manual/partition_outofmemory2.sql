create schema testp;
CREATE TABLE testp.t1 (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2009-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);

CREATE TABLE testp.t2 (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2009-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);

CREATE TABLE testp.t3 (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2009-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);

CREATE TABLE testp.t4 (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2009-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);

CREATE TABLE testp.t5 (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2009-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);

select count(*) from pg_partitions;

drop schema testp cascade; -- Out of shared memory, depending on the system
