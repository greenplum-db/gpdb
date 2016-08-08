-- MPP-6589
-- Johnny Soedomo
CREATE SCHEMA mpp6589;
CREATE TABLE mpp6589.mpp6589
(
  id bigint,
  day_dt date
)
DISTRIBUTED BY (id)
PARTITION BY RANGE(day_dt)
          (
          PARTITION p20090312  END ('2009-03-12'::date)
          );

ALTER TABLE mpp6589.mpp6589 SPLIT PARTITION p20090312 AT( '20090310' ) INTO( PARTITION p20090309, PARTITION p20090312_tmp);

drop schema mpp6589 cascade;
