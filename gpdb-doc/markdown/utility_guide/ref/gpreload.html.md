# gpreload 

Reloads Greenplum Database table data sorting the data based on specified columns.

## <a id="section2"></a>Synopsis 

```
gpreload -d <database> [-p <port>] {-t | --table-file} <path_to_file> [-a]

gpreload -h 

gpreload --version
```

## <a id="section3"></a>Description 

The `gpreload` utility reloads table data with column data sorted. For tables that were created with the table storage option `appendoptimized=TRUE` and compression enabled, reloading the data with sorted data can improve table compression. You specify a list of tables to be reloaded and the table columns to be sorted in a text file.

Compression is improved by sorting data when the data in the column has a relatively low number of distinct values when compared to the total number of rows.

For a table being reloaded, the order of the columns to be sorted might affect compression. The columns with the fewest distinct values should be listed first. For example, listing state then city would generally result in better compression than listing city then state.

```
public.cust_table: state, city
public.cust_table: city, state
```

For information about the format of the file used with `gpreload`, see the `--table-file` option.

## <a id="section4"></a>Notes 

To improve reload performance, indexes on tables being reloaded should be removed before reloading the data.

Running the `ANALYZE` command after reloading table data might query performance because of a change in the data distribution of the reloaded data.

For each table, the utility copies table data to a temporary table, truncates the existing table data, and inserts data from the temporary table to the table in the specified sort order. Each table reload is performed in a single transaction.

For a partitioned table, you can reload the data of a leaf child partition. However, data is inserted from the root partition table, which acquires a `ROW EXCLUSIVE` lock on the entire table.

## <a id="section5"></a>Options 

-a \(do not prompt\)
:   Optional. If specified, the `gpreload` utility does not prompt the user for confirmation.

-d database
:   The database that contains the tables to be reloaded. The `gpreload` utility connects to the database as the user running the utility.

-p port
:   The Greenplum Database coordinator port. If not specified, the value of the `PGPORT` environment variable is used. If the value is not available, an error is returned.

\{-t \| --table-file \} path\_to\_file
:   The location and name of file containing list of schema qualified table names to reload and the column names to reorder from the Greenplum Database. Only user defined tables are supported. Views or system catalog tables are not supported.

:   If indexes are defined on table listed in the file, `gpreload` prompts to continue.

:   Each line specifies a table name and the list of columns to sort. This is the format of each line in the file:

:   `schema.table_name: column [desc] [, column2 [desc] ... ]`

:   The table name is followed by a colon \( : \) and then at least one column name. If you specify more than one column, separate the column names with a comma. The columns are sorted in ascending order. Specify the keyword `desc` after the column name to sort the column in descending order.

:   Wildcard characters are not supported.

:   If there are errors in the file, `gpreload` reports the first error and exits. No data is reloaded.

:   The following example reloads three tables:

    ```
    public.clients: region, state, rep_id desc
    public.merchants: region, state
    test.lineitem: group, assy, whse 
    ```

:   In the first table `public.clients`, the data in the `rep_id` column is sorted in descending order. The data in the other columns are sorted in ascending order.

--version \(show utility version\)
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## <a id="section6"></a>Example 

This example command reloads the tables in the database `mytest` that are listed in the file `data-tables.txt`.

```
gpreload -d mytest --table-file data-tables.txt
```

## <a id="section7"></a>See Also 

`CREATE TABLE` in the *Greenplum Database Reference Guide*

