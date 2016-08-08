--
-- This test case will be skipped if # of primary is > 2
--
create table test ( a int, b text) distributed by (a);

insert into test values (generate_series(1,5),'test_1');

-- Negative Test of using 2 identical gpfdist URLS. Should Fail

CREATE WRITABLE EXTERNAL TABLE tbl_wet_2gpfdist_identical ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist.tbl', 'gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null'  ) ;

-- Negative Test of using more gpfdist URLS than valid primary segments. Should Fail

CREATE WRITABLE EXTERNAL TABLE tbl_wet_multiple_gpfdist ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist1.tbl', 'gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist2.tbl','gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist3.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null'  ) ;

insert into tbl_wet_multiple_gpfdist select * from test;


