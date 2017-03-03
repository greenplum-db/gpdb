\c gptest;

-- start_ignore
drop table if exists mpp15087_t;
-- end_ignore

create table mpp15087_t(code char(3), n numeric);
insert into mpp15087_t values ('abc',1);
insert into mpp15087_t values ('xyz',2);  
insert into mpp15087_t values ('def',3); 
