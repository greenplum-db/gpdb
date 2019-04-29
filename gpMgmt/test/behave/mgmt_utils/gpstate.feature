@gpstate
Feature: gpstate tests

    Scenario: gpstate -b logs cluster for a cluster where the mirrors failed over to primary
        Given a standard local demo cluster is running
        And the database is running
        When user stops all primary processes
        And user can start transactions
        And the user runs "gpstate -b"
        Then gpstate output has rows with keys values
            | Master instance                                         = Active                       |
            | Master standby                                          =                              |
            | Standby master state                                    = Standby host passive         |
            | Total segment instance count from metadata              = 6                            |
            | Primary Segment Status                                                                 |
            | Total primary segments                                  = 3                            |
            | Total primary segment valid \(at master\)               = 0                            |
            | Total primary segment failures \(at master\)            = 3 .* <<<<<<<<                |
            | Total number of postmaster.pid files missing            = 3 .* <<<<<<<<                |
            | Total number of postmaster.pid files found              = 0                            |
            | Total number of postmaster.pid PIDs missing             = 3 .* <<<<<<<<                |
            | Total number of postmaster.pid PIDs found               = 0                            |
            | Total number of /tmp lock files missing                 = 3 .* <<<<<<<<                |
            | Total number of /tmp lock files found                   = 0                            |
            | Total number postmaster processes missing               = 3 .* <<<<<<<<                |
            | Total number postmaster processes found                 = 0                            |
            | Mirror Segment Status                                                                  |
            | Total mirror segments                                   = 3                            |
            | Total mirror segment valid \(at master\)                = 3                            |
            | Total mirror segment failures \(at master\)             = 0                            |
            | Total number of postmaster.pid files missing            = 0                            |
            | Total number of postmaster.pid files found              = 3                            |
            | Total number of postmaster.pid PIDs missing             = 0                            |
            | Total number of postmaster.pid PIDs found               = 3                            |
            | Total number of /tmp lock files missing                 = 0                            |
            | Total number of /tmp lock files found                   = 3                            |
            | Total number postmaster processes missing               = 0                            |
            | Total number postmaster processes found                 = 3                            |
            | Total number mirror segments acting as primary segments = 3 .* <<<<<<<<                |
            | Total number mirror segments acting as mirror segments  = 0                            |

    Scenario: gpstate -c logs cluster info for a cluster where all mirrors are failed over
        Given a standard local demo cluster is running
        And the database is running
        When user stops all primary processes
        And user can start transactions
        And the user runs "gpstate -c"
        Then gpstate output looks like
            | Status                        | Data State  | Primary | Datadir                 | Port   | Mirror | Datadir                        | Port   |
            | Mirror Active, Primary Failed | Not In Sync | \S+     | .*/dbfast1/demoDataDir0 | [0-9]+ | \S+    | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ |
            | Mirror Active, Primary Failed | Not In Sync | \S+     | .*/dbfast2/demoDataDir1 | [0-9]+ | \S+    | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ |
            | Mirror Active, Primary Failed | Not In Sync | \S+     | .*/dbfast3/demoDataDir2 | [0-9]+ | \S+    | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ |
         And gpstate should print "3 segment\(s\) configured as mirror\(s\) are acting as primaries" to stdout

    Scenario: gpstate -c logs cluster info for a cluster that is unsynchronized
        Given a standard local demo cluster is running
        When user stops all mirror processes
        And user can start transactions
        And the user runs "gpstate -c"
        Then gpstate output looks like
            | Status                        | Data State  | Primary | Datadir                 | Port   | Mirror | Datadir                        | Port   |
            | Primary Active, Mirror Failed | Not In Sync | \S+     | .*/dbfast1/demoDataDir0 | [0-9]+ | \S+    | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ |
            | Primary Active, Mirror Failed | Not In Sync | \S+     | .*/dbfast2/demoDataDir1 | [0-9]+ | \S+    | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ |
            | Primary Active, Mirror Failed | Not In Sync | \S+     | .*/dbfast3/demoDataDir2 | [0-9]+ | \S+    | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ |
         And gpstate should print "3 primary segment\(s\) are not synchronized" to stdout

    Scenario: gpstate -c logs cluster info for a cluster with no mirrors
        Given the cluster is generated with "3" primaries only
        When the user runs "gpstate -c"
        Then gpstate should print "Primary list \[Mirror not used\]" to stdout
        And gpstate output looks like
            | Primary | Datadir                 | Port   |
            | \S+     | .*/dbfast1/demoDataDir0 | [0-9]+ |
            | \S+     | .*/dbfast2/demoDataDir1 | [0-9]+ |
            | \S+     | .*/dbfast3/demoDataDir2 | [0-9]+ |

    Scenario: gpstate -b logs cluster for a cluster without standbys
        Given the cluster is generated with "3" primaries only
        And the user runs "gpstate -b"
        Then gpstate output has rows with keys values
            | Master instance                              = Active                       |
            | Master standby                               = No master standby configured |
            | Total segment instance count from metadata   = 3                            |
            | Primary Segment Status                                                      |
            | Total primary segments                       = 3                            |
            | Total primary segment valid \(at master\)    = 3                            |
            | Total primary segment failures \(at master\) = 0                            |
            | Total number of postmaster.pid files missing = 0                            |
            | Total number of postmaster.pid files found   = 3                            |
            | Total number of postmaster.pid PIDs missing  = 0                            |
            | Total number of postmaster.pid PIDs found    = 3                            |
            | Total number of /tmp lock files missing      = 0                            |
            | Total number of /tmp lock files found        = 3                            |
            | Total number postmaster processes missing    = 0                            |
            | Total number postmaster processes found      = 3                            |
            | Mirror Segment Status                                                       |
            | Mirrors not configured on this array                                        |

    Scenario: gpstate -e logs no errors when there are none
        Given a standard local demo cluster is running
        And the user runs "gpstate -e"
        Then gpstate should print "Segment Mirroring Status Report" to stdout
        And gpstate should print "All segments are running normally" to stdout

    Scenario: gpstate -e logs errors when mirrors have failed over
        Given a standard local demo cluster is running
          And user stops all primary processes
          And user can start transactions
        When the user runs "gpstate -e"
        Then gpstate should print "Segments with Primary and Mirror Roles Switched" to stdout
        And gpstate output looks like
            | Current Primary | Port   | Mirror | Port   |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
        And gpstate should print "Unsynchronized Segment Pairs" to stdout
        And gpstate output looks like
            | Current Primary | Port   | Mirror | Port   |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
            | \S+             | [0-9]+ | \S+    | [0-9]+ |
        And gpstate should print "Downed Segments" to stdout
        And gpstate output looks like
            | Segment | Port   | Config status | Status                |
            | \S+     | [0-9]+ | Down          | Down in configuration |
            | \S+     | [0-9]+ | Down          | Down in configuration |
            | \S+     | [0-9]+ | Down          | Down in configuration |

    Scenario: gpstate -c logs cluster info for a mirrored cluster
        Given a standard local demo cluster is running
        When the user runs "gpstate -c"
        Then gpstate output looks like
            | Status                           | Data State   | Primary | Datadir                 | Port   | Mirror | Datadir                        | Port   |
            | Primary Active, Mirror Available | Synchronized | \S+     | .*/dbfast1/demoDataDir0 | [0-9]+ | \S+    | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ |
            | Primary Active, Mirror Available | Synchronized | \S+     | .*/dbfast2/demoDataDir1 | [0-9]+ | \S+    | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ |
            | Primary Active, Mirror Available | Synchronized | \S+     | .*/dbfast3/demoDataDir2 | [0-9]+ | \S+    | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ |

    Scenario: gpstate -b logs cluster for a default cluster
        Given a standard local demo cluster is running
        And the user runs "gpstate -b"
        Then gpstate output has rows with keys values
            | Master instance                                         = Active                       |
            | Master standby                                          =                              |
            | Standby master state                                    = Standby host passive         |
            | Total segment instance count from metadata              = 6                            |
            | Primary Segment Status                                                                 |
            | Total primary segments                                  = 3                            |
            | Total primary segment valid \(at master\)               = 3                            |
            | Total primary segment failures \(at master\)            = 0                            |
            | Total number of postmaster.pid files missing            = 0                            |
            | Total number of postmaster.pid files found              = 3                            |
            | Total number of postmaster.pid PIDs missing             = 0                            |
            | Total number of postmaster.pid PIDs found               = 3                            |
            | Total number of /tmp lock files missing                 = 0                            |
            | Total number of /tmp lock files found                   = 3                            |
            | Total number postmaster processes missing               = 0                            |
            | Total number postmaster processes found                 = 3                            |
            | Mirror Segment Status                                                                  |
            | Total mirror segments                                   = 3                            |
            | Total mirror segment valid \(at master\)                = 3                            |
            | Total mirror segment failures \(at master\)             = 0                            |
            | Total number of postmaster.pid files missing            = 0                            |
            | Total number of postmaster.pid files found              = 3                            |
            | Total number of postmaster.pid PIDs missing             = 0                            |
            | Total number of postmaster.pid PIDs found               = 3                            |
            | Total number of /tmp lock files missing                 = 0                            |
            | Total number of /tmp lock files found                   = 3                            |
            | Total number postmaster processes missing               = 0                            |
            | Total number postmaster processes found                 = 3                            |
            | Total number mirror segments acting as primary segments = 0                            |
            | Total number mirror segments acting as mirror segments  = 3                            |

    Scenario: gpstate -f logs master standyby details
        Given a standard local demo cluster is running
        When the user runs "gpstate -f"
        Then gpstate output has rows with keys values
            | Standby master details                         |
            | Standby address        =                       |
            | Standby data directory = .*/standby            |
            | Standby port           = [0-9]+                |
            | Standby PID            = [0-9]+                |
            | Standby status         = Standby host passive  |
            | pg_stat_replication                            |
            | WAL Sender State: streaming                    |
            | Sync state: sync                               |
            | Sent Location: \S+                             |
            | Flush Location: \S+                            |
            | Replay Location: \S+                           |

    Scenario: gpstate -m logs mirror details
        Given a standard local demo cluster is running
        When the user runs "gpstate -m"
        Then gpstate should print "Current GPDB mirror list and status" to stdout
        And gpstate output looks like
            | Mirror | Datadir                        | Port   | Status  | Data Status  |
            | \S+    | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ | Passive | Synchronized |
            | \S+    | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ | Passive | Synchronized |
            | \S+    | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ | Passive | Synchronized |

    Scenario: gpstate -m warns when mirrors have failed over to primary
        Given a standard local demo cluster is running
          And user stops all primary processes
          And user can start transactions
        When the user runs "gpstate -m"
        Then gpstate should print "Current GPDB mirror list and status" to stdout
        And gpstate output looks like
            | Mirror | Datadir                        | Port   | Status            | Data Status |
            | \S+    | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ | Acting as Primary | Not In Sync |
            | \S+    | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ | Acting as Primary | Not In Sync |
            | \S+    | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ | Acting as Primary | Not In Sync |
        And gpstate should print "3 segment\(s\) configured as mirror\(s\) are acting as primaries" to stdout
        And gpstate should print "3 mirror segment\(s\) acting as primaries are not synchronized" to stdout

    Scenario: gpstate -p logs port details
        Given a standard local demo cluster is running
        When the user runs "gpstate -p"
        Then gpstate should print "Master segment instance .*/demoDataDir-1  port = .*" to stdout
        And gpstate should print "Segment instance port assignments" to stdout
        And gpstate output looks like
            | Host | Datadir                         | Port   |
            | \S+  | .*/dbfast1/demoDataDir0         | [0-9]+ |
            | \S+  | .*/dbfast_mirror1/demoDataDir0  | [0-9]+ |
            | \S+  | .*/dbfast2/demoDataDir1         | [0-9]+ |
            | \S+  | .*/dbfast_mirror2/demoDataDir1  | [0-9]+ |
            | \S+  | .*/dbfast3/demoDataDir2         | [0-9]+ |
            | \S+  | .*/dbfast_mirror3/demoDataDir2  | [0-9]+ |

    Scenario: gpstate -s logs detailed information
        Given a standard local demo cluster is running
        When the user runs "gpstate -s"
        Then gpstate output has rows with keys values
            | Master Configuration & Status                          |
            | Master host                   =                        |
            | Master postgres process ID    = [0-9]+                 |
            | Master data directory         = .*/demoDataDir-1       |
            | Master port                   = [0-9]+                 |
            | Master current role           = dispatch               |
            | Greenplum initsystem version  = [0-9]+\.[0-9]+\.[0-9]+ |
            | Greenplum current version     = PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
            | Postgres version              = [0-9]+\.[0-9]+\.[0-9]+ |
            | Master standby                =                        |
            | Standby master state          = Standby host passive   |
            | Segment Instance Status Report                         |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir0      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Primary              |
            | Preferred role                  = Primary              |
            | Mirror status                   = Synchronized         |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Database status                 = Up                   |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir0      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Mirror               |
            | Preferred role                  = Mirror               |
            | Mirror status                   = Streaming            |
            | Replication Info                                       |
            | WAL Sent Location               = \S+                  |
            | WAL Flush Location              = \S+                  |
            | WAL Replay Location             = \S+                  |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Segment status                  = Up                   |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir1      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Primary              |
            | Preferred role                  = Primary              |
            | Mirror status                   = Synchronized         |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Database status                 = Up                   |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir1      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Mirror               |
            | Preferred role                  = Mirror               |
            | Mirror status                   = Streaming            |
            | Replication Info                                       |
            | WAL Sent Location               = \S+                  |
            | WAL Flush Location              = \S+                  |
            | WAL Replay Location             = \S+                  |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Segment status                  = Up                   |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir2      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Primary              |
            | Preferred role                  = Primary              |
            | Mirror status                   = Synchronized         |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Database status                 = Up                   |
            | Segment Info                                           |
            | Hostname                        =                      |
            | Address                         =                      |
            | Datadir                         = .*/demoDataDir2      |
            | Port                            = [0-9]+               |
            | Mirroring Info                                         |
            | Current role                    = Mirror               |
            | Preferred role                  = Mirror               |
            | Mirror status                   = Streaming            |
            | Replication Info                                       |
            | WAL Sent Location               = \S+                  |
            | WAL Flush Location              = \S+                  |
            | WAL Replay Location             = \S+                  |
            | Status                                                 |
            | PID                             = [0-9]+               |
            | Configuration reports status as = Up                   |
            | Segment status                  = Up                   |

    Scenario: gpstate -i logs version info for all segments
        Given a standard local demo cluster is running
        When the user runs "gpstate -i"
        Then gpstate output looks like
		  | Host | Datadir                        | Port   | Version                                                                           |
		  | \S+  | .*/qddir/demoDataDir-1         | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/standby                     | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast1/demoDataDir0        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast2/demoDataDir1        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast3/demoDataDir2        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		And gpstate should print "All segments are running the same software version" to stdout

    Scenario: gpstate -i warns if any mirrors are marked down
        Given a standard local demo cluster is running
          And user stops all mirror processes
          And user can start transactions
        When the user runs "gpstate -i"
        Then gpstate output looks like
		  | Host | Datadir                        | Port   | Version                                                                           |
		  | \S+  | .*/qddir/demoDataDir-1         | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/standby                     | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast1/demoDataDir0        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ | unable to retrieve version                                                        |
		  | \S+  | .*/dbfast2/demoDataDir1        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ | unable to retrieve version                                                        |
		  | \S+  | .*/dbfast3/demoDataDir2        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ | unable to retrieve version                                                        |
		And gpstate should print "Unable to retrieve version data from all segments" to stdout

    Scenario: gpstate -i warns if any up mirrors cannot be contacted
        Given a standard local demo cluster is running
          And user stops all mirror processes
          # We intentionally do not wait for an FTS probe here; we want the
          # mirrors to still be marked up when we try to get their version.
        When the user runs "gpstate -i"
        Then gpstate output looks like
		  | Host | Datadir                        | Port   | Version                                                                           |
		  | \S+  | .*/qddir/demoDataDir-1         | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/standby                     | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast1/demoDataDir0        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror1/demoDataDir0 | [0-9]+ | unable to retrieve version                                                        |
		  | \S+  | .*/dbfast2/demoDataDir1        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror2/demoDataDir1 | [0-9]+ | unable to retrieve version                                                        |
		  | \S+  | .*/dbfast3/demoDataDir2        | [0-9]+ | PostgreSQL [0-9]+\.[0-9]+\.[0-9]+ \(Greenplum Database [0-9]+\.[0-9]+\.[0-9]+.*\) |
		  | \S+  | .*/dbfast_mirror3/demoDataDir2 | [0-9]+ | unable to retrieve version                                                        |
		And gpstate should print "Unable to retrieve version data from all segments" to stdout

    Scenario: gpstate -x logs gpexpand status
        Given the cluster is generated with "3" primaries only
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = No Expansion Detected |
        Given the file "gpexpand.status" exists under master data directory
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Replicating Meta Data |
             |   Some database tools and functionality         |
             |   are disabled during this process              |
        Given schema "gpexpand" exists in "postgres"
          And below sql is executed in "postgres" db
              """
              CREATE TABLE gpexpand.status (status text, updated timestamp);
              CREATE TABLE gpexpand.status_detail (
                  dbname text,
                  fq_name text,
                  schema_oid oid,
                  table_oid oid,
                  distribution_policy smallint[],
                  distribution_policy_names text,
                  distribution_policy_coloids text,
                  distribution_policy_type text,
                  root_partition_name text,
                  storage_options text,
                  rank int,
                  status text,
                  expansion_started timestamp,
                  expansion_finished timestamp,
                  source_bytes numeric
              );
              INSERT INTO gpexpand.status VALUES
                  ( 'SETUP',      '2001-01-01' ),
                  ( 'SETUP DONE', '2001-01-02' );
              INSERT INTO gpexpand.status_detail (dbname, fq_name, rank, status) VALUES
                  ('fake_db', 'public.t1', 2, 'NOT STARTED'),
                  ('fake_db', 'public.t2', 2, 'NOT STARTED');
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Replicating Meta Data |
             |   Some database tools and functionality         |
             |   are disabled during this process              |
        Given the user runs command "rm $MASTER_DATA_DIRECTORY/gpexpand.status"
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Paused |
             | Number of tables to be redistributed                 |
             |      Database   Count of Tables to redistribute      |
             |      fake_db    2                                    |
        Given below sql is executed in "postgres" db
              """
              INSERT INTO gpexpand.status VALUES
                  ( 'EXPANSION STARTED', '2001-01-03' );
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Active |
             | Number of tables to be redistributed                 |
             |      Database   Count of Tables to redistribute      |
             |      fake_db    2                                    |
        Given below sql is executed in "postgres" db
              """
              UPDATE gpexpand.status_detail SET STATUS='IN PROGRESS'
               WHERE fq_name='public.t1';
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Active |
             | Number of tables to be redistributed                 |
             |      Database   Count of Tables to redistribute      |
             |      fake_db    1                                    |
             | Active redistributions = 1                           |
             |      Action         Database   Table                 |
             |      Redistribute   fake_db    public.t1             |
        Given below sql is executed in "postgres" db
              """
              UPDATE gpexpand.status_detail SET STATUS='COMPLETED'
               WHERE fq_name='public.t1';
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Active |
             | Number of tables to be redistributed                 |
             |      Database   Count of Tables to redistribute      |
             |      fake_db    1                                    |
        Given below sql is executed in "postgres" db
              """
              INSERT INTO gpexpand.status VALUES
                  ( 'EXPANSION STOPPED', '2001-01-04' );
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Paused |
             | Number of tables to be redistributed                 |
             |      Database   Count of Tables to redistribute      |
             |      fake_db    1                                    |
        Given below sql is executed in "postgres" db
              """
              INSERT INTO gpexpand.status VALUES
                  ( 'EXPANSION STARTED', '2001-01-05' );
              UPDATE gpexpand.status_detail SET STATUS='IN PROGRESS'
               WHERE fq_name='public.t2';
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Active |
             | Active redistributions = 1                           |
             |      Action         Database   Table                 |
             |      Redistribute   fake_db    public.t2             |
        Given below sql is executed in "postgres" db
              """
              UPDATE gpexpand.status_detail SET STATUS='COMPLETED'
               WHERE fq_name='public.t2';
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Active |
        Given below sql is executed in "postgres" db
              """
              INSERT INTO gpexpand.status VALUES
                  ( 'EXPANSION STOPPED', '2001-01-06' );
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = Data Distribution - Paused |
        Given below sql is executed in "postgres" db
              """
              DROP SCHEMA gpexpand CASCADE;
              """
         When the user runs "gpstate -x"
         Then gpstate output looks like
             | Cluster Expansion State = No Expansion Detected |
