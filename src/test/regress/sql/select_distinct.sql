--
-- SELECT_DISTINCT
--

--
-- awk '{print $3;}' onek.data | sort -n | uniq
--
SELECT DISTINCT two FROM tmp ORDER BY 1;

--
-- awk '{print $5;}' onek.data | sort -n | uniq
--
SELECT DISTINCT ten FROM tmp ORDER BY 1;

--
-- awk '{print $16;}' onek.data | sort -d | uniq
--
SELECT DISTINCT string4 FROM tmp ORDER BY 1;

--
-- awk '{print $3,$16,$5;}' onek.data | sort -d | uniq |
-- sort +0n -1 +1d -2 +2n -3
--
SELECT DISTINCT two, string4, ten
   FROM tmp
   ORDER BY two using <, string4 using <, ten using <;

--
-- awk '{print $2;}' person.data |
-- awk '{if(NF!=1){print $2;}else{print;}}' - emp.data |
-- awk '{if(NF!=1){print $2;}else{print;}}' - student.data |
-- awk 'BEGIN{FS="      ";}{if(NF!=1){print $5;}else{print;}}' - stud_emp.data |
-- sort -n -r | uniq
--
SELECT DISTINCT p.age FROM person* p ORDER BY age using >;

--
-- Also, some tests of IS DISTINCT FROM, which doesn't quite deserve its
-- very own regression file.
--

CREATE TEMP TABLE disttable (f1 integer);
INSERT INTO DISTTABLE VALUES(1);
INSERT INTO DISTTABLE VALUES(2);
INSERT INTO DISTTABLE VALUES(3);
INSERT INTO DISTTABLE VALUES(NULL);

-- basic cases
SELECT f1, f1 IS DISTINCT FROM 2 as "not 2" FROM disttable;
SELECT f1, f1 IS DISTINCT FROM NULL as "not null" FROM disttable;
SELECT f1, f1 IS DISTINCT FROM f1 as "false" FROM disttable;
SELECT f1, f1 IS DISTINCT FROM f1+1 as "not null" FROM disttable;

-- check that optimizer constant-folds it properly
SELECT 1 IS DISTINCT FROM 2 as "yes";
SELECT 2 IS DISTINCT FROM 2 as "no";
SELECT 2 IS DISTINCT FROM null as "yes";
SELECT null IS DISTINCT FROM null as "no";

-- negated form
SELECT 1 IS NOT DISTINCT FROM 2 as "no";
SELECT 2 IS NOT DISTINCT FROM 2 as "yes";
SELECT 2 IS NOT DISTINCT FROM null as "no";
SELECT null IS NOT DISTINCT FROM null as "yes";

-- gpdb start: test inherit/partition table distinct when gp_statistics_pullup_from_child_partition is on
set gp_statistics_pullup_from_child_partition to on;
CREATE TABLE sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id);
insert into sales values (1,'20210202',20), (2,'20210602',9) ,(3,'20211002',100);
select distinct * from sales order by 1;
select distinct sales from sales order by 1;
CREATE TABLE sales_partition (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( START (date '2021-01-01') INCLUSIVE
  END (date '2022-01-01') EXCLUSIVE
  EVERY (INTERVAL '1 month') );
insert into sales_partition values (1,'20210202',20), (2,'20210602',9) ,(3,'20211002',100);
select distinct * from sales_partition order by 1;
select distinct sales_partition from sales_partition order by 1;
DROP TABLE sales;
DROP TABLE sales_partition;

CREATE TABLE cities (
    name            text,
    population      float,
    altitude        int
);
CREATE TABLE capitals (
    state           char(2)
) INHERITS (cities);
select distinct * from cities;
select distinct cities from cities;
DROP TABLE capitals;
DROP TABLE cities;
set gp_statistics_pullup_from_child_partition to off;
-- gpdb end: test inherit/partition table distinct when gp_statistics_pullup_from_child_partition is on
