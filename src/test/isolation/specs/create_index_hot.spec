#
# Test behavior with HOT and CREATE INDEX.
#
# If CREATE INDEX creates a "broken" HOT chain, the new index must not be
# used by new queries, with an old snapshot, that would need to see the
# old values. See src/backend/access/heap/README.HOT.
#
# This is enforced by setting indcheckxmin in the pg_index row. In GPDB,
# we use the pg_index row in the master for planning, but all the data is
# stored in the segments, so indcheckxmin must be set in the master, if
# it's set in any of the segments.
setup
{
    create table hot (dummy int, a int, b char, c char);
    create index idx_a on hot (a);
    insert into hot values (1,1,'A','#');
}

teardown
{
    drop table hot;
}

# Update a row, and create an index on the updated column. This produces
# a broken HOT chain.
session "s1"
step "s1update" { update hot set c = '$' where c = '#'; }
step "s1createindexonc" { create index idx_c on hot (c); }

session "s2"
step "s2begin"  { BEGIN ISOLATION LEVEL SERIALIZABLE; }
step "s2select" { select '#' as expected, c from hot where c = '#'
                  union all
                  select '$', c from hot where c = '$'; }
step "s2forceindexscan" { set enable_indexscan=on; set enable_seqscan=off; }
teardown        { abort; }

permutation
  "s2begin"
  "s2select"

  "s1update"
  "s1createindexonc"

  "s2select"
  "s2forceindexscan"
  "s2select"
