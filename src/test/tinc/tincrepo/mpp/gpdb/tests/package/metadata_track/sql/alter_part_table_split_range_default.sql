CREATE TABLE mdt_part_tbl_split_range (
c_id varchar(36),
ss_id varchar(36),
c_ts timestamp,
name varchar(36),
PRIMARY KEY (c_id,ss_id,c_ts)) partition by range (c_ts)
(
  start (date '2007-01-01')
  end (date '2007-04-01') every (interval '1 month'),
  default partition default_part

);
alter table mdt_part_tbl_split_range split default partition start ('2009-01-01') end ('2009-02-01') into (partition a3, default partition);

drop table mdt_part_tbl_split_range;
