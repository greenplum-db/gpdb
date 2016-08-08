Drop table if exists altwithoid cascade; 
create table altwithoid (col integer) with oids distributed by (col);
create table altinhoid () inherits (altwithoid) without oids;
alter table altwithoid set without oids;

