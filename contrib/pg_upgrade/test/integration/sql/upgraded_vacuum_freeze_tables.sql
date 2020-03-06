-- should be able to vacuum freeze the tables
VACUUM FREEZE vf_tbl_heap;
VACUUM FREEZE vf_tbl_ao;
VACUUM FREEZE vf_tbl_aoco;

-- should be able to create a new table without any warnings related to vacuum
CREATE TABLE upgraded_vf_tbl_heap (LIKE vf_tbl_heap);
INSERT INTO upgraded_vf_tbl_heap SELECT * FROM vf_tbl_heap;
VACUUM FREEZE upgraded_vf_tbl_heap;
SELECT * FROM upgraded_vf_tbl_heap;
