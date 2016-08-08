-- start_ignore
drop table if exists accounts cascade;
drop table if exists customers cascade;
drop function if exists tempfunc();
-- end_ignore

create table accounts(aid int not null, cid int, sales bigint) distributed by (cid);
create index accounts_idx on accounts using btree(aid);

insert into accounts(aid, cid, sales) values(1000, 1, 100000000);
insert into accounts(aid, cid, sales) values(1010, 1, 5000);
insert into accounts(aid, cid, sales) values(1020, 2, 1250000000);
insert into accounts(aid, cid, sales) values(1030, 3, 55000100);
insert into accounts(aid, cid, sales) values(2000, 2, 20000);

create table customers(cid int not null, tier text) distributed by (cid);
alter table customers add constraint customers_pkey PRIMARY KEY (cid);

insert into customers(cid) values(1);
insert into customers(cid) values(2);
insert into customers(cid) values(3);
insert into customers(cid) values(4);
insert into customers(cid) values(5);

create function tempfunc()
returns void
as 
$$
declare
   custdata record;
   rowcnt integer;
begin
   create temporary table cust_accounts(custid int, sales bigint, cust_tier text) distributed by (custid); 
   select count(*) into rowcnt from cust_accounts;
   insert into cust_accounts(custid, sales, cust_tier) values (10, 101010101, 'TEST');
   for custdata in select cid as cid, sum(sales) as sales from accounts group by cid order by cid asc 
   loop
      if custdata.sales >= 100000000 then
          insert into cust_accounts(custid, sales, cust_tier) values(custdata.cid, custdata.sales, 'PLATINUM');
      elseif custdata.sales >= 1000000 and custdata.sales < 100000000 then
          insert into cust_accounts(custid, sales, cust_tier) values(custdata.cid, custdata.sales, 'GOLD');
      elseif custdata.sales > 100000 and custdata.sales < 1000000 then
          insert into cust_accounts(custid, sales, cust_tier) values(custdata.cid, custdata.sales, 'SILVER');
      else
          insert into cust_accounts(custid, sales, cust_tier) values(custdata.cid, custdata.sales, 'BRONZE');
      end if;
      update customers c set tier = cust_tier from cust_accounts ca where ca.custid = c.cid;
      drop table if exists cust_accounts;
      create temporary table cust_accounts(custid int, sales bigint, cust_tier text) distributed by (custid); 
   end loop;
end;
$$ language plpgsql;

set gp_plpgsql_clear_cache_always = on; 

select tempfunc();

set gp_plpgsql_clear_cache_always = on; 

