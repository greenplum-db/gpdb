-- range partition type, column key: id
DROP TABLE IF EXISTS employee;
CREATE TABLE employee(id int, gender char(1), rank int)
DISTRIBUTED BY (id)
PARTITION BY RANGE (id)
( START (1) INCLUSIVE
  END (3) EXCLUSIVE
  EVERY (1) );