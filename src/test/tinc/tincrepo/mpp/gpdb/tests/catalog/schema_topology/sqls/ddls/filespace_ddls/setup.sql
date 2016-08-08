-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

-- Create tablespaces
CREATE tablespace ts_sch1 filespace cdbfast_fs_sch1;
CREATE tablespace ts_sch2 filespace cdbfast_fs_sch1;
CREATE tablespace ts_sch3 filespace cdbfast_fs_sch2;
CREATE tablespace ts_sch4 filespace cdbfast_fs_sch2;
CREATE tablespace ts_sch5 filespace cdbfast_fs_sch3;
CREATE tablespace ts_sch6 filespace cdbfast_fs_sch3;
