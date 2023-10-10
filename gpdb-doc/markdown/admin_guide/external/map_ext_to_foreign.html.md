---
title: Understanding the External Table to Foreign Table Mapping
---

Greenplum 7 uses the built-in `gp_exttable_fdw` foreign-data wrapper module to internally convert an external table that you define with the [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html) command to a foreign table.

In the foreign data paradigm, a foreign-data wrapper (FDW) provides access to an external data source. A foreign server object associated with a specific FDW defines the connection details to a particular remote data source. A foreign table specifies the foreign server with which to access the remote data source. The foreign table also defines the structure of the remote data, just as you would define with an external table. Refer to [Accessing External Data with Foreign Tables](g-foreign.html) for more information about how to manage external data with FDWs.

Greenplum internally maps the information that you provide in the `CREATE [WRITABLE] EXTERNAL TABLE` command to foreign data objects as follows:

| Foreign Data Object | Name/Value | Comments |
|----------|-------------|----------|
| foreign-data wrapper | `gp_exttable_fdw` |  Greenplum maps all external tables that you create with the `CREATE [WRITABLE] EXTERNAL TABLE` command to the `gp_exttable_fdw` FDW. This FDW handles the external to foreign table conversion. |
| foreign server | `gp_exttable_server` |  Greenplum maps all external tables that you create with the `CREATE [WRITABLE] EXTERNAL TABLE` command to the `gp_exttable_server` server. |
| foreign table | `<external_table_name>` |  Greenplum maps an external table that you create with the `CREATE [WRITABLE] EXTERNAL TABLE` command to a foreign table of the same name. |

When `gp_exttable_fdw` internally creates the foreign table, it maps the clauses and keywords that you provide in the `CREATE [WRITABLE] EXTERNAL TABLE` command to specific FDW options. In cases where no direct mapping exists, or where there is no equivalent external table clause, default FDW option values are provided.

The external table clause to foreign table option mapping follows:

| CREATE EXTERNAL TABLE Clause | FDW Option(s) and Value(s) | Description |
|----------|---------------------------|--------------------|
| CREATE EXTERNAL TABLE | `is_writable 'false'` | Readable external tables are not writable. |
| CREATE WRITABLE EXTERNAL TABLE | `is_writable 'true'` | Writable external tables are writable and alterable. |
| `LOCATION ('<location>' [, ...])` | `location_uris '<location>'` |  The location of the external data. `gp_exttable_fdw` uses a pipe (`|`) character to separate locations when you provide more than one.  |
| `FORMAT 'TEXT'` | `format 'text'` |  The external data is text format. |
| `FORMAT 'CSV'` | `format 'csv'` |  The external data is comma-separated value format. |
| `FORMAT 'CUSTOM'` | `format 'custom' formatter '<name>'` |  The external data is of a custom format, and Greenplum uses the specified formatter to parse the data. |
| formatting options | `delimiter <value>`</br> `escape <value>`</br> `"null" <value>`</br> `<option1> <value>`</br>`...` | Format-dependent formatting options. |
| `OPTIONS <key> '<value>' [, ...]` | `<key> <value>`</br>`...` |  The data access protocol-specific options. |
| `ENCODING <encoding>` | `encoding '<encoding_str>'` |  The table encoding (string). |
| `LOG ERRORS` | `log_errors 'enable'` | Log errors to an error log. `gp_exttable_fdw` sets this FDW option to `log_errors 'disable'` when the `LOG ERRORS` clause is not provided. |
| `LOG ERRORS PERSISTENTLY` | `log_errors 'persistently'` | Log errors to a persistent error log. `gp_exttable_fdw` sets this FDW option to `log_errors 'disable'` when the `LOG ERRORS` clause is not provided. |
| `SEGMENT REJECT LIMIT <num_or_pct>` | `reject_limit '<num>' reject_limit_type '<type>'` |  The number of errored rows (`<type>` is `rows`) or the errored row percentage (`<type>` is `percentage`) allowed.  |
| n/a | `execute_on 'ALL_SEGMENTS'` | Utilize the parallel processing inherent in Greenplum Database. |

## <a id="implications"></a>Implications

Because Greenplum 7 converts an external table to a foreign table and uses foreign table structures and catalogs to internally represent external tables:

- External tables are included in the foreign table catalogs. Use the [pg_foreign_table](../../ref_guide/system_catalogs/pg_foreign_table.html) system catalog table and the `ftoptions` column to view external table definitions.
- Certain command output and error, detail, and notice messages about external tables refer to the table as a foreign table.
- External tables are included when you list or examine foreign tables (for example, the `\det` `psql` meta-command).

## <a id="example"></a>Example

Given the following external table definition:

```
CREATE EXTERNAL TABLE ext_expenses ( name text, date date, amount float4, category text, desc1 varchar )
  LOCATION ('gpfdist://etlhost-1:8081/*.txt', 'gpfdist://etlhost-2:8082/*.txt')
FORMAT 'TEXT' ( DELIMITER '|' NULL ' ' )
LOG ERRORS SEGMENT REJECT LIMIT 5;
```

`gp_exttable_fdw` creates a foreign table whose `psql` `\d+` output follows:

```
\d+ ext_expenses
                                         Foreign table "public.ext_expenses"
  Column  |       Type        | Collation | Nullable | Default | FDW options | Storage  | Stats target | Description 
----------+-------------------+-----------+----------+---------+-------------+----------+--------------+-------------
 name     | text              |           |          |         |             | extended |              | 
 date     | date              |           |          |         |             | plain    |              | 
 amount   | real              |           |          |         |             | plain    |              | 
 category | text              |           |          |         |             | extended |              | 
 desc1    | character varying |           |          |         |             | extended |              | 
FDW options: (format 'text', delimiter '|', "null" ' ', escape E'\\', location_uris 'gpfdist://etlhost-1:8081/\*.txt|'gpfdist://etlhost-2:8082/\*.txt', execute_on 'ALL_SEGMENTS', reject_limit '5', reject_limit_type 'rows', log_errors 'enable', encoding 'UTF8', is_writable 'false')
```

