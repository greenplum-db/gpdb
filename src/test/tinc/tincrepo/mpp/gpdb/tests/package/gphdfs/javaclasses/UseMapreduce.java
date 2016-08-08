package javaclasses;

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

public class UseMapreduce {
    private static String datanodestr;
    private static String jobtrackerstr;
    private static String compressData;

    UseMapreduce(String hdfshostname,String jobtrackerhost,  String datanodeport, String jobtrackerport, String compressionType)
    {
        if (datanodeport.equalsIgnoreCase("none"))
        {
            //datanodestr = "maprfs:///" +hdfshostname;
            //jobtrackerstr = "maprfs:///" +hdfshostname;
            datanodestr = "maprfs:///my.cluster.com";
            jobtrackerstr = "maprfs:///my.cluster.com";
        }
        else
        {
            datanodestr= "hdfs://"+hdfshostname+":"+datanodeport;
            jobtrackerstr = "hdfs://"+jobtrackerhost+":"+jobtrackerport;
        }
        compressData = compressionType;

    }

    public static class Mapreduce_mapper_GPDB_INOUT extends Mapper<LongWritable, GPDBWritable, LongWritable, GPDBWritable> {
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, GPDBWritable value, Context context) throws IOException{
            try
            {
                String datatype = value.getString(0);

                String delims = ",";
                String[] typetokens = datatype.split(delims);
                int tablesize = typetokens.length + 2;
    
                int[] colType = new int[tablesize];
                colType[0] = GPDBWritable.VARCHAR;
                colType[1] = GPDBWritable.BIGINT;

                for (int x = 2; x < tablesize; x++)
                {   colType[x] = returnGPDBWritableType(typetokens[x-2]);  }

                GPDBWritable gw = new GPDBWritable(colType);
                gw.setString(0,value.getString(0));
                gw.setLong(1,value.getLong(1));
    
                for (int x = 2; x < tablesize; x++)
                {
                    int typetokenInd = x-2;
                    if (typetokens[typetokenInd].equalsIgnoreCase("bigint")){gw.setLong(x, value.getLong(x));}
                    else if (typetokens[typetokenInd].equalsIgnoreCase("int")){gw.setInt(x, value.getInt(x));}
			        /*else if (typetokens[typetokenInd].equalsIgnoreCase("smallint"))
                    {
                        if (value.getShort(x) == null)
                            gw.setShort(x, (short)0);
                        else
                            gw.setShort(x, (short)value.getShort(x));
                    }*/
                    else if (typetokens[typetokenInd].equalsIgnoreCase("smallint")){gw.setShort(x,value.getShort(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("float")){gw.setDouble(x, value.getDouble(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("real")){gw.setFloat(x, value.getFloat(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("varchar")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("bpchar")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("text")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("time")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("timestamp")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("date")){gw.setString(x, value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("numeric")){gw.setString(x,value.getString(x));}
			        else if (typetokens[typetokenInd].equalsIgnoreCase("boolean")){gw.setBoolean(x, value.getBoolean(x));}
                    else {gw.setString(x,value.getString(x));}
                 }
                 context.write(word, gw);

            } catch (Exception e) { throw new IOException ( "Mapreduce exception: " + e.getMessage()); }
        }
    }

    public static int returnGPDBWritableType(String datatype)
    {
        if (datatype.equalsIgnoreCase("bigint")){return GPDBWritable.BIGINT;}
        else if (datatype.equalsIgnoreCase("int")){return GPDBWritable.INTEGER;}
        else if (datatype.equalsIgnoreCase("smallint")){return GPDBWritable.SMALLINT;}
        else if (datatype.equalsIgnoreCase("float")){return GPDBWritable.FLOAT8;}
        else if (datatype.equalsIgnoreCase("real")){return GPDBWritable.REAL;}
        else if (datatype.equalsIgnoreCase("varchar")){return GPDBWritable.VARCHAR;}
        else if (datatype.equalsIgnoreCase("bpchar")){return GPDBWritable.BPCHAR;}
        else if (datatype.equalsIgnoreCase("text")){return GPDBWritable.TEXT;}
        else if (datatype.equalsIgnoreCase("time")){return GPDBWritable.TIME;}
        else if (datatype.equalsIgnoreCase("timestamp")){return GPDBWritable.TIMESTAMP;}
        else if (datatype.equalsIgnoreCase("date")){return GPDBWritable.DATE;}
        else if (datatype.equalsIgnoreCase("numeric")){return GPDBWritable.NUMERIC;}
        else if (datatype.equalsIgnoreCase("boolean")){return GPDBWritable.BOOLEAN;}
        else {return GPDBWritable.TEXT;}
    }



    public static class Mapreduce_mapper_TextIn extends Mapper<LongWritable, Text, LongWritable, GPDBWritable> {
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, Text value, Context context) throws IOException {
            try {

                String line = value.toString();
                String delims = "\t";
                String[] tokens = line.split(delims);

                String datatype = tokens[0];
                String datatypedelims = ",";
                String[] typetokens = datatype.split(datatypedelims);
                int tablesize = typetokens.length + 2;

                int[] colType = new int[tablesize];
                colType[0] = GPDBWritable.VARCHAR;
                colType[1] = GPDBWritable.BIGINT;

                for (int x = 2; x < tablesize; x++)
                {   colType[x] = returnGPDBWritableType(typetokens[x-2]);  }

                GPDBWritable gw = new GPDBWritable(colType);
                gw.setString(0,tokens[0]);
                gw.setLong(1,Long.parseLong(tokens[1]));

                for (int x = 2; x < tablesize; x++)
                {
                    int typetokenInd = x-2;
                    if (typetokens[typetokenInd].equalsIgnoreCase("bigint"))
                    {   
                        if (tokens[x].equalsIgnoreCase("\\N"))
                        {   gw.setLong(x,null);}
                        else 
                        {   gw.setLong(x,Long.parseLong(tokens[x]));}
                    }
                    else if (typetokens[typetokenInd].equalsIgnoreCase("int"))
                    {
                        if (tokens[x].equalsIgnoreCase("\\N"))
                        {   gw.setInt(x,null);}
                        else
                        {   gw.setInt(x,Integer.parseInt(tokens[x]));}
                    }
                    else if (typetokens[typetokenInd].equalsIgnoreCase("smallint"))
                    {
                        if (tokens[x].equalsIgnoreCase("\\N"))
                        {   gw.setShort(x,null);}
                        else
                        {   gw.setShort(x,Short.parseShort(tokens[x]));}
                    }
                    else if (typetokens[typetokenInd].equalsIgnoreCase("float"))
                    {
                        if (tokens[x].equalsIgnoreCase("\\N"))
                        {   gw.setDouble(x,null);}
                        else
                        {   gw.setDouble(x,Double.parseDouble(tokens[x]));} 
                    }
                    else if (typetokens[typetokenInd].equalsIgnoreCase("real"))
                    {
                        if (tokens[x].equalsIgnoreCase("\\N"))
                        {   gw.setFloat(x,null);}
                        else
                        {   gw.setFloat(x,Float.parseFloat(tokens[x]));}
                    }
                    else if (typetokens[typetokenInd].equalsIgnoreCase("boolean"))
                    {
                        if(tokens[x].equalsIgnoreCase("true"))
                            gw.setBoolean(x, true);
                        else if (tokens[x].equalsIgnoreCase("\\N"))
                            gw.setBoolean(x, null);
                        else
                            gw.setBoolean(x, false);
                    }else
                    {
                        if (tokens[x].equalsIgnoreCase("\\N"))
                            gw.setString(x, null);
                        else
                            gw.setString(x, tokens[x]);
                    }
                 }
                 context.write(word, gw);

            } catch (Exception e) { throw new IOException ("Mapreduce exception: " +e.getMessage()); }
        }
    }


    public static class Mapreduce_mapper_TextIn_customtype extends Mapper<LongWritable, Text, LongWritable, GPDBWritable> {
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, Text value, Context context) throws IOException {
            try {
                int[] colType = new int[1];

                colType[0] = GPDBWritable.TEXT;
                GPDBWritable gw = new GPDBWritable(colType);

                String line = value.toString();

                gw.setString(0,line);

                context.write(word, gw);
            } catch (Exception e) { throw new IOException ("Mapreduce exception: " +e.getMessage()); }
        }
    }


    public static class Mapreduce_mapper_GPDBIn extends Mapper<LongWritable, GPDBWritable, Text, NullWritable> {
        private Text word = new Text();

        public void map(LongWritable key, GPDBWritable value, Context context) throws IOException {
            try {
                Text word = new Text(value.toString());
                context.write(word, NullWritable.get());
            } catch (Exception e) { throw new IOException ("Mapreduce exception: " +e.getMessage()); }
        }
    }

    public static class Mapreduce_mapper_TEXT_INOUT extends Mapper<LongWritable, Text, Text, NullWritable> {
        private Text word = new Text();

        public void map(LongWritable key, Text value, Context context) throws IOException {
            try {
                Text word = new Text(value.toString());
                context.write(word, NullWritable.get());
            } catch (Exception e) { throw new IOException ("Mapreduce exception: " +e.getMessage()); }
        }
    }


    public static void testtypeerror() throws Exception{
       int[] colType = new int[2];
    

        colType[0] = GPDBWritable.VARCHAR;
        colType[1] = GPDBWritable.BIGINT;

        GPDBWritable gw = new GPDBWritable(colType);
        gw.setLong(0,4L);
        //gw.getString(1);



    }

    public static void mapreduce_readwrite(String mapperfunc, String inputpath, String outputpath ) throws Exception {
        Configuration conf = new Configuration(true);

        /* Not sure why this helps ..But it gets rid of the deprecation messages that were causing failures */
        if (System.getenv("HADOOP_INTEGRATION_CDH4") == null)
            conf.set("fs.default.name", datanodestr);
        conf.set("mapred.job.tracker", jobtrackerstr);
        conf.set("mapred.job.map.memory.mb", "3024");
        conf.set("mapred.job.reduce.memory.mb", "3024");

        Job job = new Job(conf, "mapreduce_readwrite");

        job.setJarByClass(UseMapreduce.class);

        if (mapperfunc.equalsIgnoreCase("Mapreduce_mapper_TextIn"))
        {
            job.setOutputKeyClass(LongWritable.class);
            job.setOutputValueClass(GPDBWritable.class);
            job.setMapperClass(Mapreduce_mapper_TextIn.class);
            job.setInputFormatClass(TextInputFormat.class);
            job.setOutputFormatClass(GPDBOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapreduce_mapper_TextIn_customtype"))
        {       
            job.setOutputKeyClass(LongWritable.class);
            job.setOutputValueClass(GPDBWritable.class);
            job.setMapperClass(Mapreduce_mapper_TextIn_customtype.class);
            job.setInputFormatClass(TextInputFormat.class);
            job.setOutputFormatClass(GPDBOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapreduce_mapper_GPDB_INOUT"))
        {
            job.setOutputKeyClass(LongWritable.class);
            job.setOutputValueClass(GPDBWritable.class);
            job.setMapperClass(Mapreduce_mapper_GPDB_INOUT.class);
            job.setInputFormatClass(GPDBInputFormat.class);
            job.setOutputFormatClass(GPDBOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapreduce_mapper_GPDBIn"))
        {   
            job.setOutputKeyClass(Text.class);
            job.setOutputValueClass(NullWritable.class);
            job.setMapperClass(Mapreduce_mapper_GPDBIn.class);
            job.setInputFormatClass(GPDBInputFormat.class);
            job.setOutputFormatClass(TextOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapreduce_mapper_TEXT_INOUT"))
        {
            job.setOutputKeyClass(Text.class);
            job.setOutputValueClass(NullWritable.class);
            job.setMapperClass(Mapreduce_mapper_TEXT_INOUT.class);
            job.setInputFormatClass(TextInputFormat.class);
            job.setOutputFormatClass(TextOutputFormat.class);
        }

        GPDBInputFormat.setInputPaths(job, inputpath);
        GPDBOutputFormat.setOutputPath(job, new Path(outputpath));

        if (compressData.equalsIgnoreCase("BLOCK"))
            GPDBOutputFormat.setOutputCompressionType(job, SequenceFile.CompressionType.BLOCK);
        else if (compressData.equalsIgnoreCase("RECORD"))
            GPDBOutputFormat.setOutputCompressionType(job, SequenceFile.CompressionType.RECORD);

        job.waitForCompletion(true);
    }
    
}
