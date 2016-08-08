CREATE TABLE mdt_st_GistTable1 (
 id INTEGER,
 property BOX, 
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO mdt_st_GistTable1 (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO mdt_st_GistTable1 (id, property) VALUES (2, '( (0,0), (2,2) )');

CREATE INDEX mdt_st_GistIndex1 ON mdt_st_GistTable1 USING GiST (property);
CREATE INDEX mdt_st_GistIndex2 ON mdt_st_GistTable1 USING GiST (property);
CREATE INDEX mdt_st_GistIndex3 ON mdt_st_GistTable1 USING GiST (property);

ALTER INDEX mdt_st_GistIndex1 RENAME TO mdt_new_st_GistIndex1;
ALTER INDEX mdt_new_st_GistIndex1 RENAME TO mdt_st_GistIndex1;
ALTER INDEX mdt_st_GistIndex2 SET (fillfactor =100);
ALTER INDEX mdt_st_GistIndex3 SET (fillfactor =100);
ALTER INDEX mdt_st_GistIndex3 RESET (fillfactor) ;

drop table mdt_st_GistTable1;
