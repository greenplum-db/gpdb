---
--- @tags sanity
---
CREATE TABLE statrepl(a int, b bigint, c text)
    PARTITION BY RANGE(b) (
        START (1) END (10) EVERY (1));

-- each location should not go beyond the previous one,
-- and a record should present while a standby is running.
SELECT
    sent_location >= write_location AS write_follows_sent,
    write_location >= flush_location AS flush_follows_write,
    flush_location >= replay_location AS replay_follows_flush
FROM pg_stat_replication;
