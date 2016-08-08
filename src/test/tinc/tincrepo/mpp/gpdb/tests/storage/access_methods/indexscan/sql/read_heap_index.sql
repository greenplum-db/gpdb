select * from readindex('heap_i1'::regclass) as (ictid tid, hctid tid, aotid text, istatus text, hstatus text, val int) order by ictid;
