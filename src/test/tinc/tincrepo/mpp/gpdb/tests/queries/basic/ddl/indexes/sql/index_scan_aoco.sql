create index idx_i_aoco on aoco_tab (i);
select count(*) from aoco_tab where i >= 0;
drop index idx_i_aoco;

-- Edge case: without lossy pages
truncate table aoco_tab;
insert into aoco_tab select generate_series(1, 65536);
create index idx_i_aoco on aoco_tab (i);
select count(*) from aoco_tab where i >= 0;
drop index idx_i_aoco;
