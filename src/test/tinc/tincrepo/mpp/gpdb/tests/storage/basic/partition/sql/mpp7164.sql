--start_ignore
drop schema mpp7164 cascade;
create schema mpp7164;
set search_path=mpp7164;
\i @abs_srcdir@/pviews.sql
--end_ignore

create table mpp7164r (a int, b text, c int)
    distributed by (a,b,c)
    partition by range (a)
    (
        start (0) inclusive
        end (2) exclusive
        every (1)
    );

select * from partlist where ptable='mpp7164r' order by ptemplate, plevel, rrank;
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partrank where tablename='mpp7164r';
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partagain where tablename='mpp7164r';


alter table mpp7164r
rename partition for (rank(1)) to r1;

alter table mpp7164r
rename partition for (rank(2)) to r2;

select * from partlist where ptable='mpp7164r' order by ptemplate, plevel, rrank;
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partrank where tablename='mpp7164r';
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partagain where tablename='mpp7164r';

alter table mpp7164r
add default partition d;

select * from partlist where ptable='mpp7164r' order by ptemplate, plevel, rrank;
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partrank where tablename='mpp7164r';
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partagain where tablename='mpp7164r';

alter table mpp7164r
rename partition for (rank(1)) to new_1;

alter table mpp7164r
rename partition for (rank(2)) to new_2;

select * from partlist where ptable='mpp7164r' order by ptemplate, plevel, rrank;
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partrank where tablename='mpp7164r';
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partagain where tablename='mpp7164r';

alter table mpp7164r
rename partition for (rank(0)) to new_d;

alter table mpp7164r
rename default partition to new_d;

select * from partlist where ptable='mpp7164r' order by ptemplate, plevel, rrank;
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partrank where tablename='mpp7164r';
select tablename, partitionlevel,  partitiontablename, partitionname, partitionrank from partagain where tablename='mpp7164r';
