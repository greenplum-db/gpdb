-- multi level partition, list part followed by range partition, every 2 months
CREATE TABLE dest_sales (dest_trans_id int, dest_date date, dest_amount decimal(9,2), dest_region text)
DISTRIBUTED BY (dest_trans_id)
PARTITION BY LIST (dest_region)
SUBPARTITION BY RANGE (dest_date)
SUBPARTITION TEMPLATE
(START (date '2011-01-01') INCLUSIVE
END (date '2011-05-01') EXCLUSIVE
EVERY (INTERVAL '2 months'),
DEFAULT SUBPARTITION outlying_dates )
(
PARTITION asia VALUES ('asia'),
DEFAULT PARTITION other_regions);
