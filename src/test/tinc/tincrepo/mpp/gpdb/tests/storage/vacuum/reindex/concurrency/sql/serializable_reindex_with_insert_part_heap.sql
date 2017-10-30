-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
1: BEGIN;
1: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: select 'dummy select to establish snapshot';
2: BEGIN;
2:insert into reindex_serialize_ins_tab_heap_part(id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57,650), (68, 660) )', '( (76, 76), 76)' from generate_series(1111,1112) i ;
2: COMMIT;
1: select id,owner,target,v,f from reindex_serialize_ins_tab_heap_part where id > 1000 order by 1;
1: select id,owner,target,v,f from reindex_serialize_ins_tab_heap_part where id = 1111;
1: reindex table reindex_serialize_ins_tab_heap_part;
1: COMMIT;
4: select id,owner,target,v,f from reindex_serialize_ins_tab_heap_part where id > 1000 order by 1;
4: select id,owner,target,v,f from reindex_serialize_ins_tab_heap_part where id = 1111;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select id,owner,target,v,f from reindex_serialize_ins_tab_heap_part where id = 1111;
