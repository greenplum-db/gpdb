-- two levels partition table, range(year) and range(month)
DROP TABLE IF EXISTS sales;
CREATE TABLE sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
SUBPARTITION BY RANGE (month)
SUBPARTITION TEMPLATE (
    START (1) END (3) EVERY (1),
    DEFAULT SUBPARTITION other_months )
(START (2002) END (2004) EVERY (1),
  DEFAULT PARTITION outlying_years );
