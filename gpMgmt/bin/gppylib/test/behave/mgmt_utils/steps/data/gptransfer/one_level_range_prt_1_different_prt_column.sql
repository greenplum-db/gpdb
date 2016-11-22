-- range partition type, column key: id
DROP TABLE IF EXISTS employee;
CREATE TABLE employee(id int, rank int, gender char(1))
DISTRIBUTED BY (id)
PARTITION BY RANGE (rank)
( START (1) INCLUSIVE
  END (3) EXCLUSIVE
  EVERY (1) );