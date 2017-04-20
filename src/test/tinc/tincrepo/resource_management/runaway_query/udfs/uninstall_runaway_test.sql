BEGIN; 

DROP FUNCTION gp_allocate_palloc_test_all_segs(content int, size int, sleep int, crit_section bool);
DROP FUNCTION gp_allocate_palloc_test_one_seg(content int, size int, sleep int, crit_section bool);
DROP FUNCTION gp_allocate_palloc_test_f(content int, size int, sleep int, crit_section bool);

DROP FUNCTION gp_allocate_top_memory_ctxt_test_all_segs(content int, size int, sleep int);
DROP FUNCTION gp_allocate_top_memory_ctxt_test_f(content int, size int, sleep int);

DROP FUNCTION gp_allocate_palloc_gradual_test_all_segs(content int, size int, sleep int);
DROP FUNCTION gp_allocate_palloc_gradual_test_f(content int, size int, sleep int);

DROP FUNCTION gp_inject_fault_test_all_segs(content int, fault_type int, arg int);
DROP FUNCTION gp_inject_fault_test_f(content int, fault_type int, arg int); 

DROP VIEW gp_available_vmem_mb_test_all_segs;
DROP FUNCTION gp_available_vmem_mb_test_f(); 

COMMIT;
