drop table if exists distpol;

create table distpol(c1 int, c2 text) distributed by (c1);
