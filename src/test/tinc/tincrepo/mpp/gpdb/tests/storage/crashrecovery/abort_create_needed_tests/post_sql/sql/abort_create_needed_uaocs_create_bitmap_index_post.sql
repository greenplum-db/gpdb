-- @product_version gpdb: [4.3.0.0-]
begin;
CREATE INDEX abort_create_needed_cr_uaocs_bitmap_idx ON abort_create_needed_cr_uaocs_table_bitmap_index USING bitmap (numeric_col);
insert into abort_create_needed_cr_uaocs_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into abort_create_needed_cr_uaocs_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into abort_create_needed_cr_uaocs_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into abort_create_needed_cr_uaocs_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;
commit;

select count(*) AS only_visi_tups_ins  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = false;
update abort_create_needed_cr_uaocs_table_bitmap_index set char_vary_col = char_vary_col || '_new' where bigint_col = 100;
select count(*) AS only_visi_tups_upd  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = false;
delete from abort_create_needed_cr_uaocs_table_bitmap_index  where bigint_col >= 50;
select count(*) AS only_visi_tups  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = false;
vacuum abort_create_needed_cr_uaocs_table_bitmap_index;
select count(*) AS only_visi_tups_vacuum  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from abort_create_needed_cr_uaocs_table_bitmap_index;
set gp_select_invisible = false;

drop index abort_create_needed_cr_uaocs_bitmap_idx;
drop table abort_create_needed_cr_uaocs_table_bitmap_index;
