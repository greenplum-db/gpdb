-- start_matchsubs
-- m/^INFO:/
-- s/^INFO:/GP_IGNORE:INFO:/
-- end_matchsubs
create table mpp8630(x int, y gpxlogloc) distributed randomly;
insert into mpp8630 select i, '(1/2)'::gpxlogloc from generate_series(1,20000) i;
insert into mpp8630 select i, '(1/3)'::gpxlogloc from generate_series(1,20000) i;
analyze verbose mpp8630;
drop table mpp8630;
