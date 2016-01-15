-- range partition on rank
DROP TABLE IF EXISTS employee;
CREATE TABLE employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id)
PARTITION BY RANGE (rank)
( START (1) INCLUSIVE
  END (3) EXCLUSIVE
  EVERY (1) );

-- range partition on erank
DROP TABLE IF EXISTS e_employee;
CREATE TABLE e_employee(eid int, erank int, egender char(1))
DISTRIBUTED BY (eid)
PARTITION BY RANGE (erank)
( START (1) INCLUSIVE
  END (3) EXCLUSIVE
  EVERY (1) );
