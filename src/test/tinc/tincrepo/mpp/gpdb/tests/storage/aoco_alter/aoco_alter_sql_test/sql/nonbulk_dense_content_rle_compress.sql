-- Non-bulk dense content header with RLE compression
-- This will  insert more rows than a small content header can accommodate in the same insert statement
drop table if exists nonbulk_rle_tab;
create table nonbulk_rle_tab (a int) with (appendonly=true, orientation=column, compresstype='rle_type', checksum=true);
insert into nonbulk_rle_tab select i/50 from generate_series(1, 1000000)i;
alter table nonbulk_rle_tab add column b int default round(random()*100);
insert into nonbulk_rle_tab values (-1,-5);
update nonbulk_rle_tab set b = b + 3 where a = -1;
