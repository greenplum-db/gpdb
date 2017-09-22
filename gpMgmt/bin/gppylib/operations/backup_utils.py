import fnmatch
import glob
import os
import re
import tempfile
from datetime import datetime
from gppylib import gplog
from gppylib.commands.base import WorkerPool, Command, REMOTE
from gppylib.commands.unix import Scp
from gppylib.db import dbconn
from gppylib.db.dbconn import execSQL
from gppylib.gparray import GpArray
from gppylib.mainUtils import gp
from gppylib import pgconf
from optparse import Values
from pygresql import pg
import gzip

logger = gplog.get_default_logger()

class Context(Values, object):
    filename_dict = {
        "ao": ("dump", "_ao_state_file"), "cdatabase": ("cdatabase_%(content)d_%(dbid)s", ""), "co": ("dump", "_co_state_file"), "dirty_table": ("dump", "_dirty_list"),
        "dump": ("dump_%(content)d_%(dbid)s", ""), "files": ("dump", "_regular_files"), "filter": ("dump", "_filter"), "global": ("global_%(content)d_%(dbid)s", ""),
        "increments": ("dump", "_increments"), "last_operation": ("dump", "_last_operation"), "master_config": ("master_config_files", ".tar"),
        "metadata": ("dump_%(content)d_%(dbid)s", ""), "partition_list": ("dump", "_table_list"), "pipes": ("dump", "_pipes"), "plan": ("restore", "_plan"),
        "postdata": ("dump_%(content)d_%(dbid)s", "_post_data"), "report": ("dump", ".rpt"), "schema": ("dump", "_schema"),
        "segment_config": ("segment_config_files_%(content)d_%(dbid)s", ".tar"), "stats": ("statistics_%(content)d_%(dbid)s", ""), "table": ("dump", "_table"),
        "status": ("dump_status_%(content)d_%(dbid)s", ""),
    }
    defaults = {
        "backup_dir": None, "batch_default": 64, "change_schema": None, "cleanup_date": None, "cleanup_total": None, "clear_catalog_dumps": False,
        "clear_dumps": False, "clear_dumps_only": False, "compress": True, "db_host_path": None, "ddboost": False, "ddboost_backupdir": None, "ddboost_config_remove": False,
        "ddboost_hosts": None, "ddboost_ping": True, "ddboost_remote": False, "ddboost_show_config": False, "ddboost_storage_unit": None, "ddboost_user": None,
        "ddboost_verify": False, "drop_db": False, "dump_config": False, "dump_databases": [], "dump_dir": "db_dumps", "dump_global": False, "dump_prefix": "",
        "dump_schema": "", "dump_stats": False, "encoding": None, "exclude_dump_schema": "", "exclude_dump_tables": "", "exclude_dump_tables_file": "",
        "exclude_schema_file": "", "free_space_percent": None, "history": True, "include_dump_tables": "", "include_dump_tables_file": "",
        "include_schema_file": "", "incremental": False, "list_filter_tables": False, "local_dump_prefix": None, "masterDataDirectory": None,
        "master_port": 0, "max_streams": None, "netbackup_block_size": None, "netbackup_keyword": None, "netbackup_policy": None, "netbackup_schedule": None,
        "netbackup_service_host": None, "metadata_only": False, "no_analyze": False, "no_ao_stats": False, "no_plan": False, "no_validate_table_name": False,
        "output_options": [], "post_script": "", "redirected_restore_db": None, "report_dir": "", "report_status_dir": "", "restore_global": False, "restore_schemas":
        None, "restore_stats": None, "restore_tables": [], "target_db": None, "timestamp": None, "timestamp_key": None, "full_dump_timestamp": None,
    }
    def __init__(self, values=None):
        if values:
            self.defaults.update(values.__dict__) # Ensure that context has default values for all unset variables
        super(self.__class__, self).__init__(vars(Values(self.defaults)))

        if self.masterDataDirectory:
            self.master_datadir = self.masterDataDirectory
        else:
            self.master_datadir = gp.get_masterdatadir()
        self.master_port = self.get_master_port()
        if self.local_dump_prefix:
            self.dump_prefix = self.local_dump_prefix + "_"
        else:
            self.dump_prefix = ""

        if not self.include_dump_tables: self.include_dump_tables = []
        if not self.exclude_dump_tables: self.exclude_dump_tables = []
        if not self.output_options: self.output_options = []
        if not self.dump_schema: self.dump_schema = []
        if not self.exclude_dump_schema: self.exclude_dump_schema = []

        self.gparray = GpArray.initFromCatalog(dbconn.DbURL(dbname="template1", port=self.master_port), utility=True)
        self.use_old_filename_format = False # Use new filename format by default
        self.content_map = self.setup_content_map()

    def get_master_port(self):
        pgconf_dict = pgconf.readfile(self.master_datadir + "/postgresql.conf")
        return pgconf_dict.int('port')

    def setup_content_map(self):
        content_map = {}
        content_map[1] = -1 #for master
        for seg in self.gparray.getDbList():
            content_map[seg.dbid] = seg.content
        return content_map

    def generate_filename(self, filetype, dbid=1, content=None, timestamp=None, directory=None, use_old_format=None, use_compress=True):
        """
        "Old format" filename format: <prefix>gp_<infix>_<1 if master|0 if segment>_<dbid>_<timestamp><suffix>
        "New format" filename format: <prefix>gp_<infix>_<content>_<dbid>_<timestamp><suffix>
        The "content" parameter is used to generate a filename pattern for finding files of that content id, not a single filename
        """
        if timestamp is None:
            timestamp = self.timestamp

        if directory is not None:
            use_dir = directory
        elif dbid == 1:
            use_dir = self.get_backup_dir(timestamp)
        else:
            use_dir = self.get_backup_dir(timestamp, segment_dir=self.get_datadir_for_dbid(dbid))

        if use_old_format is None:
            use_old_format = self.use_old_filename_format

        format_str =  "%s/%sgp_%s_%s%s" % (use_dir, self.dump_prefix, "%s", timestamp, "%s")
        filename = format_str % (self.filename_dict[filetype][0], self.filename_dict[filetype][1])
        if "%(content)d_%(dbid)s" in filename:
            content_num = -2
            dbid_str = ""
            if content is not None: # Doesn't use "if not content" because 0 is a valid content id
                content_num = content
                dbids = ["%d" % id for id in self.content_map if self.content_map[id] == content]
                dbid_str = "(%s)" % ("|".join(dbids))
            else:
                content_num = self.content_map[dbid]
                dbid_str = dbid
            if use_old_format:
                content_num = 1 if content_num == -1 else 0
            filename = filename % {"content": content_num, "dbid": dbid_str}
        if self.compress and filetype in ["metadata", "dump", "postdata"] and use_compress:
            filename += ".gz"
        return filename

    def generate_prefix(self, filetype, dbid=1, content=None, use_old_format=None):
        format_str =  "%sgp_%s_" % (self.dump_prefix, "%s")
        filename = format_str % (self.filename_dict[filetype][0])
        if "%(content)d_%(dbid)s" in filename:
            if use_old_format:
                if dbid == 1:
                    filename = filename % {"content": 1, "dbid": 1}
                else:
                    filename = filename % {"content": 0, "dbid": dbid}
            else:
                if content is None:
                    content = self.content_map[dbid]
                filename = filename % {"content": content, "dbid": dbid}
        return filename

    def get_datadir_for_dbid(self, dbid):
        for seg in self.gparray.getDbList():
            if seg.getSegmentDbId() == dbid:
                return seg.getSegmentDataDirectory()
        raise Exception("Segment with dbid %d not found" % dbid)

    def get_current_primaries(self):
        return [seg for seg in self.gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]

    def is_timestamp_in_old_format(self, timestamp=None):
        if not timestamp:
            timestamp = self.timestamp
        dump_dirs = self.get_dump_dirs()
        report_file = None
        using_nbu = (self.netbackup_service_host is not None)
        if using_nbu:
            restore_file_with_nbu(self, "report", timestamp=timestamp)
            report_file = self.generate_filename('report', timestamp=timestamp)
        else:
            for dump_dir in dump_dirs:
                report_file_attempt = self.generate_filename('report', timestamp=timestamp, directory=dump_dir)
                if os.path.exists(report_file_attempt):
                    report_file = report_file_attempt
                    break
        if not report_file:
            raise Exception("Unable to locate report file for timestamp %s" % timestamp)
        report_contents = get_lines_from_file(report_file)
        # We specify directory='' because we only care about matching the
        # filename and not the full path
        old_metadata = self.generate_filename("metadata", timestamp=timestamp, use_old_format=True, use_compress=False, directory='')
        old_format = False
        for line in report_contents:
            if old_metadata in line:
                return True
        return False

    def get_backup_dir(self, timestamp=None, segment_dir=None):
        if self.backup_dir and not self.ddboost:
            use_dir = self.backup_dir
        elif segment_dir is not None:
            use_dir = segment_dir
        elif self.master_datadir:
            use_dir = self.master_datadir
        else:
            raise Exception("Cannot locate backup directory with existing parameters")

        if timestamp:
            use_timestamp = timestamp
        else:
            use_timestamp = self.timestamp

        if not use_timestamp:
            raise Exception("Cannot locate backup directory without timestamp")

        if not validate_timestamp(use_timestamp):
            raise Exception('Invalid timestamp: "%s"' % use_timestamp)

        return "%s/%s/%s" % (use_dir, self.dump_dir, use_timestamp[0:8])

    def get_backup_root(self):
        if self.backup_dir and not self.ddboost:
            return self.backup_dir
        else:
            return self.master_datadir

    def get_gpd_path(self):
        gpd_path = os.path.join(self.dump_dir, self.timestamp[0:8])
        if self.backup_dir:
            gpd_path = os.path.join(self.backup_dir, gpd_path)
        return gpd_path

    def get_date_dir(self):
        if self.db_date_dir:
            date_dir = self.db_date_dir
        else:
            date_dir = self.timestamp[0:8]
        return os.path.join(self.get_backup_root(), self.dump_dir, date_dir)

    def backup_dir_is_writable(self):
        if self.backup_dir and not self.report_status_dir:
            try:
                check_dir_writable(self.get_backup_dir())
            except Exception as e:
                logger.warning('Backup directory %s is not writable. Error %s' % (self.get_backup_dir(), str(e)))
                logger.warning('Since --report-status-dir option is not specified, report and status file will be written in segment data directory.')
                return False
        return True

    def generate_dump_timestamp(self):
        if self.timestamp_key:
            timestamp_key = self.timestamp_key
        else:
            timestamp_key = datetime.now().strftime("%Y%m%d%H%M%S")

        if not validate_timestamp(timestamp_key):
            raise Exception('Invalid timestamp key')

        year = int(timestamp_key[:4])
        month = int(timestamp_key[4:6])
        day = int(timestamp_key[6:8])
        hours = int(timestamp_key[8:10])
        minutes = int(timestamp_key[10:12])
        seconds = int(timestamp_key[12:14])

        self.timestamp = timestamp_key
        self.db_date_dir = "%4d%02d%02d" % (year, month, day)
        self.timestamp_object = datetime(year, month, day, hours, minutes, seconds)

    def get_dump_dirs(self):

        use_dir = self.get_backup_root()
        dump_path = os.path.join(use_dir, self.dump_dir)

        if not os.path.isdir(dump_path):
            return []

        initial_list = os.listdir(dump_path)
        initial_list = fnmatch.filter(initial_list, '[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]')

        dirnames = []
        for d in initial_list:
            pth = os.path.join(dump_path, d)
            if os.path.isdir(pth):
                dirnames.append(pth)

        dirnames = sorted(dirnames, key=lambda x: int(os.path.basename(x)), reverse=True)
        return dirnames

    def get_report_files_and_paths(self, backup_root):
        reports = []
        prefix = "%s*.rpt" % self.generate_prefix("report")
        for path, dirs, files in os.walk(backup_root):
            matching = fnmatch.filter(files, "%s*" % prefix)
            reports.extend([(path, report_file) for report_file in matching])
        if len(reports) == 0:
            raise Exception("No report files located")
        return reports

    def get_compress_and_dbname_from_report_file(self, report_file):
        contents = get_lines_from_file(report_file)
        compress = None
        target_db = ""
        name_pattern = re.compile(r'Port [0-9]+ Database (.*) BackupFile')
        for line in contents:
            if "Compression Program: gzip" in line:
                compress = True
            elif "Compression Program: None" in line:
                compress = False
            matching = name_pattern.search(line)
            if matching and matching.group(1):
                target_db = matching.group(1)
        if compress is None or not target_db:
            raise Exception("Could not determine database name and compression type from report file %s" % report_file)
        return compress, target_db

def get_filename_for_content(context, filetype, content, remote_directory=None, host=None):
    filetype_glob = context.generate_filename(filetype, content=content, directory=remote_directory)
    if remote_directory:
        if not host:
            raise Exception("Must supply name of remote host to check for %s file" % filetype)
        cmd = Command(name = "Find file of type %s for content %d on host %s" % (filetype, content, host),
                cmdStr = 'echo "import glob, re\nfor f in glob.glob(\'%s/*\'):\n    if re.match(\'%s\', f):\n        print f\n        break" | python'
                            % (remote_directory, filetype_glob), ctxt = REMOTE, remoteHost = host)
        cmd.run()

        if cmd.get_results().rc == 0 and cmd.get_results().stdout:
            return cmd.get_results().stdout
        return None
    else:
        filenames = glob.glob("%s/*" % context.get_backup_dir())
        for filename in filenames:
            if re.match(filetype_glob, filename):
                return filename
        return None

def expand_partitions_and_populate_filter_file(context, partition_list, file_prefix):
    expanded_partitions = expand_partition_tables(context, partition_list)
    dump_partition_list = list(set(expanded_partitions + partition_list))
    return create_temp_file_from_list(dump_partition_list, file_prefix)

def populate_filter_tables(table, rows, non_partition_tables, partition_leaves):
    if not rows:
        non_partition_tables.append(table)
    else:
        for (schema_name, partition_leaf_name) in rows:
            partition_leaf = schema_name.strip() + '.' + partition_leaf_name.strip()
            partition_leaves.append(partition_leaf)
    return (non_partition_tables, partition_leaves)

def get_all_parent_tables(dbname):
    SQL = "SELECT DISTINCT (schemaname || '.' || tablename) FROM pg_partitions"
    data = []
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        curs = dbconn.execSQL(conn, SQL)
        data = curs.fetchall()
    return set([d[0] for d in data])

def list_to_quoted_string(conn, filter_tables):
    filter_string = "'" + "', '".join([escape_string(t, conn) for t in filter_tables]) + "'"
    return filter_string

def convert_parents_to_leafs(context, parents):
    partition_leaves_sql = """
                           SELECT x.partitionschemaname || '.' || x.partitiontablename
                           FROM (
                                SELECT distinct schemaname, tablename, partitionschemaname, partitiontablename, partitionlevel
                                FROM pg_partitions
                                WHERE schemaname || '.' || tablename in (%s)
                                ) as X,
                           (SELECT schemaname, tablename maxtable, max(partitionlevel) maxlevel
                            FROM pg_partitions
                            group by (tablename, schemaname)
                           ) as Y
                           WHERE x.schemaname = y.schemaname and x.tablename = Y.maxtable and x.partitionlevel = Y.maxlevel;
"""
    if not parents:
        return []

    conn = dbconn.connect(dbconn.DbURL(dbname=context.target_db))
    partition_sql = partition_leaves_sql % list_to_quoted_string(conn, parents)
    curs = dbconn.execSQL(conn, partition_sql)
    rows = curs.fetchall()
    curs.close()
    return [r[0] for r in rows]


#input: list of tables to be filtered
#output: same list but parent tables converted to leafs
def expand_partition_tables(context, filter_tables):

    if not filter_tables or len(filter_tables) == 0:
        return filter_tables
    parent_tables = list()
    non_parent_tables = list()
    expanded_list = list()

    all_parent_tables = get_all_parent_tables(context.target_db)
    for table in filter_tables:
        if table in all_parent_tables:
            parent_tables.append(table)
        else:
            non_parent_tables.append(table)

    expanded_list += non_parent_tables

    local_batch_size = 1000
    for (s, e) in get_batch_from_list(len(parent_tables), local_batch_size):
        tmp = convert_parents_to_leafs(context, parent_tables[s:e])
        expanded_list += tmp

    return expanded_list

def get_batch_from_list(length, batch_size):
    indices = []
    for i in range(0, length, batch_size):
        indices.append((i, i+batch_size))
    return indices

def create_temp_file_from_list(entries, prefix):
    """
    When writing the entries into temp file, don't do any strip as there might be
    white space in schema name and table name.
    """
    if len(entries) == 0:
        return None

    fd = tempfile.NamedTemporaryFile(mode='w', prefix=prefix, delete=False)
    for entry in entries:
        fd.write(entry + '\n')
    tmp_file_name = fd.name
    fd.close()

    return tmp_file_name

def create_temp_file_with_tables(table_list):
    return create_temp_file_from_list(table_list, 'table_list_')

def create_temp_file_with_schemas(schema_list):
    return create_temp_file_from_list(schema_list, 'schema_file_')

def validate_timestamp(timestamp):
    if not timestamp:
        return False
    if len(timestamp) != 14:
        return False
    if timestamp.isdigit():
        return True
    else:
        return False

def check_successful_dump(report_file_contents):
    for line in report_file_contents:
        if line.strip() == 'gp_dump utility finished successfully.':
            return True
    return False

# raise exception for bad data
def convert_report_filename_to_cdatabase_filename(context, report_file):
    timestamp = report_file[-18:-4]
    ddboost_parent_dir = None
    if context.ddboost:
        # We pass in segment_dir='' because we don't want it included in our path for ddboost
        ddboost_parent_dir = context.get_backup_dir(timestamp=timestamp, segment_dir='')
    old_format = context.is_timestamp_in_old_format(timestamp=timestamp)
    return context.generate_filename("cdatabase", timestamp=timestamp, use_old_format=old_format, directory=ddboost_parent_dir)

def get_lines_from_dd_file(filename, ddboost_storage_unit):
    cmdStr = 'gpddboost --readFile --from-file=%s' % filename
    if ddboost_storage_unit:
        cmdStr += ' --ddboost-storage-unit=%s' % ddboost_storage_unit

    cmd = Command('DDBoost copy of master dump file', cmdStr)

    cmd.run(validateAfter=True)
    contents = cmd.get_results().stdout.splitlines()
    return contents

def check_cdatabase_exists(context, report_file):
    try:
        filename = convert_report_filename_to_cdatabase_filename(context, report_file)
    except Exception, err:
        return False

    if context.ddboost:
        cdatabase_contents = get_lines_from_dd_file(filename, context.ddboost_storage_unit)
    elif context.netbackup_service_host:
        restore_file_with_nbu(context, path=filename)
        cdatabase_contents = get_lines_from_file(filename)
    else:
        cdatabase_contents = get_lines_from_file(filename, context)

    dbname = escapeDoubleQuoteInSQLString(context.target_db, forceDoubleQuote=False)
    for line in cdatabase_contents:
        if 'CREATE DATABASE' in line:
            dump_dbname = get_dbname_from_cdatabaseline(line)
            if dump_dbname is None:
                continue
            else:
                if dbname == checkAndRemoveEnclosingDoubleQuote(dump_dbname):
                    return True
    return False

def get_dbname_from_cdatabaseline(line):
    """
    Line format: CREATE DATABASE "DBNAME" WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = gpadmin;

    To get the dbname:
    substring between the ending index of the first statement: CREATE DATABASE and the starting index
    of WITH TEMPLATE whichever is not inside any double quotes, based on the fact that double quote
    inside any name will be escaped by extra double quote, so there's always only one WITH TEMPLATE not
    inside any doubles, means its previous and post string should have only even number of double
    quotes.
    Note: OWER name can also have special characters with double quote.
    """
    cdatabase = "CREATE DATABASE "
    try:
        start = line.index(cdatabase)
    except Exception as e:
        logger.error('Failed to find substring %s in line %s, error: %s' % (cdatabase, line, str(e)))
        return None

    keyword = " WITH TEMPLATE = "
    pos = get_nonquoted_keyword_index(line, keyword, '"', len(keyword))
    if pos != -1:
        dbname = line[start+len(cdatabase) : pos]
        return dbname
    return None

def get_nonquoted_keyword_index(line, keyword, quote, keyword_len):
    # quote can be single quote or double quote
    all_positions = get_all_occurrences(keyword, line)
    if all_positions != None and len(all_positions) > 0:
        for pos in all_positions:
            pre_string = line[:pos]
            post_string = line[pos + keyword_len:]
            quotes_before = get_all_occurrences('%s' % quote, pre_string)
            quotes_after = get_all_occurrences('%s' % quote, post_string)
            num_quotes_before = 0 if (quotes_before is None or len(quotes_before) == 0) else len(quotes_before)
            num_quotes_after = 0 if (quotes_after is None or len(quotes_after) == 0) else len(quotes_after)
            if num_quotes_before % 2 == 0 and num_quotes_after % 2 == 0:
                return pos
    return -1

def get_all_occurrences(substr, line):
    # substr is used for generating the pattern, escape those special chars in regexp
    if substr is None or line is None or len(substr) > len(line):
        return None
    return [m.start() for m in re.finditer('(?=%s)' % substr, line)]

def get_type_ts_from_report_file(context, report_file, backup_type):
    report_file_contents = get_lines_from_file(report_file)

    if not check_successful_dump(report_file_contents):
        return None

    if not check_cdatabase_exists(context, report_file):
        return None

    if check_backup_type(report_file_contents, backup_type):
        return get_timestamp_val(report_file_contents)

    return None

def get_full_ts_from_report_file(context, report_file):
    return get_type_ts_from_report_file(context, report_file, 'Full')

def get_incremental_ts_from_report_file(context, report_file):
    return get_type_ts_from_report_file(context, report_file, 'Incremental')

def get_timestamp_val(report_file_contents):
    for line in report_file_contents:
        if line.startswith('Timestamp Key'):
            timestamp = line.split(':')[-1].strip()
            if not validate_timestamp(timestamp):
                raise Exception('Invalid timestamp value found in report_file')

            return timestamp
    return None

def check_backup_type(report_file_contents, backup_type):
    for line in report_file_contents:
        if line.startswith('Backup Type'):
            if line.split(':')[-1].strip() == backup_type:
                return True
    return False

def get_lines_from_zipped_file(fname):
    """
    Don't strip white space here as it may be part of schema name and table name
    """
    content = []
    fd = gzip.open(fname, 'r')
    try:
        for line in fd:
            content.append(line.strip('\n'))
    except Exception as err:
        raise Exception("Error reading from file %s: %s" % (fname, err))
    finally:
        fd.close()
    return content

def get_lines_from_file(fname, context=None):
    """
    Don't strip white space here as it may be part of schema name and table name
    """
    content = []
    if context and context.ddboost:
        contents = get_lines_from_dd_file(fname, context.ddboost_storage_unit)
        return contents
    else:
        with open(fname) as fd:
            for line in fd:
                content.append(line.strip('\n'))
        return content

def write_lines_to_file(filename, lines):
    """
    Don't do strip in line for white space in case it is part of schema name or table name
    """
    with open(filename, 'w') as fp:
        for line in lines:
            fp.write("%s\n" % line.strip('\n'))

def verify_lines_in_file(fname, expected):
    lines = get_lines_from_file(fname)

    if lines != expected:
        raise Exception("After writing file '%s' contents not as expected.\nLines read from file: %s\nLines expected from file: %s\n" % (fname, lines, expected))

def check_dir_writable(directory):
    fp = None
    try:
        tmp_file = os.path.join(directory, 'tmp_file')
        fp = open(tmp_file, 'w')
    except IOError as e:
        raise Exception('No write access permission on %s' % directory)
    except Exception as e:
        raise Exception(str(e))
    finally:
        if fp is not None:
            fp.close()
        if os.path.isfile(tmp_file):
            os.remove(tmp_file)

def execute_sql(query, master_port, dbname):
    dburl = dbconn.DbURL(port=master_port, dbname=dbname)
    conn = dbconn.connect(dburl)
    cursor = execSQL(conn, query)
    return cursor.fetchall()

def execute_sql_with_connection(query, conn):
    return execSQL(conn, query).fetchall()

def get_latest_report_timestamp(context):
    dump_dirs = context.get_dump_dirs()

    for d in dump_dirs:
        latest = get_latest_report_in_dir(d, context.dump_prefix)
        if latest:
            return latest

    return None

def get_latest_report_in_dir(report_dir, dump_prefix):
    files = os.listdir(report_dir)

    if len(files) == 0:
        return None

    dump_report_files = fnmatch.filter(files, '%sgp_dump_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].rpt' % dump_prefix)

    if len(dump_report_files) == 0:
        return None

    dump_report_files = sorted(dump_report_files, key=lambda x: int(x.split('_')[-1].split('.')[0]), reverse=True)
    return dump_report_files[0][-18:-4]

def get_timestamp_from_increments_filename(filename, dump_prefix):
    fname = os.path.basename(filename)
    parts = fname.split('_')
    # Check for 4 underscores if there is no prefix, or more than 4 if there is a prefix
    if not ((not dump_prefix and len(parts) == 4) or (dump_prefix and len(parts) > 4)):
        raise Exception("Invalid increments file '%s' passed to get_timestamp_from_increments_filename" % filename)
    return parts[-2].strip()

def get_full_timestamp_for_incremental(context):
    full_timestamp = None
    if context.netbackup_service_host:
        full_timestamp = get_full_timestamp_for_incremental_with_nbu(context)
    else:
        pattern = '%s/%s/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]/%sgp_dump_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]_increments' % \
                  (context.get_backup_root(), context.dump_dir, context.dump_prefix)
        increments_files = glob.glob(pattern)

        for increments_file in increments_files:
            if os.path.exists(increments_file):
                increment_ts = get_lines_from_file(increments_file)
            else:
                continue

            if context.timestamp in increment_ts:
                full_timestamp = get_timestamp_from_increments_filename(increments_file, context.dump_prefix)
                break

    if not full_timestamp:
        raise Exception("Could not locate full backup associated with timestamp '%s'. "
                        "Either increments file or full backup is missing.\n"
                        % (context.timestamp))

    return full_timestamp

# backup_dir will be either MDD or some other directory depending on call
def get_latest_full_dump_timestamp(context):
    dump_dirs = context.get_dump_dirs()

    for dump_dir in dump_dirs:
        files = sorted(os.listdir(dump_dir))

        if len(files) == 0:
            logger.warn('Dump directory %s is empty' % dump_dir)
            continue

        dump_report_files = fnmatch.filter(files, '%sgp_dump_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].rpt' % context.dump_prefix)

        if len(dump_report_files) == 0:
            logger.warn('No dump report files found in dump directory %s' % dump_dir)
            continue

        dump_report_files = sorted(dump_report_files, key=lambda x: int(x.split('_')[-1].split('.')[0]), reverse=True)
        for dump_report_file in dump_report_files:
            report_path = os.path.join(dump_dir, dump_report_file)
            logger.debug('Checking for latest timestamp in report file %s' % report_path)
            timestamp = get_full_ts_from_report_file(context, report_path)
            logger.debug('Timestamp = %s' % timestamp)
            if timestamp is not None:
                return timestamp

    raise Exception('No full backup found for incremental')

def get_all_segment_addresses(context):
    addresses = [seg.getSegmentAddress() for seg in context.gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
    return list(set(addresses))

def scp_file_to_hosts(host_list, filename, batch_default):
    pool = WorkerPool(numWorkers=min(len(host_list), batch_default))

    for hname in host_list:
        pool.addCommand(Scp('Copying table_filter_file to %s' % hname,
                            srcFile=filename,
                            dstFile=filename,
                            dstHost=hname))
    pool.join()
    pool.haltWork()
    pool.check_results()

def run_pool_command(host_list, cmd_str, batch_default, check_results=True):
    pool = WorkerPool(numWorkers=min(len(host_list), batch_default))

    for host in host_list:
        cmd = Command(host, cmd_str, ctxt=REMOTE, remoteHost=host)
        pool.addCommand(cmd)

    pool.join()
    pool.haltWork()
    if check_results:
        pool.check_results()

def check_funny_chars_in_names(names, is_full_qualified_name=True):
    """
    '\n' inside table name makes it hard to specify the object name in shell command line,
    this may be worked around by using table file, but currently we read input line by line.
    '!' inside table name will mess up with the shell history expansion.
    ',' is used for separating tables in plan file during incremental restore.
    '.' dot is currently being used for full qualified table name in format: schema.table
    """
    if names and len(names) > 0:
        for name in names:
            if ('\t' in name or '\n' in name or '!' in name or ',' in name or
               (is_full_qualified_name and name.count('.') > 1) or (not is_full_qualified_name and name.count('.') > 0)):
                raise Exception('Name has an invalid character "\\t" "\\n" "!" "," ".": "%s"' % name)

def backup_file_with_ddboost(context, filetype=None, dbid=1, timestamp=None):
    if filetype is None:
        raise Exception("Cannot call backup_file_with_ddboost without a filetype argument")
    if timestamp is None:
        timestamp = context.timestamp
    path = context.generate_filename(filetype, dbid=dbid, timestamp=timestamp)
    copy_file_to_dd(context, path, timestamp)

def copy_file_to_dd(context, filename, timestamp=None):
    if timestamp is None:
        timestamp = context.timestamp
    basefilename = os.path.basename(filename)
    cmdStr = "gpddboost --copyToDDBoost --from-file=%s --to-file=%s/%s/%s" % (filename, context.dump_dir, context.timestamp[0:8], basefilename)
    if context.ddboost_storage_unit:
        cmdStr += " --ddboost-storage-unit=%s" % context.ddboost_storage_unit
    cmd = Command('copy file %s to DD machine' % basefilename, cmdStr)
    cmd.run(validateAfter=True)

#Form and run command line to backup individual file with NBU
def backup_file_with_nbu(context, filetype=None, path=None, dbid=1, hostname=None, timestamp=None):
    if filetype and path:
        raise Exception("Cannot supply both a file type and a file path to backup_file_with_nbu")
    if filetype is None and path is None:
        raise Exception("Cannot call backup_file_with_nbu with no type or path argument")
    if timestamp is None:
        timestamp = context.timestamp
    if filetype:
        path = context.generate_filename(filetype, dbid=dbid, timestamp=timestamp)
    command_string = "cat %s | gp_bsa_dump_agent --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s" % \
                     (path, context.netbackup_service_host, context.netbackup_policy, context.netbackup_schedule, path)
    if context.netbackup_block_size is not None:
        command_string += " --netbackup-block-size %s" % context.netbackup_block_size
    if context.netbackup_keyword is not None:
        command_string += " --netbackup-keyword %s" % context.netbackup_keyword
    logger.debug("Command string inside backup_%s_file_with_nbu: %s\n", filetype, command_string)
    if hostname is None:
        Command("dumping metadata files from master", command_string).run(validateAfter=True)
    else:
        Command("dumping metadata files from segment", command_string, ctxt=REMOTE, remoteHost=hostname).run(validateAfter=True)
    logger.debug("Command ran successfully\n")

#Form and run command line to restore individual file with NBU
def restore_file_with_nbu(context, filetype=None, path=None, dbid=1, hostname=None, timestamp=None):
    if filetype and path:
        raise Exception("Cannot supply both a file type and a file path to restore_file_with_nbu")
    if filetype is None and path is None:
        raise Exception("Cannot call restore_file_with_nbu with no type or path argument")

    if timestamp is None:
        timestamp = context.timestamp
    if filetype:
        path = context.generate_filename(filetype, dbid=dbid, timestamp=timestamp)
    command_string = "gp_bsa_restore_agent --netbackup-service-host %s" % context.netbackup_service_host
    if context.netbackup_block_size is not None:
        command_string += " --netbackup-block-size %s" % context.netbackup_block_size

    command_string += " --netbackup-filename %s > %s" % (path, path)
    logger.debug("Command string inside restore_file_with_nbu: %s\n", command_string)
    if hostname is None:
        Command("restoring metadata files to master", command_string).run(validateAfter=True)
    else:
        Command("restoring metadata files to segment", command_string, ctxt=REMOTE, remoteHost=hostname).run(validateAfter=True)

def check_file_dumped_with_nbu(context, filetype=None, path=None, dbid=1, hostname=None):
    if filetype and path:
        raise Exception("Cannot supply both a file type and a file path to check_file_dumped_with_nbu")
    if filetype is None and path is None:
        raise Exception("Cannot call check_file_dumped_with_nbu with no type or path argument")
    if filetype:
        path = context.generate_filename(filetype, dbid=dbid)
    command_string = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (context.netbackup_service_host, path)
    logger.debug("Command string inside 'check_file_dumped_with_nbu': %s\n", command_string)
    if hostname is None:
        cmd = Command("Querying NetBackup server to check for dumped file", command_string)
    else:
        cmd = Command("Querying NetBackup server to check for dumped file", command_string, ctxt=REMOTE, remoteHost=hostname)

    cmd.run(validateAfter=True)
    if cmd.get_results().stdout.strip() == path:
        return True
    else:
        return False

def get_full_timestamp_for_incremental_with_nbu(context):
    if context.dump_prefix:
        get_inc_files_cmd = "gp_bsa_query_agent --netbackup-service-host=%s --netbackup-list-dumped-objects=%sgp_dump_*_increments" % (context.netbackup_service_host, context.dump_prefix)
    else:
        get_inc_files_cmd = "gp_bsa_query_agent --netbackup-service-host=%s --netbackup-list-dumped-objects=gp_dump_*_increments" % context.netbackup_service_host

    cmd = Command("Query NetBackup server to get the list of increments files backed up", get_inc_files_cmd)
    cmd.run(validateAfter=True)
    files_list = cmd.get_results().stdout.strip().split('\n')

    for line in files_list:
        fname = line.strip()
        restore_file_with_nbu(context, path=fname)
        contents = get_lines_from_file(fname)
        if context.timestamp in contents:
            full_timestamp = get_timestamp_from_increments_filename(fname, context.dump_prefix)
            return full_timestamp

    return None

def get_latest_full_ts_with_nbu(context):
    if context.dump_prefix:
        get_rpt_files_cmd = "gp_bsa_query_agent --netbackup-service-host=%s --netbackup-list-dumped-objects=%sgp_dump_*.rpt" % \
                            (context.netbackup_service_host, context.dump_prefix)
    else:
        get_rpt_files_cmd = "gp_bsa_query_agent --netbackup-service-host=%s --netbackup-list-dumped-objects=gp_dump_*.rpt" % context.netbackup_service_host

    cmd = Command("Query NetBackup server to get the list of report files backed up", get_rpt_files_cmd)
    cmd.run(validateAfter=True)
    files_list = cmd.get_results().stdout.strip().split('\n')

    for line in files_list:
        fname = line.strip()
        if fname == '':
            continue
        if context.backup_dir is not None and context.backup_dir not in fname:
            continue
        if ("No object matched the specified predicate" in fname) or ("No objects of the format" in fname):
            return None
        restore_file_with_nbu(context, path=fname)
        timestamp = get_full_ts_from_report_file(context, report_file=fname)
        logger.debug('Timestamp = %s' % timestamp)
        if timestamp is not None:
            return timestamp

    raise Exception('No full backup found for given incremental on the specified NetBackup server')

def getRows(conn, exec_sql):
    curs = dbconn.execSQL(conn, exec_sql)
    results = curs.fetchall()
    curs.close()
    return results

def check_change_schema_exists(context, use_redirect):
    with dbconn.connect(dbconn.DbURL(port=context.master_port, dbname=context.target_db)) as conn:
        schemaname = escape_string(context.change_schema, conn)
        dbname = context.target_db if not use_redirect else context.redirected_restore_db
        schema_check_sql = "select * from pg_catalog.pg_namespace where nspname='%s';" % schemaname
        if len(getRows(conn, schema_check_sql)) < 1:
            return False
        return True

def escape_string(string, conn):
    return pg.DB(db=conn).escape_string(string)

def unescape_string(string):
    if string:
        string = string.replace('\\\\', '\\').replace("''", "'")
    return string

def isDoubleQuoted(string):
    if len(string) > 2 and string[0] == '"' and string[-1] == '"':
        return True
    return False

def checkAndRemoveEnclosingDoubleQuote(string):
    if isDoubleQuoted(string):
        string = string[1 : len(string) - 1]
    return string

def checkAndAddEnclosingDoubleQuote(string):
    if not isDoubleQuoted(string):
        string = '"' + string + '"'
    return string

def escapeDoubleQuoteInSQLString(string, forceDoubleQuote=True):
    """
    Accept true database name, schema name, table name, escape the double quote
    inside the name, add enclosing double quote by default.
    """
    string = string.replace('"', '""')

    if forceDoubleQuote:
        string = '"' + string + '"'
    return string

def removeEscapingDoubleQuoteInSQLString(string, forceDoubleQuote=True):
    """
    Remove the escaping double quote in database/schema/table name.
    """
    if string is None:
        return string

    string = string.replace('""', '"')

    if forceDoubleQuote:
        string = '"' + string + '"'
    return string

def formatSQLString(rel_file, isTableName=False):
    """
    Read the full qualified schema or table name, do a split
    if each item is a table name into schema and table,
    escape the double quote inside the name properly.
    """
    relnames = []
    if rel_file and os.path.exists(rel_file):
        with open(rel_file, 'r') as fr:
            lines = fr.read().strip('\n').split('\n')
            for line in lines:
                if isTableName:
                    schema, table = split_fqn(line)
                    schema = escapeDoubleQuoteInSQLString(schema)
                    table = escapeDoubleQuoteInSQLString(table)
                    relnames.append(schema + '.' + table)
                else:
                    schema = escapeDoubleQuoteInSQLString(line)
                    relnames.append(schema)
        if len(relnames) > 0:
            write_lines_to_file(rel_file, relnames)
            return rel_file

def split_fqn(fqn_name):
    """
    Split full qualified table name into schema and table by separator '.',
    """
    try:
        schema, table = fqn_name.split('.')
    except Exception as e:
        logger.error("Failed to split name %s into schema and table, please check the format is schema.table" % fqn_name)
        raise Exception('%s' % str(e))
    return schema, table

def remove_file_on_segments(context, filename):
    addresses = get_all_segment_addresses(context)

    try:
        cmd = 'rm -f %s' % filename
        run_pool_command(addresses, cmd, context.batch_default, check_results=False)
    except Exception as e:
        logger.error("cleaning up file failed: %s" % e.__str__())

def get_table_info(line):
    """
    It's complex to split when table name/schema name/user name/ tablespace name
    contains full context of one of others', which is very unlikely, but in
    case it happens, return None.

    Since we only care about table name, type, and schema name, strip the input
    is safe here.

    line: contains the true (un-escaped) schema name, table name, and user name.
    """

    COMMENT_EXPR = '-- Name: '
    TYPE_EXPR = '; Type: '
    SCHEMA_EXPR = '; Schema: '
    OWNER_EXPR = '; Owner: '
    TABLESPACE_EXPR = '; Tablespace: '

    temp = line.strip('\n')
    type_start = get_all_occurrences(TYPE_EXPR, temp)
    schema_start = get_all_occurrences(SCHEMA_EXPR, temp)
    owner_start = get_all_occurrences(OWNER_EXPR, temp)
    tblspace_start = get_all_occurrences(TABLESPACE_EXPR, temp)
    if len(type_start) != 1 or len(schema_start) != 1 or len(owner_start) != 1:
        return (None, None, None, None)
    name = temp[len(COMMENT_EXPR) : type_start[0]]
    type = temp[type_start[0] + len(TYPE_EXPR) : schema_start[0]]
    schema = temp[schema_start[0] + len(SCHEMA_EXPR) : owner_start[0]]
    if not tblspace_start:
        tblspace_start.append(None)
    owner = temp[owner_start[0] + len(OWNER_EXPR) : tblspace_start[0]]
    return (name, type, schema, owner)

def validate_netbackup_params(param_dict):
    max_len = 127
    for label, param in param_dict.iteritems():
        if param and len(param) > max_len:
            raise Exception("Netbackup {0} ({1}) exceeds the maximum length of {2} characters".format(label, param, max_len))

