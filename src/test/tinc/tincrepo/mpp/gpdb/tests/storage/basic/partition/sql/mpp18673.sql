--
-- @product_version gpdb: [4.3.5.0-]
--
-- MPP-18673: Split partition should not result in relcache reference
-- corruption.
SET gp_test_system_cache_flush_force = plain;
DROP TABLE IF EXISTS splitpart;
CREATE TABLE splitpart (id int, date date, amt decimal(10,2))
    WITH (appendonly=true)
    DISTRIBUTED BY (id)
    PARTITION BY RANGE (date) (
        PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE,
        PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE,
        PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE
        END (date '2014-01-01') EXCLUSIVE);

\d+ splitpart

INSERT INTO splitpart VALUES(1,'2013-07-05',19.20);
INSERT INTO splitpart VALUES(2,'2013-08-15',10.20);
INSERT INTO splitpart VALUES(3,'2013-09-09',14.20);
SELECT count(*) FROM splitpart;

-- Add default partition
ALTER TABLE splitpart ADD DEFAULT PARTITION part_others
    WITH (appendonly=true);
\d+ splitpart

INSERT INTO splitpart VALUES(10,'2013-04-22',12.52);
INSERT INTO splitpart VALUES(11,'2013-02-22',13.53);
INSERT INTO splitpart VALUES(12,'2013-01-22',14.54);
SELECT * FROM splitpart ORDER BY date;

-- Split default partition
ALTER TABLE splitpart SPLIT DEFAULT PARTITION
    START(date '2013-01-01') END(date '2013-03-01')
    INTO (PARTITION p1, PARTITION part_others);
\d+ splitpart
SELECT * FROM splitpart ORDER BY date;

-- Try basic operations on newly created paritions
UPDATE splitpart SET amt=20.5 WHERE date='2013-02-22';
SELECT * FROM splitpart ORDER BY date;
DELETE FROM splitpart WHERE date='2013-01-22';
SELECT * FROM splitpart ORDER BY date;
