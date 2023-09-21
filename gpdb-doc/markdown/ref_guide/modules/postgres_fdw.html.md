# postgres\_fdw 

The `postgres_fdw` module is a foreign-data wrapper \(FDW\) that you can use to access data stored in a remote PostgreSQL or Greenplum database.

The Greenplum Database `postgres_fdw` module is a modified version of the PostgreSQL `postgres_fdw` module. The module behaves as described in the PostgreSQL [postgres\_fdw](https://www.postgresql.org/docs/12/postgres-fdw.html) documentation when you use it to access a single remote PostgreSQL database. The Greenplum `postgres_fdw` module also includes enhancements that allow you to access multiple remote PostgreSQL servers from a single foreign table.

> **Note** There are some restrictions and limitations when you use this foreign-data wrapper module, described below.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `postgres_fdw` module is installed when you install Greenplum Database. Before you can use the foreign-data wrapper, you must register the `postgres_fdw` extension in each database in which you want to use the foreign-data wrapper. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="using_mrs"></a>Using postgres_fdw to Access Multiple Remote PostgreSQL Servers

Before you [create a foreign table](../sql_commands/CREATE_FOREIGN_TABLE.html) using the `postgres_fdw` foreign-data wrapper, you must configure a server with the [CREATE SERVER](../sql_commands/CREATE_SERVER.html) command. You can use a `postgres_fdw` foreign table to access data that is distributed across multiple remote PostgreSQL servers when you set certain Greenplum and `postgres_fdw`-specific options on the `CREATE SERVER` command:

| Option Name | Description | Value |
|-------------|-----|-------------|
| mpp_execute | Greenplum Database option that identifies the host(s) from which `postgres_fdw` reads or writes data. | Set to `'all segments'`, which, when specified for `postgres_fdw`, translates to all remote PostgreSQL servers specified in `multi_hosts`. |
| num_segments<sup>1</sup> | Greenplum Database option that identifies the number of query executors that Greenplum spawns on the source Greenplum cluster. | Set to the number of remote PostgreSQL servers. If this option is not set, defaults to the number of segments in the local Greenplum cluster.|
| multi_hosts | Space-separated list of remote PostgreSQL server host names. | You must specify exactly `num_segments` number of hosts in the list. |
| multi_ports | Space-separated list of port numbers for the PostgreSQL servers. | You must specify exactly one port number for each host specified in `multi_hosts`, in order. |

<sup>1</sup> The Greenplum query optimizer (GPORCA) can plan and optimize queries only when `num_segments` is equal to the number of segments in the local Greenplum cluster. When `num_segments` is any other value, a query always falls back to the Postgres-based planner.

Setting these options instructs `postgres_fdw` to treat a foreign table that you create that specifes this `SERVER` as a *distributed* foreign table. That is, a foreign table whose underlying data is stored on multiple remote PostgreSQL servers. `postgres_fdw` directs a query on such a foreign table to each PostgreSQL server specified in `multi_hosts`.

An example `CREATE SERVER` command that configures access to PostgreSQL servers running on `pghost1` and `pghost2` follows:

``` sql
CREATE SERVER dist_pgserver
  FOREIGN DATA WRAPPER postgres_fdw
OPTIONS (multi_hosts 'pghost1 pghost2', multi_ports '5432 5555', num_segments '2', mpp_execute 'all segments');
```

## <a id="agg_pushdown"></a>About Aggregate Pushdown Support

`postgres_fdw` supports partial aggregate pushdown for a *distributed* query under the following conditions:

- The aggregate contains no `DISTINCT` or `ORDER BY` clauses.
- The aggregate does not contain `HAVING` clause.
- The aggregate function is not `array_agg()`.
- The query contains no `LIMIT` or `JOIN` clauses.

> **Note** `postgres_fdw` does not support partial aggregate pushdown when the Greenplum query optimizer (GPORCA) is enabled for a query.

## <a id="limit_pushdown"></a>About Limit Pushdown Support

`postgres_fdw` supports limit pushdown for a *distributed* query under the following conditions:

- The query contains no `OFFSET` clause.
- The query contains no aggregates.

## <a id="topic_gp_limit"></a>Distributed postgres_fdw Limitations 

When you use the foreign-data wrapper to access multiple remote PostgreSQL servers, Greenplum Database `postgres_fdw` has the following limitations:

- You must set the `gp_enable_minmax_optimization` server configuration parameter to `off` to enable partial aggregate pushdown.
- `INSERT`, `UPDATE`, and `DELETE` operations on distributed foreign tables are not supported.
- `IMPORT FOREIGN SCHEMA` is not supported.


## <a id="topic_gp_limit"></a>Greenplum Database Limitations 

When you use the foreign-data wrapper to access Greenplum Database, `postgres_fdw` has the following limitations:

-   The `ctid` is not guaranteed to uniquely identify the physical location of a row within its table. For example, the following statements may return incorrect results when the foreign table references a Greenplum Database table:

    ```
    INSERT INTO rem1(f2) VALUES ('test') RETURNING ctid;
    SELECT * FROM ft1, t1 WHERE t1.ctid = '(0,2)'; 
    ```

-   `postgres_fdw` does not support local or remote triggers when you use it to access a foreign table that references a Greenplum Database table.
-   `UPDATE` or `DELETE` operations on a foreign table that references a Greenplum table are not guaranteed to work correctly.


## <a id="topic_info"></a>Additional Module Documentation 

For more information about using foreign tables in Greenplum Database, see [Accessing External Data with Foreign Tables](../../admin_guide/external/g-foreign.html).

For detailed information about this module, refer to the [postgres\_fdw](https://www.postgresql.org/docs/12/postgres-fdw.html) PostgreSQL documentation.

The `postgres_fdw` foreign-data wrapper is included in the Greenplum Database open source github repository. You can view the [source code](https://github.com/greenplum-db/gpdb/tree/main/contrib/postgres_fdw) for the module which is located in the `contrib/` directory.

