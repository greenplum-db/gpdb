-- MPP-7980
-- Johnny Soedomo
create table mpp7980
(
 month_id date,
 bill_stmt_id  character varying(30),
 cust_type     character varying(10),
 subscription_status      character varying(30),
 voice_call_min           numeric(15,2),
 minute_per_call          numeric(15,2),
 subscription_id          character varying(15)
)
distributed by (subscription_id, bill_stmt_id)
  PARTITION BY RANGE(month_id)
    (
    start ('2009-02-01'::date) end ('2009-08-01'::date)  exclusive EVERY (INTERVAL '1 month')
    );

insert into mpp7980 values('2009-04-01','xyz','zyz','1',1,1,'1');
insert into mpp7980 values('2009-04-01','zxyz','zyz','2',2,1,'1');
insert into mpp7980 values('2009-03-03','xyz','zyz','4',1,3,'1');


select cust_type, subscription_status,count(distinct subscription_id),sum(voice_call_min),sum(minute_per_call) from mpp7980 where month_id ='2009-04-01' group by rollup(1,2);

drop table mpp7980;
