-- two levels partition, list part followed by range partition
DROP TABLE IF EXISTS sales;
CREATE TABLE sales (trans_id int, sdate date, amount decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (sdate)
SUBPARTITION TEMPLATE
(START (date '2011-01-01') INCLUSIVE
END (date '2011-05-01') EXCLUSIVE
EVERY (INTERVAL '1 month'),
DEFAULT SUBPARTITION outlying_dates )
(
PARTITION asia VALUES ('asia'),
DEFAULT PARTITION other_regions);
