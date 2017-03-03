   CREATE TABLE co_gist (id INTEGER,property BOX,filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Located on the Eastern coast of Japan, the six nuclear power reactors at Daiichi are boiling water reactors. A massive earthquake on 11 March disabled off-site power to the plant and triggered the automatic shutdown of the three operating reactors') WITH (orientation='column',appendonly=true) ;

   INSERT INTO co_gist (id, property) VALUES (1, '( (0,0), (1,1) )');
   INSERT INTO co_gist (id, property) VALUES (2, '( (0,0), (2,2) )');
   INSERT INTO co_gist (id, property) VALUES (3, '( (0,0), (3,3) )');
   INSERT INTO co_gist (id, property) VALUES (4, '( (0,0), (4,4) )');
   INSERT INTO co_gist (id,property) select  id+2, property from co_gist;
   INSERT INTO co_gist (id,property) select  id+2, property from co_gist;

   CREATE INDEX co_gist_idx ON co_gist USING GiST (property);
