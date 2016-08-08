-- External Tables

-- create a normal heap table
CREATE TABLE mdt_REGION_1   ( R_REGIONKEY  INTEGER NOT NULL,
                            R_NAME       CHAR(25) NOT NULL,
                            R_COMMENT    VARCHAR(152)) ;


-- create a RET to load data into the normal heap table
CREATE external web TABLE mdt_e_REGION  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                        execute 'echo "0|AFRICA|lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to
                                       1|AMERICA|hs use ironic, even requests. s
                                       2|ASIA|ges. thinly even pinto beans ca
                                       3|EUROPE|ly final courts cajole furiously final excuse
                                       4|MIDDLE EAST|uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl"'
                        on 1 format 'text' (delimiter '|');

-- create WET with similiar schema def as the original heap table
CREATE WRITABLE EXTERNAL TABLE mdt_wet_region ( like mdt_region_1) LOCATION ('gpfdist://10.1.2.10:8088/view/wet_region.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

-- create a RET reading data from the file created by WET
CREATE EXTERNAL TABLE mdt_ret_region ( like mdt_region_1) LOCATION ('gpfdist://10.1.2.10:8088/view/wet_region.tbl') FORMAT 'TEXT' (DELIMITER AS
'|');

CREATE external web TABLE mdt_e_SUPPLIER ( S_SUPPKEY     INTEGER ,
                             S_NAME        CHAR(25) ,
                             S_ADDRESS     VARCHAR(40) ,
                             S_NATIONKEY   INTEGER ,
                             S_PHONE       CHAR(15) ,
                             S_ACCTBAL     DECIMAL(15,2) ,
                             S_COMMENT     VARCHAR(101) )
                        execute 'echo "1|Supplier#000000001| N kD4on9OM Ipw3,gf0JBoQDd7tgrzrddZ|17|27-918-335-1736|5755.94|each slyly above the careful
                                       2|Supplier#000000002|89eJ5ksX3ImxJQBvxObC,|5|15-679-861-2259|4032.68| slyly bold instructions. idle dependen
                                       3|Supplier#000000003|q1,G3Pj6OjIuUYfUoH18BFTKP5aU9bEV3|1|11-383-516-1199|4192.40|blithely silent requests after the express dependencies are sl
                                       4|Supplier#000000004|Bk7ah4CK8SYQTepEmvMkkgMwg|15|25-843-787-7479|4641.08|riously even requests above the exp
                                       5|Supplier#000000005|Gcdm2rJRzl5qlTVzc|11|21-151-690-3663|-283.84|. slyly regular pinto bea"'
                        on 2 format 'text' (delimiter '|');

drop table mdt_REGION_1;
drop external table mdt_e_REGION;
drop external table mdt_wet_region;
drop external table mdt_ret_region;
drop external table mdt_e_SUPPLIER;
