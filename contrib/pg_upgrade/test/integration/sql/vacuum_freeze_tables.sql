CREATE TABLE vf_tbl_heap (a int, b int);
INSERT INTO vf_tbl_heap SELECT i, i FROM GENERATE_SERIES(1,10)i;
VACUUM FREEZE vf_tbl_heap;

CREATE TABLE vf_tbl_ao (a int, b int) WITH (appendonly=true);
CREATE INDEX vf_tbl_ao_idx1 ON vf_tbl_ao(b);
INSERT INTO vf_tbl_ao SELECT i, i FROM GENERATE_SERIES(1,10)i;
VACUUM FREEZE vf_tbl_ao;

CREATE TABLE vf_tbl_aoco (a int, b int) WITH (appendonly=true, orientation=column);
CREATE INDEX vf_tbl_aoco_idx1 ON vf_tbl_aoco(b);
INSERT INTO vf_tbl_aoco SELECT i, i FROM GENERATE_SERIES(1,10)i;
VACUUM FREEZE vf_tbl_aoco;
