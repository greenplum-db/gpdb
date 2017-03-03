#!/usr/bin/env perl

####################################################
#Author  : Bharath
#Comment : Takes the scale factor, number of primary segments, master port number and database name as input and load the tpch data in parallel by streaming the output of dbgen using external tables. Expects the dbgen program and dists.dss file to be present in the $GPHOME/bin directory on all the segment hosts.
#Usage   : sample usage > perl load_tpch.pl -scale <size of data to be loaded. scale of 1 means 1GB, 1000 means 1TB> -num <total number of primary segments in the cluster> -port <master port number> -db <name of the database in which the tpch tables will be created>
###################################################

#use strict;
use warnings;
use Getopt::Long;

# Arguments to the program
my ($scale, $num_primary_segs, $port, $dbname, $table, $orientation, $compress_type, $compress_level, $haspartition);

$table = "default";
$orientation = "column";
$compress_type = "quicklz";
$compress_level = "1";
$haspartition = "False";

GetOptions(
	'scale=i' => \$scale,
	'num=i' => \$num_primary_segs,
        'port=i' => \$port,
        'db=s' => \$dbname, 
        'table=s' => \$table,
        'orient=s' => \$orientation,
        'type=s' => \$compress_type,
        'level=s' => \$compress_level,
        'partition=s' => \$haspartition
	);

my $ddl_dml_file = "/tmp/load_tpch_".`date "+%Y%m%d%H%M"`;
chomp($ddl_dml_file);

unless (defined($scale) && defined($num_primary_segs) && defined($port) && defined($dbname) && defined($haspartition))
{
    print "\nUsage: $0 -scale <size of data to be loaded. scale of 1 means 1GB, 1000 means 1TB> -num <total number of primary segments in the cluster> -port <master port number> -db <name of the database in which the tpch tables will be created> -table <table type, support ao/heap. if table type is set ao,  then orient, type, level parameters is needed optionally.> -orient <row-based or column-based. row means row-based and column means column-based> -type <compression type of append-only tables. Support zlib and QuickLZ)> -level <compression level for each compression type. QuickLZ compression level can only be set to level 1; no other options are available. Compression level with zlib can be set at any value from 1 - 9>\n\n";
    print "The program expects database <dbname> to be present. Also, it expects dbgen program and dists.dss file to be present in GPHOME/bin directory. \n\n";
    exit;
}

open(OUT,">$ddl_dml_file") or die("Unable to open file $ddl_dml_file\n");

print "\nWriting the necessary ddl and dml statements to file: $ddl_dml_file\n";

# Define table name

$name_suffix="";
$sql_suffix="";
if ($table eq "ao")
{    
     $table="ao";
     $ori=$orientation;
     $type=$compress_type;
     $level=$compress_level;
     $name_suffix="";
     $sql_suffix = "WITH (appendonly=true, orientation=$ori, compresstype=$type, compresslevel=$level)";
}
else
{    
     $table="default";
     $name_suffix="";
     
}

$nation = "nation".$name_suffix;
$region = "region".$name_suffix;
$part = "part".$name_suffix;
$supplier = "supplier".$name_suffix;
$partsupp = "partsupp".$name_suffix;
$customer = "customer".$name_suffix;
$orders = "orders".$name_suffix;
$lineitem = "lineitem".$name_suffix;

$e_nation = "e_nation".$name_suffix;
$e_region = "e_region".$name_suffix;
$e_part = "e_part".$name_suffix;
$e_supplier = "e_supplier".$name_suffix;
$e_partsupp = "e_partsupp".$name_suffix;
$e_customer = "e_customer".$name_suffix;
$e_orders = "e_orders".$name_suffix;
$e_lineitem = "e_lineitem".$name_suffix;
 
# Drop table statments

print OUT "\\timing on\n";
print OUT "drop external web table if exists $e_nation;\n";
print OUT "drop external web table if exists $e_customer;\n";
print OUT "drop external web table if exists $e_region;\n";
print OUT "drop external web table if exists $e_part;\n";
print OUT "drop external web table if exists $e_supplier;\n";
print OUT "drop external web table if exists $e_partsupp;\n";
print OUT "drop external web table if exists $e_orders;\n";
print OUT "drop external web table if exists $e_lineitem;\n\n";
print OUT "drop table if exists $nation cascade;\n";
print OUT "drop table if exists $region cascade;\n";
print OUT "drop table if exists $part cascade;\n";
print OUT "drop table if exists $supplier cascade;\n";
print OUT "drop table if exists $partsupp cascade;\n";
print OUT "drop table if exists $customer cascade;\n";
print OUT "drop table if exists $orders cascade;\n";
print OUT "drop table if exists $lineitem cascade;\n\n";

# Create table statements

print OUT "CREATE TABLE $nation  ( N_NATIONKEY  INTEGER NOT NULL,
                            N_NAME       CHAR(25) NOT NULL,
                            N_REGIONKEY  INTEGER NOT NULL,
                            N_COMMENT    VARCHAR(152)) $sql_suffix;\n";

print OUT "CREATE TABLE $region  ( R_REGIONKEY  INTEGER NOT NULL,
                            R_NAME       CHAR(25) NOT NULL,
                            R_COMMENT    VARCHAR(152)) $sql_suffix;\n";

print OUT "CREATE TABLE $part  ( P_PARTKEY     INTEGER NOT NULL,
                          P_NAME        VARCHAR(55) NOT NULL,
                          P_MFGR        CHAR(25) NOT NULL,
                          P_BRAND       CHAR(10) NOT NULL,
                          P_TYPE        VARCHAR(25) NOT NULL,
                          P_SIZE        INTEGER NOT NULL,
                          P_CONTAINER   CHAR(10) NOT NULL,
                          P_RETAILPRICE DECIMAL(15,2) NOT NULL,
                          P_COMMENT     VARCHAR(23) NOT NULL ) $sql_suffix;\n";

print OUT "CREATE TABLE $supplier ( S_SUPPKEY     INTEGER NOT NULL,
                             S_NAME        CHAR(25) NOT NULL,
                             S_ADDRESS     VARCHAR(40) NOT NULL,
                             S_NATIONKEY   INTEGER NOT NULL,
                             S_PHONE       CHAR(15) NOT NULL,
                             S_ACCTBAL     DECIMAL(15,2) NOT NULL,
                             S_COMMENT     VARCHAR(101) NOT NULL) $sql_suffix;\n";

print OUT "CREATE TABLE $partsupp ( PS_PARTKEY     INTEGER NOT NULL,
                             PS_SUPPKEY     INTEGER NOT NULL,
                             PS_AVAILQTY    INTEGER NOT NULL,
                             PS_SUPPLYCOST  DECIMAL(15,2)  NOT NULL,
                             PS_COMMENT     VARCHAR(199) NOT NULL ) $sql_suffix;\n";

print OUT "CREATE TABLE $customer ( C_CUSTKEY     INTEGER NOT NULL,
                             C_NAME        VARCHAR(25) NOT NULL,
                             C_ADDRESS     VARCHAR(40) NOT NULL,
                             C_NATIONKEY   INTEGER NOT NULL,
                             C_PHONE       CHAR(15) NOT NULL,
                             C_ACCTBAL     DECIMAL(15,2)   NOT NULL,
                             C_MKTSEGMENT  CHAR(10) NOT NULL,
                             C_COMMENT     VARCHAR(117) NOT NULL) $sql_suffix;\n";

print OUT "CREATE TABLE $orders  ( O_ORDERKEY       INT8 NOT NULL,
                           O_CUSTKEY        INTEGER NOT NULL,
                           O_ORDERSTATUS    CHAR(1) NOT NULL,
                           O_TOTALPRICE     DECIMAL(15,2) NOT NULL,
                           O_ORDERDATE      DATE NOT NULL,
                           O_ORDERPRIORITY  CHAR(15) NOT NULL,
                           O_CLERK          CHAR(15) NOT NULL,
                           O_SHIPPRIORITY   INTEGER NOT NULL,
                           O_COMMENT        VARCHAR(79) NOT NULL) $sql_suffix";
if ($haspartition eq "True")
{
print OUT "PARTITION BY RANGE(o_orderdate)
          (
          PARTITION p1_1 START ('1992-01-01'::date) END ('1992-01-21'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_1', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_2 START ('1992-01-21'::date) END ('1992-02-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_2', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_3 START ('1992-02-10'::date) END ('1992-03-01'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_3', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_4 START ('1992-03-01'::date) END ('1992-03-21'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_4', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_5 START ('1992-03-21'::date) END ('1992-04-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_5', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_6 START ('1992-04-10'::date) END ('1992-04-30'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_6', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_7 START ('1992-04-30'::date) END ('1992-05-20'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_7', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_8 START ('1992-05-20'::date) END ('1992-06-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_8', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_9 START ('1992-06-09'::date) END ('1992-06-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_9', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_10 START ('1992-06-29'::date) END ('1992-07-19'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_10', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_11 START ('1992-07-19'::date) END ('1992-08-08'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_11', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_12 START ('1992-08-08'::date) END ('1992-08-28'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_12', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_13 START ('1992-08-28'::date) END ('1992-09-17'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_13', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_14 START ('1992-09-17'::date) END ('1992-10-07'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_14', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_15 START ('1992-10-07'::date) END ('1992-10-27'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_15', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_16 START ('1992-10-27'::date) END ('1992-11-16'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_16', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_17 START ('1992-11-16'::date) END ('1992-12-06'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_17', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_18 START ('1992-12-06'::date) END ('1992-12-26'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_18', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_19 START ('1992-12-26'::date) END ('1993-01-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_19', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_20 START ('1993-01-15'::date) END ('1993-02-04'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_20', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_21 START ('1993-02-04'::date) END ('1993-02-24'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_21', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_22 START ('1993-02-24'::date) END ('1993-03-16'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_22', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_23 START ('1993-03-16'::date) END ('1993-04-05'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_23', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_24 START ('1993-04-05'::date) END ('1993-04-25'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_24', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_25 START ('1993-04-25'::date) END ('1993-05-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_25', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_26 START ('1993-05-15'::date) END ('1993-06-04'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_26', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_27 START ('1993-06-04'::date) END ('1993-06-24'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_27', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_28 START ('1993-06-24'::date) END ('1993-07-14'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_28', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_29 START ('1993-07-14'::date) END ('1993-08-03'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_29', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_30 START ('1993-08-03'::date) END ('1993-08-23'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_30', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_31 START ('1993-08-23'::date) END ('1993-09-12'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_31', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_32 START ('1993-09-12'::date) END ('1993-10-02'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_32', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_33 START ('1993-10-02'::date) END ('1993-10-22'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_33', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_34 START ('1993-10-22'::date) END ('1993-11-11'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_34', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_35 START ('1993-11-11'::date) END ('1993-12-01'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_35', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_36 START ('1993-12-01'::date) END ('1993-12-21'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_36', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_37 START ('1993-12-21'::date) END ('1994-01-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_37', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_38 START ('1994-01-10'::date) END ('1994-01-30'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_38', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_39 START ('1994-01-30'::date) END ('1994-02-19'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_39', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_40 START ('1994-02-19'::date) END ('1994-03-11'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_40', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_41 START ('1994-03-11'::date) END ('1994-03-31'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_41', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_42 START ('1994-03-31'::date) END ('1994-04-20'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_42', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_43 START ('1994-04-20'::date) END ('1994-05-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_43', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_44 START ('1994-05-10'::date) END ('1994-05-30'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_44', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_45 START ('1994-05-30'::date) END ('1994-06-19'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_45', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_46 START ('1994-06-19'::date) END ('1994-07-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_46', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_47 START ('1994-07-09'::date) END ('1994-07-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_47', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_48 START ('1994-07-29'::date) END ('1994-08-18'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_48', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_49 START ('1994-08-18'::date) END ('1994-09-07'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_49', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_50 START ('1994-09-07'::date) END ('1994-09-27'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_50', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_51 START ('1994-09-27'::date) END ('1994-10-17'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_51', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_52 START ('1994-10-17'::date) END ('1994-11-06'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_52', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_53 START ('1994-11-06'::date) END ('1994-11-26'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_53', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_54 START ('1994-11-26'::date) END ('1994-12-16'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_54', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_55 START ('1994-12-16'::date) END ('1995-01-05'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_55', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_56 START ('1995-01-05'::date) END ('1995-01-25'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_56', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_57 START ('1995-01-25'::date) END ('1995-02-14'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_57', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_58 START ('1995-02-14'::date) END ('1995-03-06'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_58', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_59 START ('1995-03-06'::date) END ('1995-03-26'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_59', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_60 START ('1995-03-26'::date) END ('1995-04-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_60', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_61 START ('1995-04-15'::date) END ('1995-05-05'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_61', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_62 START ('1995-05-05'::date) END ('1995-05-25'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_62', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_63 START ('1995-05-25'::date) END ('1995-06-14'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_63', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_64 START ('1995-06-14'::date) END ('1995-07-04'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_64', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_65 START ('1995-07-04'::date) END ('1995-07-24'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_65', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_66 START ('1995-07-24'::date) END ('1995-08-13'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_66', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_67 START ('1995-08-13'::date) END ('1995-09-02'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_67', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_68 START ('1995-09-02'::date) END ('1995-09-22'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_68', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_69 START ('1995-09-22'::date) END ('1995-10-12'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_69', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_70 START ('1995-10-12'::date) END ('1995-11-01'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_70', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_71 START ('1995-11-01'::date) END ('1995-11-21'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_71', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_72 START ('1995-11-21'::date) END ('1995-12-11'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_72', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_73 START ('1995-12-11'::date) END ('1995-12-31'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_73', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_74 START ('1995-12-31'::date) END ('1996-01-20'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_74', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_75 START ('1996-01-20'::date) END ('1996-02-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_75', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_76 START ('1996-02-09'::date) END ('1996-02-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_76', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_77 START ('1996-02-29'::date) END ('1996-03-20'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_77', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_78 START ('1996-03-20'::date) END ('1996-04-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_78', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_79 START ('1996-04-09'::date) END ('1996-04-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_79', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_80 START ('1996-04-29'::date) END ('1996-05-19'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_80', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_81 START ('1996-05-19'::date) END ('1996-06-08'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_81', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_82 START ('1996-06-08'::date) END ('1996-06-28'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_82', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_83 START ('1996-06-28'::date) END ('1996-07-18'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_83', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_84 START ('1996-07-18'::date) END ('1996-08-07'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_84', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_85 START ('1996-08-07'::date) END ('1996-08-27'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_85', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_86 START ('1996-08-27'::date) END ('1996-09-16'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_86', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_87 START ('1996-09-16'::date) END ('1996-10-06'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_87', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_88 START ('1996-10-06'::date) END ('1996-10-26'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_88', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_89 START ('1996-10-26'::date) END ('1996-11-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_89', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_90 START ('1996-11-15'::date) END ('1996-12-05'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_90', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_91 START ('1996-12-05'::date) END ('1996-12-25'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_91', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_92 START ('1996-12-25'::date) END ('1997-01-14'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_92', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_93 START ('1997-01-14'::date) END ('1997-02-03'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_93', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_94 START ('1997-02-03'::date) END ('1997-02-23'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_94', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_95 START ('1997-02-23'::date) END ('1997-03-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_95', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_96 START ('1997-03-15'::date) END ('1997-04-04'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_96', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_97 START ('1997-04-04'::date) END ('1997-04-24'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_97', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_98 START ('1997-04-24'::date) END ('1997-05-14'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_98', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_99 START ('1997-05-14'::date) END ('1997-06-03'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_99', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_100 START ('1997-06-03'::date) END ('1997-06-23'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_100', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_101 START ('1997-06-23'::date) END ('1997-07-13'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_101', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_102 START ('1997-07-13'::date) END ('1997-08-02'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_102', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_103 START ('1997-08-02'::date) END ('1997-08-22'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_103', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_104 START ('1997-08-22'::date) END ('1997-09-11'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_104', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_105 START ('1997-09-11'::date) END ('1997-10-01'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_105', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_106 START ('1997-10-01'::date) END ('1997-10-21'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_106', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_107 START ('1997-10-21'::date) END ('1997-11-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_107', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_108 START ('1997-11-10'::date) END ('1997-11-30'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_108', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_109 START ('1997-11-30'::date) END ('1997-12-20'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_109', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_110 START ('1997-12-20'::date) END ('1998-01-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_110', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_111 START ('1998-01-09'::date) END ('1998-01-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_111', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_112 START ('1998-01-29'::date) END ('1998-02-18'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_112', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_113 START ('1998-02-18'::date) END ('1998-03-10'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_113', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_114 START ('1998-03-10'::date) END ('1998-03-30'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_114', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_115 START ('1998-03-30'::date) END ('1998-04-19'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_115', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_116 START ('1998-04-19'::date) END ('1998-05-09'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_116', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_117 START ('1998-05-09'::date) END ('1998-05-29'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_117', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_118 START ('1998-05-29'::date) END ('1998-06-18'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_118', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_119 START ('1998-06-18'::date) END ('1998-07-08'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_119', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_120 START ('1998-07-08'::date) END ('1998-07-28'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_120', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_121 START ('1998-07-28'::date) END ('1998-08-17'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_121', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_122 START ('1998-08-17'::date) END ('1998-09-06'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_122', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_123 START ('1998-09-06'::date) END ('1998-09-26'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_123', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_124 START ('1998-09-26'::date) END ('1998-10-16'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_124', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_125 START ('1998-10-16'::date) END ('1998-11-05'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_125', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_126 START ('1998-11-05'::date) END ('1998-11-25'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_126', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_127 START ('1998-11-25'::date) END ('1998-12-15'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_127', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_128 START ('1998-12-15'::date) END ('1998-12-31'::date) EVERY ('20 days'::interval) WITH (tablename='${orders}_1_prt_p1_128', orientation=$orientation , appendonly=true, compresstype=$compress_type, compresslevel=$compress_level )
          )";
}
print OUT ";\n\n";       




print OUT "CREATE TABLE $lineitem ( L_ORDERKEY    INT8 NOT NULL,
                              L_PARTKEY     INTEGER NOT NULL,
                              L_SUPPKEY     INTEGER NOT NULL,
                              L_LINENUMBER  INTEGER NOT NULL,
                              L_QUANTITY    DECIMAL(15,2) NOT NULL,
                              L_EXTENDEDPRICE  DECIMAL(15,2) NOT NULL,
                              L_DISCOUNT    DECIMAL(15,2) NOT NULL,
                              L_TAX         DECIMAL(15,2) NOT NULL,
                              L_RETURNFLAG  CHAR(1) NOT NULL,
                              L_LINESTATUS  CHAR(1) NOT NULL,
                              L_SHIPDATE    DATE NOT NULL,
                              L_COMMITDATE  DATE NOT NULL,
                              L_RECEIPTDATE DATE NOT NULL,
                              L_SHIPINSTRUCT CHAR(25) NOT NULL,
                              L_SHIPMODE     CHAR(10) NOT NULL,
                              L_COMMENT      VARCHAR(44) NOT NULL) $sql_suffix";
if ($haspartition eq "True")
{
print OUT "PARTITION BY RANGE(l_shipdate)(
          PARTITION p1_1 START ('1992-01-01'::date) END ('1992-01-21'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_1', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level),
          PARTITION p1_2 START ('1992-01-21'::date) END ('1992-02-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_2', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_3 START ('1992-02-10'::date) END ('1992-03-01'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_3', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_4 START ('1992-03-01'::date) END ('1992-03-21'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_4', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_5 START ('1992-03-21'::date) END ('1992-04-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_5', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_6 START ('1992-04-10'::date) END ('1992-04-30'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_6', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level),
          PARTITION p1_7 START ('1992-04-30'::date) END ('1992-05-20'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_7', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_8 START ('1992-05-20'::date) END ('1992-06-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_8', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_9 START ('1992-06-09'::date) END ('1992-06-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_9', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_10 START ('1992-06-29'::date) END ('1992-07-19'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_10', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_11 START ('1992-07-19'::date) END ('1992-08-08'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_11', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_12 START ('1992-08-08'::date) END ('1992-08-28'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_12', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_13 START ('1992-08-28'::date) END ('1992-09-17'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_13', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_14 START ('1992-09-17'::date) END ('1992-10-07'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_14', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level),
          PARTITION p1_15 START ('1992-10-07'::date) END ('1992-10-27'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_15', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_16 START ('1992-10-27'::date) END ('1992-11-16'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_16', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_17 START ('1992-11-16'::date) END ('1992-12-06'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_17', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_18 START ('1992-12-06'::date) END ('1992-12-26'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_18', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_19 START ('1992-12-26'::date) END ('1993-01-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_19', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_20 START ('1993-01-15'::date) END ('1993-02-04'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_20', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_21 START ('1993-02-04'::date) END ('1993-02-24'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_21', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_22 START ('1993-02-24'::date) END ('1993-03-16'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_22', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_23 START ('1993-03-16'::date) END ('1993-04-05'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_23', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_24 START ('1993-04-05'::date) END ('1993-04-25'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_24', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_25 START ('1993-04-25'::date) END ('1993-05-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_25', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_26 START ('1993-05-15'::date) END ('1993-06-04'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_26', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_27 START ('1993-06-04'::date) END ('1993-06-24'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_27', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_28 START ('1993-06-24'::date) END ('1993-07-14'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_28', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_29 START ('1993-07-14'::date) END ('1993-08-03'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_29', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_30 START ('1993-08-03'::date) END ('1993-08-23'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_30', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_31 START ('1993-08-23'::date) END ('1993-09-12'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_31', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_32 START ('1993-09-12'::date) END ('1993-10-02'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_32', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_33 START ('1993-10-02'::date) END ('1993-10-22'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_33', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_34 START ('1993-10-22'::date) END ('1993-11-11'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_34', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_35 START ('1993-11-11'::date) END ('1993-12-01'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_35', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_36 START ('1993-12-01'::date) END ('1993-12-21'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_36', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_37 START ('1993-12-21'::date) END ('1994-01-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_37', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_38 START ('1994-01-10'::date) END ('1994-01-30'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_38', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_39 START ('1994-01-30'::date) END ('1994-02-19'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_39', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_40 START ('1994-02-19'::date) END ('1994-03-11'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_40', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_41 START ('1994-03-11'::date) END ('1994-03-31'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_41', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_42 START ('1994-03-31'::date) END ('1994-04-20'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_42', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_43 START ('1994-04-20'::date) END ('1994-05-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_43', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_44 START ('1994-05-10'::date) END ('1994-05-30'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_44', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_45 START ('1994-05-30'::date) END ('1994-06-19'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_45', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_46 START ('1994-06-19'::date) END ('1994-07-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_46', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_47 START ('1994-07-09'::date) END ('1994-07-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_47', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_48 START ('1994-07-29'::date) END ('1994-08-18'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_48', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_49 START ('1994-08-18'::date) END ('1994-09-07'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_49', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_50 START ('1994-09-07'::date) END ('1994-09-27'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_50', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_51 START ('1994-09-27'::date) END ('1994-10-17'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_51', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_52 START ('1994-10-17'::date) END ('1994-11-06'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_52', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_53 START ('1994-11-06'::date) END ('1994-11-26'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_53', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_54 START ('1994-11-26'::date) END ('1994-12-16'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_54', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_55 START ('1994-12-16'::date) END ('1995-01-05'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_55', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_56 START ('1995-01-05'::date) END ('1995-01-25'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_56', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_57 START ('1995-01-25'::date) END ('1995-02-14'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_57', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_58 START ('1995-02-14'::date) END ('1995-03-06'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_58', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_59 START ('1995-03-06'::date) END ('1995-03-26'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_59', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_60 START ('1995-03-26'::date) END ('1995-04-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_60', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_61 START ('1995-04-15'::date) END ('1995-05-05'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_61', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_62 START ('1995-05-05'::date) END ('1995-05-25'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_62', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_63 START ('1995-05-25'::date) END ('1995-06-14'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_63', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_64 START ('1995-06-14'::date) END ('1995-07-04'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_64', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_65 START ('1995-07-04'::date) END ('1995-07-24'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_65', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_66 START ('1995-07-24'::date) END ('1995-08-13'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_66', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_67 START ('1995-08-13'::date) END ('1995-09-02'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_67', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_68 START ('1995-09-02'::date) END ('1995-09-22'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_68', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_69 START ('1995-09-22'::date) END ('1995-10-12'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_69', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_70 START ('1995-10-12'::date) END ('1995-11-01'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_70', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_71 START ('1995-11-01'::date) END ('1995-11-21'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_71', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_72 START ('1995-11-21'::date) END ('1995-12-11'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_72', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_73 START ('1995-12-11'::date) END ('1995-12-31'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_73', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_74 START ('1995-12-31'::date) END ('1996-01-20'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_74', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_75 START ('1996-01-20'::date) END ('1996-02-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_75', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_76 START ('1996-02-09'::date) END ('1996-02-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_76', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_77 START ('1996-02-29'::date) END ('1996-03-20'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_77', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_78 START ('1996-03-20'::date) END ('1996-04-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_78', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_79 START ('1996-04-09'::date) END ('1996-04-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_79', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_80 START ('1996-04-29'::date) END ('1996-05-19'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_80', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_81 START ('1996-05-19'::date) END ('1996-06-08'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_81', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_82 START ('1996-06-08'::date) END ('1996-06-28'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_82', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_83 START ('1996-06-28'::date) END ('1996-07-18'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_83', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_84 START ('1996-07-18'::date) END ('1996-08-07'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_84', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_85 START ('1996-08-07'::date) END ('1996-08-27'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_85', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_86 START ('1996-08-27'::date) END ('1996-09-16'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_86', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_87 START ('1996-09-16'::date) END ('1996-10-06'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_87', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_88 START ('1996-10-06'::date) END ('1996-10-26'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_88', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_89 START ('1996-10-26'::date) END ('1996-11-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_89', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_90 START ('1996-11-15'::date) END ('1996-12-05'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_90', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_91 START ('1996-12-05'::date) END ('1996-12-25'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_91', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_92 START ('1996-12-25'::date) END ('1997-01-14'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_92', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_93 START ('1997-01-14'::date) END ('1997-02-03'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_93', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_94 START ('1997-02-03'::date) END ('1997-02-23'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_94', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_95 START ('1997-02-23'::date) END ('1997-03-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_95', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_96 START ('1997-03-15'::date) END ('1997-04-04'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_96', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_97 START ('1997-04-04'::date) END ('1997-04-24'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_97', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_98 START ('1997-04-24'::date) END ('1997-05-14'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_98', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_99 START ('1997-05-14'::date) END ('1997-06-03'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_99', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_100 START ('1997-06-03'::date) END ('1997-06-23'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_100', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_101 START ('1997-06-23'::date) END ('1997-07-13'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_101', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_102 START ('1997-07-13'::date) END ('1997-08-02'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_102', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_103 START ('1997-08-02'::date) END ('1997-08-22'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_103', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_104 START ('1997-08-22'::date) END ('1997-09-11'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_104', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_105 START ('1997-09-11'::date) END ('1997-10-01'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_105', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_106 START ('1997-10-01'::date) END ('1997-10-21'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_106', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_107 START ('1997-10-21'::date) END ('1997-11-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_107', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_108 START ('1997-11-10'::date) END ('1997-11-30'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_108', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_109 START ('1997-11-30'::date) END ('1997-12-20'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_109', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_110 START ('1997-12-20'::date) END ('1998-01-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_110', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_111 START ('1998-01-09'::date) END ('1998-01-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_111', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_112 START ('1998-01-29'::date) END ('1998-02-18'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_112', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_113 START ('1998-02-18'::date) END ('1998-03-10'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_113', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_114 START ('1998-03-10'::date) END ('1998-03-30'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_114', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_115 START ('1998-03-30'::date) END ('1998-04-19'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_115', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_116 START ('1998-04-19'::date) END ('1998-05-09'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_116', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_117 START ('1998-05-09'::date) END ('1998-05-29'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_117', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_118 START ('1998-05-29'::date) END ('1998-06-18'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_118', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_119 START ('1998-06-18'::date) END ('1998-07-08'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_119', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_120 START ('1998-07-08'::date) END ('1998-07-28'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_120', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_121 START ('1998-07-28'::date) END ('1998-08-17'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_121', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_122 START ('1998-08-17'::date) END ('1998-09-06'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_122', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_123 START ('1998-09-06'::date) END ('1998-09-26'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_123', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_124 START ('1998-09-26'::date) END ('1998-10-16'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_124', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_125 START ('1998-10-16'::date) END ('1998-11-05'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_125', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_126 START ('1998-11-05'::date) END ('1998-11-25'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_126', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_127 START ('1998-11-25'::date) END ('1998-12-15'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_127', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level ),
          PARTITION p1_128 START ('1998-12-15'::date) END ('1998-12-31'::date) EVERY ('20 days'::interval) WITH (tablename='${lineitem}_1_prt_p1_128', orientation=$orientation, appendonly=true, compresstype=$compress_type, compresslevel=$compress_level)
          )";
}
print OUT ";\n\n";

print OUT "create external web table $e_nation (N_NATIONKEY  INTEGER ,
                            N_NAME       CHAR(25) ,
                            N_REGIONKEY  INTEGER ,
                            N_COMMENT    VARCHAR(152)) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T n -s $scale\"' on 1 format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_region  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152)) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T r -s $scale\"'
                        on 1 format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_part  ( P_PARTKEY     INTEGER ,
                          P_NAME        VARCHAR(55) ,
                          P_MFGR        CHAR(25) ,
                          P_BRAND       CHAR(10) ,
                          P_TYPE        VARCHAR(25) ,
                          P_SIZE        INTEGER ,
                          P_CONTAINER   CHAR(10) ,
                          P_RETAILPRICE DECIMAL(15,2) ,
                          P_COMMENT     VARCHAR(23) ) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T P -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"'
                        on $num_primary_segs format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_supplier ( S_SUPPKEY     INTEGER ,
                             S_NAME        CHAR(25) ,
                             S_ADDRESS     VARCHAR(40) ,
                             S_NATIONKEY   INTEGER ,
                             S_PHONE       CHAR(15) ,
                             S_ACCTBAL     DECIMAL(15,2) ,
                             S_COMMENT     VARCHAR(101) ) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T s -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"'
                        on $num_primary_segs format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_partsupp ( PS_PARTKEY     INTEGER ,
                             PS_SUPPKEY     INTEGER ,
                             PS_AVAILQTY    INTEGER ,
                             PS_SUPPLYCOST  DECIMAL(15,2)  ,
                             PS_COMMENT     VARCHAR(199) ) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T S -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"'
                        on $num_primary_segs format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_customer ( C_CUSTKEY     INTEGER ,
                             C_NAME        VARCHAR(25) ,
                             C_ADDRESS     VARCHAR(40) ,
                             C_NATIONKEY   INTEGER ,
                             C_PHONE       CHAR(15) ,
                             C_ACCTBAL     DECIMAL(15,2) ,
                             C_MKTSEGMENT  CHAR(10) ,
                             C_COMMENT     VARCHAR(117) ) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T c -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"'
                        on $num_primary_segs format 'text' (delimiter '|');\n";

print OUT "CREATE external web TABLE $e_orders  ( O_ORDERKEY       INT8 ,
                           O_CUSTKEY        INTEGER ,
                           O_ORDERSTATUS    CHAR(1) ,
                           O_TOTALPRICE     DECIMAL(15,2) ,
                           O_ORDERDATE      DATE ,
                           O_ORDERPRIORITY  CHAR(15) ,
                           O_CLERK          CHAR(15) ,
                           O_SHIPPRIORITY   INTEGER ,
                           O_COMMENT        VARCHAR(79) ) 
                        execute 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T O -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"'
                        on $num_primary_segs format 'text' (delimiter '|');\n";

print OUT "CREATE EXTERNAL WEB TABLE $e_lineitem ( L_ORDERKEY    INT8 ,
                              L_PARTKEY     INTEGER ,
                              L_SUPPKEY     INTEGER ,
                              L_LINENUMBER  INTEGER ,
                              L_QUANTITY    DECIMAL(15,2) ,
                              L_EXTENDEDPRICE  DECIMAL(15,2) ,
                              L_DISCOUNT    DECIMAL(15,2) ,
                              L_TAX         DECIMAL(15,2) ,
                              L_RETURNFLAG  CHAR(1) ,
                              L_LINESTATUS  CHAR(1) ,
                              L_SHIPDATE    DATE ,
                              L_COMMITDATE  DATE ,
                              L_RECEIPTDATE DATE ,
                              L_SHIPINSTRUCT CHAR(25) ,
                              L_SHIPMODE     CHAR(10) ,
                              L_COMMENT      VARCHAR(44) )
                              EXECUTE 'bash -c \"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T L -s $scale -N $num_primary_segs -n \$((GP_SEGMENT_ID + 1))\"' 
                              on $num_primary_segs format 'text' (delimiter '|');\n\n";

# Now statements to load the data

print OUT "insert into $nation select * from $e_nation;\n";
print OUT "insert into $region select * from $e_region;\n";
print OUT "insert into $part select * from $e_part;\n";
print OUT "insert into $supplier select * from $e_supplier;\n";
print OUT "insert into $partsupp select * from $e_partsupp;\n";
print OUT "insert into $customer select * from $e_customer;\n";
print OUT "insert into $orders select * from $e_orders;\n";
print OUT "insert into $lineitem select * from $e_lineitem;\n";
print OUT "select current_timestamp;\n";
print OUT "VACUUM ANALYZE;\n";
print OUT "select current_timestamp;\n";
print OUT "\\timing off";

close(OUT);

print "Creating tables, loading data and running VACUUM ANALYZE\n";

my $tmp_outfile = $ddl_dml_file."_out";

system("psql -p $port -d $dbname -a -f $ddl_dml_file > $tmp_outfile 2>&1");

print "Done.\n";
