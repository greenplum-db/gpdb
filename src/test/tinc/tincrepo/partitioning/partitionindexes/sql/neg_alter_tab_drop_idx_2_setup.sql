DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
CREATE INDEX idx1 on pt_lt_tab(col2);
ANALYZE pt_lt_tab;
