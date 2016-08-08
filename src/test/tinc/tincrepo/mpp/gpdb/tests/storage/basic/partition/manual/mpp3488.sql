select mode,mppsessionid,count(*) from pg_locks group by mode,mppsessionid;
select count(*) from pg_partitions;
