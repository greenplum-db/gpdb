The Greenplum Partner Connector (GPPC) is a Greenplum Database extension
that lets you create a Greenplum Database user-defined function (UDF)
written in C/C++.

Take GPPC as a wrapper that links the C/C++ UDF and Greenplum Database.
You can define a UDF with with the GPPC API, and GPPC translates the UDF
to Greenplum Database compatible UDFs. This lets the GPPC UDF work on
different GPDB platforms, for example, different version of GPDB,
without the need to recompile or modify the GPPC UDF.

GPPC supports UDFs written in C/C++ and is similar to the Postgres UDFs written
in C.

In a Greenplum Database UDF:

```
PG_FUNCTION_INFO_V1(int2mulfunc);

Datum
int2mulfunc(PG_FUNCTION_ARGS)
{
    int16   arg1 = PG_GETARG_INT16(0);
    int16   arg2 = PG_GETARG_INT16(1);

    PG_RETURN_INT16(arg1 * arg2);
}
```

In a GPPC UDF:

```
GPPC_FUNCTION_INFO(int2mulfunc);
GppcDatum int2mulfunc(GPPC_FUNCTION_ARGS);

GppcDatum int2mulfunc(GPPC_FUNCTION_ARGS)
{
    GppcInt2    arg1 = GPPC_GETARG_INT2(0);
    GppcInt2    arg2 = GPPC_GETARG_INT2(1);

    GPPC_RETURN_INT2(arg1 * arg2);
}
```

### Usage

1. Compile the UDF as a dynamic library (with include "gppc.h")  and
place it in a location accessible to Greenplum Database. For example,
$libdir

2. Define a GPPC function in the database. For example:

```
CREATE FUNCTION textcopyfunc(text, bool) RETURNS text
    AS '$libdir/textcopyfunc'
    LANGUAGE C;
```

3. Call the function:

```
SELECT textcopyfunc('white', true), textcopyfunc('white', false);
```

Following the previous steps, you can use the following example UDF
definition to create a GPPC function. The function name is textcopyfunc
with two input parameters, a text and a bool. If the second parameter is
true, the function changes the first character of the input text to "!",
otherwise, it returns the original text.

```
#include "gppc.h"

GPPC_FUNCTION_INFO(textcopyfunc);
GppcDatum textcopyfunc(GPPC_FUNCTION_ARGS);

GppcDatum textcopyfunc(GPPC_FUNCTION_ARGS)
{
    GppcText    copy = GPPC_GETARG_TEXT_COPY(0);
    GppcText    arg = GPPC_GETARG_TEXT(0);
    GppcBool    needcopy = GPPC_GETARG_BOOL(1);

    *(GppcGetTextPointer(copy)) = '!';

    GPPC_RETURN_TEXT(needcopy ? copy : arg);
}
```

### To create a GPPC UDF in Greenplum Database:

Copy the code for the function into a text file named textcopy.c.

Run `make` and `make install` to create a dynamic library textcopy.so
and install the library into Greenplum Database. In the Makefile, we
need to link the GPPC and may use PGXS to compile. E.g. we can add the
following option into Makefile

```
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc
include $(PGXS)
```

Assuming the dynamic library is textcopy.so, you can use following SQL
statement to define the UDF in GPDB database.

```
CREATE OR REPLACE FUNCTION textcopyfunc(text, bool) RETURNS text
   AS '$libdir/textcopy.so'
   LANGUAGE C;
```

After defining the UDF, we can use it in an SQL command. This SELECT command
runs the UDF twice.

```
SELECT textcopyfunc('white', true), textcopyfunc('white', false);
```

There is a demo folder which include the usage for the APIs in GPPC, the link
is included in the reference section.

Notes:
1. The minimum required version of Greenplum Database is 4.2.2.0.
2. The UDF must be written in C/C++ language, and it must use the GPPC API and
   macros.
3. Memory must be allocated or freed with the GPPC wrapped functions (for
   example. GppcAlloc(), GppcAlloc0(), GppcRealloc(), and GppcFree()).
4. The UDF cannot use Greenplum Database C language UDF API or macros.
5. The GPPC API and macros can be found in gppc.h located in
`$GPDB_INSTALLED_DIR/include/gppc.h`

### SPI Functions

GPPC API provides a set of SPI wrapper functions:

```
SPI_connect -> GppcSPIConnect
SPI_finish -> GppcSPIFinish
SPI_exec -> GppcSPIExec
SPI_getvalue -> GppcSPIGetValue
```

### GPPC Functions

GPPC API provides a set of GPPC-specific functions:

```
/**
 * \brief Retrieves an SQL result attribute as a ::GppcDatum.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fnumber attribute number to extract, starting from 1.
 * \param isnull to be set true if the returned value is SQL NULL.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 */
GppcDatum GppcSPIGetDatum(GppcSPIResult result, int fnumber, bool *isnull, bool makecopy);

/**
 * \brief Retrieves an SQL result attribute as a character string.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fname attribute name to extract.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 *
 * Note that GppcSPIGetValue() is faster than this function.
 */
char *GppcSPIGetValueByName(GppcSPIResult result, const char *fname, bool makecopy);

/**
 * \brief Retrieves an SQL result attribute as a ::GppcDatum.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fname attribute name to extract.
 * \param isnull to be set true if the returned value is SQL NULL.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 *
 * Note that GppcSPIGetDatum() is faster than this function.
 */
GppcDatum GppcSPIGetDatumByName(GppcSPIResult result, const char *fname, bool *isnull, bool makecopy);
```

### Table Functions

The GPPC API provides a set of functions that support tables as the
input or output value for a UDF. For example, the GPPC transform function which
is implemented with GPPC table function:

```
FUNCTION transform(a anytable) RETURNS SETOF outtable
```

You can use the function to process a table and use select statement to select
the element from a result set:

```
SELECT * FROM transform(
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id)
    )  AS t (a text, b int) order by b;
```

The more examples of using GPPC table function are listed in  
https://github.com/greenplum-db/gpdb/tree/main/src/interfaces/gppc/test/tabfunc_gppc_demo.c

### Reference
https://www.postgresql.org/docs/devel/xfunc-c.html  
https://github.com/greenplum-db/gpdb/blob/main/src/include/gppc/gppc.h  
https://github.com/greenplum-db/gpdb/tree/main/src/interfaces/gppc/test/gppc_demo.c  
https://github.com/greenplum-db/gpdb/tree/main/src/interfaces/gppc/test/tabfunc_gppc_demo.c

