# gpmt gpstatscheck 

This tool can be used to verify if all tables involved in a query have optimal statistics. When a query is running slower than expected, it is possible that outdated or invalid statistics on the tables involved in the query are causing the slowness. This can happen if new data has been loaded into the table but `analyze` was never run, so the database is using wrong statistics when generating the query plan.

## <a id="usage"></a>Usage 

```
gpmt gpstatscheck -f <query-file>
[-p <port>] [-d <database>] 
```

## <a id="opts"></a>Options 

-f
:   File containing the query to analyze.

-p
:   Database port.

-d
:   Database where the query is being run.

## <a id="exs"></a>Examples 

Run the query in `query1.sql` in database `Postgres` and check for missing stats.

```
gpmt gpstatscheck -f query1.sql -d postgres
```

If invalid statistics are detected the tool will generate a script listing the suggested commands to run. For example:

```
$ cat gpstatscheck_20160926_134946.sql
ANALYZE public.nums;
```

You can then run the provided script against the affected database:

```
$ psql -p 5432 -d postgres -f gpstatscheck_20160926_134946.sql
```

