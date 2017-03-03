-- @author prabhd
-- @created 2014-02-24 12:00:00
-- @modified 2014-02-24 12:00:00
-- @tags dml
-- @optimizer_mode=on
-- @db_name dmldb
-- @gucs client_min_messages='log'
-- @description MPP-22599 DML queries that fallback to planner don't check for updates on the distribution key. This test will fail once Multi-level partitioned tables are supported in ORCA.
update dml_multi_pt_update set a = 1;
