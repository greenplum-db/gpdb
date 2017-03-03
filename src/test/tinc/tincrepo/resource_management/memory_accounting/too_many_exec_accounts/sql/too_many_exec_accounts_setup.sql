drop table if exists minimal_mpp_25339;
create table minimal_mpp_25339(id serial, creationdate bigint NOT NULL);
insert into minimal_mpp_25339(id, creationdate) select generate_series(1, 10000000), 123456789;
