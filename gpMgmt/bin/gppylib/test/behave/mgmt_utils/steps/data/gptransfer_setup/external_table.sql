--
-- create a RET table
--
CREATE external web TABLE e_REGION  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                            EXECUTE 'echo "1|region1|region 1. some description
                                           2|region2|region 2. some description
                                           3|region3|region 3. some description
                                           4|region4|region 4. some description
                                           5|region5|region 5. some description"'
                            on 1 format 'text' (delimiter '|');

--
-- create WET with similiar schema def as the original heap table
--
CREATE WRITABLE EXTERNAL TABLE wet_region ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                            LOCATION ('gpfdist://10.1.2.12:2345/wet_region.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

INSERT INTO wet_region SELECT * FROM e_REGION;

--
-- create a RET reading data from the file created by WET
--
CREATE EXTERNAL TABLE ret_region ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                            LOCATION ('gpfdist://10.1.2.12:2345/wet_region.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

CREATE TABLE region ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152));

INSERT INTO region SELECT * FROM e_REGION;
