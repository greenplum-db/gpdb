--
--  Test Greenplum extensions for citext datatype
--

create table citext_dist (a citext) distributed by (a);
insert into citext_dist values ('XxX'), ('AaA'), ('JjJ');
select count(distinct gp_segment_id) > 1 from citext_dist;
