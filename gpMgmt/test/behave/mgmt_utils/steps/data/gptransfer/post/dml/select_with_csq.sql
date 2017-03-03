\c gptest;

-- -- -- --
-- Basic queries with ANY clause
-- -- -- --
select a, x from csq_t1, csq_t2 where csq_t1.a = any (select x);
select A.i from A where A.i = any (select B.i from B where A.i = B.i) order by A.i;

select * from A where A.j = any (select C.j from C where C.j = A.j) order by 1,2;
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C)) order by 1,2,3,4;
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C where C.i != 10 and C.i = A.i)) order by 1,2,3,4; -- Not supported, should fail

select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;



-- -- -- --
-- Basic CSQ with UPDATE statements
-- -- -- --
select * from csq_t4 order by a;
update csq_t4 set a = (select y from csq_t5 where x=a) where b < 8;
select * from csq_t4 order by a;
update csq_t4 set a = 9999 where csq_t4.a = (select max(x) from csq_t5);
select * from csq_t4 order by a;
update csq_t4 set a = (select max(y) from csq_t5 where x=a) where csq_t4.a = (select min(x) from csq_t5);
select * from csq_t4 order by a;
update csq_t4 set a = 8888 where (select (y*2)>b from csq_t5 where a=x);
select * from csq_t4 order by a;
update csq_t4 set a = 3333 where csq_t4.a in (select x from csq_t5);
select * from csq_t4 order by a;

update A set i = 11111 from C where C.i = A.i and exists (select C.j from C,B where C.j = B.j and A.j < 10);
select * from A order by A.i, A.j;
update A set i = 22222 from C where C.i = A.i and not exists (select C.j from C,B where C.j = B.j and A.j < 10);
select * from A order by A.i, A.j;

-- -- -- --
-- Basic CSQ with DELETE statements
-- -- -- --
select * from csq_t4 order by a;
delete from csq_t4 where a <= (select min(y) from csq_t5 where x=a);
select * from csq_t4 order by a;
delete from csq_t4 where csq_t4.a = (select x from csq_t5);
select * from csq_t4 order by a;
delete from csq_t4 where exists (select (y*2)>b from csq_t5 where a=x);
select * from csq_t4 order by a;
delete from csq_t4  where csq_t4.a = (select x from csq_t5 where a=x);
select * from csq_t4 order by a;

delete from  A TableA where exists (select C.j from C, B where C.j = B.j and TableA.j < 10);
select * from A order by A.i;
delete from A TableA where not exists (select C.j from C,B where C.j = B.j and TableA.j < 10);
select * from A order by A.i;
