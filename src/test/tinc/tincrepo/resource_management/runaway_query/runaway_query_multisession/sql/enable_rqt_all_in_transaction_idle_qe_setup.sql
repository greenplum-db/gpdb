drop table if exists rqt_it_iq;

create table rqt_it_iq(c1 int, c2 int);

insert into rqt_it_iq select i, i+1 from generate_series(1,1000)i;

analyze rqt_it_iq;