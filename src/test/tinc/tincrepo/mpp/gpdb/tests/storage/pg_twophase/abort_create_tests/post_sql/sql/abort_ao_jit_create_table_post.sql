-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE ao_jit_table( phase text,a int,aol001 char DEFAULT 'z',aol002 numeric,aol003 boolean DEFAULT false,aol004 bit(3) DEFAULT '111',
aol005 text DEFAULT 'pookie', aol006 integer[] DEFAULT '{5, 4, 3, 2, 1}', aol007 character varying(512) DEFAULT 'Now is the time', aol008 character varying DEFAULT 'Now is the time', 
aol009 character varying(512)[], aol010 numeric(8),aol011 int,aol012 double precision, aol013 bigint, aol014 char(8), aol015 bytea,aol016 timestamp with time zone,aol017 interval, 
aol018 cidr, aol019 inet, aol020 macaddr,aol022 money, aol024 timetz,aol025 circle, aol026 box, aol027 name,aol028 path, aol029 int2, aol031 bit varying(256),
aol032 date, aol034 lseg,aol035 point,aol036 polygon,aol037 real,aol039 time, aol040 timestamp ) WITH (appendonly=true) tablespace ao_ts ;
\d ao_jit_table
INSERT INTO ao_jit_table VALUES ('sync1_ao1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1
', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '
((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
DROP table ao_jit_table;
drop tablespace ao_ts;
