--
-- See MPP-6861
--
CREATE TABLE ds_4
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10),
  source character varying(12),
  vas_group numeric(10),
  vas_type numeric(10),
  count_vas integer,
  amt_vas numeric(10,2),
  network_type character varying(3),
  execution_id integer
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803')
);

-- this is the case that worked before MPP-6861
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id = '200800';


-- now we can evaluate this function at planning/prune time
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int = 200800;

-- this will be satisfied by 200800
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int - 801 < 200000;

-- test OR case -- should NOT get pruning
explain select * from ds_4 where month_id::int - 801 < 200000 OR count_vas > 10;

-- test AND case -- should still get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int - 801 < 200000 AND count_vas > 10;

-- test expression case : should get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id = '200800' then 100 else 2 end = 100;

-- test expression case : should get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id = '200800' then NULL else 2 end IS NULL;

-- should still get pruning here -- count_vas is only used in the path for month id = 200800
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id::int = 200800 then count_vas else 2 end IS NULL;

-- do one that matches a couple partitions
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int in (200801, 1,55,6,6,6,6,66,565,65,65,200803);

-- cleanup
drop table ds_4;


--
-- See MPP-18979
--

CREATE TABLE ds_2
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10),
  source character varying(12),
  vas_group numeric(10),
  vas_type numeric(10),
  count_vas integer,
  amt_vas numeric(10,2),
  network_type character varying(3),
  execution_id integer
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803'),
          PARTITION p200804 VALUES('200804'),
          PARTITION p200805 VALUES('200805'),
          PARTITION p200806 VALUES('200806'),
          PARTITION p200807 VALUES('200807'),
          PARTITION p200808 VALUES('200808'),
          PARTITION p200809 VALUES('200809')
);

insert into ds_2(month_id) values('200800');
insert into ds_2(month_id) values('200801');
insert into ds_2(month_id) values('200802');
insert into ds_2(month_id) values('200803');
insert into ds_2(month_id) values('200804');
insert into ds_2(month_id) values('200805');
insert into ds_2(month_id) values('200806');
insert into ds_2(month_id) values('200807');
insert into ds_2(month_id) values('200808');
insert into ds_2(month_id) values('200809');

-- queries without bitmap scan
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
set optimizer_segments=2;
explain select * from ds_2 where month_id::int in (200808, 1315) order by month_id;

-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain  select * from ds_2 where month_id::int in (200808, 200801, 2008010) order by month_id;
reset optimizer_segments;
select * from ds_2 where month_id::int in (200907, 1315) order by month_id;

select * from ds_2 where month_id::int in (200808, 1315) order by month_id;

select * from ds_2 where month_id::int in (200808, 200801) order by month_id;

select * from ds_2 where month_id::int in (200808, 200801, 2008010) order by month_id;

-- cleanup
drop table ds_2;

Create or replace function public.reverse(text) Returns text as $BODY$
DECLARE
   Original alias for $1;
   Reverse_str text;
   I int4;
BEGIN
   Reverse_str :='';
   For I in reverse length(original)..1 LOOP
   Reverse_str := reverse_str || substr(original,I,1);
END LOOP;
RETURN reverse_str;
END;
$BODY$ LANGUAGE plpgsql IMMUTABLE;

drop table if exists dnsdata cascade;

CREATE TABLE dnsdata(dnsname text) DISTRIBUTED RANDOMLY;

CREATE INDEX dnsdata_d1_idx ON dnsdata USING bitmap (split_part(reverse(dnsname),'.'::text,1));


CREATE INDEX dnsdata_d2_idx ON dnsdata USING bitmap (split_part(reverse(dnsname),'.'::text,2));

insert into dnsdata values('www.google.com');
insert into dnsdata values('www.google1.com');
insert into dnsdata values('1.google.com');
insert into dnsdata values('2.google.com');
insert into dnsdata select 'www.b.com' from generate_series(1, 100000) as x(a);

analyze dnsdata;

-- queries with bitmap scan enabled
set enable_bitmapscan=on;
set enable_indexscan=on;
set enable_seqscan=off;

Select dnsname from dnsdata
where (split_part(reverse('cache.google.com'),'.',1))=(split_part(reverse(dnsname),'.',1))
and (split_part(reverse('cache.google.com'),'.',2))=(split_part(reverse(dnsname),'.',2)) 
order by dnsname;

Select dnsname from dnsdata
where (split_part(reverse('cache.google.com'),'.',1))=(split_part(reverse(dnsname),'.',1))
and (split_part(reverse('cache.google.com'),'.',2))=(split_part(reverse(dnsname),'.',2))
and dnsname = 'cache.google.com'
order by dnsname;

-- cleanup
drop table dnsdata cascade;
drop function public.reverse(text) cascade;


Create or replace function public.ZeroFunc(int) Returns int as $BODY$
BEGIN
  RETURN 0;
END;
$BODY$ LANGUAGE plpgsql IMMUTABLE;

drop table if exists mytable cascade;

create table mytable(i int, j int);
insert into mytable select x, x+1 from generate_series(1, 100000) as x;
analyze mytable;

CREATE INDEX mytable_idx1 ON mytable USING bitmap(zerofunc(i));


select * from mytable where ZeroFunc(i)=0 and i=100 order by i;

select * from mytable where ZeroFunc(i)=0 and i=-1 order by i;

-- cleanup
drop function ZeroFunc(int) cascade;
drop table mytable cascade;
