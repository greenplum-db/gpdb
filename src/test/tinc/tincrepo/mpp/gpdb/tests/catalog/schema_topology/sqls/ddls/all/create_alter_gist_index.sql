-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description create and alter gist index

-- start_ignore
DROP TABLE IF EXISTS st_GistTable1;
-- end_ignore

CREATE TABLE st_GistTable1 (
 id INTEGER,
 property BOX, 
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO st_GistTable1 (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO st_GistTable1 (id, property) VALUES (2, '( (0,0), (2,2) )');

CREATE INDEX st_GistIndex1 ON st_GistTable1 USING GiST (property);


-- Create some functions that will help us add a large volume of data.
CREATE OR REPLACE FUNCTION st_TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION st_insertIntost_GistTable1 (seed INTEGER) RETURNS VOID
AS
$$
DECLARE 
   str1 VARCHAR;
   ss VARCHAR;
   s2 VARCHAR;
BEGIN
   ss = CAST(seed AS VARCHAR);
   s2 = CAST((seed - 1) AS VARCHAR);
   str1 = '((' || ss || ', ' || ss || '), (' || s2 || ', ' || s2 || '))';
   INSERT INTO st_GistTable1(id, property) VALUES (seed, st_TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE OR REPLACE FUNCTION st_insertManyIntost_GistTable1 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM st_insertIntost_GistTable1(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Insert approximately 2000 records.
SELECT st_insertManyIntost_GistTable1(3, 2000);

-- Force the server to use indexes.
SET enable_seqscan = FALSE;

ANALYZE st_GistTable1;

SELECT COUNT(*) FROM st_GistTable1;

SELECT id, property AS "ProperTee" FROM st_GistTable1
 WHERE property ~= '( (999,999), (998,998) )';

CREATE INDEX st_GistIndex2 ON st_GistTable1 USING GiST (property);
CREATE INDEX st_GistIndex3 ON st_GistTable1 USING GiST (property);

ALTER INDEX st_GistIndex1 RENAME TO new_st_GistIndex1;
ALTER INDEX new_st_GistIndex1 RENAME TO st_GistIndex1;
ALTER INDEX st_GistIndex2 SET (fillfactor =100);
ALTER INDEX st_GistIndex3 SET (fillfactor =100);
ALTER INDEX st_GistIndex3 RESET (fillfactor) ;


