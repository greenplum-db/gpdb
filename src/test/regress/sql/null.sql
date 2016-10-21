-- check behavior when values are NULL with date type
create table date_null (a int4, b date);
insert into date_null values (1, '1957-04-09');
insert into date_null values (2, NULL);
select a from date_null where b <= date '1957-04-10';
select a from date_null where b <= date '1957-04-10' - interval '90' day;

-- check behavior when values are NULL with float8 type
create table float8_null (a float8, b float8, c float8);
insert into float8_null values (1, NULL, NULL);
select a from float8_null where b <=a;
select a from float8_null where b*a <=0;
select a from float8_null where b*c <=0;
select a+b from float8_null;
select b+c from float8_null;
select a-b from float8_null;
select b-c from float8_null;

-- check behavior when values are NULL with int4 type
create table int4_null (a int4, b int4, c int4);
insert into int4_null values (1, NULL, NULL);
select a from int4_null where b <=a;
select a from int4_null where b*a <=0;
select a from int4_null where b*c <=0;
select a+b from int4_null;
select b+c from int4_null;
select a-b from int4_null;
select b-c from int4_null;

-- check behavior when values are NULL with int8 type
create table int8_null (a int8, b int8, c int8);
insert into int8_null values (1, NULL, NULL);
select a from int8_null where b <=a;
select a from int8_null where b*a <=0;
select a from int8_null where b*c <=0;
select a+b from int8_null;
select b+c from int8_null;
select a-b from int8_null;
select b-c from int8_null;
