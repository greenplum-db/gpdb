# pg_buffercache 

The `pg_buffercache` module provides five views for obtaining cluster-wide shared buffer metrics:

- `gp_buffercache`
- `gp_buffercache_summary`
- `gp_buffercache_usage_counts`
- `gp_buffercache_summary_aggregated` 
- `gp_buffercache_usage_counts_aggregated`

The `gp_buffercache` view is a cluster-wide view that displays the [`pg_buffercache`](https://www.postgresql.org/docs/current/pgbuffercache.html) information from the coordinator and every primary segment for each buffer in the shared cache.

The `gp_buffercache_summary` view is a cluster-wide view that displays the [`pg_buffercache_summary`](https://www.postgresql.org/docs/16/pgbuffercache.html#PGBUFFERCACHE-SUMMARY) information from the coordinator and each primary segment, summarized as one row per segment. Similar and more detailed information is provided by `gp_buffercache`, but `gp_buffercache_summary` is significantly better performance-wise.

The `gp_buffercache_usage_counts` view is a cluster-wide view that displays the [`pg_puffercache_usage_counts`](https://www.postgresql.org/docs/16/pgbuffercache.html#PGBUFFERCACHE-USAGE-COUNTS) information from the coordinator and each primary segment returning summarized states of all shared buffers, aggregated over the possible usage count values. Similar and more detailed information is provided by `gp_buffercache`, but `gp_buffercache_usage_counts` is significantly better performance-wise.

The `gp_buffercache_summary_aggregated` view returns a single row with the aggregated output of `gp_buffercache_summary`.

The `gp_buffercache_usage_counts_aggregated` view returns a single row with the aggregated output of `gp_buffercache_usage_counts`.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `pg_buffercache` module is installed when you install VMware Greenplum. Before you can use any of the functions defined in the module, you must register the `pg_buffercache` extension in each database in which you want to use the functions:

```
CREATE EXTENSION pg_buffercache;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

See [pg\_buffercache](https://www.postgresql.org/docs/12/pgbuffercache.html) in the PostgreSQL documentation for detailed information about the individual functions in this module.

