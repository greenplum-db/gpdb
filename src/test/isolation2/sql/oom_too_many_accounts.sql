drop table if exists t_too_many_accounts;
create table t_too_many_accounts(id serial, creationdate bigint NOT NULL);
insert into t_too_many_accounts(id, creationdate) select generate_series(1, 10000000), 123456789;

select count(distinct created_at) as fake from (select id, to_timestamp(creationdate) as created_at from t_too_many_accounts) as test;
