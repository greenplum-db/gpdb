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
CREATE INDEX st_GistIndex2 ON st_GistTable1 USING GiST (property);
CREATE INDEX st_GistIndex3 ON st_GistTable1 USING GiST (property);

ALTER INDEX st_GistIndex1 RENAME TO new_st_GistIndex1;
ALTER INDEX st_GistIndex2 SET (fillfactor =100);
ALTER INDEX st_GistIndex3 SET (fillfactor =100);
ALTER INDEX st_GistIndex3 RESET (fillfactor) ;
