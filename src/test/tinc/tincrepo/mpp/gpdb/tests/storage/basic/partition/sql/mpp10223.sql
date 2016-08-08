drop table if exists mpp10223 cascade;

CREATE TABLE "mpp10223"
(
rnc VARCHAR(100),
wbts VARCHAR(100),
axc VARCHAR(100),
vptt VARCHAR(100),
vcct VARCHAR(100),
agg_level CHAR(5),
period_start_time TIMESTAMP WITH TIME ZONE,
load_time TIMESTAMP WITH TIME ZONE DEFAULT now(),
interval INTEGER,
totcellsegress double precision,
totcellsingress double precision,
 
  CONSTRAINT "mpp10223_pk" PRIMARY KEY (rnc,wbts,axc,vptt,vcct,agg_level,period_start_time)
)
 
DISTRIBUTED BY (rnc,wbts,axc,vptt,vcct)
 
PARTITION BY LIST (AGG_LEVEL)
  SUBPARTITION BY RANGE (PERIOD_START_TIME)
(
  PARTITION min15part  VALUES ('15min')
    (
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                                               END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION hourpart   VALUES ('hour')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                                               END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION daypart    VALUES ('day')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                                               END (date '2999-12-31') EXCLUSIVE
    )
);


select * from pg_partitions where tablename = 'mpp10223';


ALTER TABLE mpp10223 ALTER PARTITION min15part SPLIT PARTITION  P_FUTURE AT ('2010-06-25') INTO (PARTITION P20010101, PARTITION P_FUTURE);


select * from pg_partitions where tablename = 'mpp10223';

