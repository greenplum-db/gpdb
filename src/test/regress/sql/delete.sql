CREATE TABLE delete_test (
    id SERIAL PRIMARY KEY,
    a INT,
    b text
) DISTRIBUTED BY (id);

INSERT INTO delete_test (a) VALUES (10);
INSERT INTO delete_test (a, b) VALUES (50, repeat('x', 10000));
INSERT INTO delete_test (a) VALUES (100);

-- allow an alias to be specified for DELETE's target table
DELETE FROM delete_test AS dt WHERE dt.a > 75;

-- if an alias is specified, don't allow the original table name
-- to be referenced
DELETE FROM delete_test dt WHERE delete_test.a > 25;

SELECT id, a, char_length(b) FROM delete_test;

-- delete a row with a TOASTed value
DELETE FROM delete_test WHERE a > 25;

SELECT id, a, char_length(b) FROM delete_test;

DROP TABLE delete_test;

-- delete query for issue 12161, found by sqlsmith
explain(costs off) delete from information_schema.sql_languages
where
   cast(null as float8) = case when ((select tgattr from pg_catalog.pg_trigger limit 1 offset 6)
               <@ (select distkey from pg_catalog.gp_distribution_policy limit 1 offset 5))
   then (select pg_catalog.stddev(sotusize) from gp_toolkit.gp_size_of_table_uncompressed)
   else (select checkpoint_write_time from pg_catalog.pg_stat_bgwriter limit 1 offset 4)
   end
returning
 cast(nullif(pg_catalog.pg_xlog_location_diff(
      cast(case when (cast(null as box) ?# cast(null as box))
          and ((select typtype from pg_catalog.pg_type limit 1 offset 2)
               <> cast(null as "char")) then (select sent_location from pg_catalog.pg_stat_replication limit 1 offset 5)
           else cast(coalesce(cast(coalesce((select sent_location from pg_catalog.gp_stat_replication limit 1 offset 4)
              ,
            cast(null as pg_lsn)) as pg_lsn),
          case when cast(null as date) > cast(null as "timestamp") then (select restart_lsn from pg_catalog.pg_replication_slots limit 1 offset 75)
               else cast(null as pg_lsn) end
            ) as pg_lsn) end
         as pg_lsn),
      cast(pg_catalog.pg_last_xlog_receive_location() as pg_lsn)), cast(null as "numeric")) as "numeric") as c24;

begin;
delete from information_schema.sql_languages
where
   cast(null as float8) = case when ((select tgattr from pg_catalog.pg_trigger limit 1 offset 6)
               <@ (select distkey from pg_catalog.gp_distribution_policy limit 1 offset 5))
   then (select pg_catalog.stddev(sotusize) from gp_toolkit.gp_size_of_table_uncompressed)
   else (select checkpoint_write_time from pg_catalog.pg_stat_bgwriter limit 1 offset 4)
   end
returning
 cast(nullif(pg_catalog.pg_xlog_location_diff(
      cast(case when (cast(null as box) ?# cast(null as box))
          and ((select typtype from pg_catalog.pg_type limit 1 offset 2)
               <> cast(null as "char")) then (select sent_location from pg_catalog.pg_stat_replication limit 1 offset 5)
           else cast(coalesce(cast(coalesce((select sent_location from pg_catalog.gp_stat_replication limit 1 offset 4)
              ,
            cast(null as pg_lsn)) as pg_lsn),
          case when cast(null as date) > cast(null as "timestamp") then (select restart_lsn from pg_catalog.pg_replication_slots limit 1 offset 75)
               else cast(null as pg_lsn) end
            ) as pg_lsn) end
         as pg_lsn),
      cast(pg_catalog.pg_last_xlog_receive_location() as pg_lsn)), cast(null as "numeric")) as "numeric") as c24;
rollback;
