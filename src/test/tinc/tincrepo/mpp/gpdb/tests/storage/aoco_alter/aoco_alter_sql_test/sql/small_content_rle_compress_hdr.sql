-- Small content header with RLE header inside 
drop table if exists sml_rle_hdr;
create table sml_rle_hdr (a int) with (appendonly=true, orientation=column, compresstype='rle_type');
insert into sml_rle_hdr values (1),(1),(1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),
    (20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7),
    (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
alter table sml_rle_hdr add column b float default random();
-- update / insert / select after this operation
insert into sml_rle_hdr values (-1,-1.1);
update sml_rle_hdr set b = b + 10 where a = -1;
