drop table if exists rqt_nt_iq;

create table rqt_nt_iq(c1 int, c2 int);

insert into rqt_nt_iq select i, i+1 from generate_series(1,1000)i;

analyze rqt_nt_iq;