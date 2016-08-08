alter table heap_table_for_alter set tablespace heap_ts2;
\d heap_table_for_alter
INSERT INTO heap_table_for_alter VALUES ('sync1_heap1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1
', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '
((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
DROP table heap_table_for_alter;
drop tablespace heap_ts1;
drop tablespace heap_ts2;
