-- @gucs gp_create_table_random_default_distribution=off
create table co_compr02_1( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
 partition by range(b) (
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, compresstype=quicklz));

insert into co_compr02_1 values(1, generate_series(1,14), 4, 5); 
