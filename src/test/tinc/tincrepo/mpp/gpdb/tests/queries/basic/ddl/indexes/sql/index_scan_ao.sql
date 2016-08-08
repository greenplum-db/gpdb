create index idx_i_ao on ao_tab (i);
select count(*) from ao_tab where i >= 0;
drop index idx_i_ao;

-- Edge case: without lossy pages
truncate table ao_tab;
insert into ao_tab select generate_series(1, 65536);
create index idx_i_ao on ao_tab (i);
select count(*) from ao_tab where i >= 0;
drop index idx_i_ao;
