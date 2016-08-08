--start_ignore
DROP TABLE IF EXISTS mpp7002_albums;
--end_ignore

-- Create Partitioned Table
CREATE TABLE mpp7002_albums
 (id INTEGER, performerName VARCHAR, year INTEGER)
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (year)
  (
  START (1964)
  END (1970) INCLUSIVE
  EVERY (1),
  DEFAULT PARTITION probableErrors
  )
 ;

-- Drop a column
-- ALTER TABLE mpp7002_albums DROP COLUMN songList;
ALTER TABLE mpp7002_albums DROP COLUMN performerName;

-- Add a new Child Partition
ALTER TABLE mpp7002_albums
 SPLIT DEFAULT PARTITION
  START (1971) INCLUSIVE
  END (1978) INCLUSIVE
 ;

\d+ mpp7002_albums
