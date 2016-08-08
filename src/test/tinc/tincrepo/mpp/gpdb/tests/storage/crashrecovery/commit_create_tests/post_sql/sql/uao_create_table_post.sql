-- @product_version gpdb: [4.3.0.0-]
INSERT INTO cr_uao_table VALUES ('sync1_heap1',1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

INSERT INTO cr_uao_table VALUES ('sync1_heap1',2,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

select count(*) AS only_visi_tups_ins  from cr_uao_table;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from cr_uao_table;
set gp_select_invisible = false;
update cr_uao_table set  col001 = 'e'   where a=1;
select count(*) AS only_visi_tups_upd  from cr_uao_table;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from cr_uao_table;
set gp_select_invisible = false;
delete from cr_uao_table  where a =  2;
select count(*) AS only_visi_tups  from cr_uao_table;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from cr_uao_table;
set gp_select_invisible = false;

DROP TABLE cr_uao_table;
