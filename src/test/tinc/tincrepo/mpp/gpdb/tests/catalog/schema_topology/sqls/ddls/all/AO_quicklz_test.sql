-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

\c ao_db

-- Heap Table 

CREATE TABLE be_heap (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) distributed randomly;

-- Insert values into the Heap Table

insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',5,'value_5');
insert into be_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',generate_series(6,100),repeat('text',100));


-- Select from the Heap Table

select count(*) from be_heap ;

-- AO Table with quicklz compression level 1

CREATE TABLE be_quicklz_1 (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) WITH (appendonly=true, compresslevel=1, compresstype=quicklz) distributed by (transactionid);

-- Insert values into the AO quicklz 1 table

insert into be_quicklz_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into be_quicklz_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into be_quicklz_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into be_quicklz_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into be_quicklz_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',generate_series(5,100),repeat('text',100));

-- Select from AO quicklz 3 table

select count(*) from be_quicklz_1 ;

-- AO Table with quicklz compression level 1

CREATE TABLE be_quicklz1 (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) WITH (appendonly=true, compresslevel=1, compresstype=quicklz) distributed by (transactionid); 

-- Insert values into the AO quicklz 1 table 

insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',5,'value_5');
insert into be_quicklz1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',generate_series(6,100),repeat('text',100));

-- Select from AO quicklz 1 table

select count(*) from be_quicklz1 ;

-- Adding checkpoint
checkpoint;
