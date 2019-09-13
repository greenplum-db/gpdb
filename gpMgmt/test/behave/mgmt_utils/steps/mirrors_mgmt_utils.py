from os import path

from behave import given, when, then
from test.behave_utils.utils import *

from mgmt_utils import *

# This file contains steps for gpaddmirrors and gpmovemirrors tests

# This class is intended to store per-Scenario state that is built up over
# a series of steps.
class MirrorMgmtContext:
    def __init__(self):
        self.working_directory = None
        self.input_file = None

    def input_file_path(self):
        if self.working_directory is None:
            raise Exception("working directory not set")
        if self.input_file is None:
            raise Exception("input file not set")
        return path.normpath(path.join(self.working_directory,self.input_file))


def _generate_input_config(spread=False):
    datadir_config = _write_datadir_config()
    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -a -o %s -m %s' % (mirror_config_output_file, datadir_config)
    if spread:
        cmd_str += " -s"
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)
    return mirror_config_output_file


def do_write(template, config_file_path):
    mirror_data_dir = make_data_directory_called('mirror')
    with open(config_file_path, 'w') as fp:
        contents = template.format(mirror_data_dir)
        fp.write(contents)


def _write_datadir_config():
    datadir_config = '/tmp/gpaddmirrors_datadir_config'
    template = """
{0}
{0}
"""
    do_write(template, datadir_config)
    return datadir_config


def _write_datadir_config_for_three_mirrors():
    datadir_config='/tmp/gpaddmirrors_datadir_config'
    template = """
{0}
{0}
{0}
"""
    do_write(template, datadir_config)
    return datadir_config


@when("gpaddmirrors adds 3 mirrors")
def add_three_mirrors(context):
    datadir_config = _write_datadir_config_for_three_mirrors()
    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -o %s -m %s' % (mirror_config_output_file, datadir_config)
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)
    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % mirror_config_output_file)
    cmd.run(validateAfter=True)


def add_mirrors(context, options):
    context.mirror_config = _generate_input_config()
    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s %s' % (context.mirror_config, options))
    cmd.run(validateAfter=True)


def make_data_directory_called(data_directory_name):
    mdd_parent_parent = os.path.realpath(
        os.getenv("MASTER_DATA_DIRECTORY") + "../../../")
    mirror_data_dir = os.path.join(mdd_parent_parent, data_directory_name)
    if not os.path.exists(mirror_data_dir):
        os.mkdir(mirror_data_dir)
    return mirror_data_dir


def _get_mirror_count():
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        sql = """SELECT count(*) FROM gp_segment_configuration WHERE role='m'"""
        count_row = dbconn.execSQL(conn, sql).fetchone()
        return count_row[0]

# take the item in search_item_list, search pg_hba if it contains atleast one entry
# for the item
@given('pg_hba file "{filename}" on host "{host}" contains entries for "{search_items}"')
@then('pg_hba file "{filename}" on host "{host}" contains entries for "{search_items}"')
def impl(context, search_items, host, filename):
    cmd_str = "ssh %s cat %s" % (host, filename)
    cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
    cmd.run(validateAfter=False)
    search_item_list = [search_item.strip() for search_item in search_items.split(',')]
    pghba_contents= cmd.get_stdout().strip().split('\n')
    for search_item in search_item_list:
        found = False
        for entry in pghba_contents:
            contents = entry.strip()
            # for example: host all all hostname    trust
            if contents.startswith("host") and contents.endswith("trust"):
                tokens = contents.split()
                if len(tokens) != 5:
                    raise Exception("failed to parse pg_hba.conf line '%s'" % contents)
                hostname = tokens[3].strip()
                if search_item == hostname:
                    found = True
                    break
        if not found:
            raise Exception("entry for expected item %s not existing in pg_hba.conf '%s'" % (search_item, pghba_contents))

# ensure pg_hba contains only cidr addresses, exclude mandatory entries for replication samenet if existing
@given('pg_hba file "{filename}" on host "{host}" contains only cidr addresses')
@then('pg_hba file "{filename}" on host "{host}" contains only cidr addresses')
def impl(context, host, filename):
    cmd_str = "ssh %s cat %s" % (host, filename)
    cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
    cmd.run(validateAfter=False)
    pghba_contents= cmd.get_stdout().strip().split('\n')
    for entry in pghba_contents:
        contents = entry.strip()
        # for example: host all all hostname    trust
        if contents.startswith("host") and contents.endswith("trust"):
            tokens = contents.split()
            if len(tokens) != 5:
                raise Exception("failed to parse pg_hba.conf line '%s'" % contents)
            hostname = tokens[3].strip()
            # ignore replication entries
            if hostname == "samenet":
                continue
            if "/" in hostname:
                continue
            else:
                raise Exception("not a valid cidr '%s' address" % hostname)

@then('verify the database has mirrors')
def impl(context):
    if _get_mirror_count() == 0:
        raise Exception('No mirrors found')


@given('gpaddmirrors adds mirrors with options "{options}"')
@given('gpaddmirrors adds mirrors')
@when('gpaddmirrors adds mirrors')
@then('gpaddmirrors adds mirrors')
def impl(context, options=" "):
    add_mirrors(context, options)


@given('gpaddmirrors adds mirrors with temporary data dir')
def impl(context):
    context.mirror_config = _generate_input_config()
    mdd = os.getenv('MASTER_DATA_DIRECTORY', "")
    del os.environ['MASTER_DATA_DIRECTORY']
    try:
        cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s -d %s' % (context.mirror_config, mdd))
        cmd.run(validateAfter=True)
    finally:
        os.environ['MASTER_DATA_DIRECTORY'] = mdd


@given('gpaddmirrors adds mirrors in spread configuration')
def impl(context):
    context.mirror_config = _generate_input_config(spread=True)
    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % context.mirror_config)
    cmd.run(validateAfter=True)


@then('save the gparray to context')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    context.gparray = gparray


@then('mirror hostlist matches the one saved in context')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    old_content_to_host = {}
    curr_content_to_host = {}

    # Map content IDs to hostnames for every mirror, for both the saved GpArray
    # and the current one.
    for (array, hostMap) in [(context.gparray, old_content_to_host), (gparray, curr_content_to_host)]:
        for host in array.get_hostlist(includeMaster=False):
            for mirror in array.get_list_of_mirror_segments_on_host(host):
                hostMap[mirror.getSegmentContentId()] = host

    if len(curr_content_to_host) != len(old_content_to_host):
        raise Exception("Number of mirrors doesn't match between old and new clusters")

    for key in old_content_to_host.keys():
        if curr_content_to_host[key] != old_content_to_host[key]:
            raise Exception("Mirror host doesn't match for content %s (old host=%s) (new host=%s)"
            % (key, old_content_to_host[key], curr_content_to_host[key]))


@given('verify that mirror segments are in "{mirror_config}" configuration')
@then('verify that mirror segments are in "{mirror_config}" configuration')
def impl(context, mirror_config):
    if mirror_config not in ["group", "spread"]:
        raise Exception('"%s" is not a valid mirror configuration for this step; options are "group" and "spread".')

    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    host_list = gparray.get_hostlist(includeMaster=False)

    primary_to_mirror_host_map = {}
    primary_content_map = {}
    # Create a map from each host to the hosts holding the mirrors of all the
    # primaries on the original host, e.g. the primaries for contents 0 and 1
    # are on sdw1, the mirror for content 0 is on sdw2, and the mirror for
    # content 1 is on sdw4, then primary_content_map[sdw1] = [sdw2, sdw4]
    for segmentPair in gparray.segmentPairs:
        primary_host, mirror_host = segmentPair.get_hosts()
        pair_content = segmentPair.primaryDB.content

        # Regardless of mirror configuration, a primary should never be mirrored on the same host
        if primary_host == mirror_host:
            raise Exception('Host %s has both primary and mirror for content %d' % (primary_host, pair_content))

        primary_content_map[primary_host] = pair_content
        if primary_host not in primary_to_mirror_host_map:
            primary_to_mirror_host_map[primary_host] = set()
        primary_to_mirror_host_map[primary_host].add(mirror_host)


    if mirror_config == "spread":
        # In spread configuration, each primary on a given host has its mirror
        # on a different host, and no host has both the primary and the mirror
        # for a given segment.  For this to work, the cluster must have N hosts,
        # where N is 1 more than the number of segments on each host.
        # Thus, if the list of mirror hosts for a given primary host consists
        # of exactly the list of hosts in the cluster minus that host itself,
        # the mirrors in the cluster are correctly spread.

        for primary_host in primary_to_mirror_host_map:
            mirror_host_set = primary_to_mirror_host_map[primary_host]

            other_host_set = set(host_list)
            other_host_set.remove(primary_host)
            if other_host_set != mirror_host_set:
                raise Exception('Expected primaries on %s to be mirrored to %s, but they are mirrored to %s' %
                        (primary_host, other_host_set, mirror_host_set))
    elif mirror_config == "group":
        # In group configuration, all primaries on a given host are mirrored to
        # a single other host.
        # Thus, if the list of mirror hosts for a given primary host consists
        # of a single host, and that host is not the same as the primary host,
        # the mirrors in the cluster are correctly grouped.

        for primary_host in primary_to_mirror_host_map:
            num_mirror_hosts = len(primary_to_mirror_host_map[primary_host])

            if num_mirror_hosts != 1:
                raise Exception('Expected primaries on %s to all be mirrored to the same host, but they are mirrored to %d different hosts' %
                        (primary_host, num_mirror_hosts))

@given("a gpmovemirrors directory under '{parent_dir}' with mode '{mode}' is created")
def impl(context, parent_dir, mode):
    make_temp_dir(context,parent_dir, mode)
    context.mirror_context.working_directory = context.temp_base_dir


@given("a '{file_type}' gpmovemirrors file is created")
def impl(context, file_type):
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()
    mirror = segments[0].mirrorDB

    valid_config = '%s|%s|%s' % (mirror.getSegmentHostName(),
                                 mirror.getSegmentPort(),
                                 mirror.getSegmentDataDirectory())

    if file_type == 'malformed':
        contents = 'I really like coffee.'
    elif file_type == 'badhost':
        badhost_config = '%s|%s|%s' % ('badhost',
                                       mirror.getSegmentPort(),
                                       context.mirror_context.working_directory)
        contents = '%s %s' % (valid_config, badhost_config)
    elif file_type == 'samedir':
        valid_config_with_same_dir = '%s|%s|%s' % (
            mirror.getSegmentHostName(),
            mirror.getSegmentPort() + 1000,
            mirror.getSegmentDataDirectory()
        )
        contents = '%s %s' % (valid_config, valid_config_with_same_dir)
    elif file_type == 'identicalAttributes':
        valid_config_with_identical_attributes = '%s|%s|%s' % (
            mirror.getSegmentHostName(),
            mirror.getSegmentPort(),
            mirror.getSegmentDataDirectory()
        )
        contents = '%s %s' % (valid_config, valid_config_with_identical_attributes)
    elif file_type == 'good':
        valid_config_with_different_dir = '%s|%s|%s' % (
            mirror.getSegmentHostName(),
            mirror.getSegmentPort(),
            context.mirror_context.working_directory
        )
        contents = '%s %s' % (valid_config, valid_config_with_different_dir)
    else:
        raise Exception('unknown gpmovemirrors file_type specified')

    context.mirror_context.input_file = "gpmovemirrors_%s.txt" % file_type
    with open(context.mirror_context.input_file_path(), 'w') as fd:
        fd.write(contents)


@when('the user runs gpmovemirrors')
def impl(context):
    run_gpmovemirrors(context)


@when('the user runs gpmovemirrors with additional args "{extra_args}"')
def run_gpmovemirrors(context, extra_args=''):
    cmd = "gpmovemirrors --input=%s %s" % (
        context.mirror_context.input_file_path(), extra_args)
    run_gpcommand(context, cmd)


@then('verify that mirrors are recognized after a restart')
def impl(context):
    context.execute_steps( u'''
    When an FTS probe is triggered
    And the user runs "gpstop -a"
    And wait until the process "gpstop" goes down
    And the user runs "gpstart -a"
    And wait until the process "gpstart" goes down
    Then all the segments are running
    And the segments are synchronized''')


@given('a sample gpmovemirrors input file is created in "{mirror_config}" configuration')
def impl(context, mirror_config):
    if mirror_config not in ["group", "spread"]:
        raise Exception('"%s" is not a valid mirror configuration for this step; options are "group" and "spread".')

    # Port numbers and addresses are hardcoded to TestCluster values, assuming a 3-host 2-segment cluster.
    input_filename = "/tmp/gpmovemirrors_input_%s" % mirror_config
    line_template = "%s|%d|%s %s|%d|%s\n"

    # The mirrors for contents 0 and 3 are excluded from the two maps below because they are the same in either configuration
    # NOTE: this configuration of the GPDB cluster assumes that configuration set up in concourse's
    #   gpinitsystem task.  The maps below are from {contentID : (port|hostname)}.

    # Group mirroring (TestCluster default): sdw1 mirrors to sdw2, sdw2 mirrors to sdw3, sdw3 mirrors to sdw2
    group_port_map = {0: 21000, 1: 21001, 2: 21000, 3: 21001, 4: 21000, 5: 21001}
    group_address_map = {0: "sdw2", 1: "sdw2", 2: "sdw3", 3: "sdw3", 4: "sdw1", 5: "sdw1"}

    # Spread mirroring: each host mirrors one primary to each of the other two hosts
    spread_port_map = {0: 21000, 1: 21000, 2: 21000, 3: 21001, 4: 21001, 5: 21001}
    spread_address_map = {0: "sdw2", 1: "sdw3", 2: "sdw1", 3: "sdw3", 4: "sdw1", 5: "sdw2"}

    # Create a map from each host to the hosts holding the mirrors of all the
    # primaries on the original host, e.g. the primaries for contents 0 and 1
    # are on sdw1, the mirror for content 0 is on sdw2, and the mirror for
    # content 1 is on sdw4, then primary_content_map[sdw1] = [sdw2, sdw4]
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    with open(input_filename, "w") as fd:
        for content in [1,2,4,5]:
            if mirror_config == "spread":
                old_port = group_port_map[content]
                old_address = group_address_map[content]
                new_port = spread_port_map[content]
                new_address = spread_address_map[content]
            else:
                old_port = spread_port_map[content]
                old_address = spread_address_map[content]
                new_port = group_port_map[content]
                new_address = group_address_map[content]

            mirrors = map(lambda segmentPair: segmentPair.mirrorDB, gparray.getSegmentList())
            mirror = next(iter(filter(lambda mirror: mirror.getSegmentContentId() == content, mirrors)), None)

            old_directory = mirror.getSegmentDataDirectory()
            new_directory = '%s_moved' % old_directory

            fd.write(line_template % (old_address, old_port, old_directory, new_address, new_port, new_directory))
        fd.flush()
