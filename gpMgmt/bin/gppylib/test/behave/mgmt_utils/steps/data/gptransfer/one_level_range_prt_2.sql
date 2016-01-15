-- partition table 
DROP TABLE IF EXISTS sales;
CREATE TABLE sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (month)
(START (1) END (3) EVERY (1),
DEFAULT PARTITION other_months );
