-- money check

-- In PostgreSQL, this test uses WITH OIDS, but in GPDB, OIDs are not unique
-- across segments, so use a 'serial' column instead.
CREATE TABLE moneytmp (a money, oid serial);

\copy moneytmp (a) from 'data/cash.data'

SET enable_seqscan=on;

SELECT count(*) FROM moneytmp WHERE a <  '22649.64';

SELECT count(*) FROM moneytmp WHERE a <= '22649.64';

SELECT count(*) FROM moneytmp WHERE a  = '22649.64';

SELECT count(*) FROM moneytmp WHERE a >= '22649.64';

SELECT count(*) FROM moneytmp WHERE a >  '22649.64';

CREATE INDEX moneyidx ON moneytmp USING gist ( a );

SET enable_seqscan=off;

SELECT count(*) FROM moneytmp WHERE a <  '22649.64'::money;

SELECT count(*) FROM moneytmp WHERE a <= '22649.64'::money;

SELECT count(*) FROM moneytmp WHERE a  = '22649.64'::money;

SELECT count(*) FROM moneytmp WHERE a >= '22649.64'::money;

SELECT count(*) FROM moneytmp WHERE a >  '22649.64'::money;
