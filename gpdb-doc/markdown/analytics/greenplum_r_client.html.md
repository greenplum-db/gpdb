---
title: Greenplum Database R Client 
---

> *Note* Greenplum Database R Client is deprecated and will be removed in a future Greenplum Database release.

This chapter contains the following information:

-   [About Greenplum R](#about)
-   [Supported Platforms](#supplat)
-   [Prerequisites](#prereqs)
-   [Installing the Greenplum R Client](#install)
-   [Example Data Sets](#exampleds)
-   [Using the Greenplum R Client](#use)
-   [Limitations](#limits)
-   [Function Summary](#ref)

## <a id="about"></a>About Greenplum R 

The Greenplum R Client \(GreenplumR\) is an interactive in-database data analytics tool for Greenplum Database. The client provides an R interface to tables and views, and requires no SQL knowledge to operate on these database objects.

You can use GreenplumR with the Greenplum PL/R procedural language to run an R function on data stored in Greenplum Database. GreenplumR parses the R function and creates a user-defined function \(UDF\) for execution in Greenplum. Greenplum runs the UDF in parallel on the segment hosts.

You can similarly use GreenplumR with Greenplum PL/Container 3 \(Beta\), to run an R function against Greenplum data in a high-performance R sandbox runtime environment.

No analytic data is loaded into R when you use GreenplumR, a key requirement when dealing with large data sets. Only the R function and minimal data is transferred between R and Greenplum.

## <a id="supplat"></a>Supported Platforms 

GreenplumR supports the following component versions:

|GreenplumR Version|R Version|Greenplum Version|PL/R Version|PL/Container Version|
|------------------|---------|-----------------|------------|--------------------|
|1,1,0, 1.0.0|3.6+|6.1+|3.0.2+|3.0.0 \(Beta\)|

## <a id="prereqs"></a>Prerequisites 

You can use GreenplumR with Greenplum Database and the PL/R and PL/Container procedural languages. Before you install and run GreenplumR on a client system:

-   Ensure that your Greenplum Database installation is running version 6.1 or newer.
-   Ensure that your client development system has connectivity to the Greenplum Database master host.
-   Ensure that `R` version 3.6.0 or newer is installed on your client system, and that you set the `$R_HOME` environment variable appropriately.
-   Determine the procedural language\(s\) you plan to use with GreenplumR, and ensure that the language\(s\) is installed and configured in your Greenplum Database cluster. Refer to [PL/R Language](pl_r.html) and [PL/Container Language](pl_container.html) \(3.0 Beta\) for language installation and configuration instructions.
-   Verify that you have registered the procedural language\(s\) in each database in which you plan to use GreenplumR to read data from or write data to Greenplum. For example, the following command lists the extensions and languages registered in the database named `testdb`:

    ```
    $ psql -h gpmaster -d testdb -c '\dx'
                                         List of installed extensions
        Name     | Version  |   Schema   |                          Description     
                          
    -------------+----------+------------+----------------------------------------------------------------
     plcontainer | 1.0.0    | public     | GPDB execution sandboxing for Python and R
     plpgsql     | 1.0      | pg_catalog | PL/pgSQL procedural language
     plr         | 8.3.0.16 | public     | load R interpreter and run R script from within a database
    
    ```

    Check for the `plr` and/or `plcontainer` extension `Name`.


## <a id="install"></a>Installing the Greenplum R Client 

GreenplumR is an R package. You obtain the package from VMware Tanzu Network (or the GreenplumR `github` repository) and install the package within the R console.

1.  Download the package from the *Greenplum Procedural Languages* filegroup on [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). The naming format of the downloaded file is `greenplumR‑<version>‑gp6.tar.gz`.

    Note the file system location of the downloaded file.

2.  Follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the **Greenplum Procedural Languages GreenplumR** software.
3.  Install the dependent R packages: `ini`, `shiny`, and `RPostgreSQL`. For example, enter the R console and run the following, or equivalent, command:

    ```
    user@clientsys$ R
    > install.packages(c("ini", "shiny", "RPostgreSQL"))
    ```

    You may be prompted to select a CRAN download mirror. And, depending on your client system configuration, you may be prompted to use or create a personal library.

    After downloading, R builds and installs these and dependent packages.

4.  Install the GreenplumR R package:

    ```
    > install.packages("/path/to/greenplumR‑<version>‑gp6.tar.gz", repos = NULL, type = "source")
    ```

5.  Install the `Rdpack` documentation package:

    ```
    > install.packages("Rdpack")
    ```

## <a id="exampleds"></a>Example Data Sets 

GreenplumR includes the `abalone` and `null.data` sample datasets. Refer to the reference pages for more information:

```
> help( abalone )
> help( null.data )
```

## <a id="use"></a>Using the Greenplum R Client 

You use GreenplumR to perform in-database analytics. Typical operations that you may perform include:

-   Loading the GreenplumR package.
-   Connecting to and disconnecting from Greenplum Database.
-   Examining database objects.
-   Analyzing and manipulating data.
-   Running R functions in Greenplum Database.

### <a id="load"></a>Loading GreenplumR 

Use the R `library()` function to load GreenplumR:

```
user@clientsys$ R
> library("GreenplumR")
```

### <a id="connect"></a>Connecting to Greenplum Database 

The `db.connect()` and `db.connect.dsn()` GreenplumR functions establish a connection to Greenplum Database. The `db.disconnect()` function closes a database connection.

The GreenplumR connect and disconnect function signatures follow:

```
db.connect( host = "localhost", user = Sys.getenv("USER"),
            dbname = user, password = "", port = 5432, conn.pkg = "RPostgreSQL",
            default.schemas = NULL, verbose = TRUE, quick = FALSE )

db.connect.dsn( dsn.key, db.ini = "~/db.ini", default.schemas = NULL,
                verbose = TRUE, quick = FALSE )

db.disconnect( conn.id = 1, verbose = TRUE, force = FALSE )

```

When you connect to Greenplum Database, you provide the master host, port, database name, user name, password, and other information via function arguments or a data source name \(DSN\) file. If you do not specify an argument or value, GreenplumR uses the default.

The `db.connect[.dsn]()` functions return an integer connection identifier. You specify this identifier when you operate on tables or views in the database. You also specify this identifier when you close the connection.

The `db.disconnect()` function returns a logical that identifies whether or not the connection was successfully disconnected.

To list and display information about active Greenplum connections, use the `db.list()` function.

**Example**:

```
## connect to Greenplum database named testdb on host gpmaster
> cid_to_testdb <- db.connect( host = "gpmaster", port=5432, dbname = "testdb" )
Loading required package: DBI
Created a connection to database with ID 1 
[1] 1

> db.list()
Database Connection Info
## -------------------------------
[Connection ID 1]
Host     :    gpmaster
User     :    gpadmin
Database :    testdb
DBMS     :    Greenplum 6

> db.disconnect( cid_to_testdb )
Connection 1 is disconnected!
[1] TRUE
```

### <a id="objs"></a>Examining Database Obects 

The `db.objects()` function lists the tables and views in the database identified by a specific connection identifier. The function signature is:

```
db.objects( search = NULL, conn.id = 1 )
```

If you choose, you can specify a filter string to narrow the returned results. For example, to list the tables and views in the `public` schema in the database identified by the default connection identifier, invoke the function as follows:

```
> db.objects( search = "public." )
```

### <a id="data"></a>Analyzing and Manipulating Data 

The fundamental data structure of R is the `data.frame`. A data frame is a collection of variables represented as a list of vectors. GreenplumR operates on `db.data.frame` objects, and exposes functions to convert to and manipulate objects of this type:

-   `as.db.data.frame()` - writes data in a file or a `data.frame` into a Greenplum table. You can also use the function to write the results of a query into a table, or to create a local `db.data.frame`.
-   `db.data.frame()` - creates a temporary R object that references a view or table in a Greenplum database. No data is loaded into R when you use this function.

**Example**:

```
## create a db.data.frame from the abalone example data set;
## abalone is a data.frame
> abdf1 <- as.db.data.frame(abalone, conn.id = cid_to_testdb, verbose = FALSE)

## sort on the id column and preview the first 5 rows
> lk( sort( abdf1, INDICES=abdf1$id ), 5 )
  id sex length diameter height  whole shucked viscera shell rings
1  1   M  0.455    0.365  0.095 0.5140  0.2245  0.1010 0.150    15
2  2   M  0.350    0.265  0.090 0.2255  0.0995  0.0485 0.070     7
3  3   F  0.530    0.420  0.135 0.6770  0.2565  0.1415 0.210     9
4  4   M  0.440    0.365  0.125 0.5160  0.2155  0.1140 0.155    10
5  5   I  0.330    0.255  0.080 0.2050  0.0895  0.0395 0.055     7

## write the data frame to a Greenplum table named abalone_from_r;
## use most of the function defaults
> as.db.data.frame( abdf1, table.name = "public.abalone_from_r" ) 
The data contained in table "pg_temp_93"."gp_temp_5bdf4ec7_42f9_9f9799_a0d76231be8f" which is wrapped by abdf1 is c
opied into "public"."abalone_from_r" in database testdb on gpmaster !
Table       :    "public"."abalone_from_r"
Database    :    testdb
Host        :    gpmaster
Connection  :    1

## list database objects, should display the newly created table
> db.objects( search = "public.")
[1] "public.abalone_from_r"
```

### <a id="run_greenplum"></a>Running R Functions in Greenplum Database 

GreenplumR supports two functions that allow you to run an R function, in-database, on every row of a Greenplum Database table: [db.gpapply\(\)](https://github.com/greenplum-db/GreenplumR/blob/main/db.gpapply.md) and [db.gptapply\(\)](https://github.com/greenplum-db/GreenplumR/blob/main/db.gptapply.md). You can use the Greenplum PL/R or PL/Container procedural language as the vehicle in which to run the function.

The function signatures follow:

```
db.gpapply( X, MARGIN = NULL, FUN = NULL, output.name = NULL, output.signature = NULL,
            clear.existing = FALSE, case.sensitive = FALSE, output.distributeOn = NULL,
            debugger.mode = FALSE, runtime.id = "plc_r_shared", language = "plcontainer",
            input.signature = NULL, ... )

db.gptapply( X, INDEX, FUN = NULL, output.name = NULL, output.signature = NULL,
             clear.existing = FALSE, case.sensitive = FALSE,
             output.distributeOn = NULL, debugger.mode = FALSE,
             runtime.id = "plc_r_shared", language = "plcontainer",
             input.signature = NULL, ... )
```

Use the second variant of the function when the table data is indexed.

By default, `db.gp[t]apply()` passes a single data frame input argument to the R function `FUN`. If you define `FUN` to take a list of arguments, you must specify the function argument name to Greenplum table column name mapping in `input.signature`. You must specify this mapping in table column order.

**Example 1**:

Create a Greenplum table named `table1` in the database named `testdb`. This table has a single integer-type field. Populate the table with some data:

```
user@clientsys$ psql -h gpmaster -d testdb
testdb=# CREATE TABLE table1( id int );
testdb=# INSERT INTO table1 SELECT generate_series(1,13);
testdb=# \q
```

Create an R function that increments an integer. Run the function on `table1` in Greenplum using the PL/R procedural language. Then write the new values to a table named `table1_r_inc`:

```
user@clientsys$ R
> ## create a reference to table1
> t1 <- db.data.frame("public.table1")

> ## create an R function that increment an integer by 1
> fn.function_plus_one <- function(num)
{
    return (num[[1]] + 1)
}

> ## create the function output signature
> .sig <- list( "num" = "int" )

> ## run the function in Greenplum and print
> x <- db.gpapply( t1, output.name = NULL, FUN = fn.function_plus_one,
  output.signature = .sig, clear.existing = TRUE, case.sensitive = TRUE, language = "plr" )
> print(x)
   num
1    2
2    6
3   12
4   13
5    3
6    4
7    5
8    7
9    8
10   9
11  10
12  11
13  14

> ## run the function in Greenplum and write to the output table
> db.gpapply(t1, output.name = "public.table1_r_inc", FUN = fn.function_plus_one,
   output.signature = .sig, clear.existing = TRUE, case.sensitive = TRUE,
   language = "plr" )

## list database objects, should display the newly created table
> db.objects( search = "public.")
[1] "public.abalone_from_r"		"public.table1_r_inc"

```

**Example 2**:

Create a Greenplum table named `table2` in the database named `testdb`. This table has two integer-type fields. Populate the table with some data:

```
user@clientsys$ psql -h gpmaster -d testdb
testdb=# CREATE TABLE table2( c1 int, c2 int );
testdb=# INSERT INTO table2 VALUES (1, 2);
testdb=#\q
```

Create an R function that takes two integer arguments, manipulates the arguments, and returns both. Run the function on the data in `table2` in Greenplum using the PL/R procedural language, writing the new values to a table named `table2_r_upd`:

```
user@clientsys$ R
> ## create a reference to table2
> t2 <- db.data.frame("public.table2")

> ## create the R function
> fn.func_with_two_args <- function(a, b)
{
    a <- a * 20
    b <- a + 66
    c <- list(a, b)
    return (as.data.frame(c))
}

> ## create the function input signature, mapping function argument name to
> ## table column name
> input.sig <- list('a' = 'c1', 'b' = 'c2')

> ## create the function output signature
> return.sig <- list('a' = 'int', 'b' = 'int')

> ## run the function in Greenplum and write to the output table
> db.gpapply(t2, output.name = "public.table2_r_upd", FUN = fn.func_with_two_args,
   output.signature = return.sig, clear.existing = TRUE, case.sensitive = TRUE,
   language = "plr", input.signature = input.sig )

```

View the contents of the Greenplum table named `table2_r_upd`:

```
user@clientsys$ psql -h gpmaster -d testdb
testdb=# SELECT * FROM table2_r_upd;
 a  | b  
----+----
 20 | 86
(1 row)
testdb=# \q
```

## <a id="limits"></a>Limitations 

GreenplumR has the following limitations:

-   The `db.gpapply()` and `db.gptapply()` functions currently operate only on `conn.id = 1`.
-   The GreenplumR function reference pages are not yet published. You can find `html` versions of the reference pages in your R library `R/<platform>-library/3.6/GreenplumR/html` directory. Open the `00Index.html` file in the browser of your choice and select the function of interest.
-   The `GreenplumR()` graphical user interface is currently unsupported.

## <a id="ref"></a>Function Summary 

GreenplumR provides several functions. To obtain reference information for GreenplumR functions while running the R console, invoke the `help()` function. For example, to display the reference information for the GreenplumR `db.data.frame()` function:

```
> help( db.data.frame )
```

GreenplumR functions include:

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 2. </span>GreenplumR Functions</span></caption><colgroup><col style="width:100pt" /><col style="width:100pt" /><col style="width:500pt" /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d24857e700">Category</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d24857e703">Name</th>
<th class="entry cell-norowborder" style="vertical-align:top;" id="d24857e706">Description</th>
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e700 ">Aggregate Functions</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">mean(), sum(), count(), max(), min(), sd(), var(), colMeans(), colSums(), colAgg(), db.array()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Functions that perform a calculation on
                multiple values and return a single value.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" rowspan="5" style="vertical-align:top;" headers="d24857e700 ">Connectivity Functions</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">connection info</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Extract connection information.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.connect()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Create a database connection.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.connect.dsn()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Create a database connection using a DSN.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.disconnect()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Disconnect a database connection.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.list()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">List database connections.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" rowspan="4" style="vertical-align:top;" headers="d24857e700 ">Database Object Functions</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">as.db.data.frame()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Create a db.data.frame from a file or data.frame,
                optionally writing the data to a Greenplum table.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.data.frame()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Create a data frame that references a view or
                 table in the database.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.objects()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">List the table and view objects in the database.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.existsObject()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Identifies whether a table or view exists in the
                database.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e700 ">Mathematical Functions</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">exp(), abs(), log(), log10(), sign(), sqrt(), factorial(), sin(), cos(), tan(), asin(), acos(), atan(), atan2()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Mathematical functions that take db.obj as an
                argument.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" rowspan="2" style="vertical-align:top;" headers="d24857e700 ">Greenplum Functions</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d24857e703 ">db.gpapply()</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d24857e706 ">Wrap an R function with a UDF and run it on
                every row of data in a Greenplum table.</td>
</tr>
<tr class="row">
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d24857e703 ">db.gptapply()</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d24857e706 ">Wrap an R function with a UDF and run it on
                every row of data grouped by an index in a Greenplum table.</td>
</tr>
</tbody>
</table>
</div>
