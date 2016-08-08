create index idx_i_heapaoco on heapaoco_part (i);
select count(*) from heapaoco_part where i >= 0;
drop index idx_i_heapaoco;

-- Edge case: without lossy pages
truncate table heapaoco_part;
insert into heapaoco_part select generate_series(1, 65536);
create index idx_i_heapaoco on heapaoco_part (i);
select count(*) from heapaoco_part where i >= 0;
drop index idx_i_heapaoco;
