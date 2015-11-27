#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
from gppylib.mainUtils import addMasterDirectoryOptionForSingleClusterProgram, addStandardLoggingAndHelpOptions, gp, simple_main, \
                              ExceptionNoStackTraceNeeded, UserAbortedException
import os
import re
import shutil
import socket
import stat
import sys
import time
from datetime import datetime
from optparse import OptionGroup
from sets import Set

try:
    import gpmfr
    import threading
    import yaml
    from collections import defaultdict
    from getpass import getpass
    from gppylib import gplog
    from gppylib import pgconf
    from gppylib import userinput
    from gppylib.commands.base import Command
    from gppylib.db import dbconn
    from gppylib.gparray import GpArray
    from gppylib.gpparseopts import OptParser, OptChecker
    from gppylib.operations import Operation
    from gppylib.operations.backup_utils import check_funny_chars_in_tablenames, create_temp_file_from_list, expand_partitions_and_populate_filter_file, \
                                                generate_files_filename, generate_pipes_filename, generate_schema_filename, get_backup_directory, \
                                                get_latest_full_dump_timestamp, get_latest_full_ts_with_nbu, get_lines_from_file, remove_file_from_segments, \
                                                validate_timestamp, verify_lines_in_file, write_lines_to_file
    from gppylib.operations.dump import CreateIncrementsFile, DeleteCurrentDump, DeleteOldestDumps, DumpConfig, DumpDatabase, DumpGlobal, \
                                        MailDumpEvent, PostDumpDatabase, UpdateHistoryTable, VacuumDatabase, ValidateDatabaseExists, ValidateGpToolkit, \
                                        ValidateSchemaExists, backup_cdatabase_file_with_nbu, backup_config_files_with_nbu, backup_dirty_file_with_nbu, \
                                        backup_global_file_with_nbu, backup_increments_file_with_ddboost, backup_increments_file_with_nbu, \
                                        backup_partition_list_file_with_nbu, backup_report_file_with_ddboost, backup_report_file_with_nbu, \
                                        backup_schema_file_with_ddboost, backup_schema_file_with_nbu, backup_state_files_with_nbu, filter_dirty_tables, \
                                        generate_dump_timestamp, get_ao_partition_state, get_co_partition_state, get_dirty_heap_tables, get_dirty_tables, \
                                        get_filter_file, get_include_schema_list_from_exclude_schema, get_last_operation_data, get_user_table_list_for_schema, \
                                        update_filter_file, validate_current_timestamp, write_dirty_file, write_dirty_file_to_temp, write_last_operation_file, \
                                        write_partition_list_file, write_state_file
    from gppylib.operations.utils import DEFAULT_NUM_WORKERS
except ImportError, e:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

INJECT_ROLLBACK = False

EXECNAME = 'gpcrondump'
GPCRONDUMP_PID_FILE = 'gpcrondump.pid'
FREE_SPACE_PERCENT = 10
CATALOG_SCHEMA = [
    'gp_toolkit',
    'information_schema',
    'pg_aoseg',
    'pg_bitmapindex',
    'pg_catalog',
    'pg_toast'
]

logger = gplog.get_default_logger()
conn = None
lock_file_name = None

class GpCronDump(Operation):

    def __init__(self, options, args):
        if args:
            logger.warn("please note that some of the arguments (%s) aren't valid and will be ignored.", args)
        if options.masterDataDirectory is None:
            options.masterDataDirectory = gp.get_masterdatadir()
        self.interactive = options.interactive
        self.master_datadir = options.masterDataDirectory
        self.master_port = self._get_master_port(self.master_datadir)
        self.pgport = self._get_pgport()
        self.options_list = " ".join(sys.argv[1:])
        self.cur_host = socket.gethostname()

        self.clear_dumps_only = options.clear_dumps_only
        self.post_script = options.post_script
        self.dump_config = options.dump_config
        self.history = options.history
        self.pre_vacuum = options.pre_vacuum
        self.post_vacuum = options.post_vacuum
        self.rollback = options.rollback

        self.compress = options.compress
        self.free_space_percent = options.free_space_percent
        self.clear_dumps = options.clear_dumps
        self.dump_schema = options.dump_schema
        self.include_schema_file = options.include_schema_file
        self.exclude_schema_file = options.exclude_schema_file
        self.exclude_dump_schema = options.exclude_dump_schema
        self.dump_databases = options.dump_databases
        self.dump_global = options.dump_global
        self.clear_catalog_dumps = options.clear_catalog_dumps
        self.batch_default = options.batch_default
        self.include_dump_tables = options.include_dump_tables
        self.exclude_dump_tables = options.exclude_dump_tables
        self.include_dump_tables_file = options.include_dump_tables_file
        self.exclude_dump_tables_file = options.exclude_dump_tables_file
        self.backup_dir = options.backup_dir
        self.encoding = options.encoding
        self.output_options = options.output_options
        self.incremental = options.incremental
        self.timestamp_key = options.timestamp_key
        self.list_backup_files = options.list_backup_files
        self.list_filter_tables = options.list_filter_tables

        if options.local_dump_prefix:
             self.dump_prefix = options.local_dump_prefix + "_"
        else:
            self.dump_prefix = options.local_dump_prefix

        self.include_email_file = options.include_email_file
        self.email_details = None
        self.ddboost_hosts = options.ddboost_hosts
        self.ddboost_user = options.ddboost_user
        self.ddboost_config_remove = options.ddboost_config_remove
        self.ddboost_verify = options.ddboost_verify
        self.ddboost_remote = options.ddboost_remote
        self.ddboost_ping = options.ddboost_ping
        self.ddboost_backupdir = options.ddboost_backupdir
        # This variable indicates whether we need to exit after verifying the DDBoost credentials
        self.ddboost_verify_and_exit = False
        # NetBackup params
        self.netbackup_service_host = options.netbackup_service_host
        self.netbackup_policy = options.netbackup_policy
        self.netbackup_schedule = options.netbackup_schedule
        self.netbackup_block_size = options.netbackup_block_size
        self.netbackup_keyword = options.netbackup_keyword
        if (self.netbackup_keyword is not None) and (len(self.netbackup_keyword) > 100):
            raise Exception('NetBackup Keyword provided has more than max limit (100) characters. Cannot proceed with backup.')

        # TODO: if action is 'append', wouldn't you expect a lack of input to result in [], as opposed to None?
        if self.include_dump_tables is None: self.include_dump_tables = []
        if self.exclude_dump_tables is None: self.exclude_dump_tables = []
        if self.output_options is None: self.output_options = []
        if self.ddboost_hosts is None: self.ddboost_hosts = []
        if self.dump_schema is None: self.dump_schema = []
        if self.exclude_dump_schema is None: self.exclude_dump_schema = []

        if options.report_dir:
            logger.warn("-y is a deprecacted option.  Report files are always generated with the backup set.")

        if self.incremental and not self.dump_databases:
            raise ExceptionNoStackTraceNeeded("Must supply -x <database name> with incremental option")

        if self.dump_databases is not None:
            if self.incremental and len(self.dump_databases.split(',')) > 1:
                raise ExceptionNoStackTraceNeeded('multi-database backup is not supported with incremental backup: %s databases selected' % len(self.dump_databases))

        if self.clear_dumps_only and self.incremental:
            raise Exception('-o option cannot be selected with incremental backup')

        if self.list_backup_files and not self.timestamp_key:
            raise Exception('Must supply -K option when listing backup files')

        if self.dump_databases is not None:
            if self.timestamp_key and len(self.dump_databases.split(',')) > 1:
                raise ExceptionNoStackTraceNeeded('multi-database backup is not supported with -K option')

        if not (self.clear_dumps_only or bool(self.ddboost_hosts) or bool(self.ddboost_user) or self.ddboost_config_remove):
            if self.dump_databases is not None:
                self.dump_databases = self.dump_databases.split(",")
            elif 'PGDATABASE' in os.environ:
                self.dump_databases = [os.environ['PGDATABASE']]
                logger.info("Setting dump database to value of $PGDATABASE which is %s" % self.dump_databases[0])
            else:
                if self.ddboost_verify is True:
                    # We would expect to verify the credentials here and return some exit code,
                    # but __init__() should return None, not 'int' - hence we are forced to use
                    # some kind of flag
                    self.ddboost_verify_and_exit = True
                else:
                    raise ExceptionNoStackTraceNeeded("Must supply -x <database name> because $PGDATABASE is not set")

        self.ddboost = options.ddboost

        if self.list_backup_files and (self.ddboost or bool(self.ddboost_verify) or bool(self.ddboost_hosts) or bool(self.ddboost_user) or bool(self.ddboost_config_remove)):
            raise Exception('list backup files not supported with ddboost option')

        if self.list_filter_tables and (not self.dump_prefix or not self.incremental):
            raise Exception('list filter tables option requires --prefix and --incremental')

        if self.include_email_file:
            self._validate_parse_email_File()

        # NetBackup params check
        if self.netbackup_service_host or self.netbackup_policy or self.netbackup_schedule:
            if self.netbackup_service_host is None:
                raise Exception("NetBackup service hostname (--netbackup-service-host) param should be provided in order to use NetBackup")
            if self.netbackup_policy is None:
                raise Exception("NetBackup policy name (--netbackup-policy) param should be provided in order to use NetBackup")
            if self.netbackup_schedule is None:
                raise Exception("NetBackup schedule name (--netbackup-schedule) param should be provided in order to use NetBackup")

        if self.ddboost and self.netbackup_service_host:
            raise Exception('--ddboost is not supported with NetBackup')

        self.replicate = options.replicate
        self.max_streams = options.max_streams
        self.quiet = options.quiet
        self.verbose = options.verbose

        # disk space checks
        if self.free_space_percent and self.ddboost:
            raise Exception('-f option cannot be selected with ddboost')

        if self.free_space_percent and self.incremental:
            raise Exception('-f option cannot be selected with incremental backup')

        if self.free_space_percent:
            self.free_space_percent = int(self.free_space_percent)
        else:
            self.free_space_percent = FREE_SPACE_PERCENT

        if self.ddboost:
            self.free_space_percent = None
            logger.info('Bypassing disk space checks due to DDBoost parameters')
        elif self.incremental:
            self.free_space_percent = None
            logger.info('Bypassing disk space checks for incremental backup')
        elif options.bypass_disk_check:
            logger.info("Bypassing disk space check as '-b' option is specified")
            self.free_space_percent = None

        if self.ddboost:
            if (not self.replicate and self.max_streams is not None) or (self.replicate and self.max_streams is None):
                raise ExceptionNoStackTraceNeeded("--max-streams must be specified along with --replicate")
            if self.replicate:
                try:
                    if int(self.max_streams) <= 0:
                        raise ValueError()
                except ValueError:
                    raise ExceptionNoStackTraceNeeded("--max-streams must be a number greater than zero")
                # List of tuples (dbname, timestamp), to be replicated to remote
                # Data Domain after they are backed up to local Data Domain.
                self.dump_timestamps = []
            if self.backup_dir is not None:
                raise ExceptionNoStackTraceNeeded('-u cannot be used with DDBoost parameters.')
            dd = gpmfr.DDSystem("local")
            self.dump_dir = dd.defaultBackupDir
        elif self.replicate or self.max_streams:
            raise ExceptionNoStackTraceNeeded("--replicate and --max-streams cannot be used without --ddboost")
        else:
            self.dump_dir = 'db_dumps'

        # Cannot combine include and exclude schema filters
        if self.exclude_schema_file and self.dump_schema:
            raise Exception('-s can not be selected with --exclude-schema-file option')

        if self.include_schema_file and self.dump_schema:
            raise Exception('-s can not be selected with --schema-file option')

        if self.dump_schema and self.exclude_dump_schema:
            raise Exception('-s can not be selected with -S option')

        if self.exclude_schema_file and self.exclude_dump_schema:
            raise Exception('-S can not be selected with --exclude-schema-file option')

        if self.include_schema_file and self.exclude_dump_schema:
            raise Exception('-S can not be selected with --schema-file option')

        if self.include_schema_file and self.exclude_schema_file:
            raise Exception('--exclude-schema-file can not be selected with --schema-file option')


        # Incremental selections not allowed with schema filters
        if self.dump_schema and self.incremental:
            raise Exception('-s option can not be selected with incremental backup')

        if self.exclude_dump_schema and self.incremental:
            raise Exception('-S option can not be selected with incremental backup')

        if self.include_schema_file and self.incremental:
            raise Exception('--schema-file option can not be selected with incremental backup')

        if self.exclude_schema_file and self.incremental:
            raise Exception('--exclude-schema-file option can not be selected with incremental backup')


        # Cannot combine table and schema filters
        if self.exclude_schema_file and (self.include_dump_tables_file or self.exclude_dump_tables_file):
            raise Exception('--table-file and --exclude-table-file can not be selected with --exclude-schema-file option')

        if self.include_schema_file and (self.include_dump_tables_file or self.exclude_dump_tables_file):
            raise Exception('--table-file and --exclude-table-file can not be selected with --schema-file option')

        if self.dump_schema and (self.include_dump_tables_file or self.exclude_dump_tables_file):
            raise Exception('--table-file and --exclude-table-file can not be selected with -s option')

        if self.exclude_dump_schema and (self.include_dump_tables_file or self.exclude_dump_tables_file):
            raise Exception('--table-file and --exclude-table-file can not be selected with -S option')

        if self.exclude_schema_file and (self.include_dump_tables or self.exclude_dump_tables):
            raise Exception('-t and -T can not be selected with --exclude-schema-file option')

        if self.include_schema_file and (self.include_dump_tables or self.exclude_dump_tables):
            raise Exception('-t and -T can not be selected with --schema-file option')

        if self.dump_schema and (self.include_dump_tables or self.exclude_dump_tables):
            raise Exception('-t and -T can not be selected with -s option')

        if self.exclude_dump_schema and (self.include_dump_tables or self.exclude_dump_tables):
            raise Exception('-t and -T can not be selected with -S option')


        if self.include_dump_tables and self.incremental:
            raise Exception('include table list can not be selected with incremental backup')

        if self.exclude_dump_tables and self.incremental:
            raise Exception('exclude table list can not be selected with incremental backup')

        if self.include_dump_tables_file and self.incremental:
            raise Exception('include table file can not be selected with incremental backup')

        if self.exclude_dump_tables_file and self.incremental:
            raise Exception('exclude table file can not be selected with incremental backup')

        if self.clear_dumps and self.incremental:
            raise Exception('-c option can not be selected with incremental backup')

        if self.clear_catalog_dumps and self.incremental:
            raise Exception('-C option can not be selected with incremental backup')

        if self.include_schema_file and self.include_dump_tables:
            raise Exception('-t can not be selected with --schema-file option')

        if self.include_dump_tables and self.include_dump_tables_file:
            raise Exception('-t can not be selected with --table-file option')

        if self.include_dump_tables and self.exclude_dump_tables_file:
            raise Exception('-t can not be selected with --exclude-table-file option')

        if self.exclude_dump_tables and self.exclude_dump_tables_file:
            raise Exception('-T can not be selected with --exclude-table-file option')

        if self.exclude_dump_tables and self.include_dump_tables_file:
            raise Exception('-T can not be selected with --table-file option')

        if self.include_dump_tables and self.exclude_dump_tables:
            raise Exception('-t can not be selected with -T option')

        if self.include_dump_tables_file and self.exclude_dump_tables_file:
            raise Exception('--table-file can not be selected with --exclude-table-file option')

        if ('--inserts' in self.output_options or '--oids' in self.output_options or '--column-inserts' in self.output_options) and self.incremental:
            raise Exception('--inserts, --column-inserts, --oids cannot be selected with incremental backup')

        if self.include_schema_file or self.dump_schema or self.exclude_schema_file or self.exclude_dump_schema:
            self.validate_dump_schema()

        if self.incremental:
            if self.netbackup_service_host is None:
                self.full_dump_timestamp = get_latest_full_dump_timestamp(self.dump_databases[0], self.getBackupDirectoryRoot(), self.dump_dir, self.dump_prefix, self.ddboost)
            else:
                self.full_dump_timestamp = get_latest_full_ts_with_nbu(self.dump_databases[0], self.getBackupDirectoryRoot(), self.dump_prefix, self.netbackup_service_host, self.netbackup_block_size)

    def validate_dump_schema(self):
        if self.dump_schema:
            for schema in self.dump_schema:
                if schema in CATALOG_SCHEMA:
                    raise Exception("can not specify catalog schema '%s' using -s option" % schema)
        elif self.include_schema_file:
            schema_list = get_lines_from_file(self.include_schema_file)
            for schema in schema_list:
                if schema in CATALOG_SCHEMA:
                    raise Exception("can not include catalog schema '%s' in schema file '%s'" % (schema, self.include_schema_file))
        elif self.exclude_dump_schema:
            for schema in self.exclude_dump_schema:
                if schema in CATALOG_SCHEMA:
                    raise Exception("can not specify catalog schema '%s' using -S option" % schema)
        elif self.exclude_schema_file:
            schema_list = get_lines_from_file(self.exclude_schema_file)
            for schema in schema_list:
                if schema in CATALOG_SCHEMA:
                    raise Exception("can not exclude catalog schema '%s' in schema file '%s'" % (schema, self.exclude_schema_file))

    def getBackupDirectoryRoot(self):
        if self.backup_dir:
            return self.backup_dir
        else:
            return self.master_datadir

    def getGpArray(self):
        return GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)

    def getHostSet(self, gparray):
        hostlist = gparray.get_hostlist(includeMaster=True)
        hostset = Set(hostlist)
        return hostset

    # check if one of gpcrondump option was specified, except of ddboost options
    def only_ddboost_options(self):
        return ((self.dump_databases is None) and (not self.dump_schema)
                and (self.backup_dir is None)
                and not (self.include_dump_tables or self.output_options or self.clear_dumps_only or self.pre_vacuum
                         or self.clear_dumps_only or self.dump_global or self.rollback or self.exclude_dump_tables
                         or self.dump_config or self.clear_dumps_only or self.clear_dumps or self.post_vacuum or self.history))

    def validateUserName(self, userName):
        if not re.search(r'^[a-zA-Z0-9-_]{1,30}$', userName):
            legal_username_str = """
			The username length must be between 1 to 30 characters.

			The following characters are allowed:
                1) Lowercase letters (a-z)
                2) Uppercase letters (A-Z)
                3) Numbers (0-9)
                4) Special characters (- and _)

		    Note: whitespace characters are not allowed.
			"""
            raise ExceptionNoStackTraceNeeded(legal_username_str)

    def validatePassword(self, password):
        if not re.search(r'^[a-zA-Z0-9!@#$%^&+=\*\\/\(\)-_~;:\'\"<>\{\}\[\]|\?\.\,`"]{1,40}$', password):
            legal_password_str = """
            The password length must be between 1 to 40 characters.

            The following characters are allowed:
                1) Lowercase letters (a-z)
                2) Uppercase letters (A-Z)
                3) Numbers (0-9)
                4) Special characters (! @ # $ % ^ & + = * \ / - _ ~ ; : ' " { [ ( < > ) ] } | ? . and ,).
            """
            raise ExceptionNoStackTraceNeeded(legal_password_str)

    def writeToFile(self, content, filePathName):
        f = open(filePathName, 'w')
        f.write(content)
        f.close()
        os.chmod(filePathName, stat.S_IREAD | stat.S_IWRITE)

    def removeDDBoostConfig(self, gpHostsSet):
        """
        Remove DD Boost configuration (local and remote) from master and segment
        servers.
        """
        for index, host in enumerate(gpHostsSet):
            ddcmd = "rm -f $HOME/DDBOOST*CONFIG*"
            logger.debug("Removing DD Boost config on host %s: %s" %
                         (host, ddcmd))
            cmdline = 'gpssh -h %s \'%s\'' % (host, ddcmd)
            if not os.system(cmdline):
                logger.info("config removed, host: %s " % host)
            else:
                logger.error("problem removing config, host: %s " % host)
                return
        logger.debug("DD Boost config removed from all GPDB servers.")

    def createDDBoostConfig(self, gpHostsSet, ddboostHostsSet, user, password, backupDir):
        """
        Creates config file for local/remote Data Domain on master and all the
        segment servers.  A brief outline of this function is as follows.

           1. [Optional] Ping Data Domain host.  Abort if DD host not reachable
           from master.

           2. Enforce that local and remote DD systems are different hosts.
           Block the user from configuring them as the same host.

           3. If local DD is being configured and a config file for local DD
           already exists on the master, show its content.  Prompt the user for
           replacing the contents.  Only if 'y', go ahead.  Else abort.
           Similarly for remote DD.

           4. After configuration is written, run "gpddboost --verify".  This
           may fail if username/password is incorrect.  It will also create a
           storage unit named "GPDB" on the DD if it doesn't already exist.

           5. The previous steps verify user-specified DD Boost config
           parameters.  Upon successful verification, this step crates the DD
           Boost config file on master and all segment hosts.

           Gotcha: One case when local and remote Data Domains may be
           misconfigured as the same host may go undetected in the current
           algorithm.  We currently compare new hostname being set for remote
           Data Domain with hostname of the local Data Domain that is already
           configured on the master node.  If they match, we flag the error and
           abort.  When a Data Domain has multiple NICs with different IP
           addresses for load balancing, gpcrondump distributes these addresses
           evenly within GPDB segment servers.  Consider the case when local
           Data Domain with IPs IP1 and IP2 is already configured on GPDB and
           the master node got IP1.  Now if the remote Data Domain is being
           configured by mistake with IP2, we will not detect the fact that IP2
           is the local Data Domain.  As a result, this misconfiguration will
           succeed without error.  As of Q1, 2013, we leave this issue to future
           releases.
        """
        logger.debug("DDboost host(s): %s" % ddboostHostsSet)
        numberOfDdboostNics = len(ddboostHostsSet)
        if self.ddboost_ping:
            for dd in ddboostHostsSet:
                cmd = Command("Ping DD Host", "ping -c 5 %s" % dd)
                cmd.run()
                if not cmd.was_successful():
                    msg = "Data Domain host %s not reachable by ping. " % dd
                    msg += "Use --ddboost-skip-ping option to skip this step."
                    logger.error(msg)
                    return
            logger.debug("Connectivity to Data Domain host(s) verified.")
        localDD = None
        remoteDD = None
        try:
            localDD = gpmfr.DDSystem("local")
            logger.debug(
                "Found existing configuration for local Data Domain.")
        except:
            pass
        try:
            remoteDD = gpmfr.DDSystem("remote")
            logger.debug(
                "Found existing configuration for remote Data Domain.")
        except:
            pass
        if self.ddboost_remote:
            pairDD = localDD
            selfDD = remoteDD
        else:
            pairDD = remoteDD
            selfDD = localDD
        if pairDD and pairDD.hostname in ddboostHostsSet:
            msg = "%s Data Domain is already configured with hostname %s." +\
                " Local and remote Data Domains must be different."
            logger.error(msg % (pairDD.id, pairDD.hostname))
            return
        if selfDD:
            logger.info("Found existing %s Data Domain configuration:" % \
                            selfDD.id)
            logger.info("\tHostname: %s" % selfDD.hostname)
            logger.info("\tDefault Backup Directory: %s" % \
                            selfDD.defaultBackupDir)
            if not userinput.ask_yesno(None,
                                       "\nDo you want to replace " +
                                       "this configuration?",
                                       'N'):
                return
        ddcmdTemplate = "source " + os.environ["GPHOME"] + \
            "/greenplum_path.sh; " + os.environ["GPHOME"] + \
            "/bin/gpddboost  --setCredential "+ \
            "--hostname=%s --user=" + re.escape(user)
        if self.ddboost_remote:
            ddcmdTemplate += " --remote"
        else:
            ddcmdTemplate += " --defaultBackupDirectory=" + backupDir
        # Create a DD Boost config on master for each DD Boost host.  Try
        # logging into each DD Boost using the config.
        for dd in ddboostHostsSet:
            ddcmd = ddcmdTemplate % dd
            logger.debug("Verifying configuration: "+ddcmd)
            ddcmd += " --password=" + re.escape(password)
            cmd = Command("Configuring DD Boost on localhost", ddcmd)
            cmd.run()
            if not cmd.was_successful():
                r = cmd.get_results()
                raise Exception("gpddboost failed to run on localhost. %s" % \
                                    r.printResult())
            if self.ddboost_remote:
                selfDD = gpmfr.DDSystem("remote")
            else:
                selfDD = gpmfr.DDSystem("local")
            # This step creates storage unit "GPDB" on the Data Domain if one
            # doesn't exist.  It raises exception on failure.
            logger.debug("Verifying connection to DD Host (%s)." % dd)
            selfDD.verifyLogin()
            logger.debug("Connection succeeded.")

        # At this point, verification (steps 1-4) are complete and step 5
        # starts.
        logger.debug("Writing config files on GPDB servers.")
        for index, host in enumerate(gpHostsSet):
            # If more then one NIC is specified, we configure the GP hosts with
            # these ddboost NICs in a round robin manner.  This is to address
            # the case when a single Data Domain host has multiple IP addresses
            # for load balancing.  We distribute segment servers evenly across
            # the NICs.
            currentDdboostHost = ddboostHostsSet[index % numberOfDdboostNics]
            ddcmd = ddcmdTemplate % currentDdboostHost
            logger.debug("Writing configuration on host %s: %s --password=*" %
                         (host, ddcmd))
            # Note: the password can't be printed to log/stdout and therefore
            # was concatenated to ddcmd only after priniting it to log.
            ddcmd += " --password=" + re.escape(password)
            cmdline = 'gpssh -h %s \'%s\'' % (host, ddcmd)
            if 0 == os.system(cmdline):
                logger.info("config delivered successfully, host: %s " % host)
            else:
                logger.error("problem delivering config, host: %s " % host)
                return
        logger.debug("DD Boost configured on all GPDB servers.")

    # Verify the DDBoost credentials using the credentials that stored on the Master
    # TODO: verify also all the hosts in self.ddboost_hosts (ping to the hosts, I think there is some Python function for that)
    def _ddboostVerify(self):
        verifyCmdLine = os.environ['GPHOME'] + '/bin/gpddboost --verify'
        logger.debug("Executing gpddboost to verify DDBoost credentials: %s" % verifyCmdLine)
        if not os.system(verifyCmdLine):
            logger.info("The specified DDBoost credentials are OK")
        else:
            raise ExceptionNoStackTraceNeeded('Failed to connect to DD_host with DD_user and the DD_password')

    # Creating a temp file from the schema list provided by user,
    # if include_schema_file is not provided else returns include_schema_file
    def get_schema_list_file(self, dbname):
        schema_file = None
        if self.include_schema_file:
            schema_file = self.include_schema_file
        elif self.dump_schema:
            schema_file = create_temp_file_from_list(self.dump_schema, 'schema_list')
            self.dump_schema = []
        elif self.exclude_dump_schema:
            schema_list = get_include_schema_list_from_exclude_schema(self.exclude_dump_schema, CATALOG_SCHEMA, self.master_port, dbname)
            schema_file = create_temp_file_from_list(schema_list, 'schema_list')
            self.exclude_dump_schema = []
        elif self.exclude_schema_file:
            schema_list = get_include_schema_list_from_exclude_schema(get_lines_from_file(self.exclude_schema_file), CATALOG_SCHEMA, self.master_port, dbname)
            schema_file = create_temp_file_from_list(schema_list, 'schema_list')

        return schema_file

    # This method is called only when prefix option is specified with schema filter,
    # for full backup.
    # Method updates the include_dump_tables_file based on the schema list provided,
    # i.e All the tables in the specified schema's are added to the include_dump_tables_file.

    def generate_include_table_list_from_schema_file(self, dbname, schema_file):
        dump_schemas = get_lines_from_file(schema_file)
        table_list = []
        for schema in dump_schemas:
            tables = get_user_table_list_for_schema(self.master_port, dbname, schema)
            for table in tables:
                table_name = table[0].strip() + "." + table[1].strip()
                table_list.append(table_name)
        return create_temp_file_from_list(table_list, 'include_dump_tables_file')

    def get_include_exclude_for_dump_database(self, dirty_file, dbname):
        include_file = None
        exclude_file = None

        if self.incremental:
            return (dirty_file, exclude_file)

        if self.include_dump_tables:
            include_file = expand_partitions_and_populate_filter_file(dbname,
                                                                      self.include_dump_tables,
                                                                      'include_dump_tables_file')
            self.include_dump_tables = []

        if self.exclude_dump_tables:
            exclude_file = expand_partitions_and_populate_filter_file(dbname,
                                                                      self.exclude_dump_tables,
                                                                      'exclude_dump_tables_file')
            self.exclude_dump_tables = []

        if self.include_dump_tables_file:
            include_tables = get_lines_from_file(self.include_dump_tables_file)
            include_file = expand_partitions_and_populate_filter_file(dbname,
                                                                      include_tables,
                                                                      'include_dump_tables_file')

        if self.exclude_dump_tables_file:
            exclude_tables = get_lines_from_file(self.exclude_dump_tables_file)
            exclude_file = expand_partitions_and_populate_filter_file(dbname,
                                                                      exclude_tables,
                                                                      'exclude_dump_tables_file')

        return (include_file, exclude_file)

    # Thread to poll for existence of a signal file telling us that we can drop the pg_class lock
    def lock_handler(self, timestamp, thread_stop):
        dump_dir = get_backup_directory(self.master_datadir, self.backup_dir, self.dump_dir, timestamp)
        global lock_file_name
        lock_file_name = "%s/gp_lockfile_%s" % (dump_dir, timestamp[8:])
        while not thread_stop.is_set():
            if os.path.exists(lock_file_name):
                thread_stop.set()
                try:
                    global conn
                    if conn:
                        logger.info("Releasing pg_class lock")
                        conn.commit()
                        conn.close()
                        conn = None
                    else:
                        logger.debug("Did not release pg_class lock because connection was already closed.")
                except Exception, e:
                    logger.info("Failed to release pg_class lock, backup will proceed as normal.")
            thread_stop.wait(5)
        try:
            # Check that the file still exists and if so remove it.
            # This avoids throwing the exception if the file has already been removed by the main thread.
            if os.path.exists(lock_file_name):
                os.remove(lock_file_name)
        # This should only throw the Exception if "os.remove" fails which means there was a genuine failure.
        except Exception, e:
            logger.warning("Could not remove signal file %s, backup will proceed without releasing pg_class lock." % lock_file_name)

    def execute(self):
        if bool(self.ddboost_hosts) or bool(self.ddboost_user):
            # check that both options are specified
            if not (bool(self.ddboost_hosts) and bool(self.ddboost_user)):
                raise ExceptionNoStackTraceNeeded('For DDBoost config, both options are required --ddboost-host <ddboost_hostname> --ddboost-user <ddboost_user>.')

            # check that no other gpcrondump option was specified
            if self.only_ddboost_options() and not self.ddboost_config_remove:
                if not self.ddboost_remote and not self.ddboost_backupdir:
                    raise ExceptionNoStackTraceNeeded("--ddboost-backupdir must be specified when configuring local Data Domain.")
                self.validateUserName(userName=self.ddboost_user)
                hostset = self.getHostSet(self.getGpArray())
                p = getpass()
                self.validatePassword(password=p)
                self.createDDBoostConfig(hostset, self.ddboost_hosts, self.ddboost_user, p, self.ddboost_backupdir)
                return 0
            else:
                raise ExceptionNoStackTraceNeeded('The options --ddboost-host and --ddboost-user are standalone. They are NOT used in conjunction with any other gocrondump options (only with --ddboost-verify).')

        # If --ddboost-verify or --ddboost is specified, we check the credentials that stored on the Master
        # In case of --ddboost we force the verification (MPP-17510)
        if bool(self.ddboost_verify) or bool(self.ddboost):
            self._ddboostVerify()

            # Check if we need to exit now
            if self.ddboost_verify_and_exit is True:
                return 0

        if self.ddboost_config_remove:
            if self.only_ddboost_options():
                hostset = self.getHostSet(self.getGpArray())
                # The username and the password must be at least 2 characters long, so we just use 2 spaces for each
                self.removeDDBoostConfig(hostset)
                return 0
            else:
                raise ExceptionNoStackTraceNeeded('The option --ddboost-config-remove is standalone. It is NOT used in conjunction with any other gocrondump options.')

        if self.clear_dumps_only:
            generate_dump_timestamp(self._get_timestamp_object(self.timestamp_key))
            DeleteOldestDumps(master_datadir=self.master_datadir,
                              master_port=self.master_port,
                              dump_dir=self.dump_dir,
                              ddboost=self.ddboost).run()
            return

        if self.post_script is not None:
            self._validate_run_program()

        self._validate_dump_target()

        dump_db_info_list = []
        # final_exit_status := numeric outcome of this python program
        # current_exit_status := numeric outcome of one particular dump,
        #                        in this loop over the -x arguments
        final_exit_status = current_exit_status = 0

        for dump_database in self.dump_databases:
            timestamp = self._get_timestamp_object(self.timestamp_key)
            timestamp_str = timestamp.strftime('%Y%m%d%H%M%S')
            generate_dump_timestamp(timestamp)
            if self.list_backup_files:
                self._list_backup_files(timestamp_str)
                logger.info('Successfully listed the names of backup files and pipes')
                return 0
            if self.list_filter_tables:
                db = self.dump_databases[0]
                self._list_filter_tables(db, get_filter_file(db, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.ddboost))
                return 0

            validate_current_timestamp(self.getBackupDirectoryRoot(), self.dump_dir, self.dump_prefix)
            if self.interactive:
                self._prompt_continue(dump_database)

            if self.pre_vacuum:
                logger.info('Commencing pre-dump vacuum')
                VacuumDatabase(database=dump_database,
                               master_port=self.master_port).run()

            if self.free_space_percent is not None:
                ValidateGpToolkit(database=dump_database,
                                  master_port=self.master_port).run()

            global conn
            conn = None
            try:
                thread_stop = threading.Event()
                lock_thread = threading.Thread(target=self.lock_handler, args=(timestamp_str, thread_stop))
                lock_thread.start()

                LOCK_TABLE_SQL = "LOCK TABLE pg_catalog.pg_class IN EXCLUSIVE MODE;"
                conn = dbconn.connect(dbconn.DbURL(port=self.pgport, dbname=dump_database), utility=True)
                dbconn.execSQL(conn, LOCK_TABLE_SQL)

                ao_partition_list = get_ao_partition_state(self.master_port, dump_database)
                co_partition_list = get_co_partition_state(self.master_port, dump_database)
                heap_partitions = get_dirty_heap_tables(self.master_port, dump_database)
                last_operation_list = get_last_operation_data(self.master_port, dump_database)

                self._verify_tablenames(ao_partition_list, co_partition_list, heap_partitions)

                dirty_partitions = None
                dirty_file = None
                if self.incremental:
                    dirty_partitions = get_dirty_tables(self.master_port, dump_database,
                                                        self.master_datadir, self.backup_dir,
                                                        self.dump_dir, self.dump_prefix,
                                                        self.full_dump_timestamp, ao_partition_list,
                                                        co_partition_list, last_operation_list,
                                                        self.netbackup_service_host, self.netbackup_block_size)
                    if self.dump_prefix and \
                       get_filter_file(dump_database, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix,
                                       self.ddboost, self.netbackup_service_host, self.netbackup_block_size):
                        update_filter_file(dump_database, self.master_datadir, self.backup_dir, self.master_port, self.dump_dir, self.dump_prefix,
                                           self.ddboost, self.netbackup_service_host, self.netbackup_policy,
                                           self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)
                    dirty_partitions = filter_dirty_tables(dirty_partitions, dump_database, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix,
                                                           self.ddboost, self.netbackup_service_host, self.netbackup_block_size)
                    dirty_file = write_dirty_file_to_temp(dirty_partitions)

                schema_file = self.get_schema_list_file(dump_database)
                (include_file, exclude_file) = self.get_include_exclude_for_dump_database(dirty_file, dump_database)

                if self.dump_prefix and schema_file and not self.incremental:
                    include_file = self.generate_include_table_list_from_schema_file(dump_database, schema_file)

                dump_outcome = DumpDatabase(dump_database = dump_database,
                                            dump_schema = self.dump_schema,
                                            include_dump_tables = self.include_dump_tables,
                                            exclude_dump_tables = self.exclude_dump_tables,
                                            include_dump_tables_file = include_file,
                                            exclude_dump_tables_file = exclude_file,
                                            backup_dir = self.backup_dir,
                                            free_space_percent = self.free_space_percent,
                                            compress = self.compress,
                                            clear_catalog_dumps = self.clear_catalog_dumps,
                                            encoding = self.encoding,
                                            output_options = self.output_options,
                                            batch_default = self.batch_default,
                                            master_datadir = self.master_datadir,
                                            master_port = self.master_port,
                                            dump_dir = self.dump_dir,
                                            dump_prefix = self.dump_prefix,
                                            ddboost = self.ddboost,
                                            netbackup_service_host = self.netbackup_service_host,
                                            netbackup_policy = self.netbackup_policy,
                                            netbackup_schedule = self.netbackup_schedule,
                                            netbackup_block_size = self.netbackup_block_size,
                                            netbackup_keyword = self.netbackup_keyword,
                                            incremental = self.incremental,
                                            include_schema_file = schema_file).run()

                post_dump_outcome = PostDumpDatabase(timestamp_start = dump_outcome['timestamp_start'],
                                                     compress = self.compress,
                                                     backup_dir = self.backup_dir,
                                                     batch_default = self.batch_default,
                                                     master_datadir = self.master_datadir,
                                                     master_port = self.master_port,
                                                     dump_dir = self.dump_dir,
                                                     dump_prefix = self.dump_prefix,
                                                     ddboost = self.ddboost,
                                                     netbackup_service_host = self.netbackup_service_host,
                                                     incremental = self.incremental).run()
                if self.netbackup_service_host and self.netbackup_policy and self.netbackup_schedule:
                    backup_cdatabase_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)
                    backup_report_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)

                if self.incremental:
                    if os.path.isfile(dirty_file):
                        if self.ddboost:
                            time.sleep(5)
                        os.remove(dirty_file)
                        remove_file_from_segments(self.master_port, dirty_file)
                    write_dirty_file(self.master_datadir, dirty_partitions, self.backup_dir, self.dump_dir, self.dump_prefix, None, self.ddboost)
                    if self.netbackup_service_host:
                        backup_dirty_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)

                if include_file is not None and os.path.exists(include_file):
                    os.remove(include_file)
                    remove_file_from_segments(self.master_port, include_file)
                if exclude_file is not None and os.path.exists(exclude_file):
                    os.remove(exclude_file)
                    remove_file_from_segments(self.master_port, exclude_file)
                if schema_file:
                    schema_filename = generate_schema_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp_str, self.ddboost)
                    shutil.copyfile(schema_file, schema_filename)
                    remove_file_from_segments(self.master_port, schema_file)
                if schema_file is not None and os.path.exists(schema_file):
                    os.remove(schema_file)

                write_state_file('ao', self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, ao_partition_list, self.ddboost)
                write_state_file('co', self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, co_partition_list, self.ddboost)
                write_last_operation_file(self.master_datadir, self.backup_dir, last_operation_list, self.dump_dir, self.dump_prefix, timestamp_key=None, ddboost=self.ddboost)

                if self.netbackup_service_host and self.netbackup_policy and self.netbackup_schedule:
                    backup_state_files_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host,
                                                self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)
                    schema_filename = generate_schema_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp_str, self.ddboost)
                    if os.path.exists(schema_filename) and not self.incremental:
                        backup_schema_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host,
                                                    self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)

                if self.ddboost:
                    backup_report_file_with_ddboost(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix)
                    schema_filename = generate_schema_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp_str, self.ddboost)
                    if os.path.exists(schema_filename) and not self.incremental:
                        backup_schema_file_with_ddboost(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix)

                current_exit_status = max(dump_outcome['exit_status'], post_dump_outcome['exit_status'])
                if self.replicate and current_exit_status == 0:
                    self.dump_timestamps.append((dump_database, post_dump_outcome["timestamp"]))
                final_exit_status = max(final_exit_status, current_exit_status)

                if self.incremental:
                    if final_exit_status:
                        logger.info('non-zero exit status from gp_dump, skipping generation of increments file')
                    else:
                        # if this fails, it raises an exception and bombs out RC and to the screen
                        CreateIncrementsFile(dump_database = dump_database,
                                             full_timestamp = self.full_dump_timestamp,
                                             timestamp = post_dump_outcome['timestamp'],
                                             master_datadir = self.master_datadir,
                                             backup_dir = self.backup_dir,
                                             ddboost = self.ddboost,
                                             dump_dir = self.dump_dir,
                                             dump_prefix = self.dump_prefix,
                                             netbackup_service_host = self.netbackup_service_host,
                                             netbackup_block_size = self.netbackup_block_size).run()
                        if self.ddboost:
                            backup_increments_file_with_ddboost(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.full_dump_timestamp)

                        write_partition_list_file(self.master_datadir, self.backup_dir, post_dump_outcome['timestamp'], \
                                                  self.master_port, dump_database, self.dump_dir, self.dump_prefix, self.ddboost, self.netbackup_service_host)
                        if self.netbackup_service_host:
                            backup_increments_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.full_dump_timestamp, self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)
                            backup_partition_list_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)

            finally:
                thread_stop.set()
                try:
                    if lock_file_name and os.path.exists(lock_file_name):
                        os.remove(lock_file_name)
                    if conn:
                        conn.commit()
                        conn.close()
                        conn = None
                except Exception, e:
                    logger.debug("Failed to remove signal file or close connection, lock_handler thread probably already got to it.")

            if self.history:
                # This certainly does not belong under DumpDatabase, due to CLI assumptions. Note the use of options_list.
                UpdateHistoryTable(dump_database = dump_database,
                                   options_list = self.options_list,
                                   time_start = dump_outcome['time_start'],
                                   time_end = dump_outcome['time_end'],
                                   dump_exit_status = dump_outcome['exit_status'],
                                   timestamp = post_dump_outcome['timestamp'],
                                   pseudo_exit_status = current_exit_status,
                                   master_port = self.master_port).run()

            if self.dump_global:
                if current_exit_status == 0:
                    DumpGlobal(timestamp = post_dump_outcome['timestamp'],
                               master_datadir = self.master_datadir,
                               master_port = self.master_port,
                               backup_dir = self.backup_dir,
                               dump_dir = self.dump_dir,
                               dump_prefix = self.dump_prefix,
                               ddboost = self.ddboost).run()
                    if self.netbackup_service_host and self.netbackup_policy and self.netbackup_schedule:
                        backup_global_file_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix,
                                                    self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)
                else:
                    logger.info("Skipping global dump due to issues during post-dump checks.")

            deleted_dump_set = None
            if current_exit_status > 0 or INJECT_ROLLBACK:
                if current_exit_status >= 2 and self.rollback:
                    # A current_exit_status of 2 or higher likely indicates either a serious issue in gp_dump
                    # or that the PostDumpDatabase could not determine a timestamp. Thus, do not attempt rollback.
                    logger.warn("Dump request was incomplete, however cannot rollback since no timestamp was found.")
                    MailDumpEvent("Report from gpcrondump on host %s [FAILED]" % self.cur_host,
                                  "Failed for database %s with return code %d, dump files not rolled back, because no new timestamp was found. [Start=%s End=%s] Options passed [%s]"
                                  % (dump_database, current_exit_status, dump_outcome['time_start'], dump_outcome['time_end'], self.options_list)).run()
                    raise ExceptionNoStackTraceNeeded("Dump incomplete, rollback not processed")
                elif self.rollback:
                    logger.warn("Dump request was incomplete, rolling back dump")
                    DeleteCurrentDump(timestamp = post_dump_outcome['timestamp'],
                                      master_datadir = self.master_datadir,
                                      master_port = self.master_port,
                                      dump_dir = self.dump_dir,
                                      ddboost = self.ddboost).run()
                    MailDumpEvent("Report from gpcrondump on host %s [FAILED]" % self.cur_host,
                                  "Failed for database %s with return code %d dump files rolled back. [Start=%s End=%s] Options passed [%s]"
                                  % (dump_database, current_exit_status, dump_outcome['time_start'], dump_outcome['time_end'], self.options_list)).run()
                    raise ExceptionNoStackTraceNeeded("Dump incomplete, completed rollback")
                else:
                    logger.warn("Dump request was incomplete, not rolling back because -r option was not supplied")
                    MailDumpEvent("Report from gpcrondump on host %s [FAILED]" % self.cur_host,
                                  "Failed for database %s with return code %s dump files not rolled back. [Start=%s End=%s] Options passed [%s]"
                                  % (dump_database, current_exit_status, dump_outcome['time_start'], dump_outcome['time_end'], self.options_list)).run()
                    raise ExceptionNoStackTraceNeeded("Dump incomplete, rollback not processed")
            else:
                if self.clear_dumps:
                    deleted_dump_set = DeleteOldestDumps(master_datadir = self.master_datadir,
                                                         master_port = self.master_port,
                                                         dump_dir = self.dump_dir,
                                                         ddboost = self.ddboost).run()

            if self.post_vacuum:
                logger.info('Commencing post-dump vacuum...')
                VacuumDatabase(database = dump_database,
                               master_port = self.master_port).run()

            dump_type = 'Incremental' if self.incremental else 'Full database'
            self._status_report(dump_database,
                                post_dump_outcome['timestamp'],
                                dump_outcome,
                                current_exit_status,
                                deleted_dump_set,
                                dump_type)

            if self.dump_config:
                dump_info_dict = defaultdict(dict)
                dump_info_dict['host'] = self.cur_host
                dump_info_dict['dbname'] = dump_database
                dump_info_dict['exit_status'] = current_exit_status
                dump_info_dict['start_time'] = dump_outcome['time_start']
                dump_info_dict['end_time'] = dump_outcome['time_end']
                dump_info_dict['options_passed'] = self.options_list

                dump_db_info_list.append(dump_info_dict)
            else:
                self._send_email(dump_database, current_exit_status, dump_outcome['time_start'], dump_outcome['time_end'])

        if self.dump_config:
            config_outcome = DumpConfig(backup_dir = self.backup_dir,
                                        master_datadir = self.master_datadir,
                                        master_port = self.master_port,
                                        dump_dir = self.dump_dir,
                                        dump_prefix = self.dump_prefix,
                                        ddboost = self.ddboost).run()
            if self.netbackup_service_host and self.netbackup_policy and self.netbackup_schedule:
                backup_config_files_with_nbu(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.master_port,
                                             self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword)

            for dump_db_info in dump_db_info_list:
                self._send_email(dump_db_info['dbname'], dump_db_info['exit_status'], dump_db_info['start_time'], dump_outcome['time_end'])

        if self.ddboost and self.replicate:
            logger.info("Backup to local Data Domain successful.")
            for name, ts in self.dump_timestamps:
                p = gpmfr.mfr_parser()
                arglist = ["--replicate", ts, "--max-streams", self.max_streams, "--master-port", str(self.master_port)]
                if self.quiet:
                    arglist.append("--quiet")
                if self.verbose:
                    arglist.append("--verbose")
                if not self.interactive:
                    arglist.append("-a")
                if not self.ddboost_ping:
                    arglist.append("--skip-ping")
                logger.info("Replicating %s to remote Data Domain. (gpmfr.py %s)" % (name, " ".join(arglist)))
                mfropt, mfrargs = p.parse_args(arglist, None)
                mfr = gpmfr.GpMfr(mfropt, mfrargs)
                try:
                    mfr.execute()
                except Exception, e:
                    logger.error(e)
                    logger.error("Backup was successful but replication to remote Data Domain failed.")
                finally:
                    mfr.restoreLogLevel()

        if self.post_script is not None:
            self._run_program()

        if final_exit_status == 0:
            os._exit(0)

        return final_exit_status

    def _get_files_file_list(self, master, dump_dir, timestamp):
        file_list = []
        master_file_list = ['%sgp_cdatabase_1_1_%s', '%sgp_dump_%s_ao_state_file', '%sgp_dump_%s_co_state_file',
                            '%sgp_dump_%s_last_operation', '%sgp_dump_%s.rpt', '%sgp_dump_status_1_1_%s']

        if self.dump_prefix and (self.include_dump_tables_file or self.exclude_dump_tables_file or \
                                         self.include_dump_tables or self.exclude_dump_tables):
            master_file_list.append('%sgp_dump_%s_filter')

        for master_file in master_file_list:
            file_list.append('%s:%s/%s' % (master.getSegmentHostName(), dump_dir,
                                           master_file % (self.dump_prefix, timestamp)))

        if self.incremental:
            file_list.append('%s:%s/%sgp_dump_%s_increments' % (master.getSegmentHostName(), dump_dir,
                                                                self.dump_prefix, self.full_dump_timestamp))

        return file_list

    def _get_pipes_file_list(self, master, segdbs, dump_dir, timestamp):
        pipe_list = []
        master_pipe_list = ['%sgp_dump_1_1_%s', '%sgp_dump_1_1_%s_post_data']
        seg_pipe = '%sgp_dump_0_%s_%s'
        config_pipe = '%sgp_master_config_files_%s.tar'
        seg_config = '%sgp_segment_config_files_0_%s_%s.tar'
        global_pipe = '%sgp_global_1_1_%s'

        for segdb in segdbs:
            seg_dump_dir = get_backup_directory(segdb.getSegmentDataDirectory(), self.backup_dir, self.dump_dir, str(timestamp))
            pipe_list.append('%s:%s/%s' % (segdb.getSegmentHostName(),
                                           seg_dump_dir,
                                           seg_pipe % (self.dump_prefix, segdb.getSegmentDbId(), timestamp)))
            if self.compress:
                pipe_list[-1] += '.gz'

        for pipe_file in master_pipe_list:
            pipe_list.append('%s:%s/%s' % (master.getSegmentHostName(), dump_dir,
                                           pipe_file % (self.dump_prefix, timestamp)))
            if self.compress:
                pipe_list[-1] += '.gz'

        if self.dump_global:
            pipe_list.append('%s:%s/%s' % (master.getSegmentHostName(), dump_dir,
                                           global_pipe % (self.dump_prefix, timestamp)))

        if self.dump_config:
            pipe_list.append('%s:%s/%s' % (master.getSegmentHostName(), dump_dir,
                                           config_pipe % (self.dump_prefix, timestamp)))
            for segdb in segdbs:
                seg_dump_dir = get_backup_directory(segdb.getSegmentDataDirectory(), self.backup_dir, self.dump_dir, str(timestamp))
                pipe_list.append('%s:%s/%s' % (segdb.getSegmentHostName(),
                                               seg_dump_dir,
                                               seg_config % (self.dump_prefix, segdb.getSegmentDbId(), timestamp)))

        return pipe_list

    def _list_backup_files(self, timestamp):
        pipe_list = []
        file_list = []

        gparray = self.getGpArray()
        segdbs = [segdb for segdb in gparray.getDbList() if segdb.isSegmentPrimary()]

        dump_dir = get_backup_directory(self.master_datadir, self.backup_dir, self.dump_dir, timestamp)
        pipe_list = self._get_pipes_file_list(gparray.master, segdbs, dump_dir, timestamp)
        file_list = self._get_files_file_list(gparray.master, dump_dir, timestamp)

        pipes_list_fname = generate_pipes_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        files_list_fname = generate_files_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

        dirname = os.path.dirname(pipes_list_fname)
        if not os.path.isdir(dirname):
            os.makedirs(dirname)

        write_lines_to_file(pipes_list_fname, pipe_list)
        verify_lines_in_file(pipes_list_fname, pipe_list)
        logger.info('Added the list of pipe names to the file: %s' % pipes_list_fname)

        write_lines_to_file(files_list_fname, file_list)
        verify_lines_in_file(files_list_fname, file_list)
        logger.info('Added the list of file names to the file: %s' % files_list_fname)

    def _list_filter_tables(self, db, filter_filename):
        logger.info("---------------------------------------------------")
        if not filter_filename:
            logger.info('No filter file found for database %s and prefix %s.' % (db, self.dump_prefix[:-1]))
            logger.info('All tables in %s will be included in the dump.' % db)
            return
        tables = get_lines_from_file(filter_filename)
        logger.info('Filtering %s for the following tables:' % db)
        for table in tables:
            logger.info('%s' % table)

    def _get_timestamp_object(self, timestamp_key):
        if timestamp_key is None:
            return datetime.now()

        if not validate_timestamp(timestamp_key):
            raise Exception('Invalid timestamp key')

        year = int(self.timestamp_key[:4])
        month = int(self.timestamp_key[4:6])
        day = int(self.timestamp_key[6:8])
        hours = int(self.timestamp_key[8:10])
        minutes = int(self.timestamp_key[10:12])
        seconds = int(self.timestamp_key[12:14])
        return datetime(year, month, day, hours, minutes, seconds)

    def _get_table_names_from_partition_list(self, partition_list):
        tablenames = []
        for part in partition_list:
            fields = part.split(',')
            if len(fields) != 3:
                raise Exception('Invalid partition entry "%s"' % part)
            tname = '%s.%s' % (fields[0].strip(), fields[1].strip())
            tablenames.append(tname)
        return tablenames

    def _verify_tablenames(self, ao_partition_list, co_partition_list, heap_partitions):
        tablenames = []

        tablenames.extend(self._get_table_names_from_partition_list(ao_partition_list))
        tablenames.extend(self._get_table_names_from_partition_list(co_partition_list))
        tablenames.extend(heap_partitions)

        tablenames = set(tablenames)

        check_funny_chars_in_tablenames(tablenames)

    def _prompt_continue(self, dump_database):
        logger.info("---------------------------------------------------")
        logger.info("Master Greenplum Instance dump parameters")
        logger.info("---------------------------------------------------")
        if len(self.include_dump_tables) > 0 or self.include_dump_tables_file is not None:
            logger.info("Dump type                            = Single database, specific table")
            logger.info("---------------------------------------------------")
            if len(self.include_dump_tables) > 0:
                logger.info("Table inclusion list ")
                logger.info("---------------------------------------------------")
                for table in self.include_dump_tables:
                    logger.info("Table name                             = %s" % table)
                logger.info("---------------------------------------------------")
            if self.include_dump_tables_file is not None:
                logger.info("Table file name                      = %s" % self.include_dump_tables_file)
                logger.info("---------------------------------------------------")
        elif len(self.exclude_dump_tables) > 0 or self.exclude_dump_tables_file is not None:
            logger.info("Dump type                            = Single database, exclude table")
            logger.info("---------------------------------------------------")
            if len(self.exclude_dump_tables) > 0:
                logger.info("Table exclusion list ")
                logger.info("---------------------------------------------------")
                for table in self.exclude_dump_tables:
                    logger.info("Table name                               = %s" % table)
                logger.info("---------------------------------------------------")
            if self.exclude_dump_tables_file is not None:
                logger.info("Table file name                      = %s" % self.exclude_dump_tables_file)
                logger.info("---------------------------------------------------")
        elif self.incremental:
            logger.info("Dump type                            = Incremental")
            filter_name = get_filter_file(dump_database, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix,
                                          self.ddboost, self.netbackup_service_host)
            if filter_name is not None:
                logger.info("Filtering tables using:")
                logger.info("\t Prefix                        = %s" % (self.dump_prefix[:-1]))
                logger.info("\t Full dump timestamp           = %s" % (self.full_dump_timestamp))
                logger.info("---------------------------------------------------")
        else:
            logger.info("Dump type                            = Full database")
        logger.info("Database to be dumped                = %s" % dump_database)
        if self.dump_schema:
            logger.info("Schema to be dumped                  = %s" % self.dump_schema)
        if self.backup_dir is not None:
            logger.info("Dump directory                       = %s" % self.backup_dir)
        logger.info("Master port                          = %s" % self.master_port)
        logger.info("Master data directory                = %s" % self.master_datadir)
        if self.post_script is not None:
            logger.info("Run post dump program                = %s" % self.post_script)
        else:
            logger.info("Run post dump program                = Off")
                    # TODO: failed_primary_count. do we care though? the end user shouldn't be continuing if
                    # we've already detected a primary failure. also, that particular validation is currently
                    # occurring after the _prompt_continue step in GpCronDump, as it should be...
                    #if [ $FAILED_PRIMARY_COUNT -ne 0 ];then
                    #    LOG_MSG "[WARN]:-Failed primary count             = $FAILED_PRIMARY_COUNT $WARN_MARK" 1
                    #else
                    #    LOG_MSG "[INFO]:-Failed primary count             = $FAILED_PRIMARY_COUNT" 1
                    #fi

                    # TODO: TRY_COMPRESSION is a result of insufficient disk space, compelling us to
                    # attempt compression in order to fit on disk
                    #if [ $TRY_COMPRESSION -eq 1 ];then
                    #    LOG_MSG "[INFO]:-Compression override             = On" 1
                    #else
                    #    LOG_MSG "[INFO]:-Compression override             = Off" 1
                    #fi
        on_or_off = {False: "Off", True: "On"}
        logger.info("Rollback dumps                       = %s" % on_or_off[self.rollback])
        logger.info("Dump file compression                = %s" % on_or_off[self.compress])
        logger.info("Clear old dump files                 = %s" % on_or_off[self.clear_dumps])
        logger.info("Update history table                 = %s" % on_or_off[self.history])
        logger.info("Secure config files                  = %s" % on_or_off[self.dump_config])
        logger.info("Dump global objects                  = %s" % on_or_off[self.dump_global])
        if self.pre_vacuum and self.post_vacuum:
            logger.info("Vacuum mode type                     = pre-dump, post-dump")
        elif self.pre_vacuum:
            logger.info("Vacuum mode type                     = pre-dump")
        elif self.post_vacuum:
            logger.info("Vacuum mode type                     = post-dump")
        else:
            logger.info("Vacuum mode type                     = Off")
        if self.clear_catalog_dumps:
            logger.info("Additional options                   = -c")
        if self.free_space_percent is not None:
            logger.info("Ensuring remaining free disk         > %d" % self.free_space_percent)

        if not userinput.ask_yesno(None, "\nContinue with Greenplum dump", 'N'):
            raise UserAbortedException()

    def _status_report(self, dump_database, timestamp, dump_outcome, exit_status, deleted_dump_set, dump_type):
        logger.info("Dump status report")
        logger.info("---------------------------------------------------")
        logger.info("Target database                          = %s" % dump_database)
        logger.info("Dump subdirectory                        = %s" % timestamp[0:8])
        logger.info("Dump type                                = %s" % dump_type)
        if self.clear_dumps:
            logger.info("Clear old dump directories               = On")
            logger.info("Backup set deleted                       = %s" % deleted_dump_set)
        else:
            logger.info("Clear old dump directories               = Off")
        logger.info("Dump start time                          = %s" % dump_outcome['time_start'])
        logger.info("Dump end time                            = %s" % dump_outcome['time_end'])
        # TODO: logger.info("Number of segments dumped...
        if exit_status != 0:
            if self.rollback:
                logger.warn("Status                                   = FAILED, Rollback Called")
            else:
                logger.warn("Status                                   = FAILED, Rollback Not Called")
            logger.info("See dump log file for errors")
            logger.warn("Dump key                                 = Not Applicable")
        else:
            logger.info("Status                                   = COMPLETED")
            logger.info("Dump key                                 = %s" % timestamp)
        if self.compress:
            logger.info("Dump file compression                    = On")
        else:
            logger.info("Dump file compression                    = Off")
        if self.pre_vacuum and self.post_vacuum:
            logger.info("Vacuum mode type                         = pre-dump, post-dump")
        elif self.pre_vacuum:
            logger.info("Vacuum mode type                         = pre-dump")
        elif self.post_vacuum:
            logger.info("Vacuum mode type                         = post-dump")
        else:
            logger.info("Vacuum mode type                         = Off")
        if exit_status != 0:
            logger.warn("Exit code not zero, check log file")
        else:
            logger.info("Exit code zero, no warnings generated")
        logger.info("---------------------------------------------------")

    def _validate_dump_target(self):
        if len(self.dump_databases) > 1:
            if self.dump_schema or self.include_schema_file is not None:
                raise ExceptionNoStackTraceNeeded("Cannot include specific schema if multiple database dumps are requested")
            if self.exclude_dump_schema or self.exclude_schema_file is not None:
                raise ExceptionNoStackTraceNeeded("Cannot exclude specific schema if multiple database dumps are requested")
            if len(self.include_dump_tables) > 0 or self.include_dump_tables_file is not None:
                raise ExceptionNoStackTraceNeeded("Cannot supply a table dump list if multiple database dumps are requested")
            if len(self.exclude_dump_tables) > 0 or self.exclude_dump_tables_file is not None:
                raise ExceptionNoStackTraceNeeded("Cannot exclude specific tables if multiple database dumps are requested")
            logger.info("Configuring for multiple database dump")

        for dump_database in self.dump_databases:
            ValidateDatabaseExists(database=dump_database, master_port=self.master_port).run()

        if self.dump_schema:
            for schema in self.dump_schema:
                ValidateSchemaExists(database = self.dump_databases[0],
                                     schema = schema,
                                     master_port = self.master_port).run()

    def _validate_run_program(self):
        #Check to see if the file exists
        cmd = Command('Seeking post script', "which %s" % self.post_script)
        cmd.run()
        if cmd.get_results().rc != 0:
            cmd = Command('Seeking post script file', '[ -f %s ]' % self.post_script)
            cmd.run()
            if cmd.get_results().rc != 0:
                logger.warn("Could not locate %s" % self.post_script)
                self.post_script = None
                return
        logger.info("Located %s, will call after dump completed" % self.post_script)

    def _run_program(self):
        Command('Invoking post script', self.post_script).run()

    def _get_pgport(self):
        env_pgport = os.getenv('PGPORT')
        if not env_pgport:
            return self.master_port
        return env_pgport

    def _get_master_port(self, datadir):
        """ TODO: This function will be widely used. Move it elsewhere?
            Its necessity is a direct consequence of allowing the -d <master_data_directory> option. From this,
            we need to deduce the proper port so that the GpArrays can be generated properly. """
        logger.debug("Obtaining master's port from master data directory")
        pgconf_dict = pgconf.readfile(datadir + "/postgresql.conf")
        return pgconf_dict.int('port')

    def _validate_parse_email_File(self):
        if os.path.isfile(self.include_email_file) is False:
            raise Exception("\'%s\' file does not exist." % self.include_email_file)
        if not self.include_email_file.endswith('.yaml'):
            raise Exception("\'%s\' is not \'.yaml\' file. File containing email details should be \'.yaml\' file." % self.include_email_file)
        if (os.path.getsize(self.include_email_file) > 0) is False:
            raise Exception("\'%s\' file is empty." % self.include_email_file)
        email_key_list = ["DBNAME", "FROM", "SUBJECT"]
        try:
            with open(self.include_email_file, 'r') as f:
                doc = yaml.load(f)
            self.email_details = doc['EMAIL_DETAILS']
            for email in self.email_details:
                if not all(keys in email for keys in email_key_list):
                    raise Exception("File not formatted")
                if email['DBNAME'] is None:
                    raise Exception("Database name is not provided")
        except Exception as e:
            raise Exception("\'%s\' file is not formatted properly:" % self.include_email_file)

    def _send_email(self, dump_database, exit_status, start_time, end_time):
        default_email = True
        default_subject = "Report from gpcrondump on host %s [COMPLETED]" % self.cur_host
        default_msg = "Completed for database %s with return code %d [Start=%s End=%s] Options passed [%s]" % (dump_database, exit_status, start_time, end_time, self.options_list)
        if self.include_email_file:
            for email in self.email_details:
                if dump_database in email['DBNAME']:
                    default_email = False
                    if email['FROM'] is None:
                        MailDumpEvent(email['SUBJECT'], default_msg).run()
                    elif email['SUBJECT'] is None:
                        MailDumpEvent(default_subject, default_msg, email['FROM']).run()
                    else:
                        MailDumpEvent(email['SUBJECT'], default_msg, email['FROM']).run()
        if default_email is True:
            MailDumpEvent(default_subject, default_msg).run()

def create_parser():
    parser = OptParser(option_class=OptChecker,
                       version='%prog version $Revision: #5 $',
                       description='Dumps a Greenplum database')

    addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=True)

    addTo = OptionGroup(parser, 'Connection opts')
    parser.add_option_group(addTo)
    addMasterDirectoryOptionForSingleClusterProgram(addTo)

    addTo = OptionGroup(parser, 'Dump options')
    addTo.add_option('-r', action='store_true', dest='rollback', default=False,
                     help="Rollback dump files if dump failure detected [default: no rollback]")
    addTo.add_option('-b', action='store_true', dest='bypass_disk_check', default=False,
                     help="Bypass disk space checking [default: check disk space]")
    addTo.add_option('-j', action='store_true', dest='pre_vacuum', default=False,
                     help="Run vacuum before dump starts.")
    addTo.add_option('-k', action='store_true', dest='post_vacuum', default=False,
                     help="Run vacuum after dump has completed successfully.")
    addTo.add_option('-z', action='store_false', dest='compress', default=True,
                     help="Do not use compression [default: use compression]")
    addTo.add_option('-f', dest='free_space_percent', metavar="<0-99>",
                     help="Percentage of disk space to ensure is reserved after dump.")
    addTo.add_option('-c', action='store_true', dest='clear_dumps', default=False,
                     help="Clear old dump directories [default: do not clear]. Will remove the oldest dump directory other than the current dump directory.")
    addTo.add_option('-o', action='store_true', dest='clear_dumps_only', default=False,
                     help="Clear dump files only. Do not run a dump. Like -c, this will clear the oldest dump directory, other than the current dump directory.")
    addTo.add_option('-s', action='append', dest='dump_schema', metavar="<schema name>",
                     help="Dump the schema contained within the database name supplied via -x. Option can be used more than once")
    addTo.add_option('--schema-file', dest='include_schema_file', metavar="<filename>",
                     help="Dump tables from only the schema named in this file for the specified database. Option can be used only once.")
    addTo.add_option('-S', action='append', dest='exclude_dump_schema', metavar="<schema name>",
                     help="Exclude the specified schema's, in database specified through -x, from the dump.")
    addTo.add_option('--exclude-schema-file', dest='exclude_schema_file', metavar="<filename>",
                     help="Exclude the schemas named in this file from the dump. Option can be used only once.")
    addTo.add_option('-x', dest='dump_databases', metavar="<database name,...>",
                     help="Database name(s) to dump. Multiple database names will preclude the schema and table options.")
    addTo.add_option('-g', action='store_true', dest='dump_config', default=False,
                     help="Dump configuration files: postgresql.conf, pg_ident.conf, and pg_hba.conf.")
    addTo.add_option('-G', action='store_true', dest='dump_global', default=False,
                     help="Dump global objects, i.e. user accounts")
    addTo.add_option('-C', action='store_true', dest='clear_catalog_dumps', default=False,
                     help="Clean (drop) schema prior to dump [default: do not clean]")
    addTo.add_option('-R', dest='post_script', metavar="<program name>",
                     help="Run named program after successful dump. Note: program will only be invoked once, even if multi-database dump requested.")
    addTo.add_option('-B', dest='batch_default', type='int', default=DEFAULT_NUM_WORKERS, metavar="<number>",
                     help="Dispatches work to segment hosts in batches of specified size [default: %s]" % DEFAULT_NUM_WORKERS)
    addTo.add_option('-t', action='append', dest='include_dump_tables', metavar="<schema.tableN>",
                     help="Dump the named table(s) for the specified database. -t can be provided multiple times to include multiple tables.")
    addTo.add_option('-T', action='append', dest='exclude_dump_tables', metavar="<schema.tableN>",
                     help="Exclude the named table(s) from the dump. -T can be provided multiple times to exclude multiple tables.")
    addTo.add_option('-K', dest='timestamp_key', metavar="<YYYYMMDDHHMMSS>",
                     help="Timestamp key for the dump.")

    addTo.add_option('--list-backup-files', dest='list_backup_files', default=False, action='store_true',
                     help="Files created during the dump operation for a particular input timestamp")
    addTo.add_option('--prefix', dest='local_dump_prefix', default='', metavar='<filename prefix>',
                     help="Prefix to be added to all files created in the dump")
    addTo.add_option('--list-filter-tables', dest='list_filter_tables', default=False, action='store_true',
                     help="List tables to be included in a dump based on the current filter.")

    # TODO: HACK to remove existing -h
    help = parser.get_option('-h')
    parser.remove_option('-h')
    help._short_opts.remove('-h')
    parser.add_option(help)
    addTo.add_option('-h', action='store_true', dest='history', default=False,
                     help="Record details of database dump in database table %s in database supplied via -x option. Utility will create table if it does not currently exist." % UpdateHistoryTable.HISTORY_TABLE)

    addTo.add_option('-u', dest='backup_dir', metavar="<BACKUPFILEDIR>",
                     help="Directory where backup files are placed [default: data directories]")
    addTo.add_option('-y', dest='report_dir', metavar="<REPORTFILEDIR>",
                     help="DEPRECATED OPTION: Directory where report file is placed")
    addTo.add_option('-E', dest='encoding', metavar="<encoding>",
                     help="Dump the data under the given encoding")
    addTo.add_option('--clean', const='--clean', action='append_const', dest='output_options',
                     help="Clean (drop) schema prior to dump")
    addTo.add_option('--inserts', const='--inserts', action='append_const', dest='output_options',
                     help="Dump data as INSERT, rather than COPY, commands.")
    addTo.add_option('--column-inserts', const='--column-inserts', action='append_const', dest='output_options',
                     help="Dump data as INSERT commands with colun names.")
    addTo.add_option('--oids', const='--oids', action='append_const', dest='output_options',
                     help="Include OIDs in dump.")
    addTo.add_option('--no-owner', const='--no-owner', action='append_const', dest='output_options',
                     help="Do not output commands to set object ownership.")
    addTo.add_option('--no-privileges', const='--no-privileges', action='append_const', dest='output_options',
                     help="Do not dump privileges (grant/revoke).")
    addTo.add_option('--use-set-session-authorization', const='--use-set-session-authorization', action='append_const', dest='output_options',
                     help="Use SESSION AUTHORIZATION commands instead of ALTER OWNER commands.")
    addTo.add_option('--rsyncable', const='--rsyncable', action='append_const', dest='output_options',
                     help="Pass the --rsyncable option to gzip, if compression is being used.")
    addTo.add_option('--table-file', dest='include_dump_tables_file', metavar="<filename>",
                     help="Dump the tables named in this file for the specified database. Option can be used only once.")
    addTo.add_option('--exclude-table-file', dest='exclude_dump_tables_file', metavar="<filename>",
                     help="Exclude the tables named in this file from the dump. Option can be used only once.")
    addTo.add_option('--email-file', dest='include_email_file', metavar="<filename>",
                     help="Customize the 'Sender' and 'Subject' of the email to be sent after backup")

    parser.add_option_group(addTo)

    # Incremental option(s)
    incrOpt = OptionGroup(parser, "Incremental")
    incrOpt.add_option('--incremental', action='store_true', dest='incremental', default=False, help='Dump incremental backup.')
    parser.add_option_group(incrOpt)

    # NetBackup option(s)
    nbuOpt = OptionGroup(parser, "NetBackup")
    nbuOpt.add_option('--netbackup-service-host', dest='netbackup_service_host', metavar="<server name>",
                      help="NetBackup service hostname")
    nbuOpt.add_option('--netbackup-policy', dest='netbackup_policy', metavar="<policy name>",
                      help="NetBackup policy name")
    nbuOpt.add_option('--netbackup-schedule', dest='netbackup_schedule', metavar="<schedule name>",
                      help="NetBackup schedule name")
    nbuOpt.add_option('--netbackup-block-size', dest='netbackup_block_size', metavar="<block size>",
                      help="NetBackup data transfer block size")
    nbuOpt.add_option('--netbackup-keyword', dest='netbackup_keyword', metavar="<keyword>",
                      help="NetBackup Keyword")
    parser.add_option_group(nbuOpt)

    # DDBoost option(s)
    ddOpt = OptionGroup(parser, "DDBoost")
    ddOpt.add_option('--ddboost', dest='ddboost', help="Dump to DDBoost using ~/.ddconfig", action="store_true", default=False)
    ddOpt.add_option('--replicate', dest='replicate', help="Post dump, replicate the backup to remote Data Domain system.", action="store_true", default=False)
    ddOpt.add_option('--max-streams', dest='max_streams', action="store", default=None,
                     help="Maximum number of Data Domain I/O streams to be used for replication.")
    parser.add_option_group(ddOpt)

    # DDBoostConfig option(s)
    ddConfigOpt = OptionGroup(parser, "DDBoostConfig")
    # ddboost-host may have more then one host
    ddConfigOpt.add_option('--ddboost-host', dest='ddboost_hosts', action='append', default=None,
                           help="Configuration of ddboost hostname.")
    ddConfigOpt.add_option('--ddboost-user', dest='ddboost_user', action='store', default=None,
                           help="Configuration of ddboost user.")
    ddConfigOpt.add_option('--ddboost-backupdir', dest='ddboost_backupdir', action='store', default=None,
                           help="Default backup directory on local Data Domain system.")
    ddConfigOpt.add_option('--ddboost-remote', dest='ddboost_remote', action='store_true', default=False,
                           help="Configuration parameters for remote Data Domain system.")
    ddConfigOpt.add_option('--ddboost-config-remove', dest='ddboost_config_remove', action='store_true', default=False,
                           help="Remove ~/.ddconfig file.")
    ddConfigOpt.add_option('--ddboost-verify', dest='ddboost_verify', action='store_true', default=False,
                           help="Verify DDBoost credentials on DDBoost host.")
    ddConfigOpt.add_option('--ddboost-skip-ping', dest='ddboost_ping', action='store_false', default=True,
                           help="Ping DDBoost host as a sanity check before writing configuration.")
    parser.add_option_group(ddConfigOpt)

    parser.setHelp([
    """
Crontab entry (example):
SHELL=/bin/bash
5 0 * * * . $HOME/.bashrc; $GPHOME/bin/gpcrondump -x template1 -aq >> <name of cronout file>
Set the shell to /bin/bash (default for cron is /bin/sh
Dump the template1 database, start process 5 minutes past midnight on a daily basis
    """,
    """
Mail configuration
This utility will send an email to a list of email addresses contained in a file
named mail_contacts. This file can be located in the GPDB super user home directory
or the utility bin directory ${GPHOME}/bin. The format of the file is one email
address per line. If no mail_contacts file is found in either location, a warning message
will be displayed.
    """
    ])

    return parser

if __name__ == '__main__':
    sys.argv[0] = EXECNAME                                                              # for cli_help
    simple_main(create_parser, GpCronDump, {"pidfilename" : GPCRONDUMP_PID_FILE,
                                            "programNameOverride" : EXECNAME})        # for logger
