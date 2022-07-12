# Greenplum PL/R Language Extension 

This chapter contains the following information:

-   [About Greenplum Database PL/R](#topic2)
-   [Installing PL/R](#topic3)
-   [Uninstalling PL/R](#topic6)
-   [Enabling PL/R Language Support](#topic5)
-   [Examples](#topic9)
-   [Downloading and Installing R Packages](#topic13)
-   [Displaying R Library Information](#topic14)
-   [References](#topic15)

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

## About Greenplum Database PL/R 

PL/R is a procedural language. With the Greenplum Database PL/R extension you can write database functions in the R programming language and use R packages that contain R functions and data sets.

For information about supported PL/R versions, see the *Greenplum Database Release Notes*.

### Installing PL/R 

The PL/R extension is available as a package. Download the package from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb) and install it with the Greenplum Package Manager \(`gppkg`\).

The `gppkg` utility installs Greenplum Database extensions, along with any dependencies, on all hosts across a cluster. It also automatically installs extensions on new hosts in the case of system expansion and segment recovery.

For information about `gppkg`, see the *Greenplum Database Utility Guide*.

#### Installing the Extension Package 

Before you install the PL/R extension, make sure that your Greenplum Database is running, you have sourced `greenplum_path.sh`, and that the `$MASTER_DATA_DIRECTORY` and `$GPHOME` variables are set.

1.  Download the PL/R extension package from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).
2.  Follow the instructions in [Verifying the Greenplum Database Software Download](/vmware/install_guide/verify_sw.html) to verify the integrity of the **Greenplum Procedural Languages PL/R** software.
3.  Copy the PL/R package to the Greenplum Database master host.
4.  Install the software extension package by running the `gppkg` command. This example installs the PL/R extension on a Linux system:

    ```
    $ gppkg -i plr-2.3.1-gp5-rhel6-x86_64.gppkg
    ```

5.  Source the file `$GPHOME/greenplum_path.sh`.
6.  Restart the database.

    ```
    $ gpstop -r
    ```


The extension and the R environment is installed in this directory:

```
$GPHOME/ext/R-3.3.3/
```

**Note:** The version of some shared libraries installed with the operating system might not be compatible with the Greenplum Database PL/R extension.

If a shared library is not compatible, edit the file `$GPHOME/greenplum_path.sh` in all Greenplum Database master and segment hosts and set environment variable `LD_LIBRARY_PATH` to specify the location that is installed with the PL/R extension.

```
export *LD\_LIBRARY\_PATH= 
   $GPHOME/ext/R-3.3.3/lib:$LD\_LIBRARY\_PATH*
```

### Enabling PL/R Language Support 

For each database that requires its use, register the PL/R language with the SQL command `CREATE EXTENSION`. Because PL/R is an untrusted language, only superusers can register PL/R with a database. For example, run this command as the `gpadmin` user to register the language with the database named `testdb`:

```
$ psql -d testdb -c 'CREATE EXTENSION plr;'
```

PL/R is registered as an untrusted language.

### Uninstalling PL/R 

-   [Remove PL/R Support for a Database](#topic7)
-   [Uninstall the Extension Package](#topic8)

When you remove PL/R language support from a database, the PL/R routines that you created in the database will no longer work.

#### Remove PL/R Support for a Database 

For a database that no longer requires the PL/R language, remove support for PL/R with the SQL command `DROP EXTENSION`. Because PL/R is an untrusted language, only superusers can remove support for the PL/R language from a database. For example, run this command as the `gpadmin` user to remove support for PL/R from the database named `testdb`:

```
$ psql -d testdb -c 'DROP EXTENSION plr;'
```

The default command fails if any existing objects \(such as functions\) depend on the language. Specify the `CASCADE` option to also drop all dependent objects, including functions that you created with PL/R.

#### Uninstall the Extension Package 

If no databases have PL/R as a registered language, uninstall the Greenplum PL/R extension with the `gppkg` utility. This example uninstalls PL/R package version 2.3.1

```
$ gppkg -r plr-2.3.1
```

You can run the `gppkg` utility with the options `-q --all` to list the installed extensions and their versions.

Restart the database.

```
$ gpstop -r
```

### Examples 

The following are simple PL/R examples.

#### Example 1: Using PL/R for single row operators 

This function generates an array of numbers with a normal distribution using the R function `rnorm()`.

```
CREATE OR REPLACE FUNCTION r_norm(n integer, mean float8, 
  std_dev float8) RETURNS float8[ ] AS
$$
  x<-rnorm(n,mean,std_dev)
  return(x)
$$
LANGUAGE 'plr';
```

The following CREATE TABLE command uses the r\_norm function to populate the table. The r\_norm function creates an array of 10 numbers.

```
CREATE TABLE test_norm_var
  AS SELECT id, r_norm(10,0,1) as x
  FROM (SELECT generate_series(1,30:: bigint) AS ID) foo
  DISTRIBUTED BY (id);
```

#### Example 2: Returning PL/R data.frames in Tabular Form 

Assuming your PL/R function returns an R `data.frame` as its output, unless you want to use arrays of arrays, some work is required to see your `data.frame` from PL/R as a simple SQL table:

-   Create a `TYPE` in a Greenplum database with the same dimensions as your R `data.frame:`

    ```
    CREATE TYPE t1 AS ...
    ```

-   Use this `TYPE` when defining your PL/R function

    ```
    ... RETURNS SET OF t1 AS ...
    ```


Sample SQL for this is given in the next example.

#### Example 3: Hierarchical Regression using PL/R 

The SQL below defines a `TYPE` and runs hierarchical regression using PL/R:

```
--Create TYPE to store model results
DROP TYPE IF EXISTS wj_model_results CASCADE;
CREATE TYPE wj_model_results AS (
  cs text, coefext float, ci_95_lower float, ci_95_upper float, 
  ci_90_lower, float, ci_90_upper float, ci_80_lower, 
  float, ci_80_upper float);

--Create PL/R function to run model in R
DROP FUNCTION wj.plr.RE(response float [ ], cs text [ ])
RETURNS SETOF wj_model_results AS
$$
  library(arm)
  y<- log(response)
  cs<- cs
  d_temp<- data.frame(y,cs)
  m0 <- lmer (y ~ 1 + (1 | cs), data=d_temp)
  cs_unique<- sort(unique(cs))
  n_cs_unique<- length(cs_unique)
  temp_m0<- data.frame(matrix0,n_cs_unique, 7))
  for (i in 1:n_cs_unique){temp_m0[i,]<-
    c(exp(coef(m0)$cs[i,1] + c(0,-1.96,1.96,-1.65,1.65
      -1.28,1.28)*se.ranef(m0)$cs[i]))}
  names(temp_m0)<- c("Coefest", "CI_95_Lower",
    "CI_95_Upper", "CI_90_Lower", "CI_90_Upper",
   "CI_80_Lower", "CI_80_Upper")
  temp_m0_v2<- data.frames(cs_unique, temp_m0)
  return(temp_m0_v2)
$$
LANGUAGE 'plr';

--Run modeling plr function and store model results in a 
--table
DROP TABLE IF EXISTS wj_model_results_roi;
CREATE TABLE wj_model_results_roi AS SELECT * 
  FROM wj.plr_RE((SELECT wj.droi2_array), 
  (SELECT cs FROM wj.droi2_array));
```

### Downloading and Installing R Packages 

R packages are modules that contain R functions and data sets. You can install R packages to extend R and PL/R functionality in Greenplum Database.

Greenplum Database provides a collection of data science-related R libraries that can be used with the Greenplum Database PL/R language. You can download these libraries in `.gppkg` format from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). For information about the libraries, see [R Data Science Library Package](/vmware/install_guide/install_r_dslib.html).

**Note:** If you expand Greenplum Database and add segment hosts, you must install the R packages in the R installation of the new hosts.

1.  For an R package, identify all dependent R packages and each package web URL. The information can be found by selecting the given package from the following navigation page:

    [http://cran.r-project.org/web/packages/available\_packages\_by\_name.html](http://cran.r-project.org/web/packages/available_packages_by_name.html)

    As an example, the page for the R package arm indicates that the package requires the following R libraries: Matrix, lattice, lme4, R2WinBUGS, coda, abind, foreign, and MASS.

    You can also try installing the package with `R CMD INSTALL` command to determine the dependent packages.

    For the R installation included with the Greenplum Database PL/R extension, the required R packages are installed with the PL/R extension. However, the Matrix package requires a newer version.

2.  From the command line, use the `wget` utility to download the `tar.gz` files for the arm package to the Greenplum Database master host:

    ```
    wget http://cran.r-project.org/src/contrib/Archive/arm/arm_1.5-03.tar.gz
    ```

    ```
    wget http://cran.r-project.org/src/contrib/Archive/Matrix/Matrix_0.9996875-1.tar.gz
    ```

3.  Use the `gpscp` utility and the `hosts_all` file to copy the `tar.gz` files to the same directory on all nodes of the Greenplum Database cluster. The `hosts_all` file contains a list of all the Greenplum Database segment hosts. You might require root access to do this.

    ```
    gpscp -f hosts_all Matrix_0.9996875-1.tar.gz =:/home/gpadmin 
    ```

    ```
    gpscp -f /hosts_all arm_1.5-03.tar.gz =:/home/gpadmin
    ```

4.  Use the `gpssh` utility in interactive mode to log into each Greenplum Database segment host \(`gpssh -f all_hosts`\). Install the packages from the command prompt using the `R CMD INSTALL` command. Note that this may require root access. For example, this R install command installs the packages for the arm package.

    ```
    $R_HOME/bin/R CMD INSTALL Matrix_0.9996875-1.tar.gz   arm_1.5-03.tar.gz
    ```

5.  Ensure that the package is installed in the `$R_HOME/library` directory on all the segments \(the `gpssh` can be use to install the package\). For example, this `gpssh` command list the contents of the R library directory.

    ```
    gpssh -s -f all_hosts "ls $R_HOME/library"
    ```

    The `gpssh` option `-s` sources the `greenplum_path.sh` file before running commands on the remote hosts.

6.  Test if the R package can be loaded.

    This function performs a simple test to if an R package can be loaded:

    ```
    CREATE OR REPLACE FUNCTION R_test_require(fname text)
    RETURNS boolean AS
    $BODY$
        return(require(fname,character.only=T))
    $BODY$
    LANGUAGE 'plr';
    ```

    This SQL command checks if the R package arm can be loaded:

    ```
    SELECT R_test_require('arm');
    ```


### Displaying R Library Information 

You can use the R command line to display information about the installed libraries and functions on the Greenplum Database host. You can also add and remove libraries from the R installation. To start the R command line on the host, log into the host as the `gadmin` user and run the script R from the directory `$GPHOME/ext/R-3.3.3/bin`.

This R function lists the available R packages from the R command line:

```
> library()
```

Display the documentation for a particular R package

```
> library(help="<package_name>")
> help(package="<package_name>")
```

Display the help file for an R function:

```
> help("<function_name>")
> ?<function_name>
```

To see what packages are installed, use the R command `installed.packages()`. This will return a matrix with a row for each package that has been installed. Below, we look at the first 5 rows of this matrix.

```
> installed.packages()
```

Any package that does not appear in the installed packages matrix must be installed and loaded before its functions can be used.

An R package can be installed with `install.packages()`:

```
> install.packages("<package_name>") 
> install.packages("mypkg", dependencies = TRUE, type="source")
```

Load a package from the R command line.

```
> library(" <package_name> ") 
```

An R package can be removed with `remove.packages`

```
> remove.packages("<package_name>")
```

You can use the R command `-e` option to run functions from the command line. For example, this command displays help on the R package MASS.

```
$ R -e 'help("MASS")'
```

### References 

[http://www.r-project.org/](http://www.r-project.org/) - The R Project home page

[https://cran.r-project.org/web/packages/PivotalR/](https://cran.r-project.org/web/packages/PivotalR/) - The home page for PivotalR, a package that provides an R interface to operate on Greenplum Database tables and views that is similar to the R data.frame. PivotalR also supports using the machine learning package [MADlib](http://madlib.apache.org/) directly from R.

R documentation is installed with the Greenplum R package:

`$GPHOME/ext/R-3.3.3/doc`

#### R Functions and Arguments 

-   See [http://www.joeconway.com/doc/plr-funcs.html](http://www.joeconway.com/doc/plr-funcs.html)

#### Passing Data Values in R 

-   See [http://www.joeconway.com/doc/plr-data.html](http://www.joeconway.com/doc/plr-data.html)

#### Aggregate Functions in R 

-   See [http://www.joeconway.com/doc/plr-aggregate-funcs.html](http://www.joeconway.com/doc/plr-aggregate-funcs.html)

