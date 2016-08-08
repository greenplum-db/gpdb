-- Bulk dense content header with RLE compression
drop table if exists bulk_rle_tab;
create table bulk_rle_tab (a int) with (appendonly=true, orientation=column, compresstype='rle_type', compresslevel=3, checksum=true);
insert into bulk_rle_tab select i/50 from generate_series(1, 1000000)i;
insert into bulk_rle_tab values (1),(1),(1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),
    (20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7),
    (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
alter table bulk_rle_tab add column b varchar default 'abc' encoding(blocksize=8192);
insert into bulk_rle_tab values (-1, 'xyz');
update bulk_rle_tab set b = 'green' where a = -1;
