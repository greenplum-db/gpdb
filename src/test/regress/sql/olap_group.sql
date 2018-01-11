--
-- OLAP_GROUP - Test OLAP GROUP BY extensions
--

-- Syntactic equivalents --

--start_equiv
select count(*) from sale;
select count(*) from sale group by ();
--end_equiv

--start_equiv
select cn, count(*) from sale group by cn;
select cn, count(*) from sale group by (), cn;
select cn, count(*) from sale group by cn, ();
--end_equiv

--start_equiv
select cn, vn, count(*) from sale group by cn, vn;
select cn, vn, count(*) from sale group by (), cn, vn;
select cn, vn, count(*) from sale group by cn, (), vn;
select cn, vn, count(*) from sale group by cn, vn, ();
select cn, vn, count(*) from sale group by (), cn, (), vn, ();
--end_equiv


-- Semantic equivalents --

--start_equiv
select cn, vn, pn, sum(qty*prc) from sale group by cn, vn, pn
union all
select cn, vn, null, sum(qty*prc) from sale group by cn, vn
union all
select cn, null, null, sum(qty*prc) from sale group by cn
union all
select null, null, null, sum(qty*prc) from sale;    
select cn, vn, pn, sum(qty*prc) from sale group by rollup(cn,vn,pn);
select cn, vn, pn, sum(qty*prc) from sale group by grouping sets((), (cn), (cn,vn), (cn,vn,pn));
select cn, vn, pn, sum(qty*prc) from sale group by grouping sets((cn,vn), (), (cn,vn,pn), (cn));
--end_equiv

--start_equiv
select cn, vn, pn, sum(qty*prc) from sale group by cn, vn, pn
union all
select cn, vn, null, sum(qty*prc) from sale group by cn, vn
union all
select cn, null, null, sum(qty*prc) from sale group by cn
union all
select null, null, null, sum(qty*prc) from sale
union all
select cn, null, pn, sum(qty*prc) from sale group by cn, pn
union all
select null, vn, pn, sum(qty*prc) from sale group by vn, pn
union all
select null, vn, null, sum(qty*prc) from sale group by vn
union all
select null, null, pn, sum(qty*prc) from sale group by pn;
select cn, vn, pn, sum(qty*prc) from sale group by cube (cn, vn, pn);
select cn, vn, pn, sum(qty*prc) from sale group by grouping sets ((), (cn), (vn), (pn), (cn,vn), (cn,pn), (vn,pn), (cn,vn,pn));
--end_equiv

-- start_equiv
select cn, vn, pn, count(distinct dt) from sale group by cn, vn, pn
union all
select cn, vn, null, count(distinct dt) from sale group by cn, vn
union all
select cn, null, null, count(distinct dt) from sale group by cn
union all
select null, null, null, count(distinct dt) from sale;    --mvd 1,2,3->4
select cn, vn, pn, count(distinct dt) from sale group by rollup(cn,vn,pn);--mvd 1,2,3->4
select cn, vn, pn, count(distinct dt) from sale group by grouping sets((), (cn), (cn,vn), (cn,vn,pn));--mvd 1,2,3->4
-- end_equiv

--start_equiv order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by cn, vn, pn
union all
select cn, vn, null, count(distinct dt) from sale group by cn, vn
union all
select cn, null, null, count(distinct dt) from sale group by cn
union all
select null, null, null, count(distinct dt) from sale
union all
select cn, null, pn, count(distinct dt) from sale group by cn, pn
union all
select null, vn, pn, count(distinct dt) from sale group by vn, pn
union all
select null, vn, null, count(distinct dt) from sale group by vn
union all
select null, null, pn, count(distinct dt) from sale group by pn
order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by cube (cn, vn, pn) order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by grouping sets ((), (cn), (vn), (pn), (cn,vn), (cn,pn), (vn,pn), (cn,vn,pn)) order by 1,2,3; -- order 1,2,3
--end_equiv
--start_equiv order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by cn, vn, pn
union all
select cn, vn, null, sum(qty*prc) from sale group by cn, vn
union all
select cn, null, null, sum(qty*prc) from sale group by cn
union all
select null, null, null, sum(qty*prc) from sale 
order by 1,2,3; -- order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by rollup(cn,vn,pn) order by 1,2,3; -- order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by grouping sets((), (cn), (cn,vn), (cn,vn,pn)) order by 1,2,3; -- order 1,2,3
--end_equiv

--start_equiv order 3,4,1
select pn, sum(qty*prc), cn, vn from sale group by cn, vn, pn
union all
select null, sum(qty*prc), cn, vn from sale group by cn, vn
union all
select null, sum(qty*prc), cn, null from sale group by cn
union all
select null, sum(qty*prc), null, null from sale 
order by cn, vn, pn; -- order 3,4,1
select pn, sum(qty*prc), cn, vn from sale group by rollup(cn,vn,pn) order by cn, vn, pn; -- order 3,4,1
select pn, sum(qty*prc), cn, vn from sale group by grouping sets((), (cn), (cn,vn), (cn,vn,pn)) order by cn, vn, pn; -- order 3,4,1
--end_equiv

--start_equiv order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by cn, vn, pn
union all
select cn, vn, null, sum(qty*prc) from sale group by cn, vn
union all
select cn, null, null, sum(qty*prc) from sale group by cn
union all
select null, null, null, sum(qty*prc) from sale
union all
select cn, null, pn, sum(qty*prc) from sale group by cn, pn
union all
select null, vn, pn, sum(qty*prc) from sale group by vn, pn
union all
select null, vn, null, sum(qty*prc) from sale group by vn
union all
select null, null, pn, sum(qty*prc) from sale group by pn 
order by 1,2,3; -- order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by cube (cn, vn, pn) order by 1,2,3; -- order 1,2,3
select cn, vn, pn, sum(qty*prc) from sale group by grouping sets ((), (cn), (vn), (pn), (cn,vn), (cn,pn), (vn,pn), (cn,vn,pn)) order by 1,2,3; -- order 1,2,3
--end_equiv

-- start_ignore
-- GPDB_84_MERGE_FIXME: ORCA started supporting this feature, investigate why
set optimizer = off;
-- end_ignore

-- ***BUG*** The extended groupings aren't correctly ordered! Maybe they wrongly parallel sorted!
--start_equiv order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by cn, vn, pn
union all
select cn, vn, null, count(distinct dt) from sale group by cn, vn
union all
select cn, null, null, count(distinct dt) from sale group by cn
union all
select null, null, null, count(distinct dt) from sale 
order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by rollup(cn,vn,pn) order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by grouping sets((), (cn), (cn,vn), (cn,vn,pn)) order by 1,2,3; -- order 1,2,3
--end_equiv

--start_equiv order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by cn, vn, pn
union all
select cn, vn, null, count(distinct dt) from sale group by cn, vn
union all
select cn, null, null, count(distinct dt) from sale group by cn
union all
select null, null, null, count(distinct dt) from sale
union all
select cn, null, pn, count(distinct dt) from sale group by cn, pn
union all
select null, vn, pn, count(distinct dt) from sale group by vn, pn
union all
select null, vn, null, count(distinct dt) from sale group by vn
union all
select null, null, pn, count(distinct dt) from sale group by pn 
order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by cube (cn, vn, pn) order by 1,2,3; -- order 1,2,3
select cn, vn, pn, count(distinct dt) from sale group by grouping sets ((), (cn), (vn), (pn), (cn,vn), (cn,pn), (vn,pn), (cn,vn,pn)) order by 1,2,3; -- order 1,2,3
--end_equiv
-- start_ignore
reset optimizer;
-- end_ignore

-- Ordinary Grouping Set Specifications --

select cn, count(*) from sale group by cn;
select cn, count(*) from sale group by (cn);

select cn,pn,count(*) from sale group by cn,pn;
select cn,pn,count(*) from sale group by (cn),pn;
select cn,pn,count(*) from sale group by cn,(pn);
select cn,pn,count(*) from sale group by (cn),(pn);

select cn+vn as a, vn+pn as b, count(*) from sale group by (cn+vn), (vn+pn);
select cn+vn as a, vn+pn as b, count(*) from sale group by (1), (2);
select cn+vn as a, vn+pn as b, count(*) from sale group by (a), (b);
select cn+vn as a, vn+pn as b, count(*) from sale group by a, b;
select cn+vn as a, vn+pn as b, count(*) from sale group by (cn+vn, vn+pn);
select cn+vn as a, vn+pn as b, count(*) from sale group by (1, 2);
select cn+vn as a, vn+pn as b, count(*) from sale group by (a, b);

select cn,vn,pn,count(*) from sale group by cn,(vn,pn);

select count(*) from sale group by rollup((cn,vn),(pn,dt));


-- Distinguish grouping value NULLs from summarization NULLs --

create table olap_tmp_for_group (a int, b int, c int);

insert into olap_tmp_for_group values (1,1,1),(1,2,1),(1,2,1),(2,1,1),(2,2,1);

--start_equiv
select * from olap_tmp_for_group;
select * from (values (1,1,1),(1,2,1),(1,2,1),(2,1,1),(2,2,1)) r(a,b,c);
--end_equiv

--start_equiv
select a,b,sum(c) from olap_tmp_for_group group by rollup(a,b);
select * from (values (1,1,1), (1,2,2), (1,null,3), (2,1,1), (2,2,1),(2,null,2),(null,null,5)) r(a,b,"sum");
--end_equiv

insert into olap_tmp_for_group values (1,null,1);

--start_equiv
select a,b,sum(c) from olap_tmp_for_group group by rollup(a,b);
select * from (values (1,1,1), (1,2,2), (1,null,1), (1,null,4), (2,1,1), (2,2,1), (2,null,2), (null,null,6)) r(a,b,"sum");
--end_equiv

insert into olap_tmp_for_group values (null,null,1);

--start_equiv
select a,b,sum(c) from olap_tmp_for_group group by rollup(a,b);
select * from (values (1,1,1), (1,2,2), (1,null,1), (1,null,4), (2,1,1), (2,2,1), (2,null,2), (null,null,1), (null,null,1), (null,null,7)) r(a,b,"sum");
--end_equiv

drop table olap_tmp_for_group; --ignore

-- Grouping extension combination
--start_equiv
select cn,vn,sum(qty) from sale group by cn,vn union all
select cn,null,sum(qty) from sale group by cn union all
select cn,null,sum(qty) from sale group by cn union all
select null,null,sum(qty) from sale;
select cn,vn,sum(qty) from sale group by grouping sets (rollup(cn,vn), cn);
--end_equiv

--start_equiv
select cn,vn,sum(qty) from sale group by cn,vn union all
select cn,null,sum(qty) from sale group by cn union all
select null,null,sum(qty) from sale;
select cn,vn,sum(qty) from sale group by grouping sets (rollup(cn,vn));
--end_equiv

--start_equiv
select cn,null as vn,sum(qty) from sale group by cn union all
select null as cn,vn,sum(qty) from sale group by vn;
select cn,vn,sum(qty) from sale group by grouping sets((cn),(vn));
--end_equiv

-- GROUPING function -- 

select grouping(cn,vn,pn) from sale;
select cn,vn,pn,grouping(cn,vn,pn) from sale group by cn,vn,pn;
select cn,vn,pn,grouping(cn,vn,pn) from sale group by rollup(cn),vn,pn;
select cn,vn,pn,grouping(cn,vn,pn) from sale group by rollup(cn,vn), pn;
select cn,vn,pn,grouping(cn,vn,pn) from sale group by rollup(cn,vn,pn);
select cn,vn,pn,grouping(cn,vn,pn) from sale group by cn, rollup(vn,pn);
select cn,vn,pn,grouping(cn,vn,pn) from sale group by vn, rollup(cn, pn);
select grouping(cn), grouping(vn), grouping(pn), cn, vn, pn, count(*) from sale group by rollup(cn,vn,pn);
select grouping(cn,vn,pn) from sale group by rollup(cn,vn,pn) order by 1 desc; -- order 1
select grouping(cn), grouping(vn), grouping(pn) from sale group by rollup(cn,vn,pn) order by 1 desc, 2 desc, 3 desc; -- order 1,2,3
select cn+vn,pn, grouping(cn+vn,pn), count(*) from sale group by rollup(cn+vn,pn);

--start_equiv
select cn,vn,0 as grouping from sale group by cn,vn
union all select cn,null,1 as grouping from sale group by cn
union all (select null,null,3 as grouping from sale limit 1)
union all select null,vn,2 as grouping from sale group by vn;
select cn,vn,grouping(cn,vn) from sale group by cube(cn,vn);
select cn,vn,grouping(cn,vn) from sale group by cube(vn,cn);
select cn,vn,grouping(cn,vn) from sale group by grouping sets ((vn,cn), (vn), (cn), ());
select cn,vn,grouping(cn,vn) from sale group by grouping sets ((),(vn,cn), (vn), (cn));
--end_equiv

--start_equiv
select cn,vn,0 as grouping from sale group by cn,vn
union all select cn,null,2 as grouping from sale group by cn
union all (select null,null,3 as grouping from sale limit 1)
union all select null,vn,1 as grouping from sale group by vn;
select cn,vn,grouping(vn,cn) from sale group by cube(cn,vn);
select cn,vn,grouping(vn,cn) from sale group by cube(vn,cn);
select cn,vn,grouping(vn,cn) from sale group by grouping sets ((vn,cn), (vn), (cn), ());
select cn,vn,grouping(vn,cn) from sale group by grouping sets ((vn), (cn), (), (cn,vn));
--end_equiv

--start_equiv
select cn,dt,0 as grouping from sale group by cn,dt
union all select cn,null,1 as grouping from sale group by cn
union all (select null,null,3 as grouping from sale limit 1)
union all select null,dt,2 as grouping from sale group by dt;
select cn,dt,grouping(cn,dt) from sale group by cube(cn,dt);
select cn,dt,grouping(cn,dt) from sale group by cube(dt,cn);
--end_equiv

--start_equiv
select cn,dt,count(vn),0 as grouping from sale group by cn,dt
union all select cn,null,count(vn),1 as grouping from sale group by cn
union all (select null,null,count(vn),3 as grouping from sale limit 1)
union all select null,dt,count(vn),2 as grouping from sale group by dt;
select cn,dt,count(vn),grouping(cn,dt) from sale group by cube(cn,dt);
select cn,dt,count(vn),grouping(cn,dt) from sale group by cube(dt,cn);
--end_equiv

--start_equiv
select cn,sum(qty) from sale group by prc,cn
union all select null,sum(qty) from sale group by prc
union all select cn,sum(qty) from sale group by cn
union all select null,sum(qty) from sale;
select cn,sum(qty) from sale group by cube(prc,cn);
select cn,sum(qty) from sale group by grouping sets (cube(prc,cn));
--end_equiv

--start_equiv
select cn,sum(distinct qty) from sale group by prc,cn
union all select null,sum(distinct qty) from sale group by prc
union all select cn,sum(distinct qty) from sale group by cn
union all select null,sum(distinct qty) from sale;
select cn,sum(distinct qty) from sale group by cube(prc,cn);
select cn,sum(distinct qty) from sale group by grouping sets (cube(prc,cn));
--end_equiv

-- Ungrouped attributes in GROUPING function --

select grouping(cn,vn,pn) from sale group by cn; --error
select grouping(cn,vn,pn) from sale group by rollup(cn); --error

-- Using in View --

create view cube_view as select cn,vn,grouping(vn,cn) from sale group by cube(cn,vn);
\d+ cube_view;
create view rollup_view as select cn,vn,pn,grouping(cn,vn,pn) from sale group by rollup(cn),vn,pn;
\d+ rollup_view;
create view gs_view as select cn,vn,grouping(vn,cn) from sale group by grouping sets ((vn), (cn), (), (cn,vn));
\d+ gs_view;

-- GROUP_ID function --

select pn, sum(qty), group_id() from sale group by rollup(pn);
select pn, sum(qty), group_id() from sale group by rollup(pn),pn;

--start_equiv
select pn, sum(qty), 0 from sale group by cn,pn
union all select pn, sum(qty), 1 from sale group by cn,pn
union all select pn, sum(qty), 2 from sale group by cn,pn
union all select pn, sum(qty), 3 from sale group by cn,pn
union all select pn, sum(qty), 4 from sale group by cn,pn
union all select pn, sum(qty), 5 from sale group by cn,pn
union all select pn, sum(qty), 6 from sale group by cn,pn
union all select null, sum(qty), 0 from sale group by cn
union all select null, sum(qty), 1 from sale group by cn
union all select null, sum(qty), 2 from sale group by cn
union all select pn, sum(qty), 0 from sale group by pn
union all select null, sum(qty), 0 from sale;
select pn, sum(qty), group_id() from sale group by rollup(cn,pn), cube(cn,pn);
--end_equiv

-- Having clause --

--start_equiv
select cn,sum(qty) from sale group by cn,vn having vn=10;
select cn,sum(qty) from sale group by rollup(cn,vn) having vn=10;
--end_equiv

--start_equiv
select cn, vn, pn, count(dt) from sale group by cn, vn, pn having count(dt) <=2
union all
select cn, vn, null, count(dt) from sale group by cn, vn having count(dt) <=2
union all
select cn, null, null, count(dt) from sale group by cn having count(dt) <=2
union all
select null, null, null, count(dt) from sale having count(dt) <=2;    
select cn, vn, pn, count(dt) from sale group by rollup (cn,vn,pn) having count(dt) <=2;
--end_equiv

--start_equiv
select cn, vn, pn, count(distinct dt) from sale group by cn, vn, pn having count(distinct dt) <=2
union all
select cn, vn, null, count(distinct dt) from sale group by cn, vn having count(distinct dt) <=2
union all
select cn, null, null, count(distinct dt) from sale group by cn having count(distinct dt) <=2
union all
select null, null, null, count(distinct dt) from sale having count(distinct dt) <=2;    
select cn, vn, pn, count(distinct dt) from sale group by rollup (cn,vn,pn) having count(distinct dt) <=2;
--end_equiv

--start_equiv
select cn,dt,count(vn),0 as grouping from sale group by cn,dt having count(vn) > 2
union all select cn,null,count(vn),1 as grouping from sale group by cn having count(vn) > 2
union all (select null,null,count(vn),3 as grouping from sale  having count(vn) > 2 limit 1)
union all select null,dt,count(vn),2 as grouping from sale group by dt having count(vn) > 2;
select cn,dt,count(vn),grouping(cn,dt) from sale group by cube(cn,dt) having count(vn) > 2;
select cn,dt,count(vn),grouping(cn,dt) from sale group by cube(dt,cn) having count(vn) > 2;
--end_equiv

--start_equiv
select cn,dt,count(distinct vn),0 as grouping from sale group by cn,dt having count(distinct vn) > 2
union all select cn,null,count(distinct vn),1 as grouping from sale group by cn having count(distinct vn) > 2
union all (select null,null,count(distinct vn),3 as grouping from sale  having count(distinct vn) > 2 limit 1)
union all select null,dt,count(distinct vn),2 as grouping from sale group by dt having count(distinct vn) > 2;
select cn,dt,count(distinct vn),grouping(cn,dt) from sale group by cube(cn,dt) having count(distinct vn) > 2;
select cn,dt,count(distinct vn),grouping(cn,dt) from sale group by cube(dt,cn) having count(distinct vn) > 2;
--end_equiv

--start_equiv
select cn, null as vn, null as pn, count(dt), 1 as grouping from sale group by cn;
select cn, vn, pn, count(dt),grouping(cn,vn) from sale group by rollup (cn,vn,pn) having grouping(cn,vn)=1;
--end_equiv

--start_equiv
select cn, vn, pn, count(dt) from sale group by cn,vn,pn
union all select cn, null, null, count(dt) from sale group by cn;
select cn, vn, pn, count(dt) from sale group by grouping sets ((cn,vn,pn), (cn));
select cn, vn, pn, count(dt) from sale group by grouping sets ((cn,vn,pn), (cn), (cn), (cn))
having group_id()=0;
--end_equiv

--start_equiv
select cn,vn,pn,count(dt),0,0 as grouping from sale group by cn,vn,pn
union all select cn,null,null,count(dt),0,3 from sale group by cn
union all select cn,null,null,count(dt),1,3 from sale group by cn
order by 6; -- order 6
select cn, vn, pn, count(dt),group_id(),grouping(cn,vn,pn) from sale group by grouping sets ((cn,vn,pn), (cn), (cn)) order by grouping(cn,vn,pn); -- order 6
--end_equiv

select a,b from (select 1 as a , 2 as b) r(a,b) group by rollup(a,b);

-- tests for known bugs
select dt,pn,cn,GROUP_ID(), count(prc) FROM sale GROUP BY ROLLUP((dt)),ROLLUP((cn)),ROLLUP((vn)),ROLLUP((pn),(cn));

--start_equiv
select vn,cn,dt,0,REGR_COUNT(prc*qty,prc*qty) FROM sale GROUP BY pn,qty,vn,cn,dt
union all select vn,cn,dt,1,REGR_COUNT(prc*qty,prc*qty) FROM sale GROUP BY pn,qty,vn,cn,dt;
select vn,cn,dt,GROUP_ID(), REGR_COUNT(prc*qty,prc*qty) FROM sale GROUP BY (pn,qty),(vn),ROLLUP((qty)),cn,dt;
--end_equiv
SELECT cn,pn,GROUPING(cn),GROUP_ID(), SUM(qty) FROM sale GROUP BY ROLLUP((cn,prc)),(cn,prc,dt),(pn,qty,vn) HAVING GROUP_ID() < 0 AND STDDEV_SAMP(pn) = 9.23708093366322 ORDER BY pn,cn desc;

--start_equiv
SELECT dt,GROUPING(dt), MAX(DISTINCT prc*qty) FROM sale GROUP BY (dt,vn,vn),ROLLUP((qty,cn),(vn),(vn),(prc));
select dt,0 as grouping, max(distinct prc*qty) from sale group by dt,vn,qty,cn,prc
union all select dt,0, max(distinct prc*qty) from sale group by dt,vn,qty,cn
union all select dt,0, max(distinct prc*qty) from sale group by dt,vn,qty,cn
union all select dt,0, max(distinct prc*qty) from sale group by dt,vn,qty,cn
union all select dt,0, max(distinct prc*qty) from sale group by dt,vn;
--end_equiv

--start_equiv
select sale.cn,max(distinct sale.cn) from sale,vendor where sale.vn=vendor.vn group by sale.cn,sale.vn
union all select sale.cn,max(distinct sale.cn) from sale,vendor where sale.vn=vendor.vn group by sale.cn
union all select null,max(distinct sale.cn) from sale,vendor where sale.vn=vendor.vn
union all select null,max(distinct sale.cn) from sale,vendor where sale.vn=vendor.vn group by sale.vn;
select sale.cn,max(distinct sale.cn) from sale,vendor where sale.vn=vendor.vn group by cube(sale.cn,sale.vn);
--end_equiv

--start_equiv
select cn,sum(qty) from sale group by cn;
select cn,sum(qty) from sale group by grouping sets(cn);
--end_equiv
select cn,sum(qty),grouping((cn,vn),vn) from sale group by rollup(cn,vn);
select sum(qty),grouping(cn+pn) from sale group by cn+vn,vn;
select sum(qty),grouping(cn+vn) from sale group by rollup(cn+vn,vn);

select * from sale where exists (select cn, sum(qty) from sale group by rollup(cn,vn) having sum(qty)=10000000);

create sequence newseq start 1;
create view v7 as select nextval('newseq');
select sum(nextval) from v7 group by rollup(nextval);

-- MPP-1858
SELECT sale.vn,sale.qty,sale.cn,sale.vn,sale.prc,MIN(floor(sale.qty))
FROM sale
GROUP BY CUBE((sale.cn,sale.prc),(sale.vn),(sale.cn,sale.cn),(sale.vn,sale.qty)),
ROLLUP((sale.cn,sale.pn),(sale.prc,sale.qty),(sale.cn));
select pn,prc,cn,vn,sum(qty) from sale group by grouping sets (rollup(pn,prc), rollup(cn,vn));
SELECT sale.vn,sale.pn,GROUP_ID(),COUNT(floor(sale.vn+sale.cn)) FROM sale
GROUP BY (),GROUPING SETS(CUBE((sale.qty,sale.qty,sale.qty),(sale.pn,sale.pn),(sale.cn,sale.pn,sale.prc),(sale.cn,sale.vn,sale.vn)),
CUBE((sale.cn,sale.qty),(sale.vn,sale.prc,sale.qty),(sale.vn),(sale.vn,sale.dt),(sale.pn),(sale.qty,sale.pn)),CUBE((sale.qty,sale.dt))) 
ORDER BY sale.vn asc,sale.pn desc,sale.vn asc,sale.vn asc; --mvd 1,2->3

-- Empty sets
select * from sale group by ();
select * from sale group by grouping sets(());
select * from sale group by grouping sets((), ());

select count(*) from sale group by ();
select count(*) from sale group by grouping sets(());
select count(*) from sale group by grouping sets((), ());

-- MPP-2312
create view grp_v1 as select count(*) from sale group by ();
create view grp_v2 as select count(*) from sale group by grouping sets(());
create view grp_v3 as select count(*) from sale group by grouping sets((), ());
\d+ grp_v1;
\d+ grp_v2;
\d+ grp_v3;
drop view grp_v1;
drop view grp_v2;
drop view grp_v3;

select cn,group_id(), qty, sale.pn, prc, vn, to_char(coalesce(sum(distinct qty*prc), 0), '999999.9999')
from sale, product
where sale.pn=product.pn
group by grouping sets ((qty, sale.pn, prc, cn,vn), (qty,sale.pn,prc,vn,cn), (sale.qty, prc, cn)); --mvd 1,3,4,5,6->7

select sale.pn, count(distinct prc), count(distinct vn), grouping(sale.pn)
from sale, product where sale.pn = product.pn group by rollup(sale.pn); --mvd 1,2->3
select sale.pn, count(distinct prc), count(distinct vn), sum(prc) + grouping(sale.pn)
from sale, product where sale.pn = product.pn group by rollup(sale.pn); --mvd 1->3
select sale.pn, count(distinct prc), count(distinct vn), sum(prc) + log(sale.pn) + grouping(sale.pn)
from sale, product where sale.pn = product.pn group by rollup(sale.pn); --mvd 1->3

SELECT group_id(), sale.pn, sale.qty, sale.cn, sale.prc, STDDEV(sale.prc*sale.cn),MIN(DISTINCT sale.prc*sale.vn) from sale
GROUP BY ROLLUP((sale.qty,sale.cn),(sale.qty,sale.pn),(sale.vn),(sale.prc)),ROLLUP((sale.dt,sale.pn,sale.qty)); --mvd 2,3,4,5->6

-- MPP-6756
drop table r6756 cascade; --ignore
drop table s6756 cascade; --ignore

create table r6756 ( a int, b int, x int, y int ) 
    distributed randomly
    partition by list(a) (
        values (0), 
        values (1)
        );

create table s6756 ( c int, d int, e int ) 
    distributed randomly;

insert into s6756 values 
    (0,0,0),(0,0,1),(0,1,0),(0,1,1),(1,0,0),(1,0,1),(1,1,0),(1,1,1);

insert into r6756 values
    (0, 0, 1, 1),
    (0, 1, 2, 2),
    (0, 1, 2, 2),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4);

--start_equiv
select a, b, count(distinct x), count(distinct y)
from r6756 r, s6756 c, s6756 d, s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by a, b
union all
select a, null, count(distinct x), count(distinct y)
from r6756 r, s6756 c, s6756 d, s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by a
union all
select null, null, count(distinct x), count(distinct y)
from r6756 r, s6756 c, s6756 d, s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e;

select a, b, count(distinct x), count(distinct y)
from r6756 r, s6756 c, s6756 d, s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by rollup (a,b);
--end_equiv

--start_equiv
select a, b, array_agg((a+1)*x order by x), array_agg((b+1)*y order by y)
from r6756 r
group by a, b
union all
select a, null, array_agg((a+1)*x order by x), array_agg((b+1)*y order by y)
from r6756 r
group by a
union all
select null, null, array_agg((a+1)*x order by x), array_agg((b+1)*y order by y)
from r6756 r
order by 1,2;

select a, b, array_agg((a+1)*x order by x), array_agg((b+1)*y order by y)
from r6756 r
group by rollup (a,b)
order by 1,2;
--end_equiv

-- Test join between a partitioned table and non-partitioned table (MPP-20083)
SELECT COUNT(*) 
FROM r6756 as r, s6756 as c, s6756 as d, s6756 as e
WHERE r.a = c.c AND r.a = d.d AND r.a = e.e;

drop table r6756 cascade; --ignore
drop table s6756 cascade; --ignore

-- begin MPP-14021
select sum((select prc from sale where cn = s.cn and vn = s.vn and pn = s.pn)) from sale s;
-- end MPP-14021

-- begin MPP-14125: if prelim function is missing, do not choose hash agg.
create temp table mpp14125 as select repeat('a', a) a, a % 10 b from generate_series(1, 100)a;
-- start_ignore
-- GPDB_84_MERGE_FIXME: ORCA started supporting this feature, investigate why
set optimizer = off;
-- end_ignore
explain select string_agg(a) from mpp14125 group by b;
-- start_ignore
reset optimizer;
-- end_ignore
-- end MPP-14125

-- Test COUNT in a subquery
create table prod_agg (sale integer, prod varchar);
insert into prod_agg values
  (100, 'shirts'),
  (200, 'pants'),
  (300, 't-shirts'),
  (400, 'caps'),
  (450, 'hats');

create table cust_agg (cusname varchar, sale integer, prod varchar);
insert into cust_agg values
  ('aryan', 100, 'shirts'),
  ('jay', 200, 'pants'),
  ('mahi', 300, 't-shirts'),
  ('nitu', 400, 'caps'),
  ('verru', 450, 'hats');

-- return customer name from cust_agg with count of prod_agg table 
select cusname,(select count(*) from prod_agg) from cust_agg;

-- clean up
drop table prod_agg;
drop table cust_agg;


-- Misc GROUPING SETS tests
select x,y,count(*), grouping(x), grouping(y),grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by cube(x,y);
select x,y,count(*), grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by grouping sets((x,y),(x),(y),());
select x,y,count(*), grouping(x,y) from generate_series(1,2) x, generate_series(1,2) y group by cube(x,y);

create table test_gsets (i int, n numeric);
insert into test_gsets values (0, 0), (0, 1), (0,2);
select i,n,count(*), grouping(i), grouping(n), grouping(i,n) from test_gsets group by grouping sets((), (i,n)) having n is null;

select x, y, count(*), grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by grouping sets(x,y) having x is not null;
