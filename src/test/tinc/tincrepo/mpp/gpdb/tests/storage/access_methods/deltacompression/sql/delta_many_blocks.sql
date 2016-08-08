-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

-- Many blocks with delta compression

-- start_ignore
Drop table if exists delta_blocks;
-- end_ignore

Create table delta_blocks(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1,blocksize=8192);


Create index dl_bt_ix on delta_blocks using bitmap(a1);

Create index dl_ix on delta_blocks(a3);

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

Insert into delta_blocks select
    i/5, i/5, date '2012-02-02' + i/5,
    case when i/10=1 then time '20:13:14.343536' when i/10=2 then time '20:13:11.232421' when i/10=3 then time '20:12:13.241122' else '20:02:03' end,
    case when i/10=3 then timestamp '2012-07-30 11:13:58.356229' when i/10=2 then timestamp '2012-07-30 11:13:44.351129' else  timestamp '2012-07-30 11:00:33.778899' end,
    case when i/10=2 then timestamp '2014-07-22 14:12:23.776892-07' when i/10=3 then timestamp '2014-07-22 14:12:13.006892-07'
    else timestamp '2014-07-22 14:00:00.333892-07' end
    from generate_series(1,100000)i;

\d+ delta_blocks


Select a1,a2,a6 from delta_blocks where a4 = '20:13:11.232421' order by a1 limit 10;



