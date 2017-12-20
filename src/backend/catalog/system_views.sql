/*
 * PostgreSQL System Views
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 1996-2010, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/backend/catalog/system_views.sql,v 1.60 2009/04/07 00:31:26 tgl Exp $
 */

CREATE VIEW pg_roles AS 
    SELECT 
        rolname,
        rolsuper,
        rolinherit,
        rolcreaterole,
        rolcreatedb,
        rolcatupdate,
        rolcanlogin,
        rolconnlimit,
        '********'::text as rolpassword,
        rolvaliduntil,
        rolconfig,
		rolresqueue,
        oid,
        rolcreaterextgpfd,
        rolcreaterexthttp,
        rolcreatewextgpfd,
        rolcreaterexthdfs,
        rolcreatewexthdfs,
        rolresgroup
    FROM pg_authid;

CREATE VIEW pg_shadow AS
    SELECT
        rolname AS usename,
        oid AS usesysid,
        rolcreatedb AS usecreatedb,
        rolsuper AS usesuper,
        rolcatupdate AS usecatupd,
        rolpassword AS passwd,
        rolvaliduntil::abstime AS valuntil,
        rolconfig AS useconfig
    FROM pg_authid
    WHERE rolcanlogin;

REVOKE ALL on pg_shadow FROM public;

CREATE VIEW pg_group AS
    SELECT
        rolname AS groname,
        oid AS grosysid,
        ARRAY(SELECT member FROM pg_auth_members WHERE roleid = oid) AS grolist
    FROM pg_authid
    WHERE NOT rolcanlogin;

CREATE VIEW pg_user AS 
    SELECT 
        usename, 
        usesysid, 
        usecreatedb, 
        usesuper, 
        usecatupd, 
        '********'::text as passwd, 
        valuntil, 
        useconfig 
    FROM pg_shadow;

CREATE VIEW pg_rules AS 
    SELECT 
        N.nspname AS schemaname, 
        C.relname AS tablename, 
        R.rulename AS rulename, 
        pg_get_ruledef(R.oid) AS definition 
    FROM (pg_rewrite R JOIN pg_class C ON (C.oid = R.ev_class)) 
        LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE R.rulename != '_RETURN';

CREATE VIEW pg_views AS 
    SELECT 
        N.nspname AS schemaname, 
        C.relname AS viewname, 
        pg_get_userbyid(C.relowner) AS viewowner, 
        pg_get_viewdef(C.oid) AS definition 
    FROM pg_class C LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind = 'v';

CREATE VIEW pg_tables AS 
    SELECT 
        N.nspname AS schemaname, 
        C.relname AS tablename, 
        pg_get_userbyid(C.relowner) AS tableowner, 
        T.spcname AS tablespace,
        C.relhasindex AS hasindexes, 
        C.relhasrules AS hasrules, 
        C.relhastriggers AS hastriggers 
    FROM pg_class C LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
         LEFT JOIN pg_tablespace T ON (T.oid = C.reltablespace)
    WHERE C.relkind = 'r';

CREATE VIEW pg_indexes AS 
    SELECT 
        N.nspname AS schemaname, 
        C.relname AS tablename, 
        I.relname AS indexname, 
        T.spcname AS tablespace,
        pg_get_indexdef(I.oid) AS indexdef 
    FROM pg_index X JOIN pg_class C ON (C.oid = X.indrelid) 
         JOIN pg_class I ON (I.oid = X.indexrelid) 
         LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
         LEFT JOIN pg_tablespace T ON (T.oid = I.reltablespace)
    WHERE C.relkind = 'r' AND I.relkind = 'i';

CREATE VIEW pg_stats AS 
    SELECT 
        nspname AS schemaname, 
        relname AS tablename, 
        attname AS attname, 
        stanullfrac AS null_frac, 
        stawidth AS avg_width, 
        stadistinct AS n_distinct, 
        CASE
            WHEN stakind1 IN (1, 4) THEN stavalues1
            WHEN stakind2 IN (1, 4) THEN stavalues2
            WHEN stakind3 IN (1, 4) THEN stavalues3
            WHEN stakind4 IN (1, 4) THEN stavalues4
        END AS most_common_vals,
        CASE
            WHEN stakind1 IN (1, 4) THEN stanumbers1
            WHEN stakind2 IN (1, 4) THEN stanumbers2
            WHEN stakind3 IN (1, 4) THEN stanumbers3
            WHEN stakind4 IN (1, 4) THEN stanumbers4
        END AS most_common_freqs,
        CASE
            WHEN stakind1 = 2 THEN stavalues1
            WHEN stakind2 = 2 THEN stavalues2
            WHEN stakind3 = 2 THEN stavalues3
            WHEN stakind4 = 2 THEN stavalues4
        END AS histogram_bounds,
        CASE
            WHEN stakind1 = 3 THEN stanumbers1[1]
            WHEN stakind2 = 3 THEN stanumbers2[1]
            WHEN stakind3 = 3 THEN stanumbers3[1]
            WHEN stakind4 = 3 THEN stanumbers4[1]
        END AS correlation
    FROM pg_statistic s JOIN pg_class c ON (c.oid = s.starelid) 
         JOIN pg_attribute a ON (c.oid = attrelid AND attnum = s.staattnum) 
         LEFT JOIN pg_namespace n ON (n.oid = c.relnamespace) 
    WHERE NOT attisdropped AND has_column_privilege(c.oid, a.attnum, 'select');

REVOKE ALL on pg_statistic FROM public;

CREATE VIEW pg_locks AS 
    SELECT * FROM pg_lock_status() AS L;

CREATE VIEW pg_cursors AS
    SELECT * FROM pg_cursor() AS C;

CREATE VIEW pg_available_extensions AS
    SELECT E.name, E.default_version, X.extversion AS installed_version,
           E.comment
      FROM pg_available_extensions() AS E
           LEFT JOIN pg_extension AS X ON E.name = X.extname;

CREATE VIEW pg_available_extension_versions AS
    SELECT E.name, E.version, (X.extname IS NOT NULL) AS installed,
           E.superuser, E.relocatable, E.schema, E.requires, E.comment
      FROM pg_available_extension_versions() AS E
           LEFT JOIN pg_extension AS X
             ON E.name = X.extname AND E.version = X.extversion;

CREATE VIEW pg_prepared_xacts AS
    SELECT P.transaction, P.gid, P.prepared,
           U.rolname AS owner, D.datname AS database
    FROM pg_prepared_xact() AS P
         LEFT JOIN pg_authid U ON P.ownerid = U.oid
         LEFT JOIN pg_database D ON P.dbid = D.oid;

CREATE VIEW pg_prepared_statements AS
    SELECT * FROM pg_prepared_statement() AS P;

CREATE VIEW pg_settings AS 
    SELECT * FROM pg_show_all_settings() AS A; 

CREATE RULE pg_settings_u AS 
    ON UPDATE TO pg_settings 
    WHERE new.name = old.name DO 
    SELECT set_config(old.name, new.setting, 'f');

CREATE RULE pg_settings_n AS 
    ON UPDATE TO pg_settings 
    DO INSTEAD NOTHING;

GRANT SELECT, UPDATE ON pg_settings TO PUBLIC;

CREATE VIEW pg_timezone_abbrevs AS
    SELECT * FROM pg_timezone_abbrevs();

CREATE VIEW pg_timezone_names AS
    SELECT * FROM pg_timezone_names();

-- Statistics views

CREATE VIEW pg_stat_all_tables AS 
    SELECT 
            C.oid AS relid, 
            N.nspname AS schemaname, 
            C.relname AS relname, 
            pg_stat_get_numscans(C.oid) AS seq_scan, 
            pg_stat_get_tuples_returned(C.oid) AS seq_tup_read, 
            sum(pg_stat_get_numscans(I.indexrelid))::bigint AS idx_scan, 
            sum(pg_stat_get_tuples_fetched(I.indexrelid))::bigint +
            pg_stat_get_tuples_fetched(C.oid) AS idx_tup_fetch, 
            pg_stat_get_tuples_inserted(C.oid) AS n_tup_ins, 
            pg_stat_get_tuples_updated(C.oid) AS n_tup_upd, 
            pg_stat_get_tuples_deleted(C.oid) AS n_tup_del,
            pg_stat_get_tuples_hot_updated(C.oid) AS n_tup_hot_upd,
            pg_stat_get_live_tuples(C.oid) AS n_live_tup, 
            pg_stat_get_dead_tuples(C.oid) AS n_dead_tup,
            pg_stat_get_last_vacuum_time(C.oid) as last_vacuum,
            pg_stat_get_last_autovacuum_time(C.oid) as last_autovacuum,
            pg_stat_get_last_analyze_time(C.oid) as last_analyze,
            pg_stat_get_last_autoanalyze_time(C.oid) as last_autoanalyze
    FROM pg_class C LEFT JOIN 
         pg_index I ON C.oid = I.indrelid 
         LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind IN ('r', 't')
    GROUP BY C.oid, N.nspname, C.relname;

CREATE VIEW pg_stat_sys_tables AS 
    SELECT * FROM pg_stat_all_tables 
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_stat_user_tables AS 
    SELECT * FROM pg_stat_all_tables 
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_tables AS 
    SELECT 
            C.oid AS relid, 
            N.nspname AS schemaname, 
            C.relname AS relname, 
            pg_stat_get_blocks_fetched(C.oid) - 
                    pg_stat_get_blocks_hit(C.oid) AS heap_blks_read, 
            pg_stat_get_blocks_hit(C.oid) AS heap_blks_hit, 
            sum(pg_stat_get_blocks_fetched(I.indexrelid) - 
                    pg_stat_get_blocks_hit(I.indexrelid))::bigint AS idx_blks_read, 
            sum(pg_stat_get_blocks_hit(I.indexrelid))::bigint AS idx_blks_hit, 
            pg_stat_get_blocks_fetched(T.oid) - 
                    pg_stat_get_blocks_hit(T.oid) AS toast_blks_read, 
            pg_stat_get_blocks_hit(T.oid) AS toast_blks_hit, 
            pg_stat_get_blocks_fetched(X.oid) - 
                    pg_stat_get_blocks_hit(X.oid) AS tidx_blks_read, 
            pg_stat_get_blocks_hit(X.oid) AS tidx_blks_hit 
    FROM pg_class C LEFT JOIN 
            pg_index I ON C.oid = I.indrelid LEFT JOIN 
            pg_class T ON C.reltoastrelid = T.oid LEFT JOIN 
            pg_class X ON T.reltoastidxid = X.oid 
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind IN ('r', 't')
    GROUP BY C.oid, N.nspname, C.relname, T.oid, X.oid;

CREATE VIEW pg_statio_sys_tables AS 
    SELECT * FROM pg_statio_all_tables 
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_tables AS 
    SELECT * FROM pg_statio_all_tables 
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_stat_all_indexes AS 
    SELECT 
            C.oid AS relid, 
            I.oid AS indexrelid, 
            N.nspname AS schemaname, 
            C.relname AS relname, 
            I.relname AS indexrelname, 
            pg_stat_get_numscans(I.oid) AS idx_scan, 
            pg_stat_get_tuples_returned(I.oid) AS idx_tup_read, 
            pg_stat_get_tuples_fetched(I.oid) AS idx_tup_fetch 
    FROM pg_class C JOIN 
            pg_index X ON C.oid = X.indrelid JOIN 
            pg_class I ON I.oid = X.indexrelid 
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind IN ('r', 't');

CREATE VIEW pg_stat_sys_indexes AS 
    SELECT * FROM pg_stat_all_indexes 
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_stat_user_indexes AS 
    SELECT * FROM pg_stat_all_indexes 
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_indexes AS 
    SELECT 
            C.oid AS relid, 
            I.oid AS indexrelid, 
            N.nspname AS schemaname, 
            C.relname AS relname, 
            I.relname AS indexrelname, 
            pg_stat_get_blocks_fetched(I.oid) - 
                    pg_stat_get_blocks_hit(I.oid) AS idx_blks_read, 
            pg_stat_get_blocks_hit(I.oid) AS idx_blks_hit 
    FROM pg_class C JOIN 
            pg_index X ON C.oid = X.indrelid JOIN 
            pg_class I ON I.oid = X.indexrelid 
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind IN ('r', 't');

CREATE VIEW pg_statio_sys_indexes AS 
    SELECT * FROM pg_statio_all_indexes 
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_indexes AS 
    SELECT * FROM pg_statio_all_indexes 
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_sequences AS 
    SELECT 
            C.oid AS relid, 
            N.nspname AS schemaname, 
            C.relname AS relname, 
            pg_stat_get_blocks_fetched(C.oid) - 
                    pg_stat_get_blocks_hit(C.oid) AS blks_read, 
            pg_stat_get_blocks_hit(C.oid) AS blks_hit 
    FROM pg_class C 
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace) 
    WHERE C.relkind = 'S';

CREATE VIEW pg_statio_sys_sequences AS 
    SELECT * FROM pg_statio_all_sequences 
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_sequences AS 
    SELECT * FROM pg_statio_all_sequences 
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_stat_activity AS 
    SELECT 
            S.datid AS datid,
            D.datname AS datname,
            S.procpid,
            S.sess_id,
            S.usesysid,
            U.rolname AS usename,
            S.application_name,
            S.current_query,
            S.waiting,
            S.xact_start,
            S.query_start,
            S.backend_start,
            S.client_addr,
            S.client_port,

            S.waiting_reason,
            S.rsgid,
            S.rsgname,
            S.rsgqueueduration
    FROM pg_database D, pg_stat_get_activity(NULL) AS S, pg_authid U
    WHERE S.datid = D.oid AND 
            S.usesysid = U.oid;

CREATE VIEW pg_stat_replication AS
    SELECT
            S.procpid,
            S.usesysid,
            U.rolname AS usename,
            S.application_name,
            S.client_addr,
            S.client_port,
            S.backend_start,
            W.state,
            W.sent_location,
            W.write_location,
            W.flush_location,
            W.replay_location,
            W.sync_priority,
            W.sync_state
    FROM pg_stat_get_activity(NULL) AS S, pg_authid U,
            pg_stat_get_wal_senders() AS W
    WHERE S.usesysid = U.oid AND
            S.procpid = W.pid;

CREATE FUNCTION gp_stat_get_master_replication() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
    FROM pg_catalog.pg_stat_replication
$$
LANGUAGE SQL EXECUTE ON MASTER;

CREATE FUNCTION gp_stat_get_segment_replication() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
    FROM pg_catalog.pg_stat_replication
$$
LANGUAGE SQL EXECUTE ON ALL SEGMENTS;

CREATE FUNCTION gp_stat_get_segment_replication_error() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, pg_catalog.gp_replication_error() as sync_error
$$
LANGUAGE SQL EXECUTE ON ALL SEGMENTS;

CREATE VIEW gp_stat_replication AS
    SELECT *, pg_catalog.gp_replication_error() AS sync_error
    FROM pg_catalog.gp_stat_get_master_replication() AS R
    (gp_segment_id integer, procpid integer, usesysid oid,
     usename name, application_name text, client_addr inet,
     client_port integer, backend_start timestamptz, state text,
     sent_location text, write_location text, flush_location text,
     replay_location text, sync_priority integer, sync_state text)
    UNION ALL
    (
        SELECT G.gp_segment_id
            , R.procpid, R.usesysid, R.usename, R.application_name, R.client_addr
            , R.client_port, R.backend_start, R.state, R.sent_location
            , R.write_location, R.flush_location, R.replay_location
            , R.sync_priority, R.sync_state
            , G.sync_error
        FROM (
            SELECT E.*
            FROM pg_catalog.gp_segment_configuration C
            JOIN pg_catalog.gp_stat_get_segment_replication_error() AS E (gp_segment_id integer, sync_error text)
            ON c.content = E.gp_segment_id
            WHERE C.role = 'm'
        ) G
        LEFT OUTER JOIN pg_catalog.gp_stat_get_segment_replication() AS R
        (gp_segment_id integer, procpid integer, usesysid oid,
         usename name, application_name text, client_addr inet,
         client_port integer, backend_start timestamptz, state text,
         sent_location text, write_location text, flush_location text,
         replay_location text, sync_priority integer, sync_state text)
         ON G.gp_segment_id = R.gp_segment_id
    );

CREATE VIEW pg_stat_database AS 
    SELECT 
            D.oid AS datid, 
            D.datname AS datname, 
            pg_stat_get_db_numbackends(D.oid) AS numbackends, 
            pg_stat_get_db_xact_commit(D.oid) AS xact_commit, 
            pg_stat_get_db_xact_rollback(D.oid) AS xact_rollback, 
            pg_stat_get_db_blocks_fetched(D.oid) - 
                    pg_stat_get_db_blocks_hit(D.oid) AS blks_read, 
            pg_stat_get_db_blocks_hit(D.oid) AS blks_hit,
            pg_stat_get_db_tuples_returned(D.oid) AS tup_returned,
            pg_stat_get_db_tuples_fetched(D.oid) AS tup_fetched,
            pg_stat_get_db_tuples_inserted(D.oid) AS tup_inserted,
            pg_stat_get_db_tuples_updated(D.oid) AS tup_updated,
            pg_stat_get_db_tuples_deleted(D.oid) AS tup_deleted
    FROM pg_database D;

CREATE VIEW pg_stat_resqueues AS
	SELECT
		Q.oid AS queueid,
		Q.rsqname AS queuename,
		pg_stat_get_queue_num_exec(Q.oid) AS n_queries_exec,
		pg_stat_get_queue_num_wait(Q.oid) AS n_queries_wait,
		pg_stat_get_queue_elapsed_exec(Q.oid) AS elapsed_exec,
		pg_stat_get_queue_elapsed_wait(Q.oid) AS elapsed_wait
	FROM pg_resqueue AS Q;

-- Resource queue views

CREATE VIEW pg_resqueue_status AS
	SELECT 
			q.rsqname, 
			q.rsqcountlimit, 
			s.queuecountvalue AS rsqcountvalue,
			q.rsqcostlimit, 
			s.queuecostvalue AS rsqcostvalue,
			s.queuewaiters AS rsqwaiters,
			s.queueholders AS rsqholders
	FROM pg_resqueue AS q 
			INNER JOIN pg_resqueue_status() AS s 
			(	queueid oid, 
	 			queuecountvalue float4, 
				queuecostvalue float4,
				queuewaiters int4,
				queueholders int4)
			ON (s.queueid = q.oid);
			
-- External table views

CREATE VIEW pg_max_external_files AS
    SELECT   address::name as hostname, count(*) as maxfiles
    FROM     gp_segment_configuration
    WHERE    content >= 0 
    AND      role='p'
    GROUP BY address;

-- partitioning
create view pg_partitions as
  select 
      schemaname, 
      tablename, 
      partitionschemaname, 
      partitiontablename, 
      partitionname, 
      parentpartitiontablename, 
      parentpartitionname, 
      partitiontype, 
      partitionlevel, 
      -- Only the non-default parts of range partitions have 
      -- a non-null partition rank.  For these the rank is
      -- from (1, 2, ...) in keeping with the use of RANK(n)
      -- to identify the parts of a range partition in the 
      -- ALTER statement.
      case
          when partitiontype <> 'range'::text then null::bigint
          when partitionnodefault > 0 then partitionrank
          when partitionrank = 0 then null::bigint
          else partitionrank
          end as partitionrank, 
      partitionposition, 
      partitionlistvalues, 
      partitionrangestart, 
      case
          when partitiontype = 'range'::text then partitionstartinclusive
          else null::boolean
          end as partitionstartinclusive, partitionrangeend, 
      case
          when partitiontype = 'range'::text then partitionendinclusive
          else null::boolean
          end as partitionendinclusive, 
      partitioneveryclause, 
      parisdefault as partitionisdefault, 
      partitionboundary,
      parentspace as parenttablespace,
      partspace as partitiontablespace
  from 
      ( 
          select 
              n.nspname as schemaname, 
              cl.relname as tablename, 
              n2.nspname as partitionschemaname, 
              cl2.relname as partitiontablename, 
              pr1.parname as partitionname, 
              cl3.relname as parentpartitiontablename, 
              pr2.parname as parentpartitionname, 
              case
                  when pp.parkind = 'h'::"char" then 'hash'::text
                  when pp.parkind = 'r'::"char" then 'range'::text
                  when pp.parkind = 'l'::"char" then 'list'::text
                  else null::text
                  end as partitiontype, 
              pp.parlevel as partitionlevel, 
              pr1.parruleord as partitionposition, 
              case
                  when pp.parkind != 'r'::"char" or pr1.parisdefault then null::bigint
                  else
                      rank() over(
                      partition by pp.oid, cl.relname, pp.parlevel, cl3.relname
                      order by pr1.parisdefault, pr1.parruleord) 
                  end as partitionrank, 
              pg_get_expr(pr1.parlistvalues, pr1.parchildrelid) as partitionlistvalues, 
              pg_get_expr(pr1.parrangestart, pr1.parchildrelid) as partitionrangestart, 
              pr1.parrangestartincl as partitionstartinclusive, 
              pg_get_expr(pr1.parrangeend, pr1.parchildrelid) as partitionrangeend, 
              pr1.parrangeendincl as partitionendinclusive, 
              pg_get_expr(pr1.parrangeevery, pr1.parchildrelid) as partitioneveryclause, 
              min(pr1.parruleord) over(
                  partition by pp.oid, cl.relname, pp.parlevel, cl3.relname
                  order by pr1.parruleord) as partitionnodefault, 
              pr1.parisdefault, 
              pg_get_partition_rule_def(pr1.oid, true) as partitionboundary,
              coalesce(sp.spcname, dfltspcname) as parentspace,
              coalesce(sp3.spcname, dfltspcname) as partspace
          from 
              pg_namespace n, 
              pg_namespace n2, 
              pg_class cl
                  left join
              pg_tablespace sp on cl.reltablespace = sp.oid, 
              pg_class cl2
                  left join
              pg_tablespace sp3 on cl2.reltablespace = sp3.oid,
              pg_partition pp, 
              pg_partition_rule pr1
                  left join 
              pg_partition_rule pr2 on pr1.parparentrule = pr2.oid
                  left join 
              pg_class cl3 on pr2.parchildrelid = cl3.oid,
              (select s.spcname
               from pg_database, pg_tablespace s
               where datname = current_database()
                 and dattablespace = s.oid) d(dfltspcname)
      where 
          pp.paristemplate = false and 
          pp.parrelid = cl.oid and 
          pr1.paroid = pp.oid and 
          cl2.oid = pr1.parchildrelid and 
          cl.relnamespace = n.oid and 
          cl2.relnamespace = n2.oid) p1;

create view pg_partition_columns as											 
select																		  
n.nspname as schemaname,														
c.relname as tablename,														 
a.attname as columnname,														
p.parlevel as partitionlevel,												   
p.i + 1 as position_in_partition_key											
from pg_namespace n,															
pg_class c,																	 
pg_attribute a,																 
(select p.parrelid, p.parlevel, p.paratts[i] as attnum, i from pg_partition p,  
 generate_series(0,															 
				 (select max(array_upper(paratts, 1)) from pg_partition)																   
				) i
		where paratts[i] is not null
) p
where p.parrelid = c.oid and c.relnamespace = n.oid and
   p.attnum = a.attnum and a.attrelid = c.oid;

create view pg_partition_templates as
select
schemaname,
tablename, 
partitionname,
partitiontype, 
partitionlevel,
-- if not a range partition, no partition rank
-- for range partitions, the parruleord of the default partition is zero,
-- so if no_default (min of parruleord) > 0 then there is no default partition
-- so return the normal rank.  However, if there is a default partition, it
-- is rank 1, so skip it, and decrement remaining ranks by 1 so the first
-- non-default partition starts at 1
--
case when (partitiontype != 'range') then NULL
	 when (partitionnodefault > 0) then partitionrank
	 when (partitionrank = 1) then NULL
	 else  partitionrank - 1
end as partitionrank,
partitionposition,
partitionlistvalues,
partitionrangestart,
case when (partitiontype = 'range') then partitionstartinclusive
	 else NULL
end as partitionstartinclusive,
partitionrangeend,
case when (partitiontype = 'range') then partitionendinclusive
	else NULL
end as partitionendinclusive,
partitioneveryclause,
parisdefault as partitionisdefault,
partitionboundary
from (
select
n.nspname as schemaname,
cl.relname as tablename,
pr1.parname as partitionname,
p.parlevel as partitionlevel,
pr1.parruleord as partitionposition,
rank() over (partition by p.oid, cl.relname, p.parlevel 
			 order by pr1.parruleord) as partitionrank,
pg_get_expr(pr1.parlistvalues, p.parrelid) as partitionlistvalues,
pg_get_expr(pr1.parrangestart, p.parrelid) as partitionrangestart,
pr1.parrangestartincl as partitionstartinclusive,
pg_get_expr(pr1.parrangeend, p.parrelid) as partitionrangeend,
pr1.parrangeendincl as partitionendinclusive,
pg_get_expr(pr1.parrangeevery, p.parrelid) as partitioneveryclause,

min(pr1.parruleord) over (partition by p.oid, cl.relname, p.parlevel
	order by pr1.parruleord) as partitionnodefault,
pr1.parisdefault,
case when p.parkind = 'h' then 'hash' when p.parkind = 'r' then 'range'
	 when p.parkind = 'l' then 'list' else null end as partitiontype, 
pg_get_partition_rule_def(pr1.oid, true) as partitionboundary
from pg_namespace n, pg_class cl, pg_partition p, pg_partition_rule pr1
where 
 p.parrelid = cl.oid and 
 pr1.paroid = p.oid and
 cl.relnamespace = n.oid and
 p.paristemplate = 't'
 ) p1;

-- metadata tracking
CREATE VIEW pg_stat_operations
AS
SELECT 
'pg_authid' AS classname, 
a.rolname AS objname, 
c.objid, NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM 
pg_authid a, 
(pg_authid b FULL JOIN
pg_stat_last_shoperation c ON ((b.oid = c.stasysid))) WHERE ((a.oid
= c.objid) AND (c.classid = (SELECT pg_class.oid FROM pg_class WHERE
(pg_class.relname = 'pg_authid'::name))))
UNION 
SELECT 
'pg_class' AS classname, 
a.relname AS objname, 
c.objid,  N.nspname AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename) 
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM pg_class
a, pg_namespace n, (pg_authid b FULL JOIN 
pg_stat_last_operation c ON ((b.oid =
c.stasysid))) WHERE 
a.relnamespace = n.oid AND
((a.oid = c.objid) AND (c.classid = (SELECT
pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_class'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT
'pg_namespace' AS classname, a.nspname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_namespace a, (pg_authid b FULL JOIN pg_stat_last_operation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_namespace'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT
'pg_database' AS classname, a.datname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_database a, (pg_authid b FULL JOIN pg_stat_last_shoperation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_database'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION 
SELECT
'pg_tablespace' AS classname, a.spcname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_tablespace a, (pg_authid b FULL JOIN pg_stat_last_shoperation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_tablespace'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT 'pg_resqueue' AS classname,
a.rsqname as objname,
c.objid, NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM pg_resqueue a, (pg_authid
b FULL JOIN pg_stat_last_shoperation c ON ((b.oid = c.stasysid)))
WHERE ((a.oid = c.objid) AND (c.classid = (SELECT pg_class.oid FROM
pg_class WHERE ((pg_class.relname = 'pg_resqueue'::name) AND
(pg_class.relnamespace = (SELECT pg_namespace.oid FROM pg_namespace
WHERE (pg_namespace.nspname = 'pg_catalog'::name))))))) ORDER BY 9;

CREATE VIEW
pg_stat_partition_operations
AS
SELECT pso.*,
CASE WHEN  pr.parlevel IS NOT NULL 
THEN pr.parlevel 
ELSE pr2.parlevel END AS partitionlevel,
pcns.relname AS parenttablename,
pcns.nspname AS parentschemaname,
pr.parrelid AS parent_relid
FROM
(pg_stat_operations pso
LEFT OUTER JOIN
pg_partition_rule ppr
ON pso.objid=ppr.parchildrelid
LEFT OUTER JOIN
pg_partition pr
ON pr.oid = ppr.paroid) LEFT OUTER JOIN 
--
-- only want lowest parlevel for parenttable
--
(SELECT MIN(parlevel) AS parlevel, parrelid FROM 
pg_partition prx GROUP BY parrelid ) AS pr2
ON pr2.parrelid = pso.objid
LEFT OUTER JOIN 
( SELECT pc.oid, * FROM pg_class AS pc FULL JOIN pg_namespace AS ns 
ON ns.oid = pc.relnamespace) AS pcns
ON pcns.oid = pr.parrelid
;

-- MPP-7807: show all resqueue attributes
CREATE VIEW pg_resqueue_attributes AS
SELECT rsqname, 'active_statements' AS resname,
rsqcountlimit::text AS ressetting,
1 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'max_cost' AS resname,
rsqcostlimit::text AS ressetting,
2 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'cost_overcommit' AS resname,
case when rsqovercommit then '1'
else '0' end AS ressetting,
4 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'min_cost' AS resname,
rsqignorecostlimit::text AS ressetting,
3 AS restypid FROM pg_resqueue
UNION
SELECT rq.rsqname , rt.resname, rc.ressetting,
rt.restypid AS restypid FROM
pg_resqueue rq, pg_resourcetype rt,
pg_resqueuecapability rc WHERE
rq.oid=rc.resqueueid AND rc.restypid = rt.restypid
ORDER BY rsqname, restypid
;

CREATE VIEW pg_stat_user_functions AS 
    SELECT
            P.oid AS funcid, 
            N.nspname AS schemaname,
            P.proname AS funcname,
            pg_stat_get_function_calls(P.oid) AS calls,
            pg_stat_get_function_time(P.oid) / 1000 AS total_time,
            pg_stat_get_function_self_time(P.oid) / 1000 AS self_time
    FROM pg_proc P LEFT JOIN pg_namespace N ON (N.oid = P.pronamespace)
    WHERE P.prolang != 12  -- fast check to eliminate built-in functions   
          AND pg_stat_get_function_calls(P.oid) IS NOT NULL;

CREATE VIEW pg_stat_bgwriter AS
    SELECT
        pg_stat_get_bgwriter_timed_checkpoints() AS checkpoints_timed,
        pg_stat_get_bgwriter_requested_checkpoints() AS checkpoints_req,
        pg_stat_get_bgwriter_buf_written_checkpoints() AS buffers_checkpoint,
        pg_stat_get_bgwriter_buf_written_clean() AS buffers_clean,
        pg_stat_get_bgwriter_maxwritten_clean() AS maxwritten_clean,
        pg_stat_get_buf_written_backend() AS buffers_backend,
        pg_stat_get_buf_alloc() AS buffers_alloc;

CREATE VIEW pg_user_mappings AS
    SELECT
        U.oid       AS umid,
        S.oid       AS srvid,
        S.srvname   AS srvname,
        U.umuser    AS umuser,
        CASE WHEN U.umuser = 0 THEN
            'public'
        ELSE
            A.rolname
        END AS usename,
        CASE WHEN pg_has_role(S.srvowner, 'USAGE') OR has_server_privilege(S.oid, 'USAGE') THEN
            U.umoptions
        ELSE
            NULL
        END AS umoptions
    FROM pg_user_mapping U
         LEFT JOIN pg_authid A ON (A.oid = U.umuser) JOIN
        pg_foreign_server S ON (U.umserver = S.oid);

REVOKE ALL on pg_user_mapping FROM public;

--
-- We have a few function definitions in here, too.
-- At some point there might be enough to justify breaking them out into
-- a separate "system_functions.sql" file.
--

-- Tsearch debug function.  Defined here because it'd be pretty unwieldy
-- to put it into pg_proc.h

CREATE FUNCTION ts_debug(IN config regconfig, IN document text,
    OUT alias text,
    OUT description text,
    OUT token text,
    OUT dictionaries regdictionary[],
    OUT dictionary regdictionary,
    OUT lexemes text[])
RETURNS SETOF record AS
$$
SELECT 
    tt.alias AS alias,
    tt.description AS description,
    parse.token AS token,
    ARRAY ( SELECT m.mapdict::pg_catalog.regdictionary
            FROM pg_catalog.pg_ts_config_map AS m
            WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
            ORDER BY m.mapseqno )
    AS dictionaries,
    ( SELECT mapdict::pg_catalog.regdictionary
      FROM pg_catalog.pg_ts_config_map AS m
      WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
      ORDER BY pg_catalog.ts_lexize(mapdict, parse.token) IS NULL, m.mapseqno
      LIMIT 1
    ) AS dictionary,
    ( SELECT pg_catalog.ts_lexize(mapdict, parse.token)
      FROM pg_catalog.pg_ts_config_map AS m
      WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
      ORDER BY pg_catalog.ts_lexize(mapdict, parse.token) IS NULL, m.mapseqno
      LIMIT 1
    ) AS lexemes
FROM pg_catalog.ts_parse(
        (SELECT cfgparser FROM pg_catalog.pg_ts_config WHERE oid = $1 ), $2 
    ) AS parse,
     pg_catalog.ts_token_type(
        (SELECT cfgparser FROM pg_catalog.pg_ts_config WHERE oid = $1 )
    ) AS tt
WHERE tt.tokid = parse.tokid
$$
LANGUAGE SQL STRICT STABLE;

COMMENT ON FUNCTION ts_debug(regconfig,text) IS
    'debug function for text search configuration';

CREATE FUNCTION ts_debug(IN document text,
    OUT alias text,
    OUT description text,
    OUT token text,
    OUT dictionaries regdictionary[],
    OUT dictionary regdictionary,
    OUT lexemes text[])
RETURNS SETOF record AS
$$
    SELECT * FROM pg_catalog.ts_debug( pg_catalog.get_current_ts_config(), $1);
$$
LANGUAGE SQL STRICT STABLE;

COMMENT ON FUNCTION ts_debug(text) IS
    'debug function for current text search configuration';

--
-- Redeclare built-in functions that need default values attached to their
-- arguments.  It's impractical to set those up directly in pg_proc.h because
-- of the complexity and platform-dependency of the expression tree
-- representation.  (Note that internal functions still have to have entries
-- in pg_proc.h; we are merely causing their proargnames and proargdefaults
-- to get filled in.)
--

CREATE OR REPLACE FUNCTION
  pg_start_backup(label text, fast boolean DEFAULT false)
  RETURNS text STRICT VOLATILE LANGUAGE internal AS 'pg_start_backup';

CREATE OR REPLACE FUNCTION
  json_populate_record(base anyelement, from_json json, use_json_as_text boolean DEFAULT false)
  RETURNS anyelement LANGUAGE internal STABLE AS 'json_populate_record';

CREATE OR REPLACE FUNCTION
  json_populate_recordset(base anyelement, from_json json, use_json_as_text boolean DEFAULT false)
  RETURNS SETOF anyelement LANGUAGE internal STABLE ROWS 100  AS 'json_populate_recordset';
