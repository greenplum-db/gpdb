-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
1: BEGIN;
1: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: select 'dummy select to establish snapshot';
2: BEGIN;
2: insert into reindex_serialize_tab_heap values(99,'green',now(),10,15.10);
2: COMMIT;
1: select a,b,d,e,f,g from reindex_serialize_tab_heap order by 1;
1: select a,b,d,e,f,g from reindex_serialize_tab_heap where a = 99;
1: reindex table reindex_serialize_tab_heap;
1: COMMIT;
4: select a,b,d,e,f,g from reindex_serialize_tab_heap where a = 99;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select a,b,d,e,f,g from reindex_serialize_tab_heap where a = 99;
