-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d pg2_hybrid_part_a0
INSERT INTO pg2_hybrid_part_a0 VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
select count(*) from pg2_hybrid_part_a0;
drop table pg2_hybrid_part_a0;
