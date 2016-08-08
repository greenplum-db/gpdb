-- start_ignore
drop table if exists dcl_messaging_test cascade;
-- end_ignore

create table dcl_messaging_test
(
        message_create_date     timestamp(3) not null,
        trace_socket            varchar(1024) null,
        trace_count             varchar(1024) null,
        variable_01             varchar(1024) null,
        variable_02             varchar(1024) null,
        variable_03             varchar(1024) null,
        variable_04             varchar(1024) null,
        variable_05             varchar(1024) null,
        variable_06             varchar(1024) null,
        variable_07             varchar(1024) null,
        variable_08             varchar(1024) null,
        variable_09             varchar(1024) null,
        variable_10             varchar(1024) null,
        variable_11             varchar(1024) null,
        variable_12             varchar(1024) null,
        variable_13             varchar(1024) default('-1'),
        variable_14             varchar(1024) null,
        variable_15             varchar(1024) null,
        variable_16             varchar(1024) null,
        variable_17             varchar(1024) null,
        variable_18             varchar(1024) null,
        variable_19             varchar(1024) null,
        variable_20             varchar(1024) null,
        variable_21             varchar(1024) null,
        variable_22             varchar(1024) null,
        variable_23             varchar(1024) null,
        variable_24             varchar(1024) null,
        variable_25             varchar(1024) null,
        variable_26             varchar(1024) null,
        variable_27             varchar(1024) null,
        variable_28             varchar(1024) null,
        variable_29             varchar(1024) null,
        variable_30             varchar(1024) null,
        variable_31             varchar(1024) null,
        variable_32             varchar(1024) null,
        variable_33             varchar(1024) null,
        variable_34             varchar(1024) null,
        variable_35             varchar(1024) null,
        variable_36             varchar(1024) null,
        variable_37             varchar(1024) null,
        variable_38             varchar(1024) null,
        variable_39             varchar(1024) null,
        variable_40             varchar(1024) null,
        variable_41             varchar(1024) null,
        variable_42             varchar(1024) null,
        variable_43             varchar(1024) null,
        variable_44             varchar(1024) null,
        variable_45             varchar(1024) null,
        variable_46             varchar(1024) null,
        variable_47             varchar(1024) null,
        variable_48             varchar(1024) null,
        variable_49             varchar(1024) null,
        variable_50             varchar(1024) null,
        variable_51             varchar(1024) null,
        variable_52             varchar(1024) null,
        variable_53             varchar(1024) null,
        variable_54             varchar(1024) null,
        variable_55             varchar(1024) null,
        variable_56             varchar(1024) null,
        variable_57             varchar(1024) null,
        variable_58             varchar(1024) null,
        variable_59             varchar(1024) null,
        variable_60             varchar(1024) null
)
distributed by (message_create_date)
partition by range (message_create_date)
(
    START (timestamp '2011-09-01') END (timestamp '2011-09-15') EVERY (interval '1 day'),
    DEFAULT PARTITION outlying_dates
);
-- partial index
create index dcl_messaging_test_index13 on dcl_messaging_test(variable_13) where message_create_date > '2011-09-02';
-- expression index
create index dcl_messaging_test_index16 on dcl_messaging_test(upper(variable_16));
alter table dcl_messaging_test drop default partition;

-- ADD case
alter table dcl_messaging_test add partition start (timestamp '2011-09-15') inclusive end (timestamp '2011-09-16') exclusive;

-- EXCHANGE case
--start_ignore
drop table if exists dcl_candidate;
--end_ignore

create table dcl_candidate(like dcl_messaging_test) with (appendonly=true);
insert into dcl_candidate(message_create_date) values (timestamp '2011-09-06');
alter table dcl_messaging_test exchange partition for ('2011-09-06') with table dcl_candidate;


-- SPLIT case
alter table dcl_messaging_test split partition for (timestamp '2011-09-06') at (timestamp '2011-09-06 12:00:00') into (partition x1, partition x2);

