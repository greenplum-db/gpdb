create index idx_i_aocopart on aoco_part (i);
select count(*) from aoco_part where i >= 0;
drop index idx_i_aocopart;

-- Edge case: without lossy pages
truncate table aoco_part;
insert into aoco_part select generate_series(1, 65536);
create index idx_i_aocopart on aoco_part (i);
select count(*) from aoco_part where i >= 0;
drop index idx_i_aocopart;
