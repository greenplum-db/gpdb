package javaclasses;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
//import com.emc.greenplum.gpdb.hadoop.mapreduce.lib.input.GPDBInputFormat;
import com.emc.greenplum.gpdb.hadoop.mapred.GPDBOutputFormat;
import com.emc.greenplum.gpdb.hadoop.mapred.GPDBInputFormat;



import java.io.*;
import java.util.*;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
/*import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.lib.output.*;
import org.apache.hadoop.mapreduce.lib.input.*;*/
import org.apache.hadoop.util.*;
import org.apache.hadoop.mapred.*;


public class UseMapred{
    private static String datanodestr;
    private static String jobtrackerstr;
    private static String compressData;

    UseMapred(String hdfshostname,String jobtrackerhost, String datanodeport, String jobtrackerport, String compressionType)
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

    public static class Mapred_mapper_GPDB_INOUT extends MapReduceBase implements Mapper<LongWritable, GPDBWritable, LongWritable, GPDBWritable> {
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, GPDBWritable value, OutputCollector<LongWritable, GPDBWritable> output, Reporter reporter) throws IOException
        {
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
                    else if (typetokens[typetokenInd].equalsIgnoreCase("smallint")){gw.setShort(x, value.getShort(x));}
                   /* {
                        if (value.getShort(x) == null)
                            gw.setShort(x, (short)0);
                        else
                            gw.setShort(x, (short)value.getShort(x));
                    }*/
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
                 output.collect(word, gw);
                
            } catch (Exception e) { throw new IOException ("Mapred exception: " +e.getMessage()); }
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





    public static class Mapred_mapper_TextIn extends MapReduceBase implements Mapper<LongWritable, Text, LongWritable, GPDBWritable> {
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, Text value, OutputCollector<LongWritable, GPDBWritable> output, Reporter reporter) throws IOException 
        {
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

                 output.collect(word, gw);
            } catch (Exception e) { throw new IOException ("Mapred exception: " +e.getMessage()); }
        }
    }

    public static class Mapred_mapper_GPDBIn extends MapReduceBase implements Mapper<LongWritable, GPDBWritable, Text, NullWritable>{        
        private LongWritable word = new LongWritable(1);

        public void map(LongWritable key, GPDBWritable value, OutputCollector<Text, NullWritable> output, Reporter reporter) throws IOException {
        //public void map(LongWritable key, GPDBWritable value, OutputCollector<LongWritable, NullWritable> output, Reporter reporter) throws IOException {
            try{

                Text word = new Text(value.toString());
                output.collect(word, NullWritable.get());
                }catch (Exception e) {throw new IOException ("Mapred exception: " +e.getMessage()); }
        }
    }

    public static class Mapred_mapper_TEXT_INOUT extends MapReduceBase implements Mapper<LongWritable, Text, Text, NullWritable>{
        private LongWritable word = new LongWritable(1);
                    
        public void map(LongWritable key, Text value, OutputCollector<Text, NullWritable> output, Reporter reporter) throws IOException {
            try{    
                    
                Text word = new Text(value.toString());
                output.collect(word, NullWritable.get());
                }catch (Exception e) {throw new IOException ("Mapred exception: " +e.getMessage()); }
        }           
    } 

    /**
     * 
     */
    public static void mapred_readwrite(String mapperfunc, String inputpath, String outputpath ) throws Exception {  
        JobConf conf = new JobConf(UseMapred.class);
        conf.setJarByClass(UseMapred.class);

        /* Not sure why this helps ..But it gets rid of the deprecation messages that were causing failures */
        if (System.getenv("HADOOP_INTEGRATION_CDH4") == null)
            conf.set("fs.default.name", datanodestr);
        conf.set("mapred.job.tracker", jobtrackerstr);
        conf.setJobName("mapred_readwrite");
        conf.set("mapred.job.map.memory.mb", "3024");
        conf.set("mapred.job.reduce.memory.mb", "3024");

        if (mapperfunc.equalsIgnoreCase("Mapred_mapper_TextIn"))
        {
            conf.setOutputKeyClass(LongWritable.class);
            conf.setOutputValueClass(GPDBWritable.class);
            conf.setMapperClass(Mapred_mapper_TextIn.class);
            conf.setInputFormat(TextInputFormat.class);
            conf.setOutputFormat(GPDBOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapred_mapper_GPDBIn"))
        {
            conf.setOutputKeyClass(Text.class);
            conf.setOutputValueClass(NullWritable.class);
            conf.setMapperClass(Mapred_mapper_GPDBIn.class);
            conf.setInputFormat(GPDBInputFormat.class);
            conf.setOutputFormat(TextOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapred_mapper_GPDB_INOUT"))
        {
            conf.setOutputKeyClass(LongWritable.class);
            conf.setOutputValueClass(GPDBWritable.class);
            conf.setMapperClass(Mapred_mapper_GPDB_INOUT.class);
            conf.setInputFormat(GPDBInputFormat.class);
            conf.setOutputFormat(GPDBOutputFormat.class);
        }
        else if (mapperfunc.equalsIgnoreCase("Mapred_mapper_TEXT_INOUT"))
        {           
            conf.setOutputKeyClass(Text.class);
            conf.setOutputValueClass(NullWritable.class);
            conf.setMapperClass(Mapred_mapper_TEXT_INOUT.class);
            conf.setInputFormat(TextInputFormat.class);
            conf.setOutputFormat(TextOutputFormat.class);
        }

        if (compressData.equalsIgnoreCase("BLOCK"))
            GPDBOutputFormat.setOutputCompressionType(conf, SequenceFile.CompressionType.BLOCK);
        else if (compressData.equalsIgnoreCase("RECORD"))
            GPDBOutputFormat.setOutputCompressionType(conf, SequenceFile.CompressionType.RECORD);

         GPDBInputFormat.setInputPaths(conf, inputpath);
         GPDBOutputFormat.setOutputPath(conf, new Path(outputpath));
         JobClient.runJob(conf);
    }   

}

