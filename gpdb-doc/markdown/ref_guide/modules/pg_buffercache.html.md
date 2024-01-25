# pg_buffercache 

The `pg_buffercache` module provides a new view for obtaining cluster-wide shared buffer metrics: `gp_buffercache`

The `gp_buffercache` view is a cluster-wide view that displays the [`pg_buffercache`](https://www.postgresql.org/docs/current/pgbuffercache.html) information from the coordinator and every primary segment for each buffer in the shared cache.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `pg_buffercache` module is installed when you install VMware Greenplum. Before you can use any of the functions defined in the module, you must register the `pg_buffercache` extension in each database in which you want to use the functions:

```
CREATE EXTENSION pg_buffercache;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

See [pg\_buffercache](https://www.postgresql.org/docs/12/pgbuffercache.html) in the PostgreSQL documentation for detailed information about the individual functions in this module.

