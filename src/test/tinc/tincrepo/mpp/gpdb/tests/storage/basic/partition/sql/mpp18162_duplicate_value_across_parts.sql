-- MPP-18162 CLONE (4.2.3) - List partitioning for multiple columns gives duplicate values error

-- int
----------------------------------------
-- start_ignore
drop table if exists mpp18162;
-- end_ignore

create table mpp18162
( i1 int, i2 int)
distributed by (i1)
partition by list (i1, i2) (
  partition pi0 values ( (1,1) ),
  partition pi1 values ( (1,2) ),
  partition pi2 values ( (2,1) )
);

-- text
----------------------------------------
-- start_ignore
drop table if exists mpp18162;
-- end_ignore

create table mpp18162
( i1 text, i2 varchar(10))
distributed by (i1)
partition by list (i1, i2) (
  partition pi0 values ( ('1','1') ),
  partition pi1 values ( ('1','2') ),
  partition pi2 values ( ('2','1') )
);

-- date
----------------------------------------
-- start_ignore
drop table if exists mpp18162;
-- end_ignore

create table mpp18162
( i1 date, i2 date)
distributed by (i1)
partition by list (i1, i2) (
  partition pi1 values ( (date '2008-01-01',date '2008-02-01') ),
  partition pi2 values ( (date '2008-02-01',date '2008-01-01') ),
  partition pi3 values ( (date '2008-03-01',date '2008-04-01') ),
  partition pi4 values ( (date '2008-04-01',date '2008-03-01') )
);

