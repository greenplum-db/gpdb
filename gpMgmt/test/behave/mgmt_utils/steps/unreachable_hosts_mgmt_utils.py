import subprocess
import tempfile

from behave import given, when, then

from gppylib.gparray import GpArray
from gppylib.db import dbconn

# These utilities are intended to help various Behave tests handle disconnecting
# and reconnecting hosts from the current GPDB cluster.  They are not intended to be
# tied to any particular hardware implementation, and that's mostly the case.

@given('segment hosts "{disconnected}" are disconnected from the cluster and from the spare segment hosts "{spare}"')
@when('segment hosts "{disconnected}" are disconnected from the cluster and from the spare segment hosts "{spare}"')
@then('segment hosts "{disconnected}" are disconnected from the cluster and from the spare segment hosts "{spare}"')
def impl(context, disconnected, spare):
    disconnected_hosts = disconnected.split(',')
    spare_hosts = []
    if spare != "none":
        spare_hosts = spare.split(',')

    add_or_remove_blackhole_route(disconnected_hosts, spare_hosts, disconnect=True)


@given('segment hosts "{reconnected}" are reconnected to the cluster and to the spare segment hosts "{spare}"')
@when('segment hosts "{reconnected}" are reconnected to the cluster and to the spare segment hosts "{spare}"')
@then('segment hosts "{reconnected}" are reconnected to the cluster and to the spare segment hosts "{spare}"')
def impl(context, reconnected, spare):
    reconnected_hosts = reconnected.split(',')
    spare_hosts = []
    if spare != "none":
        spare_hosts = spare.split(',')

    add_or_remove_blackhole_route(reconnected_hosts, spare_hosts)


# We simulate a physical cutting of the ethernet cable by adding a blackhole
# route from each other node to it.
# For multiple disconnected hosts, we must disconnect them from each other first,
# since otherwise cdw will not be able to reach them to do so.
#           hosts                          disconnected_hosts      spare_hosts
# ['cdw', 'sdw1', 'sdw2', 'sdw3', 'sdw4'], ['sdw1', 'sdw3'], ['sdw5', 'sdw6'] (disconnect)
# ['cdw', 'sdw2', 'sdw4', 'sdw5', 'sdw6'], ['sdw1', 'sdw3'], [])              (reconnect)
def add_or_remove_blackhole_route(disconnected_hosts, spare_hosts, disconnect=False):
    hosts = GpArray.initFromCatalog(dbconn.DbURL()).getHostList()
    hosts.extend(spare_hosts)

    if disconnect:
        # sdw1 -x- sdw3  and  sdw3 -x- sdw1
        for host in disconnected_hosts:
            _blackhole_route_helper(host, disconnected_hosts, disconnect=True)

        for host in disconnected_hosts:
            # we have already disconnected the disconnected_hosts from each other above
            if hosts.count(host):
                hosts.remove(host)

        # cdw, sdw2, sdw4, sdw5, sdw6 -x- (sdw1, sdw3)
        for host in disconnected_hosts:
            _blackhole_route_helper(host, hosts, disconnect=True)
    else:
        # cdw, sdw2, sdw4, sdw5, sdw6 --- (sdw1, sdw3)
        for host in disconnected_hosts:
            _blackhole_route_helper(host, hosts)

        # sdw1 --- sdw3  and  sdw3 --- sdw1
        for host in disconnected_hosts:
            _blackhole_route_helper(host, disconnected_hosts)


# do not disconnect the host from itself, as this adds nothing to the test
def _blackhole_route_helper(disconnect_host, hosts, disconnect=False):
    cmd = "cat /etc/hosts | grep {} | head -1 ".format(disconnect_host)
    cmd += "| awk '{print $1}'"
    disconnect_addr = subprocess.check_output(["bash", "-c", cmd])
    disconnect_addr = disconnect_addr.decode('utf-8')
    disconnect_addr = disconnect_addr.strip()

    for host in hosts:
        if host == disconnect_host:
            continue

        subcmd = "add blackhole"
        if not disconnect:
            subcmd = "delete"

        cmd = "sudo ip route {} {}".format(subcmd, disconnect_addr)
        subprocess.check_output(["ssh", host, cmd])

@given('all postgres processes are killed on "{disconnected}" hosts')
def impl(context, disconnected):
    disconnected_hosts = disconnected.split(',')

    # clean up disconnected
    cmds = ["pkill -9 postgres",
            "rm /tmp/.s.PGSQL.*",
            "rm -fr /data/gpdata/primary/*",
            "rm -fr /data/gpdata/mirror/*"]
    for host in disconnected_hosts:
        for cmd in cmds:
            subprocess.check_output(["ssh", host, cmd])

# This step is very specific to the CCP CI cluster.
@given('the original cluster state is recreated for "{test_case}"')
@when('the original cluster state is recreated for "{test_case}"')
@then('the original cluster state is recreated for "{test_case}"')
def impl(context, test_case):
    # We will bring the cluster back to it's original state by
    # reconnecting the down host(s) and then making the segments on the spare host(s)
    # fall back to their original(read down) hosts. Note that because of -p, the port numbers on the
    # spare segment hosts are not the same as the original hosts.
    # For one_host_down: down host is sdw1 and spare host is sdw5
    # For two_hosts_down: down hosts are sdw1,sdw3 and spare hosts are sdw5,sdw6
    if test_case == "one_host_down":
        down = 'sdw1'
        spare = 'sdw5'
        hostname_filter = "hostname in ('sdw5')"
        expected_config = '''sdw5|20000|/data/gpdata/primary/gpseg0 sdw1|20000|/data/gpdata/primary/gpseg0
sdw5|20001|/data/gpdata/primary/gpseg1 sdw1|20001|/data/gpdata/primary/gpseg1
sdw5|20002|/data/gpdata/mirror/gpseg6 sdw1|21000|/data/gpdata/mirror/gpseg6
sdw5|20003|/data/gpdata/mirror/gpseg7 sdw1|21001|/data/gpdata/mirror/gpseg7'''
    elif test_case == "one_host_down-1":
        down = 'sdw1'
        spare = 'sdw5'
        hostname_filter = "hostname in ('sdw5')"
        expected_config = '''sdw5|20000|/data/gpdata/primary/gpseg0 sdw1|20000|/data/gpdata/primary/gpseg0
sdw5|20001|/data/gpdata/primary/gpseg1 sdw1|20001|/data/gpdata/primary/gpseg1
sdw5|21000|/data/gpdata/mirror/gpseg6 sdw1|21000|/data/gpdata/mirror/gpseg6
sdw5|21001|/data/gpdata/mirror/gpseg7 sdw1|21001|/data/gpdata/mirror/gpseg7'''
    elif test_case == "one_host_down-2":
        down = 'sdw1'
        spare = 'sdw5'
        hostname_filter = "hostname in ('sdw5')"
        expected_config = '''sdw5|20000|/data/gpdata/primary/gpseg0 sdw1|20000|/data/gpdata/primary/gpseg0
sdw1|20001|/data/gpdata/primary/gpseg1 sdw1|20001|/data/gpdata/primary/gpseg1
sdw1|21001|/data/gpdata/mirror/gpseg7 sdw1|21001|/data/gpdata/mirror/gpseg7
sdw5|21000|/data/gpdata/mirror/gpseg6 sdw1|21000|/data/gpdata/mirror/gpseg6'''
    elif test_case == "one_host_down-3":
        down = 'sdw1'
        spare = 'sdw5'
        hostname_filter = "hostname in ('sdw5')"
        expected_config = '''sdw5|20000|/data/gpdata/primary/gpseg0 sdw1|20000|/data/gpdata/primary/gpseg0
sdw5|20001|/data/gpdata/primary/gpseg1 sdw1|20001|/data/gpdata/primary/gpseg1
sdw1|21001|/data/gpdata/mirror/gpseg7 sdw1|21001|/data/gpdata/mirror/gpseg7
sdw1|21000|/data/gpdata/mirror/gpseg6 sdw1|21000|/data/gpdata/mirror/gpseg6'''
    elif test_case == "two_hosts_down":
        down = 'sdw1,sdw3'
        spare = 'sdw5,sdw6'
        hostname_filter = "hostname in ('sdw5', 'sdw6')"
        expected_config = '''sdw5|20000|/data/gpdata/primary/gpseg0 sdw1|20000|/data/gpdata/primary/gpseg0
sdw5|20001|/data/gpdata/primary/gpseg1 sdw1|20001|/data/gpdata/primary/gpseg1
sdw5|20002|/data/gpdata/mirror/gpseg6 sdw1|21000|/data/gpdata/mirror/gpseg6
sdw5|20003|/data/gpdata/mirror/gpseg7 sdw1|21001|/data/gpdata/mirror/gpseg7
sdw6|20002|/data/gpdata/primary/gpseg4 sdw3|20000|/data/gpdata/primary/gpseg4
sdw6|20003|/data/gpdata/primary/gpseg5 sdw3|20001|/data/gpdata/primary/gpseg5
sdw6|20000|/data/gpdata/mirror/gpseg2 sdw3|21000|/data/gpdata/mirror/gpseg2
sdw6|20001|/data/gpdata/mirror/gpseg3 sdw3|21001|/data/gpdata/mirror/gpseg3'''
    else:
        raise Exception('Invalid test case')

    with tempfile.NamedTemporaryFile() as config_file:
        config_file.write(expected_config.encode('utf-8'))
        config_file.flush()

        context.execute_steps("""
            Given segment hosts "{down}" are reconnected to the cluster and to the spare segment hosts "none"
            And all postgres processes are killed on "{down}" hosts
            And user stops all mirror processes on "{spare}"
            And user stops all primary processes on "{spare}"
            And the cluster configuration has no segments where "{hostname_filter} and status='u'"
            And the user runs "gprecoverseg -a -i {config_file_path}"
            Then gprecoverseg should return a return code of 0
            And all the segments are running
            And the cluster is rebalanced""".format(down=down, spare=spare, hostname_filter=hostname_filter,
                                                    config_file_path=config_file.name))
