create index idx_i_heap on heap_tab (i);
select count(*) from heap_tab where i >= 0;
drop index idx_i_heap;

-- Edge case: without lossy pages
truncate table heap_tab;
insert into heap_tab select generate_series(1, 65536);
create index idx_i_heap on heap_tab (i);
select count(*) from heap_tab where i >= 0;
drop index idx_i_heap;
