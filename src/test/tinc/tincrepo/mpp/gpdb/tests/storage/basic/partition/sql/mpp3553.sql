CREATE TABLE mpp3553_rolledup_interactions (
request_id CHAR(32),
bitmask BIT(64),
had_impression BOOLEAN DEFAULT FALSE,
had_clickthrough BOOLEAN DEFAULT FALSE
) WITH (OIDS=FALSE) DISTRIBUTED BY(request_id);


CREATE TABLE mpp3553_impression_ad_potential_fact
(
request_id character(32) NOT NULL,
utc_date date,
date_id integer NOT NULL,
time_id smallint NOT NULL,
app_id smallint NOT NULL,
surface_id smallint NOT NULL,
content_id integer NOT NULL,
bucket_id integer NOT NULL, 
ccid integer DEFAULT 1,
pui character(32),
ip_addr character varying(16),
interaction_bitmask1 bit(64) DEFAULT (0)::bit(64), 
beacon smallint,
ad_type character varying(10) NOT NULL, 
ad_status character(2),
had_impression boolean DEFAULT false, 
had_clickthrough boolean DEFAULT false,
dw_version smallint DEFAULT 1, 
inserted_datetime timestamp with time zone NOT NULL DEFAULT now(),
ref_id integer NOT NULL DEFAULT 0, 
expandable boolean DEFAULT true,
eapv smallint DEFAULT 0
)
PARTITION BY RANGE (utc_date) (
start (date '2008-01-01') end (date '2009-01-01') every (interval '1 month') 
);

DELETE FROM mpp3553_impression_ad_potential_fact
         where  request_id in (select mpp3553_rolledup_interactions.request_id from mpp3553_rolledup_interactions)
             and  utc_date BETWEEN '2008-09-11 05:00:00' AND '2008-09-12 05:00:00';

drop table mpp3553_impression_ad_potential_fact;
drop table mpp3553_rolledup_interactions;
