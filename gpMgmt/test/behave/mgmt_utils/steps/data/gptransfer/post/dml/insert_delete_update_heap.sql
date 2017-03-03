\c gptest;

DELETE FROM heap_tbl WHERE a1 < 5;
DELETE FROM heap_tbl WHERE a1 < 10;
DELETE FROM heap_tbl WHERE a1 < 15;

UPDATE heap_tbl SET a5='c' WHERE a1 < 20;
UPDATE heap_tbl SET a5='f' WHERE a1 > 500;

SELECT id, a1, a5 FROM heap_tbl;

SELECT count(*) FROM heap_tbl;
