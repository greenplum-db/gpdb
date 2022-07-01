# gphdfs Support for Parquet Files \(Deprecated\) 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database. You can use the Greenplum Platform Extension Framework \(PXF\) to access Parquet-format data.

You can use the Greenplum Database `gphdfs` protocol to access Parquet files on a Hadoop file system \(HDFS\).

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

## About the Parquet File Format 

The Parquet file format is designed to take advantage of compressed, efficient columnar data representation available to projects in the Hadoop ecosystem. Parquet supports complex nested data structures and uses Dremel record shredding and assembly algorithms. Parquet supports very efficient compression and encoding schemes. Parquet allows compression schemes to be specified on a per-column level, and supports adding more encodings as they are invented and implemented.

For information about the Parquet file format, see the Parquet documentation [http://parquet.apache.org/documentation/latest/](http://parquet.apache.org/documentation/latest/).

For an overview of columnar data storage and the Parquet file format, see [https://blog.twitter.com/2013/dremel-made-simple-with-parquet](https://blog.twitter.com/2013/dremel-made-simple-with-parquet).

## Required Parquet Jar Files 

The `gphdfs` protocol supports Parquet versions 1.7.0 and later. For each version, the required Parquet jar files are included in a bundled jar file `parquet-hadoop-bundle-<version>.jar`.

Earlier Parquet versions not use the Java class names `org.apache.parquet` and are not supported. The `gphdfs` protocol expects the Parquet Java class names to be `org.apache.parquet.xxx`.

**Note:** The Cloudera 5.4.x Hadoop distribution includes some Parquet jar files. However, the Java class names in the jar files are `parquet.xxx`. The jar files with the class name `org.apache.parquet` can be downloaded and installed on the Greenplum Database hosts.

For information about downloading the Parquet jar files, see [https://mvnrepository.com/artifact/org.apache.parquet/parquet-hadoop-bundle](https://mvnrepository.com/artifact/org.apache.parquet/parquet-hadoop-bundle)

On all the Greenplum Database hosts, ensure that the jar files are installed and are on the `classpath` used by the `gphdfs` protocol. The `classpath` is specified by the shell script `$GPHOME/lib/hadoop/hadoop_env.sh`. As a Hadoop 2 example, you can install the jar files in `$HADOOP_HOME/share/hadoop/common/lib`. The `hadoop_env.sh` script file adds the jar files to the `classpath`.

As an example, if the directory `$HADOOP_HOME/share/hadoop/common/lib` does not exist, create it on all Greenplum Database hosts as the `gpadmin` user. Then, add the add the jar files to the directory on all hosts.

The `hadoop_env.sh` script file adds the jar files to `classpath` for the `gphdfs` protocol. This fragment in the script file adds the jar files to the `classpath`.

```
if [ -d "${HADOOP_HOME}/share/hadoop/common/lib" ]; then
for f in ${HADOOP_HOME}/share/hadoop/common/lib/*.jar; do
            CLASSPATH=${CLASSPATH}:$f;
done
```

## Parquet File Format Support 

The Greenplum Database `gphdfs` protocol supports the Parquet file format version 1 or 2. Parquet takes advantage of compressed, columnar data representation on HDFS. In a Parquet file, the metadata \(Parquet schema definition\) contains data structure information is written after the data to allow for single pass writing.

This is an example of the Parquet schema definition format:

```
message test {
    repeated byte_array binary_field;
    required int32 int32_field;
    optional int64 int64_field;
    required boolean boolean_field;
    required fixed_len_byte_array(3) flba_field;
    required byte_array someDay (utf8);
    };
```

The definition for last field `someDay` specifies the `binary` data type with the `utf8` annotation. The data type and annotation defines the data as a UTF-8 encoded character string.

### Reading from and Writing to Parquet Files 

To read from or write to a Parquet file, you create an external table and specify the location of the parquet file in the `LOCATION` clause and `'PARQUET'` in the `FORMAT` clause. For example, this is the syntax for a readable external table.

```
CREATE EXTERNAL TABLE <tablename> (<column_spec>) LOCATION ( 'gphdfs://<location>') FORMAT 'PARQUET' 
```

The location can be an Parquet file or a directory containing a set of Parquet files. For the file name you can specify the wildcard character \* to match any number of characters. If the location specifies multiple files when reading Parquet files, Greenplum Database uses the schema in the first file that is read as the schema for the other files.

### Reading a Parquet File 

The following table identifies how Greenplum database converts the Parquet data type if the Parquet schema definition does not contain an annotation.

|Parquet Data Type|Greenplum Database Data Type|
|-----------------|----------------------------|
|boolean|boolean|
|int32|int or smallint|
|int64|long|
|int96|bytea|
|float|real|
|double|double|
|byte\_array|bytea|
|fixed\_len\_byte\_array|bytea|

**Note:** When reading the Parquet `int` data type as Greenplum Database `smallint` data type, you must ensure that the Parquet `int` values do not exceed the Greenplum Database maximum `smallint` value. If the value is too large, the Greenplum Database value will be incorrect.

The `gphdfs` protocol considers Parquet schema annotations for these cases. Otherwise, data conversion is based on the parquet schema primitive type:

|Parquet Schema Data Type and Annotation|Greenplum Database Data Type|
|---------------------------------------|----------------------------|
|binary with `json` or `utf8` annotation|text|
|binary and the Greenplum Database column data type is text|text|
|int32 with `int_16` annotation|smallint|
|int32, int64, fixed\_len\_byte\_array, or binary with `decimal` annotation|decimal|
|`repeated`|array column - The data type is converted according to [Table 1](#table_wm5_1x4_hs)|
|`optional`, `required`|Data type is converted according to [Table 1](#table_wm5_1x4_hs)|

**Note:** See [Limitations and Notes](#topic_tt4_zxz_zr) and the Parquet documentation when specifying `decimal`, `date`, `interval`, or`time*` annotations.

The `gphdfs` protocol converts the field data to text if the Parquet field type is binary without any annotation, and the data type is defined as text for the corresponding Greenplum Database external table column.

When reading Parquet type `group`, the `gphdfs` protocol converts the `group` data into an XML document.

This schema contains a required group with the name `inner`.

```
message test {
    required byte_array binary_field;
    required int64 int64_field;
**    required group inner \{
       int32 age;
       required boolean test;
       required byte\_array name \(UTF8\);
       \} **
    };
```

This how a single row of the group data would be converted to XML.

```
<inner type="group">
  <age type="int">50</age>
  <test type="boolean">true</test>
  <name type="string">fred</name>
</inner>
```

This example schema contains a repeated group with the name `inner`.

```
message test {
    required byte_array binary_field;
    required int64 int64_field;
**    repeated group inner \{
       int32 age;
       required boolean test;
       required byte\_array name \(UTF8\);
       \} **
    };
```

For a repeated `group`, the Parquet file can contain multiple sets of the group data in a single row. For the example schema, the data for the `inner` group is converted into XML data.

This is sample output if the data in the Parquet file contained two sets of data for the `inner` group.

```
<inner type="repeated">
  <inner type="group">
    <age type="int">50</age>
    <test type="boolean">true</test>
    <name type="string">fred</name>
  </inner>
  <inner>
    <age type="int">23</age>
    <test type="boolean">false</test>
    <name type="string">sam</name>
  </inner>
</inner>
```

### Reading a Hive Generated Parquet File 

The Apache Hive data warehouse software can manage and query large datasets that reside in distributed storage. Apache Hive 0.13.0 and later can store data in Parquet format files. For information about Parquet used by Apache Hive, see [https://cwiki.apache.org/confluence/display/Hive/Parquet](https://cwiki.apache.org/confluence/display/Hive/Parquet).

For Hive 1.1 data stored in Parquet files, this table lists how Greenplum database converts the data. The conversion is based on the Parquet schema that is generated by Hive. For information about the Parquet schema generated by Hive, see [Notes on the Hive Generated Parquet Schema](#hive_parquet).

|Hive Data Type|Greenplum Database Data Type|
|--------------|----------------------------|
|tinyint|int|
|smallint|int|
|int|int|
|bigint|bigint|
|decimal|numeric|
|float|real|
|double|float|
|boolean|boolean|
|string|text|
|char|text or char|
|varchar|text or varchar|
|timestamp|bytea|
|binary|bytea|
|array|xml|
|map|xml|
|struct|xml|

#### Notes on the Hive Generated Parquet Schema 

-   When writing data to Parquet files, Hive treats all integer data types `tinyint`, `smallint`, `int` as `int32`. When you create an external table in Greenplum Database for a Hive generated Parquet file, specify the column data type as `int`. For example, this Hive `CREATE TABLE` command stores data in Parquet files.

    ```
    CREATE TABLE hpSimple(c1 tinyint, c2 smallint, c3 int, c4 bigint, 
        c5 float, c6 double, c7 boolean, c8 string)
      STORED AS PARQUET;
    ```

    This is the Hive generated Parquet schema for the `hpSimple` table data.

    ```
    message hive_schema {
      optional int32 c1;
      optional int32 c2;
      optional int32 c3;
      optional int64 c4;
      optional float c5;
      optional double c6;
      optional boolean c7;
      optional binary c8 (UTF8);
    }
    ```

    The `gphdfs` protocol converts the Parquet integer data types to the Greenplum Database data type `int`.

-   For the Hive `char` data type, the Greenplum Database column data types can be either `text` or `char`. For the Hive `varchar` data type, the Greenplum Database column data type can be either `text` or `varchar`.
-   Based on the Hive generated Parquet schema, some Hive data is converted to Greenplum Database XML data. For example, Hive array column data that is stored in a Parquet file is converted to XML data. As an example, this the Hive generated Parquet schema for a Hive column `col1` of data type `array[int]`.

    ```
    optional group col1 (LIST) {
      repeated group bag {
        optional int32 array_element;
      }
    }
    ```

    The `gphdfs` protocol converts the Parquet `group` data to the Greenplum Database data type `XML`.

-   For the Hive `timestamp` data type, the Hive generated Parquet schema for the data type specifies that the data is stored as data type `int96`. The `gphdfs` protocol converts the `int96` data type to the Greenplum Database `bytea` data type.

### Writing a Parquet File 

For writable external tables, you can add parameters after the file specified in the location. You add parameters with the http query string syntax that starts with `?` and `&` between field and value pairs.

|Option|Values|Readable/Writable|Default Value|
|------|------|-----------------|-------------|
|schema|URL\_to\_schema|Write only|None.<br/><br/>If not specified, the `gphdfs` protocol creates a schema according to the external table definition.|
|pagesize|\> 1024 Bytes|Write only|1 MB|
|rowgroupsize|\> 1024 Bytes|Write only|8 MB|
|parquetversion *or* pqversion|`v1`, `v2`|Write only|`v1`|
|codec|`UNCOMPRESSED`, `GZIP`, `LZO`, `snappy`|Write only|`UNCOMPRESSED`|
|dictionaryenable<sup>1</sup>|`true`, `false`|Write only|false|
|dictionarypagesize<sup>1</sup>|\> 1024 Bytes|Write only|512 KB|

**Note:**

<sup>1</sup> Creates an internal dictionary. Enabling a dictionary can improve Parquet file compression if text columns contain similar or duplicate data.

When writing a Parquet file, the `gphdfs` protocol can generate a Parquet schema based on the table definition.

-   The table name is used as the Parquet `message` name.
-   The column name is uses as the Parquet `field` name.

When creating the Parquet schema from a Greenplum Database table definition, the schema is generated based on the column data type.

|Greenplum Database Data Type|Parquet Schema Data Type|
|----------------------------|------------------------|
|boolean|optional boolean|
|smallint|optional int32 with annotation `int_16`|
|int|optional int32|
|bigint|optional int64|
|real|optional float|
|double|optional double|
|numeric or decimal|binary with annotation `decimal`|
|bytea|optional binary|
|array column|repeated field - The data type is the same data type as the Greenplum Database the array. For example, `array[int]` is converted to `repeated int`|
|Others|binary with annotation `utf8`|

**Note:** To support `Null` data, `gphdfs` protocol specifies the Parquet `optional` schema annotation when creating a Parquet schema.

A simple example of a Greenplum Database table definition and the Parquet schema generated by the `gphdfs` protocol.

An example external table definition for a Parquet file.

```
CREATE WRITABLE EXTERNAL TABLE films (
   code char(5), 
   title varchar(40),
   id integer,
   date_prod date, 
   subtitle boolean
) LOCATION ( 'gphdfs://my-films') FORMAT 'PARQUET' ;
```

This is the Parquet schema for the Parquet file `my-films` generated by the `gphdfs` protocol.

```
message films {
    optional byte_array code;
    optional byte_array title (utf8);
    optional int32 id;
    optional binary date_prod (utf8);
    optional boolean subtitle;
    };
```

### Limitations and Notes 

-   For writable external tables, column definitions in Greenplum Database external table cannot specify `NOT NULL` to support automatically generating a Parquet schema. When the `gphdfs` protocol automatically generates a Parquet schema, the `gphdfs` protocol specifies the field attribute `optional` to support `null` in the Parquet schema. Repeated fields can be `null` in Parquet.
-   The `gphdfs` protocol supports Parquet nested `group` structures only for readable external files. The nested structures are converted to an XML document.
-   Greenplum Database does not have an unsigned `int` data type. Greenplum Database converts the Parquet unsigned `int` data type to the next largest Greenplum Database `int` type. For example, Parquet `uint_8` is converted to Greenplum Database `int` \(32 bit\).
-   Greenplum Database supports any UDT data type or UDT array data type. Greenplum Database attempts to convert the UDT to a sting. If the UDT cannot be converted to a sting, Greenplum Database returns an error.
-   The definition of the `Interval` data type in Parquet is significantly different than the `Interval` definition in Greenplum Database and cannot be converted. The Parquet `Interval` data is formatted as `bytea`.
-   The `Date` data type in Parquet is starts from 1970.1.1, while `Date` in Greenplum Database starts from 4173 BC, Greenplum Database cannot convert `date` data types because largest values are different. A similar situation occurs between `Timestamp_millis` in Parquet and `Timestamp` in Greenplum Database.

