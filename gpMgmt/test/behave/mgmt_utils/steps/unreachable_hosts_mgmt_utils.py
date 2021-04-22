import subprocess

from behave import given, when, then

from gppylib.gparray import GpArray
from gppylib.db import dbconn

# These utilities are intended to help various Behave tests handle disconnecting
# and reconnecting hosts from the current GPDB cluster.  They are not intended to be
# tied to any particular hardware implementation, and that's mostly the case.

@given('segment hosts "{disconnected}" are disconnected from the cluster and from the spare segment hosts "{spare}"')
def impl(context, disconnected, spare):
    disconnected_hosts = disconnected.split(',')
    spare_hosts = spare.split(',')

    add_or_remove_blackhole_route(disconnected_hosts, spare_hosts, disconnect=True)


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
# since otherwise mdw will not be able to reach them to do so.
#           hosts                          disconnected_hosts      spare_hosts
# ['mdw', 'sdw1', 'sdw2', 'sdw3', 'sdw4'], ['sdw1', 'sdw3'], ['sdw5', 'sdw6'] (disconnect)
# ['mdw', 'sdw2', 'sdw4', 'sdw5', 'sdw6'], ['sdw1', 'sdw3'], [])              (reconnect)
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

        # mdw, sdw2, sdw4, sdw5, sdw6 -x- (sdw1, sdw3)
        for host in disconnected_hosts:
            _blackhole_route_helper(host, hosts, disconnect=True)
    else:
        # mdw, sdw2, sdw4, sdw5, sdw6 --- (sdw1, sdw3)
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
    disconnect_addr = disconnect_addr.strip()

    for host in hosts:
        if host == disconnect_host:
            continue

        subcmd = "add blackhole"
        if not disconnect:
            subcmd = "delete"

        cmd = "sudo ip route {} {}".format(subcmd, disconnect_addr)
        subprocess.check_output(["ssh", host, cmd])


# This step is very specific to the CCP CI cluster.
@then('the original cluster state is recreated after cleaning up "{disconnected}" hosts')
def impl(context, disconnected):
    disconnected_hosts = disconnected.split(',')

    # delete the current cluster
    cmd = '''
source /usr/local/greenplum-db-devel/greenplum_path.sh
yes | gpdeletesystem -fD -d /data/gpdata/master/gpseg-1
'''
    subprocess.check_output(["bash", "-c", cmd])

    # clean up disconnected
    cmds = ["pkill -9 postgres",
            "rm /tmp/.s.PGSQL.*",
            "rm -fr /data/gpdata/primary/*",
            "rm -fr /data/gpdata/mirror/*"]
    for host in disconnected_hosts:
        for cmd in cmds:
            subprocess.check_output(["ssh", host, cmd])

    # reinitialize the cluster...see concourse-cluster-provisioner/scripts/gpinitsystem.sh
    cmd = '''
source /usr/local/greenplum-db-devel/greenplum_path.sh
if grep --quiet photon /etc/os-release
then
    gpinitsystem -a -n en_US.UTF-8 -c ~gpadmin/gpinitsystem_config -h ~gpadmin/segment_host_list || :
else
    gpinitsystem -a -c ~gpadmin/gpinitsystem_config -h ~gpadmin/segment_host_list || :
fi
'''
    subprocess.check_output(["bash", "-c", cmd])
