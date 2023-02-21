# Greenplum Partner Connector API 

With the Greenplum Partner Connector API \(GPPC API\), you can write portable Greenplum Database user-defined functions \(UDFs\) in the C and C++ programming languages. Functions that you develop with the GPPC API require no recompilation or modification to work with older or newer Greenplum Database versions.

Functions that you write to the GPPC API can be invoked using SQL in Greenplum Database. The API provides a set of functions and macros that you can use to issue SQL commands through the Server Programming Interface \(SPI\), manipulate simple and composite data type function arguments and return values, manage memory, and handle data.

You compile the C/C++ functions that you develop with the GPPC API into a shared library. The GPPC functions are available to Greenplum Database users after the shared library is installed in the Greenplum Database cluster and the GPPC functions are registered as SQL UDFs.

> **Note** The Greenplum Partner Connector is supported for Greenplum Database versions 4.3.5.0 and later.

This topic contains the following information:

-   [Using the GPPC API](#topic_dev)
    -   [Requirements](#topic_reqs)
    -   [Header and Library Files](#topic_files)
    -   [Data Types](#topic_data)
    -   [Function Declaration, Arguments, and Results](#topic_argres)
    -   [Memory Handling](#topic_mem)
    -   [Working With Variable-Length Text Types](#topic_varlentext)
    -   [Error Reporting and Logging](#topic_errrpt)
    -   [SPI Functions](#topic_spi)
    -   [About Tuple Descriptors and Tuples](#topic_tuple)
    -   [Set-Returning Functions](#topic_srf)
    -   [Table Functions](#topic_tblfunc)
    -   [Limitations](#topic_limits)
    -   [Sample Code](#topic_samplecode)
-   [Building a GPPC Shared Library with PGXS](#topic_build)
-   [Registering a GPPC Function with Greenplum Database](#topic_reg)
-   [Packaging and Deployment Considerations](#topic_deploy)
-   [GPPC Text Function Example](#topic_example_text)
-   [GPPC Set-Returning Function Example](#topic_example_srf)

## <a id="topic_dev"></a>Using the GPPC API 

The GPPC API shares some concepts with C language functions as defined by PostgreSQL. Refer to [C-Language Functions](https://www.postgresql.org/docs/12/xfunc-c.html) in the PostgreSQL documentation for detailed information about developing C language functions.

The GPPC API is a wrapper that makes a C/C++ function SQL-invokable in Greenplum Database. This wrapper shields GPPC functions that you write from Greenplum Database library changes by normalizing table and data manipulation and SPI operations through functions and macros defined by the API.

The GPPC API includes functions and macros to:

-   Operate on base and composite data types.
-   Process function arguments and return values.
-   Allocate and free memory.
-   Log and report errors to the client.
-   Issue SPI queries.
-   Return a table or set of rows.
-   Process tables as function input arguments.

### <a id="topic_reqs"></a>Requirements 

When you develop with the GPPC API:

-   You must develop your code on a system with the same hardware and software architecture as that of your Greenplum Database hosts.
-   You must write the GPPC function\(s\) in the C or C++ programming languages.
-   The function code must use the GPPC API, data types, and macros.
-   The function code must *not* use the PostgreSQL C-Language Function API, header files, functions, or macros.
-   The function code must *not* `#include` the `postgres.h` header file or use `PG_MODULE_MAGIC`.
-   You must use only the GPPC-wrapped memory functions to allocate and free memory. See [Memory Handling](#topic_mem).
-   Symbol names in your object files must not conflict with each other nor with symbols defined in the Greenplum Database server. You must rename your functions or variables if you get error messages to this effect.

### <a id="topic_files"></a>Header and Library Files 

The GPPC header files and libraries are installed in `$GPHOME`:

-   $GPHOME/include/gppc.h - the main GPPC header file
-   $GPHOME/include/gppc\_config.h - header file defining the GPPC version
-   $GPHOME/lib/libgppc.\[a, so, so.1, so.1.2\] - GPPC archive and shared libraries

### <a id="topic_data"></a>Data Types 

The GPPC functions that you create will operate on data residing in Greenplum Database. The GPPC API includes data type definitions for equivalent Greenplum Database SQL data types. You must use these types in your GPPC functions.

The GPPC API defines a generic data type that you can use to represent any GPPC type. This data type is named `GppcDatum`, and is defined as follows:

```
typedef int64_t GppcDatum;
```

The following table identifies each GPPC data type and the SQL type to which it maps.

|SQL Type|GPPC Type|GPPC Oid for Type|
|--------|---------|-----------------|
|boolean|GppcBool|GppcOidBool|
|char \(single byte\)|GppcChar|GppcOidChar|
|int2/smallint|GppcInt2|GppcOidInt2|
|int4/integer|GppcInt4|GppcOidInt4|
|int8/bigint|GppcInt8|GppcOidInt8|
|float4/real|GppcFloat4|GppcOidFloat4|
|float8/double|GppcFloat8|GppcOidFloat8|
|text|\*GppcText|GppcOidText|
|varchar|\*GppcVarChar|GppcOidVarChar|
|char|\*GppcBpChar|GppcOidBpChar|
|bytea|\*GppcBytea|GppcOidBytea|
|numeric|\*GppcNumeric|GppcOidNumeric|
|date|GppcDate|GppcOidDate|
|time|GppcTime|GppcOidTime|
|timetz|\*GppcTimeTz|GppcOidTimeTz|
|timestamp|GppcTimestamp|GppcOidTimestamp|
|timestamptz|GppcTimestampTz|GppcOidTimestampTz|
|anytable|GppcAnyTable|GppcOidAnyTable|
|oid|GppcOid| |



The GPPC API treats text, numeric, and timestamp data types specially, providing functions to operate on these types.

Example GPPC base data type declarations:

```
GppcText       message;
GppcInt4       arg1;
GppcNumeric    total_sales;
```

The GPPC API defines functions to convert between the generic `GppcDatum` type and the GPPC specific types. For example, to convert from an integer to a datum:

```

GppcInt4 num = 13;
GppcDatum num_dat = GppcInt4GetDatum(num);
```

#### <a id="topic_composite"></a>Composite Types 

A composite data type represents the structure of a row or record, and is comprised of a list of field names and their data types. This structure information is typically referred to as a tuple descriptor. An instance of a composite type is typically referred to as a tuple or row. A tuple does not have a fixed layout and can contain null fields.

The GPPC API provides an interface that you can use to define the structure of, to access, and to set tuples. You will use this interface when your GPPC function takes a table as an input argument or returns table or set of record types. Using tuples in table and set returning functions is covered later in this topic.

### <a id="topic_argres"></a>Function Declaration, Arguments, and Results 

The GPPC API relies on macros to declare functions and to simplify the passing of function arguments and results. These macros include:

|Task|Macro Signature|Description|
|----|---------------|-----------|
|Make a function SQL-invokable|`GPPC_FUNCTION_INFO(function\_name)`|Glue to make function `function\_name` SQL-invokable.|
|Declare a function|`GppcDatum function\_name(GPPC_FUNCTION_ARGS)`|Declare a GPPC function named `function\_name`; every function must have this same signature.|
|Return the number of arguments|`GPPC_NARGS()`|Return the number of arguments passed to the function.|
|Fetch an argument|`GPPC_GETARG_<ARGTYPE>(arg\_num)`|Fetch the value of argument number arg\_num \(starts at 0\), where `<ARGTYPE>` identifies the data type of the argument. For example, `GPPC_GETARG_FLOAT8(0)`.|
|Fetch and make a copy of a text-type argument|`GPPC_GETARG_<ARGTYPE>_COPY(arg\_num)`|Fetch and make a copy of the value of argument number arg\_num \(starts at 0\). `<ARGTYPE>` identifies the text type \(text, varchar, bpchar, bytea\). For example, `GPPC_GETARG_BYTEA_COPY(1)`.|
|Determine if an argument is NULL|`GPPC_ARGISNULL(arg\_num)`|Return whether or not argument number `arg\_num` is NULL.|
|Return a result|`GPPC_RETURN_<ARGTYPE>(return\_val)`|Return the value `return\_val`, where `<ARGTYPE>` identifies the data type of the return value. For example, `GPPC_RETURN_INT4(131)`.|



When you define and implement your GPPC function, you must declare it with the GPPC API using the two declarations identified above. For example, to declare a GPPC function named `add_int4s()`:

```
GPPC_FUNCTION_INFO(add_int4s);
GppcDatum add_int4s(GPPC_FUNCTION_ARGS);

GppcDatum
add_int4s(GPPC_FUNCTION_ARGS)
{
  // code here
}
```

If the `add_int4s()` function takes two input arguments of type `int4`, you use the `GPPC_GETARG_INT4(arg\_num)` macro to access the argument values. The argument index starts at 0. For example:

```
GppcInt4  first_int = GPPC_GETARG_INT4(0);
GppcInt4  second_int = GPPC_GETARG_INT4(1);
```

If `add_int4s()` returns the sum of the two input arguments, you use the `GPPC_RETURN_INT8(return\_val)` macro to return this sum. For example:

```
GppcInt8  sum = first_int + second_int;
GPPC_RETURN_INT8(sum);
```

The complete GPPC function:

```
GPPC_FUNCTION_INFO(add_int4s);
GppcDatum add_int4s(GPPC_FUNCTION_ARGS);

GppcDatum
add_int4s(GPPC_FUNCTION_ARGS)
{
  // get input arguments
  GppcInt4    first_int = GPPC_GETARG_INT4(0);
  GppcInt4    second_int = GPPC_GETARG_INT4(1);

  // add the arguments
  GppcInt8    sum = first_int + second_int;

  // return the sum
  GPPC_RETURN_INT8(sum);
}
```

### <a id="topic_mem"></a>Memory Handling 

The GPPC API provides functions that you use to allocate and free memory, including text memory. You must use these functions for all memory operations.

|Function Name|Description|
|-------------|-----------|
|void \*GppcAlloc\( size\_t num \)|Allocate num bytes of uninitialized memory.|
|void \*GppcAlloc0\( size\_t num \)|Allocate num bytes of 0-initialized memory.|
|void \*GppcRealloc\( void \*ptr, size\_t num \)|Resize pre-allocated memory.|
|void GppcFree\( void \*ptr \)|Free allocated memory.|

After you allocate memory, you can use system functions such as `memcpy()` to set the data.

The following example allocates an array of `GppcDatum`s and sets the array to datum versions of the function input arguments:

```
GppcDatum  *values;
int attnum = GPPC_NARGS();

// allocate memory for attnum values
values = GppcAlloc( sizeof(GppcDatum) * attnum );

// set the values
for( int i=0; i<attnum; i++ ) {
    GppcDatum d = GPPC_GETARG_DATUM(i);
    values[i] = d;
}
```

When you allocate memory for a GPPC function, you allocate it in the current context. The GPPC API includes functions to return, create, switch, and reset memory contexts.

|Function Name|Description|
|-------------|-----------|
|GppcMemoryContext GppcGetCurrentMemoryContext\(void\)|Return the current memory context.|
|GppcMemoryContext GppcMemoryContextCreate\(GppcMemoryContext parent\)|Create a new memory context under parent.|
|GppcMemoryContext GppcMemoryContextSwitchTo\(GppcMemoryContext context\)|Switch to the memory context context.|
|void GppcMemoryContextReset\(GppcMemoryContext context\)|Reset \(free\) the memory in memory context context.|

Greenplum Database typically calls a SQL-invoked function in a per-tuple context that it creates and deletes every time the server backend processes a table row. Do not assume that memory allocated in the current memory context is available across multiple function calls.

### <a id="topic_varlentext"></a>Working With Variable-Length Text Types 

The GPPC API supports the variable length text, varchar, blank padded, and byte array types. You must use the GPPC API-provided functions when you operate on these data types. Variable text manipulation functions provided in the GPPC API include those to allocate memory for, determine string length of, get string pointers for, and access these types:

|Function Name|Description|
|-------------|-----------|
|GppcText GppcAllocText\( size\_t len \)<br/><br/>GppcVarChar GppcAllocVarChar\( size\_t len \)<br/><br/>GppcBpChar GppcAllocBpChar\( size\_t len \)<br/><br/><br/><br/>GppcBytea GppcAllocBytea\( size\_t len \)|Allocate len bytes of memory for the varying length type.|
|size\_t GppcGetTextLength\( GppcText s \)<br/><br/>size\_t GppcGetVarCharLength\( GppcVarChar s \)<br/><br/>size\_t GppcGetBpCharLength\( GppcBpChar s \)<br/><br/>size\_t GppcGetByteaLength\( GppcBytea b \)|Return the number of bytes in the memory chunk.|
|char \*GppcGetTextPointer\( GppcText s \)<br/><br/>char \*GppcGetVarCharPointer\( GppcVarChar s \)<br/><br/>char \*GppcGetBpCharPointer\( GppcBpChar s \)<br/><br/>char \*GppcGetByteaPointer\( GppcBytea b \)|Return a string pointer to the head of the memory chunk. The string is not null-terminated.|
|char \*GppcTextGetCString\( GppcText s \)<br/><br/>char \*GppcVarCharGetCString\( GppcVarChar s \)<br/><br/>char \*GppcBpCharGetCString\( GppcBpChar s \)|Return a string pointer to the head of the memory chunk. The string is null-terminated.|
|GppcText \*GppcCStringGetText\( const char \*s \)<br/><br/>GppcVarChar \*GppcCStringGetVarChar\( const char \*s \)<br/><br/>GppcBpChar \*GppcCStringGetBpChar\( const char \*s \)|Build a varying-length type from a character string.|

Memory returned by the `GppcGet<VLEN_ARGTYPE>Pointer()` functions may point to actual database content. Do not modify the memory content. The GPPC API provides functions to allocate memory for these types should you require it. After you allocate memory, you can use system functions such as `memcpy()` to set the data.

The following example manipulates text input arguments and allocates and sets result memory for a text string concatenation operation:

```
GppcText first_textstr = GPPC_GETARG_TEXT(0);
GppcText second_textstr = GPPC_GETARG_TEXT(1);

// determine the size of the concatenated string and allocate
// text memory of this size
size_t arg0_len = GppcGetTextLength(first_textstr);
size_t arg1_len = GppcGetTextLength(second_textstr);
GppcText retstring = GppcAllocText(arg0_len + arg1_len);

// construct the concatenated return string; copying each string
// individually
memcpy(GppcGetTextPointer(retstring), GppcGetTextPointer(first_textstr), arg0_len);
memcpy(GppcGetTextPointer(retstring) + arg0_len, GppcGetTextPointer(second_textstr), arg1_len);

```

### <a id="topic_errrpt"></a>Error Reporting and Logging 

The GPPC API provides error reporting and logging functions. The API defines reporting levels equivalent to those in Greenplum Database:

```
typedef enum GppcReportLevel
{
        GPPC_DEBUG1                             = 10,
        GPPC_DEBUG2                             = 11,
        GPPC_DEBUG3                             = 12,
        GPPC_DEBUG4                             = 13,
        GPPC_DEBUG                              = 14,
        GPPC_LOG                                = 15,
        GPPC_INFO                               = 17,
        GPPC_NOTICE                             = 18,
        GPPC_WARNING                    	= 19,
        GPPC_ERROR                              = 20,
} GppcReportLevel;

```

\(The Greenplum Database [`client_min_messages`](../config_params/guc-list.html) server configuration parameter governs the current client logging level. The [`log_min_messages`](../config_params/guc-list.html) configuration parameter governs the current log-to-logfile level.\)

A GPPC report includes the report level, a report message, and an optional report callback function.

Reporting and handling functions provide by the GPPC API include:

|Function Name|Description|
|-------------|-----------|
|GppcReport\(\)|Format and print/log a string of the specified report level.|
|GppcInstallReportCallback\(\)|Register/install a report callback function.|
|GppcUninstallReportCallback\(\)|Uninstall a report callback function.|
|GppcGetReportLevel\(\)|Retrieve the level from an error report.|
|GppcGetReportMessage\(\)|Retrieve the message from an error report.|
|GppcCheckForInterrupts\(\)|Error out if an interrupt is pending.|



The `GppcReport()` function signature is:

```
void GppcReport(GppcReportLevel elevel, const char *fmt, ...);
```

`GppcReport()` takes a format string input argument similar to `printf()`. The following example generates an error level report message that formats a GPPC text argument:

```
GppcText  uname = GPPC_GETARG_TEXT(1);
GppcReport(GPPC_ERROR, "Unknown user name: %s", GppcTextGetCString(uname));
```

Refer to the [GPPC example code](https://github.com/greenplum-db/gpdb/tree/main/src/interfaces/gppc/test) for example report callback handlers.

### <a id="topic_spi"></a>SPI Functions 

The Greenplum Database Server Programming Interface \(SPI\) provides writers of C/C++ functions the ability to run SQL commands within a GPPC function. For additional information on SPI functions, refer to [Server Programming Interface](https://www.postgresql.org/docs/12/spi.html) in the PostgreSQL documentation.

The GPPC API exposes a subset of PostgreSQL SPI functions. This subset enables you to issue SPI queries and retrieve SPI result values in your GPPC function. The GPPC SPI wrapper functions are:

|SPI Function Name|GPPC Function Name|Description|
|-----------------|------------------|-----------|
|SPI\_connect\(\)|GppcSPIConnect\(\)|Connect to the Greenplum Database server programming interface.|
|SPI\_finish\(\)|GppcSPIFinish\(\)|Disconnect from the Greenplum Database server programming interface.|
|SPI\_exec\(\)|GppcSPIExec\(\)|Run a SQL statement, returning the number of rows.|
|SPI\_getvalue\(\)|GppcSPIGetValue\(\)|Retrieve the value of a specific attribute by number from a SQL result as a character string.|
|GppcSPIGetDatum\(\)|Retrieve the value of a specific attribute by number from a SQL result as a `GppcDatum`.|
|GppcSPIGetValueByName\(\)|Retrieve the value of a specific attribute by name from a SQL result as a character string.|
|GppcSPIGetDatumByName\(\)|Retrieve the value of a specific attribute by name from a SQL result as a `GppcDatum`.|



When you create a GPPC function that accesses the server programming interface, your function should comply with the following flow:

```
GppcSPIConnect();
GppcSPIExec(...)
// process the results - GppcSPIGetValue(...), GppcSPIGetDatum(...)
GppcSPIFinish()
```

You use `GppcSPIExec()` to run SQL statements in your GPPC function. When you call this function, you also identify the maximum number of rows to return. The function signature of `GppcSPIExec()` is:

```
GppcSPIResult GppcSPIExec(const char *sql_statement, long rcount);
```

`GppcSPIExec()` returns a `GppcSPIResult` structure. This structure represents SPI result data. It includes a pointer to the data, information about the number of rows processed, a counter, and a result code. The GPPC API defines this structure as follows:

```
typedef struct GppcSPIResultData
{
    struct GppcSPITupleTableData   *tuptable;
    uint32_t                       processed;
    uint32_t                       current;
    int                            rescode;
} GppcSPIResultData;
typedef GppcSPIResultData *GppcSPIResult;
```

You can set and use the `current` field in the `GppcSPIResult` structure to examine each row of the `tuptable` result data.

The following code excerpt uses the GPPC API to connect to SPI, run a simple query, loop through query results, and finish processing:

```
GppcSPIResult   result;
char            *attname = "id";
char            *query = "SELECT i, 'foo' || i AS val FROM generate_series(1, 10)i ORDER BY 1";
bool            isnull = true;

// connect to SPI
if( GppcSPIConnect() < 0 ) {
    GppcReport(GPPC_ERROR, "cannot connect to SPI");
}

// execute the query, returning all rows
result = GppcSPIExec(query, 0);

// process result
while( result->current < result->processed ) {
    // get the value of attname column as a datum, making a copy
    datum = GppcSPIGetDatumByName(result, attname, &isnull, true);

    // do something with value

    // move on to next row
    result->current++;
}

// complete processing
GppcSPIFinish();

```

### <a id="topic_tuple"></a>About Tuple Descriptors and Tuples 

A table or a set of records contains one or more tuples \(rows\). The structure of each attribute of a tuple is defined by a tuple descriptor. A tuple descriptor defines the following for each attribute in the tuple:

-   attribute name
-   object identifier of the attribute data type
-   byte length of the attribute data type
-   object identifier of the attribute modifer

The GPPC API defines an abstract type, `GppcTupleDesc`, to represent a tuple/row descriptor. The API also provides functions that you can use to create, access, and set tuple descriptors:

|Function Name|Description|
|-------------|-----------|
|GppcCreateTemplateTupleDesc\(\)|Create an empty tuple descriptor with a specified number of attributes.|
|GppcTupleDescInitEntry\(\)|Add an attribute to the tuple descriptor at a specified position.|
|GppcTupleDescNattrs\(\)|Fetch the number of attributes in the tuple descriptor.|
|GppcTupleDescAttrName\(\)|Fetch the name of the attribute in a specific position \(starts at 0\) in the tuple descriptor.|
|GppcTupleDescAttrType\(\)|Fetch the type object identifier of the attribute in a specific position \(starts at 0\) in the tuple descriptor.|
|GppcTupleDescAttrLen\(\)|Fetch the type length of an attribute in a specific position \(starts at 0\) in the tuple descriptor.|
|GppcTupleDescAttrTypmod\(\)|Fetch the type modifier object identifier of an attribute in a specific position \(starts at 0\) in the tuple descriptor.|



To construct a tuple descriptor, you first create a template, and then fill in the descriptor fields for each attribute. The signatures for these functions are:

```
GppcTupleDesc GppcCreateTemplateTupleDesc(int natts);
void GppcTupleDescInitEntry(GppcTupleDesc desc, uint16_t attno,
                            const char *attname, GppcOid typid, int32_t typmod);
```

In some cases, you may want to initialize a tuple descriptor entry from an attribute definition in an existing tuple. The following functions fetch the number of attributes in a tuple descriptor, as well as the definition of a specific attribute \(by number\) in the descriptor:

```
int GppcTupleDescNattrs(GppcTupleDesc tupdesc);
const char *GppcTupleDescAttrName(GppcTupleDesc tupdesc, int16_t attno);
GppcOid GppcTupleDescAttrType(GppcTupleDesc tupdesc, int16_t attno);
int16_t GppcTupleDescAttrLen(GppcTupleDesc tupdesc, int16_t attno);
int32_t GppcTupleDescAttrTypmod(GppcTupleDesc tupdesc, int16_t attno);
```

The following example initializes a two attribute tuple descriptor. The first attribute is initialized with the definition of an attribute from a different descriptor, and the second attribute is initialized to a boolean type attribute:

```
GppcTupleDesc       tdesc;
GppcTupleDesc       indesc = some_input_descriptor;

// initialize the tuple descriptor with 2 attributes
tdesc = GppcCreateTemplateTupleDesc(2);

// use third attribute from the input descriptor
GppcTupleDescInitEntry(tdesc, 1, 
	       GppcTupleDescAttrName(indesc, 2),
	       GppcTupleDescAttrType(indesc, 2),
	       GppcTupleDescAttrTypmod(indesc, 2));

// create the boolean attribute
GppcTupleDescInitEntry(tdesc, 2, "is_active", GppcOidBool, 0);

```

The GPPC API defines an abstract type, `GppcHeapTuple`, to represent a tuple/record/row. A tuple is defined by its tuple descriptor, the value for each tuple attribute, and an indicator of whether or not each value is NULL.

The GPPC API provides functions that you can use to set and access a tuple and its attributes:

|Function Name|Description|
|-------------|-----------|
|GppcHeapFormTuple\(\)|Form a tuple from an array of `GppcDatum`s.|
|GppcBuildHeapTupleDatum\(\)|Form a `GppcDatum` tuple from an array of `GppcDatum`s.|
|GppcGetAttributeByName\(\)|Fetch an attribute from the tuple by name.|
|GppcGetAttributeByNum\(\)|Fetch an attribute from the tuple by number \(starts at 1\).|



The signatures for the tuple-building GPPC functions are:

```
GppcHeapTuple GppcHeapFormTuple(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls);
GppcDatum    GppcBuildHeapTupleDatum(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls);
```

The following code excerpt constructs a `GppcDatum` tuple from the tuple descriptor in the above code example, and from integer and boolean input arguments to a function:

```
GppcDatum intarg = GPPC_GETARG_INT4(0);
GppcDatum boolarg = GPPC_GETARG_BOOL(1);
GppcDatum result, values[2];
bool nulls[2] = { false, false };

// construct the values array
values[0] = intarg;
values[1] = boolarg;
result = GppcBuildHeapTupleDatum( tdesc, values, nulls );

```

### <a id="topic_srf"></a>Set-Returning Functions 

Greenplum Database UDFs whose signatures include `RETURNS SETOF RECORD` or `RETURNS TABLE( ... )` are set-returning functions.

The GPPC API provides support for returning sets \(for example, multiple rows/tuples\) from a GPPC function. Greenplum Database calls a set-returning function \(SRF\) once for each row or item. The function must save enough state to remember what it was doing and to return the next row on each call. Memory that you allocate in the SRF context must survive across multiple function calls.

The GPPC API provides macros and functions to help keep track of and set this context, and to allocate SRF memory. They include:

|Function/Macro Name|Description|
|-------------------|-----------|
|GPPC\_SRF\_RESULT\_DESC\(\)|Get the output row tuple descriptor for this SRF. The result tuple descriptor is determined by an output table definition or a `DESCRIBE` function.|
|GPPC\_SRF\_IS\_FIRSTCALL\(\)|Determine if this is the first call to the SRF.|
|GPPC\_SRF\_FIRSTCALL\_INIT\(\)|Initialize the SRF context.|
|GPPC\_SRF\_PERCALL\_SETUP\(\)|Restore the context on each call to the SRF.|
|GPPC\_SRF\_RETURN\_NEXT\(\)|Return a value from the SRF and continue processing.|
|GPPC\_SRF\_RETURN\_DONE\(\)|Signal that SRF processing is complete.|
|GppSRFAlloc\(\)|Allocate memory in this SRF context.|
|GppSRFAlloc0\(\)|Allocate memory in this SRF context and initialize it to zero.|
|GppSRFSave\(\)|Save user state in this SRF context.|
|GppSRFRestore\(\)|Restore user state in this SRF context.|



The `GppcFuncCallContext` structure provides the context for an SRF. You create this context on the first call to your SRF. Your set-returning GPPC function must retrieve the function context on each invocation. For example:

```
// set function context
GppcFuncCallContext fctx;
if (GPPC_SRF_IS_FIRSTCALL()) {
    fctx = GPPC_SRF_FIRSTCALL_INIT();
}
fctx = GPPC_SRF_PERCALL_SETUP();
// process the tuple

```

The GPPC function must provide the context when it returns a tuple result or to indicate that processing is complete. For example:

```
GPPC_SRF_RETURN_NEXT(fctx, result_tuple);
// or
GPPC_SRF_RETURN_DONE(fctx);
```

Use a `DESCRIBE` function to define the output tuple descriptor of a function that uses the `RETURNS SETOF RECORD` clause. Use the `GPPC_SRF_RESULT_DESC()` macro to get the output tuple descriptor of a function that uses the `RETURNS TABLE( ... )` clause.

Refer to the [GPPC Set-Returning Function Example](#topic_example_srf) for a set-returning function code and deployment example.

### <a id="topic_tblfunc"></a>Table Functions 

The GPPC API provides the `GppcAnyTable` type to pass a table to a function as an input argument, or to return a table as a function result.

Table-related functions and macros provided in the GPPC API include:

|Function/Macro Name|Description|
|-------------------|-----------|
|GPPC\_GETARG\_ANYTABLE\(\)|Fetch an anytable function argument.|
|GPPC\_RETURN\_ANYTABLE\(\)|Return the table.|
|GppcAnyTableGetTupleDesc\(\)|Fetch the tuple descriptor for the table.|
|GppcAnyTableGetNextTuple\(\)|Fetch the next row in the table.|



You can use the `GPPC_GETARG_ANYTABLE()` macro to retrieve a table input argument. When you have access to the table, you can examine the tuple descriptor for the table using the `GppcAnyTableGetTupleDesc()` function. The signature of this function is:

```
GppcTupleDesc GppcAnyTableGetTupleDesc(GppcAnyTable t);
```

For example, to retrieve the tuple descriptor of a table that is the first input argument to a function:

```
GppcAnyTable     intbl;
GppcTupleDesc    in_desc;

intbl = GPPC_GETARG_ANYTABLE(0);
in_desc = GppcAnyTableGetTupleDesc(intbl);
```

The `GppcAnyTableGetNextTuple()` function fetches the next row from the table. Similarly, to retrieve the next tuple from the table above:

```
GppcHeapTuple    ntuple;

ntuple = GppcAnyTableGetNextTuple(intbl);
```

### <a id="topic_limits"></a>Limitations 

The GPPC API does not support the following operators with Greenplum Database version 5.0.x:

-   integer \|\| integer
-   integer = text
-   text < integer

### <a id="topic_samplecode"></a>Sample Code 

The [gppc test](https://github.com/greenplum-db/gpdb/tree/main/src/interfaces/gppc/test) directory in the Greenplum Database github repository includes sample GPPC code:

-   `gppc_demo/` - sample code exercising GPPC SPI functions, error reporting, data type argument and return macros, set-returning functions, and encoding functions
-   `tabfunc_gppc_demo/` - sample code exercising GPPC table and set-returning functions

## <a id="topic_build"></a>Building a GPPC Shared Library with PGXS 

You compile functions that you write with the GPPC API into one or more shared libraries that the Greenplum Database server loads on demand.

You can use the PostgreSQL build extension infrastructure \(PGXS\) to build the source code for your GPPC functions against a Greenplum Database installation. This framework automates common build rules for simple modules. If you have a more complicated use case, you will need to write your own build system.

To use the PGXS infrastructure to generate a shared library for functions that you create with the GPPC API, create a simple `Makefile` that sets PGXS-specific variables.

> **Note** Refer to [Extension Building Infrastructure](https://www.postgresql.org/docs/12/extend-pgxs.html) in the PostgreSQL documentation for information about the `Makefile` variables supported by PGXS.

For example, the following `Makefile` generates a shared library named `sharedlib_name.so` from two C source files named `src1.c` and `src2.c`:

```
MODULE_big = sharedlib_name
OBJS = src1.o src2.o
PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
```

`MODULE_big` identifes the base name of the shared library generated by the `Makefile`.

`PG_CPPFLAGS` adds the Greenplum Database installation include directory to the compiler header file search path.

`SHLIB_LINK` adds the Greenplum Database installation library directory to the linker search path. This variable also adds the GPPC library \(`-lgppc`\) to the link command.

The `PG_CONFIG` and `PGXS` variable settings and the `include` statement are required and typically reside in the last three lines of the `Makefile`.

## <a id="topic_reg"></a>Registering a GPPC Function with Greenplum Database 

Before users can invoke a GPPC function from SQL, you must register the function with Greenplum Database.

Registering a GPPC function involves mapping the GPPC function signature to a SQL user-defined function. You define this mapping with the `CREATE FUNCTION .. AS` command specifying the GPPC shared library name. You may choose to use the same name or differing names for the GPPC and SQL functions.

Sample `CREATE FUNCTION ... AS` syntax follows:

```
CREATE FUNCTION <sql_function_name>(<arg>[, ...]) RETURNS <return_type>
  AS '<shared_library_path>'[, '<gppc_function_name>']
LANGUAGE C STRICT [WITH (DESCRIBE=<describe_function>)];
```

You may omit the shared library `.so` extension when you specify `shared\_library\_path`.

The following command registers the example `add_int4s()` function referenced earlier in this topic to a SQL UDF named `add_two_int4s_gppc()` if the GPPC function was compiled and linked in a shared library named `gppc_try.so`:

```
CREATE FUNCTION add_two_int4s_gppc(int4, int4) RETURNS int8
  AS 'gppc_try.so', 'add_int4s'
LANGUAGE C STRICT;
```

### <a id="topic_dynload"></a>About Dynamic Loading 

You specify the name of the GPPC shared library in the SQL `CREATE FUNCTION ... AS` command to register a GPPC function in the shared library with Greenplum Database. The Greenplum Database dynamic loader loads a GPPC shared library file into memory the first time that a user invokes a user-defined function linked in that shared library. If you do not provide an absolute path to the shared library in the `CREATE FUNCTION ... AS` command, Greenplum Database attempts to locate the library using these ordered steps:

1.  If the shared library file path begins with the string `$libdir`, Greenplum Database looks for the file in the PostgreSQL package library directory. Run the `pg_config --pkglibdir` command to determine the location of this directory.
2.  If the shared library file name is specified without a directory prefix, Greenplum Database searches for the file in the directory identified by the `dynamic_library_path` server configuration parameter value.
3.  The current working directory.

## <a id="topic_deploy"></a>Packaging and Deployment Considerations 

You must package the GPPC shared library and SQL function registration script in a form suitable for deployment by the Greenplum Database administrator in the Greenplum cluster. Provide specific deployment instructions for your GPPC package.

When you construct the package and deployment instructions, take into account the following:

-   Consider providing a shell script or program that the Greenplum Database administrator runs to both install the shared library to the desired file system location and register the GPPC functions.
-   The GPPC shared library must be installed to the same file system location on the coordinator host and on every segment host in the Greenplum Database cluster.
-   The `gpadmin` user must have permission to traverse the complete file system path to the GPPC shared library file.
-   The file system location of your GPPC shared library after it is installed in the Greenplum Database deployment determines how you reference the shared library when you register a function in the library with the `CREATE FUNCTION ... AS` command.
-   Create a `.sql` script file that registers a SQL UDF for each GPPC function in your GPPC shared library. The functions that you create in the `.sql` registration script must reference the deployment location of the GPPC shared library. Include this script in your GPPC deployment package.
-   Document the instructions for running your GPPC package deployment script, if you provide one.
-   Document the instructions for installing the GPPC shared library if you do not include this task in a package deployment script.
-   Document the instructions for installing and running the function registration script if you do not include this task in a package deployment script.

## <a id="topic_example_text"></a>GPPC Text Function Example 

In this example, you develop, build, and deploy a GPPC shared library and register and run a GPPC function named `concat_two_strings`. This function uses the GPPC API to concatenate two string arguments and return the result.

You will develop the GPPC function on your Greenplum Database coordinator host. Deploying the GPPC shared library that you create in this example requires administrative access to your Greenplum Database cluster.

Perform the following procedure to run the example:

1.  Log in to the Greenplum Database coordinator host and set up your environment. For example:

    ```
    $ ssh gpadmin@<gpcoordinator>
    gpadmin@gpcoordinator$ . /usr/local/greenplum-db/greenplum_path.sh
    ```

2.  Create a work directory and navigate to the new directory. For example:

    ```
    gpadmin@gpcoordinator$ mkdir gppc_work
    gpadmin@gpcoordinator$ cd gppc_work
    ```

3.  Prepare a file for GPPC source code by opening the file in the editor of your choice. For example, to open a file named `gppc_concat.c` using `vi`:

    ```
    gpadmin@gpcoordinator$ vi gppc_concat.c
    ```

4.  Copy/paste the following code into the file:

    ```
    #include <stdio.h>
    #include <string.h>
    #include "gppc.h"
    
    // make the function SQL-invokable
    GPPC_FUNCTION_INFO(concat_two_strings);
    
    // declare the function
    GppcDatum concat_two_strings(GPPC_FUNCTION_ARGS);
    
    GppcDatum
    concat_two_strings(GPPC_FUNCTION_ARGS)
    {
        // retrieve the text input arguments
        GppcText arg0 = GPPC_GETARG_TEXT(0);
        GppcText arg1 = GPPC_GETARG_TEXT(1);
    
        // determine the size of the concatenated string and allocate
        // text memory of this size
        size_t arg0_len = GppcGetTextLength(arg0);
        size_t arg1_len = GppcGetTextLength(arg1);
        GppcText retstring = GppcAllocText(arg0_len + arg1_len);
    
        // construct the concatenated return string
        memcpy(GppcGetTextPointer(retstring), GppcGetTextPointer(arg0), arg0_len);
        memcpy(GppcGetTextPointer(retstring) + arg0_len, GppcGetTextPointer(arg1), arg1_len);
    
        GPPC_RETURN_TEXT( retstring );
    }
    ```

    The code declares and implements the `concat_two_strings()` function. It uses GPPC data types, macros, and functions to get the function arguments, allocate memory for the concatenated string, copy the arguments into the new string, and return the result.

5.  Save the file and exit the editor.
6.  Open a file named `Makefile` in the editor of your choice. Copy/paste the following text into the file:

    ```
    MODULE_big = gppc_concat
    OBJS = gppc_concat.o
    
    PG_CONFIG = pg_config
    PGXS := $(shell $(PG_CONFIG) --pgxs)
    
    PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
    SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc
    include $(PGXS)
    ```

7.  Save the file and exit the editor.
8.  Build a GPPC shared library for the `concat_two_strings()` function. For example:

    ```
    gpadmin@gpcoordinator$ make all
    ```

    The `make` command generates a shared library file named `gppc_concat.so` in the current working directory.

9.  Copy the shared library to your Greenplum Database installation. You must have Greenplum Database administrative privileges to copy the file. For example:

    ```
    gpadmin@gpcoordinator$ cp gppc_concat.so /usr/local/greenplum-db/lib/postgresql/
    ```

10. Copy the shared library to every host in your Greenplum Database installation. For example, if `seghostfile` contains a list, one-host-per-line, of the segment hosts in your Greenplum Database cluster:

    ```
    gpadmin@gpcoordinator$ gpsync -v -f seghostfile /usr/local/greenplum-db/lib/postgresql/gppc_concat.so =:/usr/local/greenplum-db/lib/postgresql/gppc_concat.so
    ```

11. Open a `psql` session. For example:

    ```
    gpadmin@gpcoordinator$ psql -d testdb
    ```

12. Register the GPPC function named `concat_two_strings()` with Greenplum Database, For example, to map the Greenplum Database function `concat_with_gppc()` to the GPPC `concat_two_strings()` function:

    ```
    testdb=# CREATE FUNCTION concat_with_gppc(text, text) RETURNS text
      AS 'gppc_concat', 'concat_two_strings'
    LANGUAGE C STRICT;
    ```

13. Run the `concat_with_gppc()` function. For example:

    ```
    testdb=# SELECT concat_with_gppc( 'happy', 'monday' );
     concat_with_gppc
    ------------------
     happymonday
    (1 row)
    
    ```


## <a id="topic_example_srf"></a>GPPC Set-Returning Function Example 

In this example, you develop, build, and deploy a GPPC shared library. You also create and run a `.sql` registration script for a GPPC function named `return_tbl()`. This function uses the GPPC API to take an input table with an integer and a text column, determine if the integer column is greater than 13, and returns a result table with the input integer column and a boolean column identifying whether or not the integer is greater than 13. `return_tbl()` utilizes GPPC API reporting and SRF functions and macros.

You will develop the GPPC function on your Greenplum Database coordinator host. Deploying the GPPC shared library that you create in this example requires administrative access to your Greenplum Database cluster.

Perform the following procedure to run the example:

1.  Log in to the Greenplum Database coordinator host and set up your environment. For example:

    ```
    $ ssh gpadmin@<gpcoordinator>
    gpadmin@gpcoordinator$ . /usr/local/greenplum-db/greenplum_path.sh
    ```

2.  Create a work directory and navigate to the new directory. For example:

    ```
    gpadmin@gpcoordinator$ mkdir gppc_work
    gpadmin@gpcoordinator$ cd gppc_work
    ```

3.  Prepare a source file for GPPC code by opening the file in the editor of your choice. For example, to open a file named `gppc_concat.c` using `vi`:

    ```
    gpadmin@gpcoordinator$ vi gppc_rettbl.c
    ```

4.  Copy/paste the following code into the file:

    ```
    #include <stdio.h>
    #include <string.h>
    #include "gppc.h"
    
    // initialize the logging level
    GppcReportLevel level = GPPC_INFO;
    
    // make the function SQL-invokable and declare the function
    GPPC_FUNCTION_INFO(return_tbl);
    GppcDatum return_tbl(GPPC_FUNCTION_ARGS);
    
    GppcDatum
    return_tbl(GPPC_FUNCTION_ARGS)
    {
        GppcFuncCallContext	fctx;
        GppcAnyTable	intbl;
        GppcHeapTuple	intuple;
        GppcTupleDesc	in_tupdesc, out_tupdesc;
        GppcBool  		resbool = false;
        GppcDatum  		result, boolres, values[2];
        bool		nulls[2] = {false, false};
    
        // single input argument - the table
        intbl = GPPC_GETARG_ANYTABLE(0);
    
        // set the function context
        if (GPPC_SRF_IS_FIRSTCALL()) {
            fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();
    
        // get the tuple descriptor for the input table
        in_tupdesc  = GppcAnyTableGetTupleDesc(intbl);
    
        // retrieve the next tuple
        intuple = GppcAnyTableGetNextTuple(intbl);
        if( intuple == NULL ) {
          // no more tuples, conclude
          GPPC_SRF_RETURN_DONE(fctx);
        }
    
        // get the output tuple descriptor and verify that it is
        // defined as we expect
        out_tupdesc = GPPC_SRF_RESULT_DESC();
        if (GppcTupleDescNattrs(out_tupdesc) != 2                ||
            GppcTupleDescAttrType(out_tupdesc, 0) != GppcOidInt4 ||
            GppcTupleDescAttrType(out_tupdesc, 1) != GppcOidBool) {
            GppcReport(GPPC_ERROR, "INVALID out_tupdesc tuple");
        }
    
        // log the attribute names of the output tuple descriptor
        GppcReport(level, "output tuple descriptor attr0 name: %s", GppcTupleDescAttrName(out_tupdesc, 0));
        GppcReport(level, "output tuple descriptor attr1 name: %s", GppcTupleDescAttrName(out_tupdesc, 1));
    
        // retrieve the attribute values by name from the tuple
        bool text_isnull, int_isnull;
        GppcDatum intdat = GppcGetAttributeByName(intuple, "id", &int_isnull);
        GppcDatum textdat = GppcGetAttributeByName(intuple, "msg", &text_isnull);
    
        // convert datum to specific type
        GppcInt4 intarg = GppcDatumGetInt4(intdat);
        GppcReport(level, "id: %d", intarg);
        GppcReport(level, "msg: %s", GppcTextGetCString(GppcDatumGetText(textdat)));
    
        // perform the >13 check on the integer
        if( !int_isnull && (intarg > 13) ) {
            // greater than 13?
            resbool = true;
            GppcReport(level, "id is greater than 13!");
        }
    
        // values are datums; use integer from the tuple and
        // construct the datum for the boolean return
        values[0] = intdat;
        boolres = GppcBoolGetDatum(resbool);
        values[1] = boolres;
    
        // build a datum tuple and return
        result = GppcBuildHeapTupleDatum(out_tupdesc, values, nulls);
        GPPC_SRF_RETURN_NEXT(fctx, result);
    
    }
    ```

    The code declares and implements the `return_tbl()` function. It uses GPPC data types, macros, and functions to fetch the function arguments, examine tuple descriptors, build the return tuple, and return the result. The function also uses the SRF macros to keep track of the tuple context across function calls.

5.  Save the file and exit the editor.
6.  Open a file named `Makefile` in the editor of your choice. Copy/paste the following text into the file:

    ```
    MODULE_big = gppc_rettbl
    OBJS = gppc_rettbl.o
    
    PG_CONFIG = pg_config
    PGXS := $(shell $(PG_CONFIG) --pgxs)
    
    PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
    SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc
    include $(PGXS)
    ```

7.  Save the file and exit the editor.
8.  Build a GPPC shared library for the `return_tbl()` function. For example:

    ```
    gpadmin@gpcoordinator$ make all
    ```

    The `make` command generates a shared library file named `gppc_rettbl.so` in the current working directory.

9.  Copy the shared library to your Greenplum Database installation. You must have Greenplum Database administrative privileges to copy the file. For example:

    ```
    gpadmin@gpcoordinator$ cp gppc_rettbl.so /usr/local/greenplum-db/lib/postgresql/
    ```

    This command copies the shared library to `$libdir`

10. Copy the shared library to every host in your Greenplum Database installation. For example, if `seghostfile` contains a list, one-host-per-line, of the segment hosts in your Greenplum Database cluster:

    ```
    gpadmin@gpcoordinator$ gpsync -v -f seghostfile /usr/local/greenplum-db/lib/postgresql/gppc_rettbl.so =:/usr/local/greenplum-db/lib/postgresql/gppc_rettbl.so
    ```

11. Create a `.sql` file to register the GPPC `return_tbl()` function. Open a file named `gppc_rettbl_reg.sql` in the editor of your choice.
12. Copy/paste the following text into the file:

    ```
    CREATE FUNCTION rettbl_gppc(anytable) RETURNS TABLE(id int4, thirteen bool)
      AS 'gppc_rettbl', 'return_tbl'
    LANGUAGE C STRICT;
    ```

13. Register the GPPC function by running the script you just created. For example, to register the function in a database named `testdb`:

    ```
    gpadmin@gpcoordinator$ psql -d testdb -f gppc_rettbl_reg.sql
    ```

14. Open a `psql` session. For example:

    ```
    gpadmin@gpcoordinator$ psql -d testdb
    ```

15. Create a table with some test data. For example:

    ```
    CREATE TABLE gppc_testtbl( id int, msg text );
    INSERT INTO gppc_testtbl VALUES (1, 'f1');
    INSERT INTO gppc_testtbl VALUES (7, 'f7');
    INSERT INTO gppc_testtbl VALUES (10, 'f10');
    INSERT INTO gppc_testtbl VALUES (13, 'f13');
    INSERT INTO gppc_testtbl VALUES (15, 'f15');
    INSERT INTO gppc_testtbl VALUES (17, 'f17');
    ```

16. Run the `rettbl_gppc()` function. For example:

    ```
    testdb=# SELECT * FROM rettbl_gppc(TABLE(SELECT * FROM gppc_testtbl));
     id | thirteen 
    ----+----------
      1 | f
      7 | f
     13 | f
     15 | t
     17 | t
     10 | f
    (6 rows)
    
    ```


