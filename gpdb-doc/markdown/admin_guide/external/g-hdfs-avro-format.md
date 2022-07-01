# gphdfs Support for Avro Files \(Deprecated\) 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database. You can use the Greenplum Platform Extension Framework \(PXF\) to access Avro-format data.

You can use the Greenplum Database `gphdfs` protocol to access Avro files on a Hadoop file system \(HDFS\).

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

## About the Avro File Format 

An Avro file stores both the data definition \(schema\) and the data together in one file making it easy for programs to dynamically understand the information stored in an Avro file. The Avro schema is in JSON format, the data is in a binary format making it compact and efficient.

The following example Avro schema defines an Avro record with 3 fields:

-   name
-   favorite\_number
-   favorite\_color

```
{"namespace": "example.avro", 
"type": "record", "name": "User",
  "fields": [
    {"name": "name", "type": "string"},     
    {"name": "favorite_number", "type": ["int", "null"]},
    {"name": "favorite_color", "type": ["string", "null"]}
  ]
}
```

These are two rows of data based on the schema:

```
{ "name" : "miguno" , "favorite_number" : 6 , "favorite_color" : "red" }
{ "name" : "BlizzardCS" , "favorite_number" : 21 , "favorite_color" : "green" }
```

For information about the Avro file format, see [http://avro.apache.org/docs/1.7.7/](http://avro.apache.org/docs/1.7.7/)

## Required Avro Jar Files 

Support for the Avro file format requires these jar files:

-   avro-1.7.7.jar
-   avro-tools-1.7.7.jar
-   avro-mapred-1.7.5-hadoop2.jar \(available with Apache Pig\)

**Note:** Hadoop 2 distributions include the Avro jar file `$HADOOP_HOME/share/hadoop/common/lib/avro-1.7.4.jar`. To avoid conflicts, you can rename the file to another file such as `avro-1.7.4.jar.bak`.

For the Cloudera 5.4.x Hadoop distribution, only the jar file `avro-mapred-1.7.5-hadoop2.jar` needs to be downloaded and installed. The distribution contains the other required jar files. The other files are included in the `classpath` used by the `gphdfs` protocol.

For information about downloading the Avro jar files, see [https://avro.apache.org/releases.html](https://avro.apache.org/releases.html).

On all the Greenplum Database hosts, ensure that the jar files are installed and are on the `classpath` used by the `gphdfs` protocol. The `classpath` is specified by the shell script `$GPHOME/lib/hadoop/hadoop_env.sh`.

As an example, if the directory `$HADOOP_HOME/share/hadoop/common/lib` does not exist, create it on all Greenplum Database hosts as the `gpadmin` user. Then, add the add the jar files to the directory on all hosts.

The `hadoop_env.sh` script file adds the jar files to `classpath` for the `gphdfs` protocol. This fragment in the script file adds the jar files to the `classpath`.

```
if [ -d "${HADOOP_HOME}/share/hadoop/common/lib" ]; then
for f in ${HADOOP_HOME}/share/hadoop/common/lib/*.jar; do
            CLASSPATH=${CLASSPATH}:$f;
done
```

## Avro File Format Support 

The Greenplum Database `gphdfs` protocol supports the Avro file type as an external table:

-   Avro file format - GPDB certified with Avro version 1.7.7
-   Reading and writing Avro files
-   Support for overriding the Avro schema when reading an Avro file
-   Compressing Avro files during writing
-   Automatic Avro schema generation when writing an Avro file

Greenplum Database returns an error if the Avro file contains unsupported features or if the specified schema does not match the data.

### Reading from and Writing to Avro Files 

To read from or write to an Avro file, you create an external table and specify the location of the Avro file in the `LOCATION` clause and `'AVRO'` in the `FORMAT` clause. For example, this is the syntax for a readable external table.

```
CREATE EXTERNAL TABLE <tablename> (<column_spec>) LOCATION ( 'gphdfs://<location>') FORMAT 'AVRO' 
```

The location can be an individual Avro file or a directory containing a set of Avro files. If the location specifies multiple files \(a directory name or a file name containing wildcard characters\), Greenplum Database uses the schema in the first file of the directory as the schema of the whole directory. For the file name you can specify the wildcard character \* to match any number of characters.

You can add parameters after the file specified in the location. You add parameters with the http query string syntax that starts with `?` and `&` between field and value pairs.

For readable external tables, the only valid parameter is `schema`. The `gphdfs` uses this schema instead of the Avro file schema when reading Avro files. See [Avro Schema Overrides for Readable External Tables](#topic_zrv_2yv_vs).

For writable external tables, you can specify `schema`, `namespace`, and parameters for compression.

|Parameter|Value|Readable/Writable|Default Value|
|---------|-----|-----------------|-------------|
|schema|URL\_to\_schema\_file|Read and Write|None.<br/><br/>For a readable external table:<br/><br/>-   The specified schema overrides the schema in the Avro file. See [Avro Schema Overrides](#topic_zrv_2yv_vs)<br/><br/>-   If not specified, Greenplum Database uses the Avro file schema.<br/><br/>For a writable external table:<br/><br/>-   Uses the specified schema when creating the Avro file.<br/><br/>-   If not specified, Greenplum Database creates a schema according to the external table definition.<br/><br/>|
|namespace|avro\_namespace|Write only|`public.avro`<br/><br/>If specified, a valid *Avro namespace*.<br/><br/>|
|compress|`true` or `false`|Write only|`false`|
|compression\_type|`block`|Write only|Optional.<br/><br/>For `avro` format, `compression_type` must be `block` if `compress` is `true`.|
|codec|`deflate` or `snappy`|Write only|`deflate`|
|codec\_level \(`deflate` codec only\)|integer between 1 and 9|Write only|`6`<br/><br/>The level controls the trade-off between speed and compression. Valid values are 1 to 9, where 1 is the fastest and 9 is the most compressed.|

This set of parameters specify `snappy` compression:

```
 'compress=true&codec=snappy'
```

These two sets of parameters specify `deflate` compression and are equivalent:

```
 'compress=true&codec=deflate&codec_level=1'
 'compress=true&codec_level=1'
```

### Data Conversion When Reading Avro Files 

When you create a readable external table to Avro file data, Greenplum Database converts Avro data types to Greenplum Database data types.

**Note:** When reading an Avro, Greenplum Database converts the Avro field data at the top level of the Avro schema to a Greenplum Database table column. This is how the `gphdfs` protocol converts the Avro data types.

-   An Avro primitive data type, Greenplum Database converts the data to a Greenplum Database type.
-   An Avro complex data type that is not `map` or `record`, Greenplum Database converts the data to a Greenplum Database type.
-   An Avro `record` that is a sub-record \(nested within the top level Avro schema record\), Greenplum Database converts the data XML.

This table lists the Avro primitive data types and the Greenplum Database type it is converted to.

|Avro Data Type|Greenplum Database Data Type|
|--------------|----------------------------|
|null|Supported only in a Avro union data type. See [Data Conversion when Writing Avro Files](#topic_skw_2yv_vs).|
|boolean|boolean|
|int|int or smallint|
|long|bigint|
|float|real|
|double|double|
|bytes|bytea|
|string|text|

**Note:** When reading the Avro `int` data type as Greenplum Database `smallint` data type, you must ensure that the Avro `int` values do not exceed the Greenplum Database maximum `smallint` value. If the Avro value is too large, the Greenplum Database value will be incorrect.

The `gphdfs` protocol converts performs this conversion for `smallint`: `short result = (short)IntValue;`.

This table lists the Avro complex data types and the and the Greenplum Database type it is converted to.

|Avro Data Type|Greenplum Database Data Type|
|--------------|----------------------------|
|enum|int<br/><br/>The integer represents the zero-based position of the symbol in the schema.|
|array|array<br/><br/>The Greenplum Database array dimensions match the Avro array dimensions. The element type is converted from the Avro data type to the Greenplum Database data type|
|maps|Not supported|
|union|The first non-null data type.|
|fixed|bytea|
|record|XML data|

#### Example Avro Schema 

This is an example Avro schema. When reading the data from the Avro file the `gphdfs` protocol performs these conversions:

-   `name` and `color` data are converted to Greenplum Database `sting`.
-   `age` data is converted to Greenplum Database `int`.
-   `clist` records are converted to `XML`.

```
{"namespace": "example.avro",
  "type": "record",
  "name": "User",
  "fields": [
    {"name": "name", "type": "string"},
    {"name": "number", "type": ["int", "null"]},
    {"name": "color", "type": ["string", "null"]},
    {"name": "clist",
      "type": {
       "type":"record",
        "name":"clistRecord",
        "fields":[
          {"name": "class", "type": ["string", "null"]},
          {"name": "score", "type": ["double", "null"]},
          {"name": "grade",
            "type": {
             "type":"record",
              "name":"inner2",
              "fields":[
                {"name":"a", "type":["double" ,"null"]},
                {"name":"b", "type":["string","null"]}
             ]}
          },
         {"name": "grade2",
           "type": {
            "type":"record",
             "name":"inner",
             "fields":[
               {"name":"a", "type":["double","null"]},
               {"name":"b", "type":["string","null"]},
               {"name":"c", "type":{
                 "type": "record",
                 "name":"inner3",
                 "fields":[
                   {"name":"c1", "type":["string", "null"]},
                   {"name":"c2", "type":["int", "null"]}
               ]}}
           ]}
         }
      ]}
    }
  ]
}
```

This XML is an example of how the `gpfist` protocol converts Avro data from the `clist` field to XML data based on the previous schema. For records nested in the Avro top-level record, `gpfist` protocol converts the Avro element name to the XML element name and the name of the record is an attribute of the XML element. For example, the name of the top most element `clist` and the `type` attribute is the name of the Avro record element `clistRecord`.

```
<clist type="clistRecord">
  <class type="string">math</class>
  <score type="double">99.5</score>
  <grade type="inner2">
    <a type="double">88.8</a>
    <b type="string">subb0</b>
  </grade>
  <grade2 type="inner">
    <a type="double">77.7</a>
    <b type="string">subb20</b>
    <c type="inner3">
       <c1 type="string">subc</c1>
       <c2 type="int& quot;>0</c2>
    </c>
  </grade2>
</clist>
```

### Avro Schema Overrides for Readable External Tables 

When you specify schema for a readable external table that specifies an Avro file as a source, Greenplum Database uses the schema when reading data from the Avro file. The specified schema overrides the Avro file schema.

You can specify a file that contains an Avro schema as part of the location paramter `CREATE EXTERNAL TABLE` command, to override the Avro file schema. If a set of Avro files contain different, related schemas, you can specify an Avro schema to retrieve the data common to all the files.

Greenplum Database extracts the data from the Avro files based on the field name. If an Avro file contains a field with same name, Greenplum Database reads the data , otherwise a `NULL` is returned.

For example, if a set of Avro files contain one of the two different schemas. This is the original schema.

```
{
	"type":"record",
	"name":"tav2",
	"namespace":"public.avro",
	"doc":"",
	"fields":[
		{"name":"id","type":["null","int"],"doc":""},
		{"name":"name","type":["null","string"],"doc":""},
		{"name":"age","type":["null","long"],"doc":""},
		{"name":"birth","type":["null","string"],"doc":""}
	]
}
```

This updated schema contains a comment field.

```
{
	"type":"record",
	"name":"tav2",
	"namespace":"public.avro",
	"doc":"",
	"fields":[
		{"name":"id","type":["null","int"],"doc":""},
		{"name":"name","type":["null","string"],"doc":""},
		{"name":"birth","type":["null","string"],"doc":""},
		{"name":"age","type":["null","long"],"doc":""},
		{"name":"comment","type":["null","string"],"doc":""}
	]
}
```

You can specify an file containing this Avro schema in a `CREATE EXTERNAL TABLE` command, to read the `id`, `name`, `birth`, and `comment` fields from the Avro files.

```
{
	"type":"record",
	"name":"tav2",
	"namespace":"public.avro",
	"doc":"",
	"fields":[
		{"name":"id","type":["null","int"],"doc":""},
		{"name":"name","type":["null","string"],"doc":""},
		{"name":"birth","type":["null","string"],"doc":""},
		{"name":"comment","type":["null","string"],"doc":""}
	]
}
```

In this example command, the customer data is in the Avro files `tmp/cust*.avro`. Each file uses one of the schemas listed previously. The file `avro/cust.avsc` is a text file that contains the Avro schema used to override the schemas in the customer files.

```
CREATE WRITABLE EXTERNAL TABLE cust_avro(id int, name text, birth date) 
   LOCATION ('gphdfs://my_hdfs:8020/tmp/cust*.avro
      ?schema=hdfs://my_hdfs:8020/avro/cust.avsc')
   FORMAT 'avro';
```

When reading the Avro data, if Greenplum Database reads a file that does not contain a `comment` field, a `NULL` is returned for the `comment` data.

### Data Conversion when Writing Avro Files 

When you create a writable external table to write data to an Avro file, each table row is an Avro record and each table column is an Avro field. When writing an Avro file, the default compression algorithm is `deflate`.

For a writable external table, if the `schema` option is not specified, Greenplum Database creates an Avro schema for the Avro file based on the Greenplum Database external table definition. The name of the table column is the Avro field name. The data type is a union data type. See the following table:

|Greenplum Database Data Type|Avro Union Data Type Definition|
|----------------------------|-------------------------------|
|boolean|`["boolean", "null"]`|
|int|`["int", "null"]`|
|bigint|`["long", "null"]`|
|smallint|`["int", "null"]`|
|real|`["float", "null"]`|
|double|`["double", "null"]`|
|bytea|`["bytes", "null"]`|
|text|`["string", "null"]`|
|array|`[{array}, "null"]`<br/><br/>The Greenplum Database array is converted to an Avro array with same dimensions and same element type as the Greenplum Database array.|
|other data types|`["string", "null"]`<br/><br/>Data are formatted strings. The `gphdfs` protocol casts the data to Greenplum Database text and writes the text to the Avro file as an Avro string. For example, date and time data are formatted as date and time strings and converted to Avro string type.|

You can specify a schema with the `schema` option. When you specify a schema, the file can be on the segment hosts or a file on the HDFS that is accessible to Greenplum Database. For a local file, the file must exist in all segment hosts in the same location. For a file on the HDFS, the file must exist in the same cluster as the data file.

This example `schema` option specifies a schema on an HDFS.

```
'schema=hdfs://mytest:8000/avro/array_simple.avsc'
```

This example `schema` option specifies a schema on the host file system.

```
'schema=file:///mydata/avro_schema/array_simple.avsc'
```

### gphdfs Limitations for Avro Files 

For a Greenplum Database writable external table definition, columns cannot specify the `NOT NULL` clause.

Greenplum Database supports only a single top-level schema in Avro files or specified with the `schema` parameter in the `CREATE EXTERNAL TABLE` command. An error is returned if Greenplum Database detects multiple top-level schemas.

Greenplum Database does not support the Avro `map` data type and returns an error when encountered.

When Greenplum Database reads an array from an Avro file, the array is converted to the literal text value. For example, the array `[1,3]` is converted to `'{1,3}'`.

User defined types \(UDT\), including array UDT, are supported. For a writable external table, the type is converted to string.

### Examples 

Simple `CREATE EXTERNAL TABLE` command that reads data from the two Avro fields `id` and `ba`.

```
 CREATE EXTERNAL TABLE avro1 (id int, ba bytea[]) 
   LOCATION ('gphdfs://my_hdfs:8020/avro/singleAvro/array2.avro') 
   FORMAT 'avro';
```

`CREATE WRITABLE EXTERNAL TABLE` command specifies the Avro schema that is the `gphdfs` protocol uses to create the Avro file.

```
CREATE WRITABLE EXTERNAL TABLE at1w(id int, names text[], nums int[]) 
   LOCATION ('gphdfs://my_hdfs:8020/tmp/at1
      ?schema=hdfs://my_hdfs:8020/avro/array_simple.avsc')
   FORMAT 'avro';
```

`CREATE WRITABLE EXTERNAL TABLE` command that writes to an Avro file and specifies a namespace for the Avro schema.

```
CREATE WRITABLE EXTERNAL TABLE atudt1 (id int, info myt, birth date, salary numeric ) 
   LOCATION ('gphdfs://my_hdfs:8020/tmp/emp01.avro
      ?namespace=public.example.avro') 
   FORMAT 'avro';
```

