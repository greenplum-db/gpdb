-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP EXTERNAL TABLE IF EXISTS exttab_conc_1 CASCADE;
DROP TABLE IF EXISTS TEST_AOCO_LOAD1 CASCADE;

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_conc_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/data.tbl') FORMAT 'TEXT' (DELIMITER '|') 
SEGMENT REJECT LIMIT 100;

CREATE TABLE TEST_AOCO_LOAD1 (i int, j text) WITH (appendonly=true, orientation=column);

DROP FUNCTION IF EXISTS duplicate_check_aoseg(tablename text, OUT content int, OUT segno int, OUT repeat_count int);
CREATE FUNCTION duplicate_check_aoseg(tablename text, OUT content int, OUT segno int, OUT repeat_count int) returns SETOF record AS 
$$
declare STATEMENT TEXT;
var_match record;
BEGIN
  STATEMENT := 'select gp_segment_id,segno, count(gp_segment_id) as count from gp_dist_random(''' || tablename || ''') GROUP BY gp_segment_id,segno HAVING ( COUNT(gp_segment_id) > 1 ) order by gp_segment_id';
  FOR var_match IN EXECUTE(STATEMENT) LOOP
	content := var_match.gp_segment_id;
	segno := var_match.segno;
	repeat_count := var_match.count;
       RETURN NEXT;
    END LOOP;
END;
$$ LANGUAGE 'plpgsql';

DROP FUNCTION IF EXISTS duplicate_check_visimap(tablename text, OUT content int, OUT segno int, OUT first_row_no int, OUT repeat_count int);
CREATE FUNCTION duplicate_check_visimap(tablename text, OUT content int, OUT segno int, OUT first_row_no int, OUT repeat_count int) returns SETOF record AS 
$$
declare STATEMENT TEXT;
var_match record;
BEGIN
  STATEMENT := 'select gp_segment_id,segno, first_row_no, count(gp_segment_id) as count from gp_dist_random(''' || tablename || ''') GROUP BY gp_segment_id,segno,first_row_no HAVING ( COUNT(gp_segment_id) > 1 ) order by gp_segment_id';
  FOR var_match IN EXECUTE(STATEMENT) LOOP
	content := var_match.gp_segment_id;
	segno := var_match.segno;
	repeat_count := var_match.count;
       RETURN NEXT;
    END LOOP;
END;
$$ LANGUAGE 'plpgsql';
