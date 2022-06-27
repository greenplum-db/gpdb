# Reading and Writing Custom-Formatted HDFS Data with gphdfs \(Deprecated\) 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database.

Use MapReduce and the `CREATE EXTERNAL TABLE` command to read and write data with custom formats on HDFS.

To read custom-formatted data:

1.  Author and run a MapReduce job that creates a copy of the data in a format accessible to Greenplum Database.
2.  Use `CREATE EXTERNAL TABLE` to read the data into Greenplum Database.

See [Example 1 - Read Custom-Formatted Data from HDFS](#topic26).

To write custom-formatted data:

1.  Write the data.
2.  Author and run a MapReduce program to convert the data to the custom format and place it on the Hadoop Distributed File System.

See [Example 2 - Write Custom-Formatted Data from Greenplum Database to HDFS](#topic29).

MapReduce code is written in Java. Greenplum provides Java APIs for use in the MapReduce code. The Javadoc is available in the `$GPHOME/docs` directory. To view the Javadoc, expand the file `gnet-1.2-javadoc.tar` and open `index.html`. The Javadoc documents the following packages:

```
com.emc.greenplum.gpdb.hadoop.io
com.emc.greenplum.gpdb.hadoop.mapred
com.emc.greenplum.gpdb.hadoop.mapreduce.lib.input
com.emc.greenplum.gpdb.hadoop.mapreduce.lib.output

```

The HDFS cross-connect packages contain the Java library, which contains the packages `GPDBWritable`, `GPDBInputFormat`, and `GPDBOutputFormat`. The Java packages are available in `$GPHOME/lib/hadoop`. Compile and run the MapReduce job with the cross-connect package. For example, compile and run the MapReduce job with `hdp-gnet-1.2.0.0.jar` if you use the HDP distribution of Hadoop.

To make the Java library available to all Hadoop users, the Hadoop cluster administrator should place the corresponding `gphdfs` connector jar in the `$HADOOP_HOME/lib` directory and restart the job tracker. If this is not done, a Hadoop user can still use the `gphdfs` connector jar; but with the *distributed cache* technique.

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

## Example 1 - Read Custom-Formatted Data from HDFS 

The sample code makes the following assumptions.

-   The data is contained in HDFS directory `/demo/data/temp` and the name node is running on port 8081.
-   This code writes the data in Greenplum Database format to `/demo/data/MRTest1` on HDFS.
-   The data contains the following columns, in order.
    1.  A long integer
    2.  A Boolean
    3.  A text string

### Sample MapReduce Code 

```
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import com.emc.greenplum.gpdb.hadoop.mapreduce.lib.input.GPDBInputFormat;
import com.emc.greenplum.gpdb.hadoop.mapreduce.lib.output.GPDBOutputFormat;
import java.io.*;
import java.util.*;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.lib.output.*;
import org.apache.hadoop.mapreduce.lib.input.*;
import org.apache.hadoop.util.*;

public class demoMR {

/*
 * Helper routine to create our generic record. This section shows the
 * format of the data. Modify as necessary. 
 */
 public static GPDBWritable generateGenericRecord() throws
      IOException {
 int[] colType = new int[3];
 colType[0] = GPDBWritable.BIGINT;
 colType[1] = GPDBWritable.BOOLEAN;
 colType[2] = GPDBWritable.VARCHAR;
 
  /*
   * This section passes the values of the data. Modify as necessary. 
   */ 
  GPDBWritable gw = new GPDBWritable(colType); 
  gw.setLong (0, (long)12345);  
  gw.setBoolean(1, true); 
  gw.setString (2, "abcdef");
  return gw; 
} 

/* 
 * DEMO Map/Reduce class test1
 * -- Regardless of the input, this section dumps the generic record
 * into GPDBFormat/
 */
 public static class Map_test1 
     extends Mapper<LongWritable, Text, LongWritable, GPDBWritable> {
 
  private LongWritable word = new LongWritable(1);

  public void map(LongWritable key, Text value, Context context) throws
       IOException { 
    try {
      GPDBWritable gw = generateGenericRecord();
      context.write(word, gw); 
      } 
      catch (Exception e) { 
        throw new IOException (e.getMessage()); 
      } 
    }
  }

  Configuration conf = new Configuration(true);
  Job job = new Job(conf, "test1");
  job.setJarByClass(demoMR.class);
  job.setInputFormatClass(TextInputFormat.class);
  job.setOutputKeyClass (LongWritable.class);
  job.setOutputValueClass (GPDBWritable.class);
  job.setOutputFormatClass(GPDBOutputFormat.class);
  job.setMapperClass(Map_test1.class);
  FileInputFormat.setInputPaths (job, new Path("/demo/data/tmp"));
  GPDBOutputFormat.setOutputPath(job, new Path("/demo/data/MRTest1"));
  job.waitForCompletion(true);
}
```

### Run CREATE EXTERNAL TABLE 

The Hadoop location corresponds to the output path in the MapReduce job.

```
=# CREATE EXTERNAL TABLE demodata 
   LOCATION ('gphdfs://hdfshost-1:8081/demo/data/MRTest1') 
   FORMAT 'custom' (formatter='gphdfs_import');

```

## Example 2 - Write Custom-Formatted Data from Greenplum Database to HDFS 

The sample code makes the following assumptions.

-   The data in Greenplum Database format is located on the Hadoop Distributed File System on `/demo/data/writeFromGPDB_42` on port 8081.
-   This code writes the data to `/demo/data/MRTest2` on port 8081.

1.  Run a SQL command to create the writable table.

    ```
    =# CREATE WRITABLE EXTERNAL TABLE demodata 
       LOCATION ('gphdfs://hdfshost-1:8081/demo/data/MRTest2') 
       FORMAT 'custom' (formatter='gphdfs_export');
    
    ```

2.  Author and run code for a MapReduce job. Use the same import statements shown in [Example 1 - Read Custom-Formatted Data from HDFS](#topic26).

## Sample MapReduce Code 

```
/*
 * DEMO Map/Reduce class test2
 * -- Convert GPDBFormat back to TEXT
 */
public static class Map_test2 extends Mapper<LongWritable, GPDBWritable,
  Text, NullWritable> { 
  public void map(LongWritable key, GPDBWritable value, Context context )
    throws IOException {
    try {
      context.write(new Text(value.toString()), NullWritable.get());
    } catch (Exception e) { throw new IOException (e.getMessage()); }
  }
}

public static void runTest2() throws Exception{
Configuration conf = new Configuration(true);
 Job job = new Job(conf, "test2");
 job.setJarByClass(demoMR.class);
 job.setInputFormatClass(GPDBInputFormat.class);
 job.setOutputKeyLClass (Text.class);
 job.setOutputValueClass(NullWritable.class);
 job.setOutputFormatClass(TextOutputFormat.class);
 job.setMapperClass(Map_test2.class);
     GPDBInputFormat.setInputPaths (job, 
     new Path("/demo/data/writeFromGPDB_42"));
 GPDBOutputFormat.setOutputPath(job, new Path("/demo/data/MRTest2"));
 job.waitForCompletion(true);
     
}
```

