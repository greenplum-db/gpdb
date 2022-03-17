from collections import defaultdict
import os
from os import path
from contextlib import closing
from gppylib.commands.base import REMOTE
import socket
import subprocess
import tempfile

from gppylib.commands.gp import get_coordinatordatadir

from behave import given, when, then
from test.behave_utils.utils import *

from test.behave.mgmt_utils.steps.mgmt_utils import *

# This file contains steps for gpaddmirrors and gpmovemirrors tests

# This class is intended to store per-Scenario state that is built up over
# a series of steps.
class MirrorMgmtContext:
    def __init__(self):
        self.working_directory = None
        self.input_file = None

    def input_file_path(self):
        if not self.working_directory:
            raise Exception("working directory not set")
        if self.input_file is None:
            raise Exception("input file not set")
        return path.normpath(path.join(self.working_directory[0], self.input_file))


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


@when('gpaddmirrors adds 3 mirrors with additional args "{args}"' )
def add_three_mirrors_with_args(context, args):
    datadir_config = _write_datadir_config_for_three_mirrors()
    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -o %s -m %s' % (mirror_config_output_file, datadir_config)
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)
    cmd = 'gpaddmirrors -a -v -i %s %s' % (mirror_config_output_file, args)
    run_gpcommand(context, command=cmd)

@when("gpaddmirrors adds 3 mirrors")
def add_three_mirrors(context):
    add_three_mirrors_with_args(context, '')

@when("gpaddmirrors adds 3 mirrors with one mirror's datadir not empty")
def add_three_mirrors_with_one_mirror_not_empty(context):
    datadir_config = _write_datadir_config_for_three_mirrors()
    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -o %s -m %s' % (mirror_config_output_file, datadir_config)
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)

    # Get the path of one of the datadirs from the config file
    datadir_cmd = "cat {} | tail -1 | awk '-F|' '{{print $4}}'".format(mirror_config_output_file)
    datadir = subprocess.check_output(["bash", "-c", datadir_cmd]).decode('utf-8').strip()

    os.mkdir(datadir, mode=0o700)
    # Create a file within this datadir to make gprecoverseg fail with a validation error
    with tempfile.NamedTemporaryFile(dir=datadir):
        cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s' % (mirror_config_output_file))
        cmd.run(validateAfter=False)

    context.ret_code = cmd.get_results().rc
    context.stdout_message = cmd.get_results().stdout
    context.error_message = cmd.get_results().stderr


def add_mirrors(context, options):
    context.mirror_config = _generate_input_config()
    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s %s' % (context.mirror_config, options))
    cmd.run(validateAfter=True)


def make_data_directory_called(data_directory_name):
    cdd_parent_parent = os.path.realpath(
        get_coordinatordatadir()+ "../../../")
    mirror_data_dir = os.path.join(cdd_parent_parent, data_directory_name)
    if not os.path.exists(mirror_data_dir):
        os.mkdir(mirror_data_dir)
    return mirror_data_dir


def _get_mirror_count():
    with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
        sql = """SELECT count(*) FROM gp_segment_configuration WHERE role='m'"""
        count_row = dbconn.query(conn, sql).fetchone()
    return count_row[0]

# take the item in search_item_list, search pg_hba if it contains atleast one entry
# for the item
@given('pg_hba file "{filename}" on host "{host}" contains entries for "{search_items}"')
@then('pg_hba file "{filename}" on host "{host}" contains entries for "{search_items}"')
def impl(context, filename, host, search_items):
    cmd_str = "ssh %s cat %s" % (host, filename)
    cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
    cmd.run(validateAfter=False)
    search_item_list = [search_item.strip() for search_item in search_items.split(',')]
    pghba_contents= cmd.get_stdout().strip().split('\n')
    for search_item in search_item_list:
        found = False
        if search_item == 'samehost':
            search_hostname = 'samehost'
            search_ip_addr = ['samehost']
        else:
            search_hostname, _, search_ip_addr = socket.gethostbyaddr(search_item)
        for entry in pghba_contents:
            contents = entry.strip()
            # for example: host all all hostname    trust
            if contents.startswith("host") and contents.endswith("trust"):
                tokens = contents.split()
                if len(tokens) != 5:
                    raise Exception("failed to parse pg_hba.conf line '%s'" % contents)
                hostname = tokens[3].strip()
                if (search_item == hostname) or (search_hostname == hostname) or (search_ip_addr[0] in hostname):
                    found = True
                    break
        if not found:
            raise Exception("entry for expected item %s, ip_addr[0] %s not existing in pg_hba.conf '%s'"
                            % (search_item, search_ip_addr[0], pghba_contents))


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
            if hostname == "samehost":
                continue
            if "/" in hostname:
                continue
            else:
                raise Exception("not a valid cidr '%s' address" % hostname)


@then('verify the database has mirrors')
def impl(context):
    if _get_mirror_count() == 0:
        raise Exception('No mirrors found')


@given('verify the database has {expected_mirror_count} mirrors')
@when('verify the database has {expected_mirror_count} mirrors')
@then('verify the database has {expected_mirror_count} mirrors')
def impl(context, expected_mirror_count):
    expected_mirror_count = 0 if expected_mirror_count == 'no' else int(expected_mirror_count)
    actual_mirror_count = _get_mirror_count()
    if actual_mirror_count != expected_mirror_count:
        raise Exception('Expected {} mirrors but found {}'.format(expected_mirror_count, actual_mirror_count))


@given('gpaddmirrors adds mirrors with options "{options}"')
@when('gpaddmirrors adds mirrors with options "{options}"')
@given('gpaddmirrors adds mirrors')
@when('gpaddmirrors adds mirrors')
@when('gpaddmirrors adds mirrors with options ""')
@then('gpaddmirrors adds mirrors')
def impl(context, options=" "):
    add_mirrors(context, options)


@given('gpaddmirrors adds mirrors with temporary data dir')
def impl(context):
    context.mirror_config = _generate_input_config()
    cdd = get_coordinatordatadir()
    del os.environ['COORDINATOR_DATA_DIRECTORY']
    try:
        cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s -d %s' % (context.mirror_config, cdd))
        cmd.run(validateAfter=True)
    finally:
        os.environ['COORDINATOR_DATA_DIRECTORY'] = cdd


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
        for host in array.get_hostlist(includeCoordinator=False):
            for mirror in array.get_list_of_mirror_segments_on_host(host):
                hostMap[mirror.getSegmentContentId()] = host

    if len(curr_content_to_host) != len(old_content_to_host):
        raise Exception("Number of mirrors doesn't match between old and new clusters")

    for key in list(old_content_to_host.keys()):
        if curr_content_to_host[key] != old_content_to_host[key]:
            raise Exception("Mirror host doesn't match for content %s (old host=%s) (new host=%s)"
            % (key, old_content_to_host[key], curr_content_to_host[key]))


@given('a gpmovemirrors cross_subnet input file is created')
def impl(context):
    context.expected_segs = []

    context.expected_segs.append("sdw1-1|21500|/tmp/gpmovemirrors/data/mirror/gpseg2_moved")
    context.expected_segs.append("sdw1-2|22501|/tmp/gpmovemirrors/data/mirror/gpseg3")

    input_filename = "/tmp/gpmovemirrors_input_cross_subnet"
    with open(input_filename, "w") as fd:
        fd.write("sdw1-1|21500|/tmp/gpmovemirrors/data/mirror/gpseg2 %s\n" % context.expected_segs[0])
        fd.write("sdw1-1|21501|/tmp/gpmovemirrors/data/mirror/gpseg3 %s" % context.expected_segs[1])


@then('verify that mirror segments are in new cross_subnet configuration')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segs = gparray.getSegmentsAsLoadedFromDb()
    actual_segs = [
        "%s|%s|%s" % (seg.hostname, seg.port, seg.datadir)
        for seg in segs
        if seg.role == 'm' and seg.content in [2, 3]
    ]

    if len(context.expected_segs) != len(actual_segs):
        raise Exception("expected number of segs to be %d, but got %d" % (len(context.expected_segs), len(actual_segs)))
    if context.expected_segs != actual_segs:
        raise Exception("expected segs to be %s, but got %s" % (context.expected_segs, actual_segs))


@given('verify that mirror segments are in "{mirror_config}" configuration')
@then('verify that mirror segments are in "{mirror_config}" configuration')
def impl(context, mirror_config):
    if mirror_config not in ["group", "spread"]:
        raise Exception('"%s" is not a valid mirror configuration for this step; options are "group" and "spread".')

    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    host_list = gparray.get_hostlist(includeCoordinator=False)

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


@given("a {utility} input file is created")
@when("a {utility} input file is created")
@then("a {utility} input file is created")
def impl(context, utility):
    context.mirror_context.input_file = "{}_config.txt".format(utility)
    context.data_dirs_created = defaultdict(list)
    context.old_data_dirs = defaultdict(list)
    context.mirror_new_location = defaultdict(str)
    open(context.mirror_context.input_file_path(), 'w').close()


@given("edit the input file to recover mirror with content {content_ids} to a new directory with mode {mode}")
@given("edit the input file to move mirror with content {content_ids} to a new directory with mode {mode}")
def impl(context, content_ids, mode):
    if content_ids == "None":
        return
    for content in [int(c) for c in content_ids.split(',')]:
        make_temp_dir(context, context.mirror_context.working_directory[0], mode)
        target_datadir = context.temp_base_dir
        segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

        for seg in segments:
            if seg.mirrorDB.getSegmentContentId() == content:
                mirror = seg.mirrorDB
                valid_config = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                                 mirror.getSegmentPort(),
                                                 mirror.getSegmentDataDirectory())
                context.old_data_dirs[content] = [mirror.getSegmentHostName(), mirror.getSegmentDataDirectory()]

                valid_config_new = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                                     mirror.getSegmentPort(),
                                                     target_datadir)
                with open(context.mirror_context.input_file_path(), 'a') as fd:
                    fd.write('{} {}\n'.format(valid_config, valid_config_new))
                context.data_dirs_created[seg.mirrorDB.getSegmentHostName()].append(target_datadir)
                context.mirror_new_location[content] = valid_config_new
                break


@given("edit the input file to add mirror with content {content_ids} to a new directory with mode {mode}")
def impl(context, content_ids, mode):
    if content_ids == "None":
        return
    for content in [int(c) for c in content_ids.split(',')]:
        make_temp_dir(context, context.mirror_context.working_directory[0], mode)
        new_datadir = context.temp_base_dir
        segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

        for seg in segments:
            if seg.primaryDB.getSegmentContentId() == content:
                new_port = seg.primaryDB.getSegmentPort() + 100 * (content + 1)
                valid_config_new = '{}|{}|{}|{}'.format(content , seg.primaryDB.getSegmentHostName(), new_port,
                                                        new_datadir)
                with open(context.mirror_context.input_file_path(), 'a') as fd:
                    fd.write('{}\n'.format(valid_config_new))
                context.data_dirs_created[seg.primaryDB.getSegmentHostName()].append(new_datadir)
                new_location = '{}|{}|{}'.format(seg.primaryDB.getSegmentHostName(), new_port, new_datadir)
                context.mirror_new_location[content] = new_location
                break


@given("edit the input file to add mirror on host {host} with contents {content_ids} to a new directory with mode {mode}")
def impl(context, content_ids, host, mode):
    if content_ids == "None":
        return
    for content in [int(c) for c in content_ids.split(',')]:
        make_temp_dir_on_remote(context, host, '/tmp', mode)
        new_datadir = context.temp_base_dir_remote
        segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

        for seg in segments:
            if seg.primaryDB.getSegmentContentId() == content:
                new_port = seg.primaryDB.getSegmentPort() + 100 * (content + 1)
                valid_config_new = '{}|{}|{}|{}'.format(seg.primaryDB.getSegmentContentId(), host, new_port, new_datadir)
                with open(context.mirror_context.input_file_path(), 'a') as fd:
                    fd.write('{}\n'.format(valid_config_new))
                context.data_dirs_created[host].append(new_datadir)
                new_location = '{}|{}|{}'.format(host, new_port, new_datadir)
                context.mirror_new_location[content] = new_location
                break


@given("edit the input file to recover mirror with content {content} to a new directory on remote host with mode {mode}")
def impl(context, content, mode):
    content = int(content)
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

    for seg in segments:
        if seg.mirrorDB.getSegmentContentId() == content:
            mirror = seg.mirrorDB
            valid_config = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                             mirror.getSegmentPort(),
                                             mirror.getSegmentDataDirectory())
            context.old_data_dirs[content] = [mirror.getSegmentHostName(), mirror.getSegmentDataDirectory()]

            make_temp_dir_on_remote(context, mirror.getSegmentHostName(), '/tmp', mode)
            new_datadir = context.temp_base_dir_remote
            valid_config_new = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                                 mirror.getSegmentPort(),
                                                 new_datadir)
            with open(context.mirror_context.input_file_path(), 'a') as fd:
                fd.write('{} {}\n'.format(valid_config, valid_config_new))
            context.data_dirs_created[seg.mirrorDB.getSegmentHostName()].append(new_datadir)
            context.mirror_new_location[content] = valid_config_new
            break


@then('the old data directories are cleaned up for content {content_ids}')
def impl(context, content_ids):
    for content in [int(c) for c in content_ids.split(',')]:
        old_host, old_datadir = context.old_data_dirs[content]
        rm_cmd = Command(name="Remove old datadirs on remote host", cmdStr='rm -rf {}'.format(old_datadir),
                      remoteHost=old_host, ctxt=REMOTE)
        rm_cmd.run(validateAfter=True)


@given("the mode of all the created data directories is changed to 0700")
@when("the mode of all the created data directories is changed to 0700")
@then("the mode of all the created data directories is changed to 0700")
def impl(context):
    for hostname, data_dirs in context.data_dirs_created.items():
        for data_dir in data_dirs:
            command = 'ssh {} "chmod -R 700 {}"'.format(hostname, data_dir)
            run_command(context, command)

    context.data_dirs_created.clear()

#TODO improve this function
def make_temp_dir_on_remote(context, hostname, tmp_base_dir_remote, mode='700'):
    if not tmp_base_dir_remote:
        raise Exception("tmp_base_dir cannot be empty")

    tempfile_cmd = Command(name="Create temp directory on remote host",
                           cmdStr=""" python -c "import tempfile; t=tempfile.mkdtemp(dir='{}');print(t)" """
                           .format(tmp_base_dir_remote),
                           remoteHost=hostname, ctxt=REMOTE)
    tempfile_cmd.run(validateAfter=True)
    tmp_dir = tempfile_cmd.get_results().stdout.strip()
    tmp_dir = "{}/recoverydir_{}".format(tmp_dir, mode)

    mkdir_cmd = Command(name="create directory remote",
                        cmdStr="mkdir -p {}; chmod {} {}".format(tmp_dir, mode, tmp_dir ),
                        remoteHost=hostname, ctxt=REMOTE)
    mkdir_cmd.run(validateAfter=True)
    context.temp_base_dir_remote = tmp_dir


@given('datadirs from "{config}" configuration for "{old_host}" are created on "{new_host}" with mode {mode}')
@then('datadirs from "{config}" configuration for "{old_host}" are created on "{new_host}" with mode {mode}')
def impl(context, config, old_host, new_host, mode):
    gparray_before = context.saved_array[config]
    for seg in gparray_before.getDbList():
        if seg.hostname == old_host:
            mkdir_cmd = Command(name="create directory remote",
                                cmdStr="mkdir -p {}; chmod {} {}".format(seg.datadir, mode, seg.datadir),
                                remoteHost=new_host, ctxt=REMOTE)
            mkdir_cmd.run(validateAfter=True)


@given("edit the input file to recover mirror with content {content} full inplace")
def impl(context, content):
    content = int(content)
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()
    for seg in segments:
        if seg.mirrorDB.getSegmentContentId() == content:
            mirror = seg.mirrorDB
            valid_config = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                             mirror.getSegmentPort(),
                                             mirror.getSegmentDataDirectory())
            with open(context.mirror_context.input_file_path(), 'a') as fd:
                fd.write('{} {}\n'.format(valid_config, valid_config))
            break


@given("edit the input file to recover mirror with content {content} incremental")
def impl(context, content):
    content = int(content)
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()
    for seg in segments:
        if seg.mirrorDB.getSegmentContentId() == content:
            mirror = seg.mirrorDB
            valid_config = '{}|{}|{}'.format(mirror.getSegmentHostName(),
                                             mirror.getSegmentPort(),
                                             mirror.getSegmentDataDirectory())
            with open(context.mirror_context.input_file_path(), 'a') as fd:
                fd.write('{}\n'.format(valid_config))
            break


@given("{num} {utility} directory under '{parent_dir}' with mode '{mode}' is created")
@when("{num} {utility} directory under '{parent_dir}' with mode '{mode}' is created")
@then("{num} {utility} directory under '{parent_dir}' with mode '{mode}' is created")
def impl(context, num, utility, parent_dir, mode):
    num_dirs = 1 if num == 'a' else int(num)
    make_temp_dir(context, parent_dir, mode)
    context.mirror_context.working_directory = []
    for i in range(num_dirs):
        ith_dir = os.path.join(context.temp_base_dir, 'tmp_' + str(i))
        os.mkdir(ith_dir, int(mode,8))
        context.mirror_context.working_directory.append(ith_dir)

@given("the mode of the saved data directory is changed to 700")
@when("the mode of the saved data directory is changed to 700")
@then("the mode of the saved data directory is changed to 700")
def impl(context):
    data_dir = context.mirror_context.working_directory[0]
    command = 'chmod -R 700 {}'.format(data_dir)
    run_command(context, command)

@then('check if mirrors on content {content_ids} are moved to new location on input file')
def impl(context, content_ids):
    if content_ids == 'None':
        return
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.getSegmentContentId() in [int(c) for c in content_ids.split(',')], all_segments)
    for seg in segments:
        expected_config = context.mirror_new_location[seg.getSegmentContentId()]
        actual_config = '{}|{}|{}'.format(seg.getSegmentHostName(), seg.getSegmentPort(), seg.getSegmentDataDirectory())

        if not (expected_config == actual_config):
            raise Exception("Expected mirror location '{}' and found: '{}'".format(expected_config, actual_config))


@then('check if mirrors on content {content_ids} are in their original configuration')
def impl(context, content_ids):
    if content_ids == 'None':
        return

    all_segments_actual = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments_to_test = filter(lambda seg: seg.getSegmentContentId() in [int(c) for c in content_ids.split(',')], all_segments_actual)

    for seg in segments_to_test:
        original_seg = context.original_seg_info['{}_{}'.format(seg.getSegmentContentId(), seg.getSegmentPreferredRole())]
        original_config = '{}|{}|{}'.format(original_seg.getSegmentHostName(), original_seg.getSegmentPort(), original_seg.getSegmentDataDirectory())
        actual_config = '{}|{}|{}'.format(seg.getSegmentHostName(), seg.getSegmentPort(), seg.getSegmentDataDirectory())

        if not (original_config == actual_config):
            raise Exception("Expected mirror location '{}' and found: '{}'".format(original_config, actual_config))


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
                                       context.mirror_context.working_directory[0])
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
            context.mirror_context.working_directory[0]
        )
        contents = '%s %s' % (valid_config, valid_config_with_different_dir)
    else:
        raise Exception('unknown gpmovemirrors file_type specified')

    context.mirror_context.input_file = "gpmovemirrors_%s.txt" % file_type
    with open(context.mirror_context.input_file_path(), 'w') as fd:
        fd.write(contents)

@given("a good gpmovemirrors file is created for moving {num} mirrors")
@given("a good gprecoverseg input file is created for moving {num} mirrors")
def impl(context, num):
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()
    contents = ''
    for i in range(int(num)):
        mirror = segments[i].mirrorDB

        valid_config = '%s|%s|%s' % (mirror.getSegmentHostName(),
                                     mirror.getSegmentPort(),
                                     mirror.getSegmentDataDirectory())
        valid_config_with_different_dir = '%s|%s|%s' % (
            mirror.getSegmentHostName(),
            mirror.getSegmentPort(),
            context.mirror_context.working_directory[i]
        )
        contents += '%s %s\n' % (valid_config, valid_config_with_different_dir)
    context.mirror_context.input_file = "gpmovemirrors_good_multi.txt"
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

@when('the user runs {utility} with input file and additional args "{extra_args}"')
def impl(context, utility, extra_args=''):
    cmd = "{} -i {} {}".format(utility, context.mirror_context.input_file_path(), extra_args)
    run_gpcommand(context, cmd)


@when('the user asynchronously runs {utility} with input file and additional args "{extra_args}" and the process is saved')
def impl(context, utility, extra_args=''):
    cmd = "%s -i %s %s" % (utility, context.mirror_context.input_file_path(), extra_args)
    run_gpcommand_async(context, cmd)


@then('verify that mirrors are recognized after a restart')
def impl(context):
    context.execute_steps( '''
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

            mirrors = [segmentPair.mirrorDB for segmentPair in gparray.getSegmentList()]
            mirror = next(iter([mirror for mirror in mirrors if mirror.getSegmentContentId() == content]), None)

            old_directory = mirror.getSegmentDataDirectory()
            new_directory = '%s_moved' % old_directory

            fd.write(line_template % (old_address, old_port, old_directory, new_address, new_port, new_directory))
        fd.flush()

@then('verify the tablespace directories on host "{host}" for content "{content}" are {status}')
def impl(context, host, content, status):
    if status not in ["deleted", "valid"]:
        raise Exception('Unknown status.  Valid values are "deleted" and "valid"')
    locations = []
    existing_dirs =[]
    dbid = -2
    with closing(dbconn.connect(dbconn.DbURL(dbname="postgres"), unsetSearchPath=False)) as conn:
        oids_query = "SELECT oid FROM pg_tablespace WHERE spcname NOT IN ('pg_default', 'pg_global')"
        location_query = "SELECT t.tblspc_loc||'/'||c.dbid FROM gp_tablespace_location(%s) t JOIN gp_segment_configuration c ON t.gp_segment_id = c.content WHERE c.content = %s AND c.preferred_role = 'm'"

        oids = [row[0] for row in dbconn.query(conn, oids_query).fetchall()]
        dbid = dbconn.querySingleton(conn, "SELECT dbid FROM gp_segment_configuration WHERE content = %s AND preferred_role = 'm'" % content)
        for oid in oids:
            locations.append(dbconn.querySingleton(conn, location_query % (oid, content)))

    for location in locations:
        cmd = Command(name="check tablespace dirs", cmdStr="find %s -name %s -type d -print 2> /dev/null ||true" % (location, dbid), ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)
        output = cmd.get_results().stdout.strip()
        if output != '':
            existing_dirs.append(output)

    if status == "deleted":
        if existing_dirs:
            raise Exception("One or more directories have not been deleted:\n%s" % existing_dirs)
    else:
        if existing_dirs != locations:
            missing_dirs = [d for d in locations if d not in existing_dirs]
            raise Exception("One or more directories are not present on %s: %s" % (host, missing_dirs))


@then('the user executes steps required for rerunning gpmovemirrors for contents {contents}')
def impl(context, contents):
    context.execute_steps( '''
    Given a gpmovemirrors input file is created
    And edit the input file to move mirror with content {contents} to a new directory with mode 0700
    When the user runs gpmovemirrors with input file and additional args " "
    And gpmovemirrors should return a return code of 0
    '''.format(contents=contents))


@given('the user executes steps required for running in place full recovery for all failed contents')
@then('the user executes steps required for running in place full recovery for all failed contents')
def impl(context):
    context.execute_steps( '''
    When the user runs "gprecoverseg -aF"
    And gprecoverseg should return a return code of 0
    ''')

@then('the cluster is recovered in full and rebalanced')
def impl(context):
    context.execute_steps(u'''
        Then the user runs "gprecoverseg -aF"
        And gprecoverseg should return a return code of 0
        And user can start transactions
        And the segments are synchronized
        And the cluster is rebalanced
        ''')


@then('the cluster is rebalanced')
def impl(context):
    context.execute_steps(u'''
        Then the user runs "gprecoverseg -a -s -r"
        And gprecoverseg should return a return code of 0
        And user can start transactions
        And the segments are synchronized
        ''')
