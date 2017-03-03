-- two levels partition table, range(date) and list(regin)
DROP TABLE IF EXISTS sales;

CREATE TABLE sales (trans_id int, date date, src_amount decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (trans_id)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
(
  SUBPARTITION asia VALUES ('asia'),
  DEFAULT SUBPARTITION other_regions)
(START (1) INCLUSIVE
END (3) EXCLUSIVE
EVERY (1),
DEFAULT PARTITION outlying_dates );
