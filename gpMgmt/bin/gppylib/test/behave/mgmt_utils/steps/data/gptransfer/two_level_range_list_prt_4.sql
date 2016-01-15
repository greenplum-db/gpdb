-- multi level partition, list part followed by range partition, with 5 subpartitions
CREATE TABLE sales (trans_id int, sdate date, amount decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (date)
SUBPARTITION TEMPLATE
(START (date '2011-01-01') INCLUSIVE
END (date '2011-03-01') EXCLUSIVE
EVERY (INTERVAL '1 month'),
DEFAULT SUBPARTITION outlying_dates )
( PARTITION usa VALUES ('usa'),
PARTITION other_asia VALUES ('asia'),
DEFAULT PARTITION other_regions);

