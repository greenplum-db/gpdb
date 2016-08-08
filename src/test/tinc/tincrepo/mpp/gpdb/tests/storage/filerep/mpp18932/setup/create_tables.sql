-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create table ao_part_table (eventdate date, data int, description varchar(256))
        with (appendonly=true, orientation=row) 
       distributed by (eventdate) partition by range (eventdate) 
       (start (date '2011-01-01') end (date '2012-01-02') every (interval '1 day'));

insert into ao_part_table select '2011-01-01', generate_series(1,10000), 'abcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyz';

create table co_part_table (eventdate date, data int, description varchar(256))
        with (appendonly=true, orientation=column) 
       distributed by (eventdate) partition by range (eventdate) 
       (start (date '2011-01-01') end (date '2012-01-02') every (interval '1 day'));

insert into co_part_table select '2011-01-01', generate_series(1,10000), 'abcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyz';

create table part_table (eventdate date, data int, description varchar(256))
        with (appendonly=true, orientation=row) 
       distributed by (eventdate) partition by range (eventdate) 
       (start (date '2011-01-01') end (date '2012-01-02') every (interval '1 day'));

insert into part_table select '2011-01-01', generate_series(1,10000), 'abcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyzabcdefghijklmnopqrstuvwqyz';
