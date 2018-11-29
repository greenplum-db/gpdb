package javaclasses;

public class TestHadoopIntegration {

    public static void main(String[] args) throws Exception {
        if (args.length != 4)
        {
            System.out.println("usage: TesthadoopIntegration <mapred|mapreduce> <mapper funtion> <inputpath> <outputpath>");
            System.exit(1);
        }
        String interfacename = args[0];
        String mrfunction = args[1];
        String inputpath = args[2];
        String outputpath = args[3];
        
        String hdfshost=System.getProperty("hdfshost");
        String jobtrackerhost=System.getProperty("jobtrackerhost");
        String datanodeport = System.getProperty("datanodeport");
        String jobtrackerport = System.getProperty("jobtrackerport");
        String compressionType = System.getProperty("compressionType");

        UseMapred mapred = new UseMapred(hdfshost,jobtrackerhost, datanodeport, jobtrackerport, compressionType);
        UseMapreduce mapreduce = new UseMapreduce(hdfshost,jobtrackerhost, datanodeport, jobtrackerport, compressionType);

        if (interfacename.equalsIgnoreCase("mapred"))
            mapred.mapred_readwrite(mrfunction, inputpath, outputpath);
        else if (interfacename.equalsIgnoreCase("mapreduce"))
            mapreduce.mapreduce_readwrite(mrfunction, inputpath, outputpath);
        else
        {
            System.out.println("not a valid interface");
            System.exit(1);
        }
    }
}
