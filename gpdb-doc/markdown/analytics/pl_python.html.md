---
title: PL/Python Language 
---

This section contains an overview of the Greenplum Database PL/Python Language.

-   [About Greenplum PL/Python](#topic2)
-   [Enabling and Removing PL/Python support](#topic4)
-   [Developing Functions with PL/Python](#topic7)
-   [Installing Python Modules](#topic10)
-   [Examples](#topic11)
-   [References](#topic12)

## <a id="topic2"></a>About Greenplum PL/Python 

PL/Python is a loadable procedural language. With the Greenplum Database PL/Python extension, you can write a Greenplum Database user-defined functions in Python that take advantage of Python features and modules to quickly build robust database applications.

You can run PL/Python code blocks as anonymous code blocks. See the [DO](../ref_guide/sql_commands/DO.html) command in the *Greenplum Database Reference Guide*.

The Greenplum Database PL/Python extension is installed by default with Greenplum Database. Greenplum Database installs a version of Python and PL/Python. This is location of the Python installation that Greenplum Database uses:

```
$GPHOME/ext/python/
```

### <a id="topic3"></a>Greenplum Database PL/Python Limitations 

-   Greenplum Database does not support PL/Python triggers.
-   PL/Python is available only as a Greenplum Database untrusted language.
-   Updatable cursors \(`UPDATE...WHERE CURRENT OF` and `DELETE...WHERE CURRENT OF`\) are not supported.

## <a id="topic4"></a>Enabling and Removing PL/Python support 

The PL/Python language is installed with Greenplum Database. To create and run a PL/Python user-defined function \(UDF\) in a database, you must register the PL/Python language with the database.

### <a id="topic5"></a>Enabling PL/Python Support 

For each database that requires its use, register the PL/Python language with the SQL command `CREATE EXTENSION`. Because PL/Python is an untrusted language, only superusers can register PL/Python with a database. For example, running this command as the `gpadmin` user registers PL/Python with the database named `testdb`:

```
$ psql -d testdb -c 'CREATE EXTENSION plpythonu;'
```

PL/Python is registered as an untrusted language.

### <a id="topic6"></a>Removing PL/Python Support 

For a database that no longer requires the PL/Python language, remove support for PL/Python with the SQL command `DROP EXTENSION`. Because PL/Python is an untrusted language, only superusers can remove support for the PL/Python language from a database. For example, running this command as the `gpadmin` user removes support for PL/Python from the database named `testdb`:

```
$ psql -d testdb -c 'DROP EXTENSION plpythonu;'
```

The default command fails if any existing objects \(such as functions\) depend on the language. Specify the `CASCADE` option to also drop all dependent objects, including functions that you created with PL/Python.

## <a id="topic7"></a>Developing Functions with PL/Python 

The body of a PL/Python user-defined function is a Python script. When the function is called, its arguments are passed as elements of the array `args[]`. Named arguments are also passed as ordinary variables to the Python script. The result is returned from the PL/Python function with `return` statement, or `yield` statement in case of a result-set statement.

PL/Python translates Python's `None` into the SQL `null` value.

### <a id="topic_datatypemap"></a>Data Type Mapping 

The Greenplum to Python data type mapping follows.

|Greenplum Primitive Type|Python Data Type|
|------------------------|----------------|
|boolean<sup>1</sup>|bool|
|bytea|bytes|
|smallint, bigint, oid|int|
|real, double|float|
|numeric|decimal|
|*other primitive types*|string|
|SQL null value|None|

<sup>1</sup> When the UDF return type is `boolean`, the Greenplum Database evaluates the return value for truth according to Python rules. That is, `0` and empty string are `false`, but notably `'f'` is `true`.

Example:

```
CREATE OR REPLACE FUNCTION pybool_func(a int) RETURNS boolean AS $$
# container: plc_python3_shared
    if (a > 0):
        return True
    else:
        return False
$$ LANGUAGE plpythonu;

SELECT pybool_func(-1);

 pybool_func
-------------
 f
(1 row)

```

### <a id="topic1113"></a>Arrays and Lists 

You pass SQL array values into PL/Python functions with a Python list. Similarly, PL/Python functions return SQL array values as a Python list. In the typical PL/Python usage pattern, you will specify an array with `[]`.

The following example creates a PL/Python function that returns an array of integers:

```
CREATE FUNCTION return_py_int_array()
  RETURNS int[]
AS $$
  return [1, 11, 21, 31]
$$ LANGUAGE plpythonu;

SELECT return_py_int_array();
 return_py_int_array 
---------------------
 {1,11,21,31}
(1 row) 
```

PL/Python treats multi-dimensional arrays as lists of lists. You pass a multi-dimensional array to a PL/Python function using nested Python lists. When a PL/Python function returns a multi-dimensional array, the inner lists at each level must all be of the same size.

The following example creates a PL/Python function that takes a multi-dimensional array of integers as input. The function displays the type of the provided argument, and returns the multi-dimensional array:

```
CREATE FUNCTION return_multidim_py_array(x int4[]) 
  RETURNS int4[]
AS $$
  plpy.info(x, type(x))
  return x
$$ LANGUAGE plpythonu;

SELECT * FROM return_multidim_py_array(ARRAY[[1,2,3], [4,5,6]]);
INFO:  ([[1, 2, 3], [4, 5, 6]], <type 'list'>)
CONTEXT:  PL/Python function "return_multidim_py_type"
 return_multidim_py_array 
--------------------------
 {{1,2,3},{4,5,6}}
(1 row) 
```

PL/Python also accepts other Python sequences, such as tuples, as function arguments for backwards compatibility with Greenplum versions where multi-dimensional arrays were not supported. In such cases, the Python sequences are always treated as one-dimensional arrays because they are ambiguous with composite types.

### <a id="topic1117"></a>Composite Types 

You pass composite-type arguments to a PL/Python function using Python mappings. The element names of the mapping are the attribute names of the composite types. If an attribute has the null value, its mapping value is `None`.

You can return a composite type result as a sequence type \(tuple or list\). You must specify a composite type as a tuple, rather than a list, when it is used in a multi-dimensional array. You cannot return an array of composite types as a list because it would be ambiguous to determine whether the list represents a composite type or another array dimension. In the typical usage pattern, you will specify composite type tuples with `()`.

In the following example, you create a composite type and a PL/Python function that returns an array of the composite type:

```
CREATE TYPE type_record AS (
  first text,
  second int4
);

CREATE FUNCTION composite_type_as_list()
  RETURNS type_record[]
AS $$              
  return [[('first', 1), ('second', 1)], [('first', 2), ('second', 2)], [('first', 3), ('second', 3)]];
$$ LANGUAGE plpythonu;

SELECT * FROM composite_type_as_list();
                               composite_type_as_list                           
------------------------------------------------------------------------------------
 {{"(first,1)","(second,1)"},{"(first,2)","(second,2)"},{"(first,3)","(second,3)"}}
(1 row) 
```

Refer to the PostgreSQL [Arrays, Lists](https://www.postgresql.org/docs/9.4/plpython-data.html#PLPYTHON-ARRAYS) documentation for additional information on PL/Python handling of arrays and composite types.

### <a id="topic_setresult"></a>Set-Returning Functions 

A Python function can return a set of scalar or composite types from any sequence type \(for example: tuple, list, set\).

In the following example, you create a composite type and a Python function that returns a `SETOF` of the composite type:

```
CREATE TYPE greeting AS (
  how text,
  who text
);

CREATE FUNCTION greet (how text)
  RETURNS SETOF greeting
AS $$
  # return tuple containing lists as composite types
  # all other combinations work also
  return ( {"how": how, "who": "World"}, {"how": how, "who": "Greenplum"} )
$$ LANGUAGE plpythonu;

select greet('hello');
       greet
-------------------
 (hello,World)
 (hello,Greenplum)
(2 rows)
```

### <a id="topic8"></a>Running and Preparing SQL Queries 

The PL/Python `plpy` module provides two Python functions to run an SQL query and prepare an execution plan for a query, `plpy.execute` and `plpy.prepare`. Preparing the execution plan for a query is useful if you run the query from multiple Python functions.

PL/Python also supports the `plpy.subtransaction()` function to help manage `plpy.execute` calls in an explicit subtransaction. See [Explicit Subtransactions](https://www.postgresql.org/docs/9.4/plpython-subtransaction.html) in the PostgreSQL documentation for additional information about `plpy.subtransaction()`.

#### <a id="topic_jnf_45f_zt"></a>plpy.execute 

Calling `plpy.execute` with a query string and an optional limit argument causes the query to be run and the result to be returned in a Python result object. The result object emulates a list or dictionary object. The rows returned in the result object can be accessed by row number and column name. The result set row numbering starts with 0 \(zero\). The result object can be modified. The result object has these additional methods:

-   `nrows` that returns the number of rows returned by the query.
-   `status` which is the `SPI_execute()` return value.

For example, this Python statement in a PL/Python user-defined function runs a query.

```
rv = plpy.execute("SELECT * FROM my_table", 5)
```

The `plpy.execute` function returns up to 5 rows from `my_table`. The result set is stored in the `rv` object. If `my_table` has a column `my_column`, it would be accessed as:

```
my_col_data = rv[i]["my_column"]
```

Since the function returns a maximum of 5 rows, the index i can be an integer between 0 and 4.

#### <a id="topic_jwf_p5f_zt"></a>plpy.prepare 

The function `plpy.prepare` prepares the execution plan for a query. It is called with a query string and a list of parameter types, if you have parameter references in the query. For example, this statement can be in a PL/Python user-defined function:

```
plan = plpy.prepare("SELECT last_name FROM my_users WHERE 
  first_name = $1", [ "text" ])
```

The string `text` is the data type of the variable that is passed for the variable $1. After preparing a statement, you use the function `plpy.execute` to run it:

```
rv = plpy.execute(plan, [ "Fred" ], 5)
```

The third argument is the limit for the number of rows returned and is optional.

When you prepare an execution plan using the PL/Python module the plan is automatically saved. See the Postgres Server Programming Interface \(SPI\) documentation for information about the execution plans [https://www.postgresql.org/docs/9.4/spi.html](https://www.postgresql.org/docs/9.4/spi.html).

To make effective use of saved plans across function calls you use one of the Python persistent storage dictionaries SD or GD.

The global dictionary SD is available to store data between function calls. This variable is private static data. The global dictionary GD is public data, available to all Python functions within a session. Use GD with care.

Each function gets its own execution environment in the Python interpreter, so that global data and function arguments from `myfunc` are not available to `myfunc2`. The exception is the data in the GD dictionary, as mentioned previously.

This example uses the SD dictionary:

```
CREATE FUNCTION usesavedplan() RETURNS trigger AS $$
  if SD.has_key("plan"):
    plan = SD["plan"]
  else:
    plan = plpy.prepare("SELECT 1")
    SD["plan"] = plan

  # rest of function

$$ LANGUAGE plpythonu;
```

### <a id="topic_s3d_vc4_xt"></a>Handling Python Errors and Messages 

The Python module `plpy` implements these functions to manage errors and messages:

-   `plpy.debug`
-   `plpy.log`
-   `plpy.info`
-   `plpy.notice`
-   `plpy.warning`
-   `plpy.error`
-   `plpy.fatal`
-   `plpy.debug`

The message functions `plpy.error` and `plpy.fatal` raise a Python exception which, if uncaught, propagates out to the calling query, causing the current transaction or subtransaction to be cancelled. The functions `raise plpy.ERROR(msg)` and `raise plpy.FATAL(msg)` are equivalent to calling `plpy.error` and `plpy.fatal`, respectively. The other message functions only generate messages of different priority levels.

Whether messages of a particular priority are reported to the client, written to the server log, or both is controlled by the Greenplum Database server configuration parameters `log_min_messages` and `client_min_messages`. For information about the parameters see the *Greenplum Database Reference Guide*.

### <a id="topic_hfj_dgg_mjb"></a>Using the dictionary GD To Improve PL/Python Performance 

In terms of performance, importing a Python module is an expensive operation and can affect performance. If you are importing the same module frequently, you can use Python global variables to load the module on the first invocation and not require importing the module on subsequent calls. The following PL/Python function uses the GD persistent storage dictionary to avoid importing a module if it has already been imported and is in the GD.

```
psql=#
   CREATE FUNCTION pytest() returns text as $$ 
      if 'mymodule' not in GD:
        import mymodule
        GD['mymodule'] = mymodule
    return GD['mymodule'].sumd([1,2,3])
$$;
```

## <a id="topic10"></a>Installing Python Modules 

When you install a Python module on Greenplum Database, the Greenplum Database Python environment must have the module added to it across all segment hosts and mirror hosts in the cluster. When expanding Greenplum Database, you must add the Python modules to the new segment hosts. You can use the Greenplum Database utilities `gpssh` and `gpscp` run commands on Greenplum Database hosts and copy files to the hosts. For information about the utilities, see the *Greenplum Database Utility Guide*.

As part of the Greenplum Database installation, the `gpadmin` user environment is configured to use Python that is installed with Greenplum Database.

To check the Python environment, you can use the `which` command:

```
which python
```

The command returns the location of the Python installation. The Python installed with Greenplum Database is in the Greenplum Database `ext/python` directory.

```
/<path_to_greenplum-db>/ext/python/bin/python
```

When running shell commands on remote hosts with `gpssh`, you can specify the `-s` option. When the option is specified, `gpssh` sources the `greenplum_path.sh` file before running commands on the remote hosts. For example, this command should display the Python installed with Greenplum Database on each host.

```
gpssh -f gpdb_hosts which python
```

If it does not, you can add the `-s` to source `greenplum_path.sh` on the remote hosts before running the command.

```
gpssh -s -f gpdb_hosts which python
```

To display the list of currently installed Python modules, run this command.

```
python -c "help('modules')"
```

Run `gpssh` in interactive mode to display Python modules on remote hosts. This example starts `gpssh` in interactive mode and lists the Python modules on the Greenplum Database host `sdw1`.

```
$ gpssh -s -h sdw1
=> python -c "help('modules')"
. . . 
=> exit
$
```

Greenplum Database provides a collection of data science-related Python libraries that can be used with the Greenplum Database PL/Python language. You can download these libraries in `.gppkg` format from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). For information about the libraries, see [Python Data Science Module Package](../install_guide/install_python_dsmod.html#topic1).

These sections describe installing and testing Python modules:

-   [Installing Python pip](#topic_yx3_yjq_rt)
-   [Installing Python Packages with pip](#topic_g4j_hmt_ycb)
-   [Building and Installing Python Modules Locally](#topic_j53_5jq_rt)
-   [Testing Installed Python Modules](#topic_e4p_gcw_vt)

### <a id="topic_yx3_yjq_rt"></a>Installing Python pip 

The Python utility `pip` installs Python packages that contain Python modules and other resource files from versioned archive files.

Run this command to install `pip`.

```
python -m ensurepip --default-pip
```

The command runs the `ensurepip` module to bootstrap \(install and configure\) the `pip` utility from the local Python installation.

You can run this command to ensure the `pip`, `setuptools` and `wheel` projects are current. Current Python projects ensure that you can install Python packages from source distributions or pre-built distributions \(wheels\).

```
python -m pip install --upgrade pip setuptools wheel
```

You can use `gpssh` to run the commands on the Greenplum Database hosts. This example runs `gpssh` in interactive mode to install `pip` on the hosts listed in the file `gpdb_hosts`.

```
$ gpssh -s -f gpdb_hosts
=> python -m ensurepip --default-pip
[centos6-mdw1] Ignoring indexes: https://pypi.python.org/simple
[centos6-mdw1] Collecting setuptools
[centos6-mdw1] Collecting pip
[centos6-mdw1] Installing collected packages: setuptools, pip
[centos6-mdw1] Successfully installed pip-8.1.1 setuptools-20.10.1
[centos6-sdw1] Ignoring indexes: https://pypi.python.org/simple
[centos6-sdw1] Collecting setuptools
[centos6-sdw1] Collecting pip
[centos6-sdw1] Installing collected packages: setuptools, pip
[centos6-sdw1] Successfully installed pip-8.1.1 setuptools-20.10.1
=> exit
$
```

The `=>` is the inactive prompt for `gpssh`. The utility displays the output from each host. The `exit` command exits from `gpssh` interactive mode.

This `gpssh` command runs a single command on all hosts listed in the file `gpdb_hosts`.

```
gpssh -s -f gpdb_hosts python -m pip install --upgrade pip setuptools wheel
```

The utility displays the output from each host.

For more information about installing Python packages, see [https://packaging.python.org/tutorials/installing-packages/](https://packaging.python.org/tutorials/installing-packages/).

### <a id="topic_g4j_hmt_ycb"></a>Installing Python Packages with pip 

After installing `pip`, you can install Python packages. This command installs the `numpy` and `scipy` packages.

```
python -m pip install --user numpy scipy
```

The `--user` option attempts to avoid conflicts when installing Python packages.

You can use `gpssh` to run the command on the Greenplum Database hosts.

For information about these and other Python packages, see [References](#topic12).

### <a id="topic_j53_5jq_rt"></a>Building and Installing Python Modules Locally 

If you are building a Python module, you must ensure that the build creates the correct executable. For example on a Linux system, the build should create a 64-bit executable.

Before building a Python module to be installed, ensure that the appropriate software to build the module is installed and properly configured. The build environment is required only on the host where you build the module.

You can use the Greenplum Database utilities `gpssh` and `gpscp` to run commands on Greenplum Database hosts and to copy files to the hosts.

### <a id="topic_e4p_gcw_vt"></a>Testing Installed Python Modules 

You can create a simple PL/Python user-defined function \(UDF\) to validate that Python a module is available in the Greenplum Database. This example tests the NumPy module.

This PL/Python UDF imports the NumPy module. The function returns `SUCCESS` if the module is imported, and `FAILURE` if an import error occurs.

```
CREATE OR REPLACE FUNCTION plpy_test(x int)
returns text
as $$
  try:
      from numpy import *
      return 'SUCCESS'
  except ImportError, e:
      return 'FAILURE'
$$ language plpythonu;
```

Create a table that contains data on each Greenplum Database segment instance. Depending on the size of your Greenplum Database installation, you might need to generate more data to ensure data is distributed to all segment instances.

```
CREATE TABLE DIST AS (SELECT x FROM generate_series(1,50) x ) DISTRIBUTED RANDOMLY ;
```

This `SELECT` command runs the UDF on the segment hosts where data is stored in the primary segment instances.

```
SELECT gp_segment_id, plpy_test(x) AS status
  FROM dist
  GROUP BY gp_segment_id, status
  ORDER BY gp_segment_id, status;
```

The `SELECT` command returns `SUCCESS` if the UDF imported the Python module on the Greenplum Database segment instance. If the `SELECT` command returns `FAILURE`, you can find the segment host of the segment instance host. The Greenplum Database system table `gp_segment_configuration` contains information about mirroring and segment configuration. This command returns the host name for a segment ID.

```
SELECT hostname, content AS seg_ID FROM gp_segment_configuration
  WHERE content = <seg_id> ;
```

If `FAILURE` is returned, these are some possible causes:

-   A problem accessing required libraries. For the NumPy example, a Greenplum Database might have a problem accessing the OpenBLAS libraries or the Python libraries on a segment host.

    Make sure you get no errors when running command on the segment host as the `gpadmin` user. This `gpssh` command tests importing the numpy module on the segment host `mdw1`.

    ```
    gpssh -s -h mdw1 python -c "import numpy"
    ```

-   If the Python `import` command does not return an error, environment variables might not be configured in the Greenplum Database environment. For example, the Greenplum Database might not have been restarted after installing the Python Package on the host system.

## <a id="topic11"></a>Examples 

This PL/Python UDF returns the maximum of two integers:

```
CREATE FUNCTION pymax (a integer, b integer)
  RETURNS integer
AS $$
  if (a is None) or (b is None):
      return None
  if a > b:
     return a
  return b
$$ LANGUAGE plpythonu;
```

You can use the `STRICT` property to perform the null handling instead of using the two conditional statements.

```
CREATE FUNCTION pymax (a integer, b integer) 
  RETURNS integer AS $$ 
return max(a,b) 
$$ LANGUAGE plpythonu STRICT ;
```

You can run the user-defined function `pymax` with `SELECT` command. This example runs the UDF and shows the output.

```
SELECT ( pymax(123, 43));
column1
---------
     123
(1 row)
```

This example that returns data from an SQL query that is run against a table. These two commands create a simple table and add data to the table.

```
CREATE TABLE sales (id int, year int, qtr int, day int, region text)
  DISTRIBUTED BY (id) ;

INSERT INTO sales VALUES
 (1, 2014, 1,1, 'usa'),
 (2, 2002, 2,2, 'europe'),
 (3, 2014, 3,3, 'asia'),
 (4, 2014, 4,4, 'usa'),
 (5, 2014, 1,5, 'europe'),
 (6, 2014, 2,6, 'asia'),
 (7, 2002, 3,7, 'usa') ;
```

This PL/Python UDF runs a `SELECT` command that returns 5 rows from the table. The Python function returns the `REGION` value from the row specified by the input value. In the Python function, the row numbering starts from 0. Valid input for the function is an integer between 0 and 4.

```
CREATE OR REPLACE FUNCTION mypytest(a integer) 
  RETURNS setof text 
AS $$ 
  rv = plpy.execute("SELECT * FROM sales ORDER BY id", 5)
  region =[]
  region.append(rv[a]["region"])
  return region
$$ language plpythonu EXECUTE ON MASTER;
```

Running this `SELECT` statement returns the `REGION` column value from the third row of the result set.

```
SELECT mypytest(2) ;
```

This command deletes the UDF from the database.

```
DROP FUNCTION mypytest(integer) ;
```

This example runs the PL/Python function in the previous example as an anonymous block with the `DO` command. In the example, the anonymous block retrieves the input value from a temporary table.

```
CREATE TEMP TABLE mytemp AS VALUES (2) DISTRIBUTED RANDOMLY;

DO $$ 
  temprow = plpy.execute("SELECT * FROM mytemp", 1)
  myval = temprow[0]["column1"]
  rv = plpy.execute("SELECT * FROM sales ORDER BY id", 5)
  region = rv[myval]["region"]
  plpy.notice("region is %s" % region)
$$ language plpythonu;
```

## <a id="topic12"></a>References 

### <a id="topic13"></a>Technical References 

For information about the Python language, see [https://www.python.org/](https://www.python.org/).

For information about PL/Python see the PostgreSQL documentation at [https://www.postgresql.org/docs/9.4/plpython.html](https://www.postgresql.org/docs/9.4/plpython.html).

For information about Python Package Index \(PyPI\), see [https://pypi.python.org/pypi](https://pypi.python.org/pypi).

These are some Python modules that can be installed:

-   SciPy library provides user-friendly and efficient numerical routines such as routines for numerical integration and optimization. The SciPy site includes other similar Python libraries [http://www.scipy.org/index.html](http://www.scipy.org/index.html).
-   Natural Language Toolkit \(nltk\) is a platform for building Python programs to work with human language data. [http://www.nltk.org/](http://www.nltk.org/). For information about installing the toolkit see [http://www.nltk.org/install.html](http://www.nltk.org/install.html).

